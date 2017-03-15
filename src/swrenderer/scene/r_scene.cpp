//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//

#include <stdlib.h>
#include <float.h>

#include "templates.h"
#include "i_system.h"
#include "w_wad.h"
#include "doomdef.h"
#include "doomstat.h"
#include "r_sky.h"
#include "stats.h"
#include "v_video.h"
#include "a_sharedglobal.h"
#include "c_console.h"
#include "c_dispatch.h"
#include "cmdlib.h"
#include "d_net.h"
#include "g_level.h"
#include "p_effect.h"
#include "po_man.h"
#include "st_stuff.h"
#include "r_data/r_interpolate.h"
#include "swrenderer/scene/r_scene.h"
#include "swrenderer/scene/r_light.h"
#include "swrenderer/scene/r_3dfloors.h"
#include "swrenderer/scene/r_opaque_pass.h"
#include "swrenderer/scene/r_translucent_pass.h"
#include "swrenderer/scene/r_portal.h"
#include "swrenderer/segments/r_clipsegment.h"
#include "swrenderer/segments/r_drawsegment.h"
#include "swrenderer/segments/r_portalsegment.h"
#include "swrenderer/plane/r_visibleplanelist.h"
#include "swrenderer/viewport/r_viewport.h"
#include "swrenderer/drawers/r_draw.h"
#include "swrenderer/drawers/r_draw_rgba.h"
#include "swrenderer/drawers/r_thread.h"
#include "swrenderer/r_memory.h"
#include "swrenderer/r_renderthread.h"
#include "swrenderer/things/r_playersprite.h"

EXTERN_CVAR(Bool, r_shadercolormaps)
EXTERN_CVAR(Int, r_clearbuffer)

CVAR(Bool, r_scene_multithreaded, false, 0);

namespace swrenderer
{
	cycle_t WallCycles, PlaneCycles, MaskedCycles, WallScanCycles;
	
	RenderScene::RenderScene()
	{
		Threads.push_back(std::make_unique<RenderThread>(this));
	}

	RenderScene::~RenderScene()
	{
		StopThreads();
	}

	void RenderScene::SetClearColor(int color)
	{
		clearcolor = color;
	}

	void RenderScene::RenderView(player_t *player)
	{
		auto viewport = MainThread()->Viewport.get();
		viewport->RenderTarget = screen;

		int width = SCREENWIDTH;
		int height = SCREENHEIGHT;
		float trueratio;
		ActiveRatio(width, height, &trueratio);
		viewport->SetViewport(MainThread(), width, height, trueratio);

		if (r_clearbuffer != 0)
		{
			if (!viewport->RenderTarget->IsBgra())
			{
				memset(viewport->RenderTarget->GetBuffer(), clearcolor, viewport->RenderTarget->GetPitch() * viewport->RenderTarget->GetHeight());
			}
			else
			{
				uint32_t bgracolor = GPalette.BaseColors[clearcolor].d;
				int size = viewport->RenderTarget->GetPitch() * viewport->RenderTarget->GetHeight();
				uint32_t *dest = (uint32_t *)viewport->RenderTarget->GetBuffer();
				for (int i = 0; i < size; i++)
					dest[i] = bgracolor;
			}
		}

		RenderActorView(player->mo);

		// Apply special colormap if the target cannot do it
		if (CameraLight::Instance()->ShaderColormap() && viewport->RenderTarget->IsBgra() && !(r_shadercolormaps && screen->Accel2D))
		{
			auto queue = std::make_shared<DrawerCommandQueue>(MainThread());
			queue->Push<ApplySpecialColormapRGBACommand>(CameraLight::Instance()->ShaderColormap(), screen);
			DrawerThreads::Execute(queue);
		}

		DrawerThreads::WaitForWorkers();
	}

	void RenderScene::RenderActorView(AActor *actor, bool dontmaplines)
	{
		WallCycles.Reset();
		PlaneCycles.Reset();
		MaskedCycles.Reset();
		WallScanCycles.Reset();
		
		R_SetupFrame(MainThread()->Viewport->viewpoint, MainThread()->Viewport->viewwindow, actor);
		CameraLight::Instance()->SetCamera(MainThread()->Viewport.get(), actor);
		MainThread()->Viewport->SetupFreelook();

		NetUpdate();

		this->dontmaplines = dontmaplines;

		// [RH] Setup particles for this frame
		P_FindParticleSubsectors();

		// Link the polyobjects right before drawing the scene to reduce the amounts of calls to this function
		PO_LinkToSubsectors();

		ActorRenderFlags savedflags = MainThread()->Viewport->viewpoint.camera->renderflags;
		// Never draw the player unless in chasecam mode
		if (!MainThread()->Viewport->viewpoint.showviewer)
		{
			MainThread()->Viewport->viewpoint.camera->renderflags |= RF_INVISIBLE;
		}

		RenderThreadSlices();
		RenderPSprites();

		MainThread()->Viewport->viewpoint.camera->renderflags = savedflags;
		interpolator.RestoreInterpolations();

		// If we don't want shadered colormaps, NULL it now so that the
		// copy to the screen does not use a special colormap shader.
		if (!r_shadercolormaps && !MainThread()->Viewport->RenderTarget->IsBgra())
		{
			CameraLight::Instance()->ClearShaderColormap();
		}
	}

	void RenderScene::RenderPSprites()
	{
		// Player sprites needs to be rendered after all the slices because they may be hardware accelerated.
		// If they are not hardware accelerated the drawers must run after all sliced drawers finished.
		DrawerThreads::WaitForWorkers();
		MainThread()->DrawQueue->Clear();
		MainThread()->PlayerSprites->Render();
		DrawerThreads::Execute(MainThread()->DrawQueue);
	}

	void RenderScene::RenderThreadSlices()
	{
		int numThreads = std::thread::hardware_concurrency();
		if (numThreads == 0)
			numThreads = 4;

		if (!r_scene_multithreaded || !r_multithreaded)
			numThreads = 1;

		if (numThreads != (int)Threads.size())
		{
			StopThreads();
			StartThreads(numThreads);
		}

		// Setup threads:
		std::unique_lock<std::mutex> start_lock(start_mutex);
		for (int i = 0; i < numThreads; i++)
		{
			*Threads[i]->Viewport = *MainThread()->Viewport;
			*Threads[i]->Light = *MainThread()->Light;
			Threads[i]->X1 = viewwidth * i / numThreads;
			Threads[i]->X2 = viewwidth * (i + 1) / numThreads;
		}
		run_id++;
		start_lock.unlock();

		// Notify threads to run
		if (Threads.size() > 1)
		{
			start_condition.notify_all();
		}

		// Do the main thread ourselves:
		RenderThreadSlice(MainThread());

		// Wait for everyone to finish:
		if (Threads.size() > 1)
		{
			std::unique_lock<std::mutex> end_lock(end_mutex);
			finished_threads++;
			end_condition.wait(end_lock, [&]() { return finished_threads == Threads.size(); });
			finished_threads = 0;
		}

		// Change main thread back to covering the whole screen for player sprites
		MainThread()->X1 = 0;
		MainThread()->X2 = viewwidth;
	}

	void RenderScene::RenderThreadSlice(RenderThread *thread)
	{
		thread->DrawQueue->Clear();
		thread->FrameMemory->Clear();
		thread->Clip3D->Cleanup();
		thread->Clip3D->ResetClip(); // reset clips (floor/ceiling)
		thread->Portal->CopyStackedViewParameters();
		thread->ClipSegments->Clear(0, viewwidth);
		thread->DrawSegments->Clear();
		thread->PlaneList->Clear();
		thread->TranslucentPass->Clear();
		thread->OpaquePass->ClearClip();
		thread->OpaquePass->ResetFakingUnderwater(); // [RH] Hack to make windows into underwater areas possible
		thread->Portal->SetMainPortal();

		// Cull things outside the range seen by this thread
		VisibleSegmentRenderer visitor;
		if (thread->X1 > 0)
			thread->ClipSegments->Clip(0, thread->X1, true, &visitor);
		if (thread->X2 < viewwidth)
			thread->ClipSegments->Clip(thread->X2, viewwidth, true, &visitor);

		if (thread->MainThread)
			WallCycles.Clock();

		thread->OpaquePass->RenderScene();
		thread->Clip3D->ResetClip(); // reset clips (floor/ceiling)

		if (thread == MainThread())
			WallCycles.Unclock();

		if (thread->MainThread)
			NetUpdate();

		if (viewactive)
		{
			if (thread->MainThread)
				PlaneCycles.Clock();

			thread->PlaneList->Render();
			thread->Portal->RenderPlanePortals();

			if (thread->MainThread)
				PlaneCycles.Unclock();

			thread->Portal->RenderLinePortals();

			if (thread->MainThread)
				NetUpdate();

			if (thread->MainThread)
				MaskedCycles.Clock();

			thread->TranslucentPass->Render();

			if (thread->MainThread)
				MaskedCycles.Unclock();

			if (thread->MainThread)
				NetUpdate();
		}

		DrawerThreads::Execute(thread->DrawQueue);
	}

	void RenderScene::StartThreads(size_t numThreads)
	{
		while (Threads.size() < (size_t)numThreads)
		{
			auto thread = std::make_unique<RenderThread>(this, false);
			auto renderthread = thread.get();
			int start_run_id = run_id;
			thread->thread = std::thread([=]()
			{
				int last_run_id = start_run_id;
				while (true)
				{
					// Wait until we are signalled to run:
					std::unique_lock<std::mutex> start_lock(start_mutex);
					start_condition.wait(start_lock, [&]() { return run_id != last_run_id || shutdown_flag; });
					if (shutdown_flag)
						break;
					last_run_id = run_id;
					start_lock.unlock();

					RenderThreadSlice(renderthread);

					// Notify main thread that we finished:
					std::unique_lock<std::mutex> end_lock(end_mutex);
					finished_threads++;
					end_lock.unlock();
					end_condition.notify_all();
				}
			});
			Threads.push_back(std::move(thread));
		}
	}

	void RenderScene::StopThreads()
	{
		std::unique_lock<std::mutex> lock(start_mutex);
		shutdown_flag = true;
		lock.unlock();
		start_condition.notify_all();
		while (Threads.size() > 1)
		{
			Threads.back()->thread.join();
			Threads.pop_back();
		}
		lock.lock();
		shutdown_flag = false;
	}

	void RenderScene::RenderViewToCanvas(AActor *actor, DCanvas *canvas, int x, int y, int width, int height, bool dontmaplines)
	{
		auto viewport = MainThread()->Viewport.get();
		
		const bool savedviewactive = viewactive;

		viewwidth = width;
		viewport->RenderTarget = canvas;

		R_SetWindow(MainThread()->Viewport->viewpoint, MainThread()->Viewport->viewwindow, 12, width, height, height, true);
		viewwindowx = x;
		viewwindowy = y;
		viewactive = true;
		viewport->SetViewport(MainThread(), width, height, MainThread()->Viewport->viewwindow.WidescreenRatio);

		RenderActorView(actor, dontmaplines);
		DrawerThreads::WaitForWorkers();

		viewport->RenderTarget = screen;

		R_ExecuteSetViewSize(MainThread()->Viewport->viewpoint, MainThread()->Viewport->viewwindow);
		float trueratio;
		ActiveRatio(width, height, &trueratio);
		screen->Lock(true);
		viewport->SetViewport(MainThread(), width, height, trueratio);
		screen->Unlock();

		viewactive = savedviewactive;
	}

	void RenderScene::ScreenResized()
	{
		auto viewport = MainThread()->Viewport.get();
		viewport->RenderTarget = screen;
		int width = SCREENWIDTH;
		int height = SCREENHEIGHT;
		float trueratio;
		ActiveRatio(width, height, &trueratio);
		screen->Lock(true);
		viewport->SetViewport(MainThread(), SCREENWIDTH, SCREENHEIGHT, trueratio);
		screen->Unlock();
	}

	void RenderScene::Init()
	{
		// viewwidth / viewheight are set by the defaults
		fillshort(zeroarray, MAXWIDTH, 0);

		R_InitShadeMaps();
	}

	void RenderScene::Deinit()
	{
		MainThread()->TranslucentPass->Deinit();
		MainThread()->Clip3D->Cleanup();
	}

	/////////////////////////////////////////////////////////////////////////

	ADD_STAT(fps)
	{
		FString out;
		out.Format("frame=%04.1f ms  walls=%04.1f ms  planes=%04.1f ms  masked=%04.1f ms",
			FrameCycles.TimeMS(), WallCycles.TimeMS(), PlaneCycles.TimeMS(), MaskedCycles.TimeMS());
		return out;
	}

	static double f_acc, w_acc, p_acc, m_acc;
	static int acc_c;

	ADD_STAT(fps_accumulated)
	{
		f_acc += FrameCycles.TimeMS();
		w_acc += WallCycles.TimeMS();
		p_acc += PlaneCycles.TimeMS();
		m_acc += MaskedCycles.TimeMS();
		acc_c++;
		FString out;
		out.Format("frame=%04.1f ms  walls=%04.1f ms  planes=%04.1f ms  masked=%04.1f ms  %d counts",
			f_acc / acc_c, w_acc / acc_c, p_acc / acc_c, m_acc / acc_c, acc_c);
		Printf(PRINT_LOG, "%s\n", out.GetChars());
		return out;
	}

	static double bestwallcycles = HUGE_VAL;

	ADD_STAT(wallcycles)
	{
		FString out;
		double cycles = WallCycles.Time();
		if (cycles && cycles < bestwallcycles)
			bestwallcycles = cycles;
		out.Format("%g", bestwallcycles);
		return out;
	}

	CCMD(clearwallcycles)
	{
		bestwallcycles = HUGE_VAL;
	}

#if 0
	// The replacement code for Build's wallscan doesn't have any timing calls so this does not work anymore.
	static double bestscancycles = HUGE_VAL;

	ADD_STAT(scancycles)
	{
		FString out;
		double scancycles = WallScanCycles.Time();
		if (scancycles && scancycles < bestscancycles)
			bestscancycles = scancycles;
		out.Format("%g", bestscancycles);
		return out;
	}

	CCMD(clearscancycles)
	{
		bestscancycles = HUGE_VAL;
	}
#endif
}
