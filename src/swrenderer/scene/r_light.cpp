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

EXTERN_CVAR(Bool, r_fullbrightignoresectorcolor)

namespace swrenderer
{
	CameraLight *CameraLight::Instance()
	{
		static CameraLight instance;
		return &instance;
	}

	void CameraLight::SetCamera(FRenderViewpoint &viewpoint, DCanvas *renderTarget, AActor *actor)
	{
		AActor *camera = viewpoint.camera;
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
				// Render everything fullbright. The copy to video memory will
				// apply the special colormap, so it won't be restricted to the
				// palette.
				fixedcolormap = &realcolormaps;
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
		if (fixedcolormap == nullptr && viewpoint.extralight == INT_MIN)
		{
			fixedcolormap = &SpecialSWColormaps[INVERSECOLORMAP];
			viewpoint.extralight = 0;
		}
	}

	/////////////////////////////////////////////////////////////////////////

	// Changes how rapidly things get dark with distance
	void LightVisibility::SetVisibility(RenderViewport *viewport, double vis)
	{
		vis = R_ClampVisibility(vis);

		CurrentVisibility = vis;

		if (viewport->viewwindow.FocalTangent == 0 || viewport->FocalLengthY == 0)
		{ // If r_visibility is called before the renderer is all set up, don't
		  // divide by zero. This will be called again later, and the proper
		  // values can be initialized then.
			return;
		}

		BaseVisibility = vis;

		MaxVisForWall = (viewport->InvZtoScale * (SCREENWIDTH*r_Yaspect) / (viewwidth*SCREENHEIGHT * viewport->viewwindow.FocalTangent));
		MaxVisForWall = 32767.0 / MaxVisForWall;
		MaxVisForFloor = 32767.0 / (viewheight >> 2) * viewport->FocalLengthY / 160;

		// Prevent overflow on walls
		if (BaseVisibility < 0 && BaseVisibility < -MaxVisForWall)
			WallVisibility = -MaxVisForWall;
		else if (BaseVisibility > 0 && BaseVisibility > MaxVisForWall)
			WallVisibility = MaxVisForWall;
		else
			WallVisibility = BaseVisibility;

		WallVisibility = (viewport->InvZtoScale * SCREENWIDTH*AspectBaseHeight(viewport->viewwindow.WidescreenRatio) /
			(viewwidth*SCREENHEIGHT * 3)) * (WallVisibility * viewport->viewwindow.FocalTangent);

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

		TiltVisibility = float(vis * viewport->viewwindow.FocalTangent * (16.f * 320.f) / viewwidth);

		NoLightFade = !!(level.flags3 & LEVEL3_NOLIGHTFADE);
	}

	fixed_t LightVisibility::LightLevelToShadeImpl(int lightlevel, bool foggy)
	{
		bool nolightfade = !foggy && ((level.flags3 & LEVEL3_NOLIGHTFADE));
		if (nolightfade)
		{
			return (MAX(255 - lightlevel, 0) * NUMCOLORMAPS) << (FRACBITS - 8);
		}
		else
		{
			// Convert a light level into an unbounded colormap index (shade). Result is
			// fixed point. Why the +12? I wish I knew, but experimentation indicates it
			// is necessary in order to best reproduce Doom's original lighting.
			return (NUMCOLORMAPS * 2 * FRACUNIT) - ((lightlevel + 12) * (FRACUNIT*NUMCOLORMAPS / 128));
		}
	}

	/////////////////////////////////////////////////////////////////////////

	void ColormapLight::SetColormap(RenderThread *thread, double z, int lightlevel, bool foggy, FDynamicColormap *basecolormap, bool fullbright, bool invertColormap, bool fadeToBlack, bool psprite, bool particle)
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
		if (cameraLight->FixedLightLevel() >= 0)
		{
			BaseColormap = (r_fullbrightignoresectorcolor) ? &FullNormalLight : basecolormap;
			ColormapNum = cameraLight->FixedLightLevel() >> COLORMAPSHIFT;
		}
		else if (cameraLight->FixedColormap())
		{
			BaseColormap = cameraLight->FixedColormap();
			ColormapNum = 0;
		}
		else if (fullbright)
		{
			BaseColormap = (r_fullbrightignoresectorcolor) ? &FullNormalLight : basecolormap;
			ColormapNum = 0;
		}
		else
		{
			double visibility = thread->Light->SpriteVis(z, foggy);
			if (particle)
				visibility *= 0.5;

			int shade = LightVisibility::LightLevelToShade(lightlevel, foggy, thread->Viewport.get());
			if (psprite)
				shade -= 24 * FRACUNIT;

			BaseColormap = basecolormap;
			ColormapNum = GETPALOOKUP(visibility, shade);
		}
	}
}
