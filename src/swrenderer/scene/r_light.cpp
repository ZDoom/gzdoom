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
#include "r_utility.h"
#include "d_player.h"
#include "swrenderer/scene/r_light.h"
#include "swrenderer/viewport/r_viewport.h"

CVAR(Bool, r_shadercolormaps, true, CVAR_ARCHIVE)
EXTERN_CVAR(Bool, r_fullbrightignoresectorcolor)

namespace swrenderer
{
	CameraLight *CameraLight::Instance()
	{
		static CameraLight instance;
		return &instance;
	}

	void CameraLight::SetCamera(AActor *actor)
	{
		player_t *player = actor->player;
		if (camera && camera->player != nullptr)
			player = camera->player;

		realfixedcolormap = nullptr;
		fixedcolormap = nullptr;
		fixedlightlev = -1;

		if (player != nullptr && camera == player->mo)
		{
			if (player->fixedcolormap >= 0 && player->fixedcolormap < (int)SpecialColormaps.Size())
			{
				realfixedcolormap = &SpecialColormaps[player->fixedcolormap];
				auto viewport = RenderViewport::Instance();
				if (viewport->RenderTarget == screen && (viewport->RenderTarget->IsBgra() || ((DFrameBuffer *)screen->Accel2D && r_shadercolormaps)))
				{
					// Render everything fullbright. The copy to video memory will
					// apply the special colormap, so it won't be restricted to the
					// palette.
					fixedcolormap = &realcolormaps;
				}
				else
				{
					fixedcolormap = &SpecialColormaps[player->fixedcolormap];
				}
			}
			else if (player->fixedlightlevel >= 0 && player->fixedlightlevel < NUMCOLORMAPS)
			{
				fixedlightlev = player->fixedlightlevel * 256;
				// [SP] Emulate GZDoom's light-amp goggles.
				if (r_fullbrightignoresectorcolor && fixedlightlev >= 0)
				{
					fixedcolormap = &FullNormalLight;
				}
			}
		}
		// [RH] Inverse light for shooting the Sigil
		if (fixedcolormap == nullptr && extralight == INT_MIN)
		{
			fixedcolormap = &SpecialColormaps[INVERSECOLORMAP];
			extralight = 0;
		}
	}

	/////////////////////////////////////////////////////////////////////////

	LightVisibility *LightVisibility::Instance()
	{
		static LightVisibility instance;
		return &instance;
	}

	// Changes how rapidly things get dark with distance
	void LightVisibility::SetVisibility(double vis)
	{
		// Allow negative visibilities, just for novelty's sake
		vis = clamp(vis, -204.7, 204.7);	// (205 and larger do not work in 5:4 aspect ratio)

		CurrentVisibility = vis;

		auto viewport = RenderViewport::Instance();
		if (FocalTangent == 0 || viewport->FocalLengthY == 0)
		{ // If r_visibility is called before the renderer is all set up, don't
		  // divide by zero. This will be called again later, and the proper
		  // values can be initialized then.
			return;
		}

		BaseVisibility = vis;

		MaxVisForWall = (viewport->InvZtoScale * (SCREENWIDTH*r_Yaspect) / (viewwidth*SCREENHEIGHT * FocalTangent));
		MaxVisForWall = 32767.0 / MaxVisForWall;
		MaxVisForFloor = 32767.0 / (viewheight >> 2) * viewport->FocalLengthY / 160;

		// Prevent overflow on walls
		if (BaseVisibility < 0 && BaseVisibility < -MaxVisForWall)
			WallVisibility = -MaxVisForWall;
		else if (BaseVisibility > 0 && BaseVisibility > MaxVisForWall)
			WallVisibility = MaxVisForWall;
		else
			WallVisibility = BaseVisibility;

		WallVisibility = (viewport->InvZtoScale * SCREENWIDTH*AspectBaseHeight(WidescreenRatio) /
			(viewwidth*SCREENHEIGHT * 3)) * (WallVisibility * FocalTangent);

		// Prevent overflow on floors/ceilings. Note that the calculation of
		// MaxVisForFloor means that planes less than two units from the player's
		// view could still overflow, but there is no way to totally eliminate
		// that while still using fixed point math.
		if (BaseVisibility < 0 && BaseVisibility < -MaxVisForFloor)
			FloorVisibility = -MaxVisForFloor;
		else if (BaseVisibility > 0 && BaseVisibility > MaxVisForFloor)
			FloorVisibility = MaxVisForFloor;
		else
			FloorVisibility = BaseVisibility;

		FloorVisibility = 160.0 * FloorVisibility / viewport->FocalLengthY;

		TiltVisibility = float(vis * FocalTangent * (16.f * 320.f) / viewwidth);
	}

	// Controls how quickly light ramps across a 1/z range. Set this, and it
	// sets all the r_*Visibility variables (except r_SkyVisibilily, which is
	// currently unused).
	CCMD(r_visibility)
	{
		if (argv.argc() < 2)
		{
			Printf("Visibility is %g\n", LightVisibility::Instance()->GetVisibility());
		}
		else if (!netgame)
		{
			LightVisibility::Instance()->SetVisibility(atof(argv[1]));
		}
		else
		{
			Printf("Visibility cannot be changed in net games.\n");
		}
	}

	/////////////////////////////////////////////////////////////////////////

	void ColormapLight::SetColormap(double visibility, int shade, FDynamicColormap *basecolormap, bool fullbright, bool invertColormap, bool fadeToBlack)
	{
		if (fadeToBlack)
		{
			if (invertColormap) // Fade to white
			{
				basecolormap = GetSpecialLights(basecolormap->Color, MAKERGB(255, 255, 255), basecolormap->Desaturate);
				invertColormap = false;
			}
			else // Fade to black
			{
				basecolormap = GetSpecialLights(basecolormap->Color, MAKERGB(0, 0, 0), basecolormap->Desaturate);
			}
		}

		if (invertColormap)
		{
			basecolormap = GetSpecialLights(basecolormap->Color, basecolormap->Fade.InverseColor(), basecolormap->Desaturate);
		}

		CameraLight *cameraLight = CameraLight::Instance();
		if (cameraLight->FixedColormap())
		{
			BaseColormap = cameraLight->FixedColormap();
			ColormapNum = 0;
		}
		else if (cameraLight->FixedLightLevel() >= 0)
		{
			BaseColormap = (r_fullbrightignoresectorcolor) ? &FullNormalLight : basecolormap;
			ColormapNum = cameraLight->FixedLightLevel() >> COLORMAPSHIFT;
		}
		else if (fullbright)
		{
			BaseColormap = (r_fullbrightignoresectorcolor) ? &FullNormalLight : basecolormap;
			ColormapNum = 0;
		}
		else
		{
			BaseColormap = basecolormap;
			ColormapNum = GETPALOOKUP(visibility, shade);
		}
	}
}
