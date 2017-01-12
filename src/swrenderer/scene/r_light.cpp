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
#include "swrenderer/scene/r_viewport.h"

CVAR(Bool, r_shadercolormaps, true, CVAR_ARCHIVE)
EXTERN_CVAR(Bool, r_fullbrightignoresectorcolor)

namespace swrenderer
{
	double r_BaseVisibility;
	double r_WallVisibility;
	double r_FloorVisibility;
	float r_TiltVisibility;
	double r_SpriteVisibility;
	double r_ParticleVisibility;

	int fixedlightlev;
	FSWColormap *fixedcolormap;
	FSpecialColormap *realfixedcolormap;

	namespace
	{
		double CurrentVisibility = 8.f;
		double MaxVisForWall;
		double MaxVisForFloor;
	}

	// Changes how rapidly things get dark with distance
	void R_SetVisibility(double vis)
	{
		// Allow negative visibilities, just for novelty's sake
		vis = clamp(vis, -204.7, 204.7);	// (205 and larger do not work in 5:4 aspect ratio)

		CurrentVisibility = vis;

		if (FocalTangent == 0 || FocalLengthY == 0)
		{ // If r_visibility is called before the renderer is all set up, don't
		  // divide by zero. This will be called again later, and the proper
		  // values can be initialized then.
			return;
		}

		r_BaseVisibility = vis;

		MaxVisForWall = (InvZtoScale * (SCREENWIDTH*r_Yaspect) / (viewwidth*SCREENHEIGHT * FocalTangent));
		MaxVisForWall = 32767.0 / MaxVisForWall;
		MaxVisForFloor = 32767.0 / (viewheight >> 2) * FocalLengthY / 160;

		// Prevent overflow on walls
		if (r_BaseVisibility < 0 && r_BaseVisibility < -MaxVisForWall)
			r_WallVisibility = -MaxVisForWall;
		else if (r_BaseVisibility > 0 && r_BaseVisibility > MaxVisForWall)
			r_WallVisibility = MaxVisForWall;
		else
			r_WallVisibility = r_BaseVisibility;

		r_WallVisibility = (InvZtoScale * SCREENWIDTH*AspectBaseHeight(WidescreenRatio) /
			(viewwidth*SCREENHEIGHT * 3)) * (r_WallVisibility * FocalTangent);

		// Prevent overflow on floors/ceilings. Note that the calculation of
		// MaxVisForFloor means that planes less than two units from the player's
		// view could still overflow, but there is no way to totally eliminate
		// that while still using fixed point math.
		if (r_BaseVisibility < 0 && r_BaseVisibility < -MaxVisForFloor)
			r_FloorVisibility = -MaxVisForFloor;
		else if (r_BaseVisibility > 0 && r_BaseVisibility > MaxVisForFloor)
			r_FloorVisibility = MaxVisForFloor;
		else
			r_FloorVisibility = r_BaseVisibility;

		r_FloorVisibility = 160.0 * r_FloorVisibility / FocalLengthY;

		r_TiltVisibility = float(vis * FocalTangent * (16.f * 320.f) / viewwidth);
		r_SpriteVisibility = r_WallVisibility;
	}

	double R_GetVisibility()
	{
		return CurrentVisibility;
	}

	void R_SetupColormap(player_t *player)
	{
		realfixedcolormap = NULL;
		fixedcolormap = NULL;
		fixedlightlev = -1;

		if (player != NULL && camera == player->mo)
		{
			if (player->fixedcolormap >= 0 && player->fixedcolormap < (int)SpecialColormaps.Size())
			{
				realfixedcolormap = &SpecialColormaps[player->fixedcolormap];
				if (RenderTarget == screen && (r_swtruecolor || ((DFrameBuffer *)screen->Accel2D && r_shadercolormaps)))
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
		if (fixedcolormap == NULL && extralight == INT_MIN)
		{
			fixedcolormap = &SpecialColormaps[INVERSECOLORMAP];
			extralight = 0;
		}
	}

	// Controls how quickly light ramps across a 1/z range. Set this, and it
	// sets all the r_*Visibility variables (except r_SkyVisibilily, which is
	// currently unused).
	CCMD(r_visibility)
	{
		if (argv.argc() < 2)
		{
			Printf("Visibility is %g\n", R_GetVisibility());
		}
		else if (!netgame)
		{
			R_SetVisibility(atof(argv[1]));
		}
		else
		{
			Printf("Visibility cannot be changed in net games.\n");
		}
	}
}
