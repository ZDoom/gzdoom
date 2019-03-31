//-----------------------------------------------------------------------------
//
// Copyright 1993-1996 id Software
// Copyright 1999-2016 Randy Heit
// Copyright 2016 Magnus Norddahl
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//-----------------------------------------------------------------------------

#include <stdlib.h>
#include <float.h>

#include "templates.h"

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
#include <chrono>

#ifdef WIN32
void PeekThreadedErrorPane();
#endif

EXTERN_CVAR(Int, r_clearbuffer)
EXTERN_CVAR(Int, r_debug_draw)

CVAR(Int, r_scene_multithreaded, 0, 0);
CVAR(Bool, r_models, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG);

bool r_modelscene = false;

namespace swrenderer
{
	cycle_t WallCycles, PlaneCycles, MaskedCycles, DrawerWaitCycles;
	
	RenderScene::RenderScene()
	{
		Threads.push_back(std::unique_ptr<RenderThread>(new RenderThread(this)));
	}

	RenderScene::~RenderScene()
	{
		StopThreads();
	}

	void RenderScene::SetClearColor(int color)
	{
		clearcolor = color;
	}

	void RenderScene::RenderView(player_t *player, DCanvas *target, void *videobuffer)
	{
		auto viewport = MainThread()->Viewport.get();
		viewport->RenderTarget = target;
		viewport->RenderingToCanvas = false;

		R_ExecuteSetViewSize(MainThread()->Viewport->viewpoint, MainThread()->Viewport->viewwindow);

		int width = SCREENWIDTH;
		int height = SCREENHEIGHT;
		float trueratio;
		ActiveRatio(width, height, &trueratio);
		viewport->SetViewport(player->camera->Level, MainThread(), width, height, trueratio);

		r_modelscene = r_models && Models.Size() > 0;
		if (r_modelscene)
		{
			PolyTriangleDrawer::ResizeBuffers(viewport->RenderTarget);
			PolyTriangleDrawer::ClearStencil(MainThread()->DrawQueue, 0);
		}

		if (r_clearbuffer != 0 || r_debug_draw != 0)
		{
			if (!viewport->RenderTarget->IsBgra())
			{
				memset(viewport->RenderTarget->GetPixels(), clearcolor, viewport->RenderTarget->GetPitch() * viewport->RenderTarget->GetHeight());
			}
			else
			{
				PalEntry bgracolor = GPalette.BaseColors[clearcolor];
				bgracolor.a = 255;
				int size = viewport->RenderTarget->GetPitch() * viewport->RenderTarget->GetHeight();
				uint32_t *dest = (uint32_t *)viewport->RenderTarget->GetPixels();
				for (int i = 0; i < size; i++)
					dest[i] = bgracolor.d;
			}
			DrawerThreads::ResetDebugDrawPos();
		}

		RenderActorView(player->mo, true, false);

		auto copyqueue = std::make_shared<DrawerCommandQueue>(MainThread()->FrameMemory.get());
		copyqueue->Push<MemcpyCommand>(videobuffer, target->GetPixels(), target->GetWidth(), target->GetHeight(), target->GetPitch(), target->IsBgra() ? 4 : 1);
		DrawerThreads::Execute(copyqueue);

		DrawerWaitCycles.Clock();
		DrawerThreads::WaitForWorkers();
		DrawerWaitCycles.Unclock();
	}

	void RenderScene::RenderActorView(AActor *actor, bool renderPlayerSprites, bool dontmaplines)
	{
		WallCycles.Reset();
		PlaneCycles.Reset();
		MaskedCycles.Reset();
		DrawerWaitCycles.Reset();
		
		R_SetupFrame(MainThread()->Viewport->viewpoint, MainThread()->Viewport->viewwindow, actor);

		if (APART(R_OldBlend)) NormalLight.Maps = realcolormaps.Maps;
		else NormalLight.Maps = realcolormaps.Maps + NUMCOLORMAPS * 256 * R_OldBlend;

		CameraLight::Instance()->SetCamera(MainThread()->Viewport->viewpoint, MainThread()->Viewport->RenderTarget, actor);
		MainThread()->Viewport->SetupFreelook();

		this->dontmaplines = dontmaplines;

		R_UpdateFuzzPosFrameStart();

		if (r_modelscene)
			MainThread()->Viewport->SetupPolyViewport(MainThread());

		FRenderViewpoint origviewpoint = MainThread()->Viewport->viewpoint;

		ActorRenderFlags savedflags = MainThread()->Viewport->viewpoint.camera->renderflags;
		// Never draw the player unless in chasecam mode
		if (!MainThread()->Viewport->viewpoint.showviewer)
		{
			MainThread()->Viewport->viewpoint.camera->renderflags |= RF_INVISIBLE;
		}

		RenderThreadSlices();

		// Mirrors fail to restore the original viewpoint -- we need it for the HUD weapon to draw correctly.
		MainThread()->Viewport->viewpoint = origviewpoint;
		if (r_modelscene)
			MainThread()->Viewport->SetupPolyViewport(MainThread());

		if (renderPlayerSprites)
			RenderPSprites();

		MainThread()->Viewport->viewpoint.camera->renderflags = savedflags;
	}

	void RenderScene::RenderPSprites()
	{
		// Player sprites needs to be rendered after all the slices because they may be hardware accelerated.
		// If they are not hardware accelerated the drawers must run after all sliced drawers finished.
		DrawerWaitCycles.Clock();
		DrawerThreads::WaitForWorkers();
		DrawerWaitCycles.Unclock();
		MainThread()->DrawQueue->Clear();
		MainThread()->PlayerSprites->Render();
		DrawerThreads::Execute(MainThread()->DrawQueue);
	}

	void RenderScene::RenderThreadSlices()
	{
		int numThreads = std::thread::hardware_concurrency();
		if (numThreads == 0)
			numThreads = 4;

		if (r_scene_multithreaded == 0 || r_multithreaded == 0)
			numThreads = 1;
		else if (r_scene_multithreaded != 1)
			numThreads = r_scene_multithreaded;

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
			using namespace std::chrono_literals;
			std::unique_lock<std::mutex> end_lock(end_mutex);
			finished_threads++;
			if (!end_condition.wait_for(end_lock, 5s, [&]() { return finished_threads == Threads.size(); }))
			{
#ifdef WIN32
				PeekThreadedErrorPane();
#endif
				// Invoke the crash reporter so that we can capture the call stack of whatever the hung worker thread is doing
				int *threadCrashed = nullptr;
				*threadCrashed = 0xdeadbeef;
			}
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

		if (r_modelscene && thread->MainThread)
			PolyTriangleDrawer::ClearStencil(MainThread()->DrawQueue, 0);

		PolyTriangleDrawer::SetViewport(thread->DrawQueue, viewwindowx, viewwindowy, viewwidth, viewheight, thread->Viewport->RenderTarget);

		// Cull things outside the range seen by this thread
		VisibleSegmentRenderer visitor;
		if (thread->X1 > 0)
			thread->ClipSegments->Clip(0, thread->X1, true, &visitor);
		if (thread->X2 < viewwidth)
			thread->ClipSegments->Clip(thread->X2, viewwidth, true, &visitor);

		thread->OpaquePass->RenderScene(thread->Viewport->Level());
		thread->Clip3D->ResetClip(); // reset clips (floor/ceiling)

		if (viewactive)
		{
			thread->PlaneList->Render();

			thread->Portal->RenderPlanePortals();
			thread->Portal->RenderLinePortals();

			thread->TranslucentPass->Render();
		}

		DrawerThreads::Execute(thread->DrawQueue);
	}

	void RenderScene::StartThreads(size_t numThreads)
	{
		while (Threads.size() < (size_t)numThreads)
		{
			std::unique_ptr<RenderThread> thread(new RenderThread(this, false));
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
		
		// Save a bunch of silly globals:
		auto savedViewpoint = viewport->viewpoint;
		auto savedViewwindow = viewport->viewwindow;
		auto savedviewwindowx = viewwindowx;
		auto savedviewwindowy = viewwindowy;
		auto savedviewwidth = viewwidth;
		auto savedviewheight = viewheight;
		auto savedviewactive = viewactive;
		auto savedRenderTarget = viewport->RenderTarget;

		// Setup the view:
		viewport->RenderTarget = canvas;
		viewport->RenderingToCanvas = true;
		R_SetWindow(MainThread()->Viewport->viewpoint, MainThread()->Viewport->viewwindow, 12, width, height, height, true);
		viewwindowx = x;
		viewwindowy = y;
		viewactive = true;
		viewport->SetViewport(actor->Level, MainThread(), width, height, MainThread()->Viewport->viewwindow.WidescreenRatio);
		if (r_modelscene)
			PolyTriangleDrawer::ResizeBuffers(viewport->RenderTarget);

		// Render:
		RenderActorView(actor, false, dontmaplines);
		DrawerWaitCycles.Clock();
		DrawerThreads::WaitForWorkers();
		DrawerWaitCycles.Unclock();

		viewport->RenderingToCanvas = false;

		// Restore silly globals:
		viewport->viewpoint = savedViewpoint;
		viewport->viewwindow = savedViewwindow;
		viewwindowx = savedviewwindowx;
		viewwindowy = savedviewwindowy;
		viewwidth = savedviewwidth;
		viewheight = savedviewheight;
		viewactive = savedviewactive;
		viewport->RenderTarget = savedRenderTarget;
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
		out.Format("frame=%04.1f ms  walls=%04.1f ms  planes=%04.1f ms  masked=%04.1f ms  drawers=%04.1f ms",
			FrameCycles.TimeMS(), WallCycles.TimeMS(), PlaneCycles.TimeMS(), MaskedCycles.TimeMS(), DrawerWaitCycles.TimeMS());
		return out;
	}

	static double f_acc, w_acc, p_acc, m_acc, drawer_acc;
	static int acc_c;

	ADD_STAT(fps_accumulated)
	{
		f_acc += FrameCycles.TimeMS();
		w_acc += WallCycles.TimeMS();
		p_acc += PlaneCycles.TimeMS();
		m_acc += MaskedCycles.TimeMS();
		drawer_acc += DrawerWaitCycles.TimeMS();
		acc_c++;
		FString out;
		out.Format("frame=%04.1f ms  walls=%04.1f ms  planes=%04.1f ms  masked=%04.1f ms  drawers=%04.1f ms  %d counts",
			f_acc / acc_c, w_acc / acc_c, p_acc / acc_c, m_acc / acc_c, drawer_acc / acc_c, acc_c);
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
}
