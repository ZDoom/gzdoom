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

EXTERN_CVAR(Bool, r_shadercolormaps)
EXTERN_CVAR(Int, r_clearbuffer)

namespace swrenderer
{
	cycle_t WallCycles, PlaneCycles, MaskedCycles, WallScanCycles;
	
	RenderScene::RenderScene()
	{
		Threads.push_back(std::make_unique<RenderThread>(this));
	}

	void RenderScene::SetClearColor(int color)
	{
		clearcolor = color;
	}

	void RenderScene::RenderView(player_t *player)
	{
		auto viewport = RenderViewport::Instance();
		viewport->RenderTarget = screen;

		int width = SCREENWIDTH;
		int height = SCREENHEIGHT;
		float trueratio;
		ActiveRatio(width, height, &trueratio);
		viewport->SetViewport(width, height, trueratio);

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
			MainThread()->DrawQueue->Push<ApplySpecialColormapRGBACommand>(CameraLight::Instance()->ShaderColormap(), screen);
		}
		
		DrawerThreads::Execute({ MainThread()->DrawQueue });
	}

	void RenderScene::RenderActorView(AActor *actor, bool dontmaplines)
	{
		WallCycles.Reset();
		PlaneCycles.Reset();
		MaskedCycles.Reset();
		WallScanCycles.Reset();
		
		R_SetupFrame(actor);
		CameraLight::Instance()->SetCamera(actor);
		RenderViewport::Instance()->SetupFreelook();

		NetUpdate();

		this->dontmaplines = dontmaplines;

		// [RH] Setup particles for this frame
		P_FindParticleSubsectors();

		// Link the polyobjects right before drawing the scene to reduce the amounts of calls to this function
		PO_LinkToSubsectors();

		ActorRenderFlags savedflags = camera->renderflags;
		// Never draw the player unless in chasecam mode
		if (!r_showviewer)
		{
			camera->renderflags |= RF_INVISIBLE;
		}

		MainThread()->FrameMemory->Clear();
		MainThread()->Clip3DFloors->Cleanup();
		MainThread()->Clip3DFloors->ResetClip(); // reset clips (floor/ceiling)
		MainThread()->Portal->CopyStackedViewParameters();
		MainThread()->ClipSegments->Clear(0, viewwidth);
		MainThread()->DrawSegments->Clear();
		MainThread()->PlaneList->Clear();
		MainThread()->TranslucentPass->Clear();
		MainThread()->OpaquePass->ClearClip();
		MainThread()->OpaquePass->ResetFakingUnderwater(); // [RH] Hack to make windows into underwater areas possible
		MainThread()->Portal->SetMainPortal();

		// Cull things outside the range seen by this thread
		if (MainThread()->X1 > 0)
			MainThread()->ClipSegments->Clip(0, MainThread()->X1, true, [](int, int) { return true; });
		if (MainThread()->X2 < viewwidth)
			MainThread()->ClipSegments->Clip(MainThread()->X2, viewwidth, true, [](int, int) { return true; });

		WallCycles.Clock();
		MainThread()->OpaquePass->RenderScene();
		MainThread()->Clip3DFloors->ResetClip(); // reset clips (floor/ceiling)
		WallCycles.Unclock();

		NetUpdate();

		if (viewactive)
		{
			PlaneCycles.Clock();
			MainThread()->PlaneList->Render();
			MainThread()->Portal->RenderPlanePortals();
			PlaneCycles.Unclock();

			MainThread()->Portal->RenderLinePortals();

			NetUpdate();

			MaskedCycles.Clock();
			MainThread()->TranslucentPass->Render();
			MaskedCycles.Unclock();

			NetUpdate();
		}

		camera->renderflags = savedflags;
		interpolator.RestoreInterpolations();

		// If we don't want shadered colormaps, NULL it now so that the
		// copy to the screen does not use a special colormap shader.
		if (!r_shadercolormaps && !RenderViewport::Instance()->RenderTarget->IsBgra())
		{
			CameraLight::Instance()->ClearShaderColormap();
		}
	}

	void RenderScene::RenderViewToCanvas(AActor *actor, DCanvas *canvas, int x, int y, int width, int height, bool dontmaplines)
	{
		auto viewport = RenderViewport::Instance();
		
		const bool savedviewactive = viewactive;

		viewwidth = width;
		viewport->RenderTarget = canvas;

		R_SetWindow(12, width, height, height, true);
		viewwindowx = x;
		viewwindowy = y;
		viewactive = true;
		viewport->SetViewport(width, height, WidescreenRatio);

		RenderActorView(actor, dontmaplines);

		DrawerThreads::Execute({ MainThread()->DrawQueue });
		
		viewport->RenderTarget = screen;

		R_ExecuteSetViewSize();
		float trueratio;
		ActiveRatio(width, height, &trueratio);
		screen->Lock(true);
		viewport->SetViewport(width, height, trueratio);
		screen->Unlock();

		viewactive = savedviewactive;
	}

	void RenderScene::ScreenResized()
	{
		auto viewport = RenderViewport::Instance();
		viewport->RenderTarget = screen;
		int width = SCREENWIDTH;
		int height = SCREENHEIGHT;
		float trueratio;
		ActiveRatio(width, height, &trueratio);
		screen->Lock(true);
		viewport->SetViewport(SCREENWIDTH, SCREENHEIGHT, trueratio);
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
		MainThread()->Clip3DFloors->Cleanup();
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
