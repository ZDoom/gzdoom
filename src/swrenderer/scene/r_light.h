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

#pragma once

#include <stddef.h>
#include "r_defs.h"
#include "v_palette.h"
#include "r_data/colormaps.h"
#include "r_utility.h"
#include "swrenderer/viewport/r_viewport.h"

// Lighting.
//
// [RH] This has changed significantly from Doom, which used lookup
// tables based on 1/z for walls and z for flats and only recognized
// 16 discrete light levels. The terminology I use is borrowed from Build.

// The size of a single colormap, in bits
#define COLORMAPSHIFT 8

// Convert a light level into an unbounded colormap index (shade). Result is
// fixed point. Why the +12? I wish I knew, but experimentation indicates it
// is necessary in order to best reproduce Doom's original lighting.
#define LIGHT2SHADE(l) ((NUMCOLORMAPS*2*FRACUNIT)-(((l)+12)*(FRACUNIT*NUMCOLORMAPS/128)))

// MAXLIGHTSCALE from original DOOM, divided by 2.
#define MAXLIGHTVIS (24.0)

// Convert a shade and visibility to a clamped colormap index.
// Result is not fixed point.
// Change R_CalcTiltedLighting() when this changes.
#define GETPALOOKUP(vis,shade) (clamp<int> (((shade)-FLOAT2FIXED(MIN(MAXLIGHTVIS,double(vis))))>>FRACBITS, 0, NUMCOLORMAPS-1))

// Calculate the light multiplier for dc_light/ds_light
// This is used instead of GETPALOOKUP when ds_colormap/dc_colormap is set to the base colormap
// Returns a value between 0 and 1 in fixed point
#define LIGHTSCALE(vis,shade) FLOAT2FIXED(clamp((FIXED2DBL(shade) - (MIN(MAXLIGHTVIS,double(vis)))) / NUMCOLORMAPS, 0.0, (NUMCOLORMAPS-1)/(double)NUMCOLORMAPS))

// Converts fixedlightlev into a shade value
#define FIXEDLIGHT2SHADE(lightlev) (((lightlev) >> COLORMAPSHIFT) << FRACBITS)

struct FSWColormap;

namespace swrenderer
{
	class CameraLight
	{
	public:
		static CameraLight *Instance();

		int FixedLightLevel() const { return fixedlightlev; }
		FSWColormap *FixedColormap() const { return fixedcolormap; }
		FSpecialColormap *ShaderColormap() const { return realfixedcolormap; }

		void SetCamera(AActor *actor);
		void ClearShaderColormap() { realfixedcolormap = nullptr; }
		
	private:
		int fixedlightlev = 0;
		FSWColormap *fixedcolormap = nullptr;
		FSpecialColormap *realfixedcolormap = nullptr;
	};

	class LightVisibility
	{
	public:
		static LightVisibility *Instance();

		void SetVisibility(double visibility);
		double GetVisibility() const { return CurrentVisibility; }

		double WallGlobVis() const { return WallVisibility; }
		double SpriteGlobVis() const { return WallVisibility; }
		double ParticleGlobVis() const { return WallVisibility * 0.5; }
		double FlatPlaneGlobVis() const { return FloorVisibility; }
		double SlopePlaneGlobVis() const { return TiltVisibility; }

		// The vis value to pass into the GETPALOOKUP or LIGHTSCALE macros
		double WallVis(double screenZ) const { return WallGlobVis() / screenZ; }
		double SpriteVis(double screenZ) const { return SpriteGlobVis() / screenZ; }
		double ParticleVis(double screenZ) const { return ParticleGlobVis() / screenZ; }
		double FlatPlaneVis(int screenY, double planeZ) const { return FlatPlaneGlobVis() / fabs(planeZ - ViewPos.Z) * fabs(RenderViewport::Instance()->CenterY - screenY); }

	private:
		double BaseVisibility = 0.0;
		double WallVisibility = 0.0;
		double FloorVisibility = 0.0;
		float TiltVisibility = 0.0f;

		double CurrentVisibility = 8.f;
		double MaxVisForWall = 0.0;
		double MaxVisForFloor = 0.0;
	};

	class ColormapLight
	{
	public:
		int ColormapNum = 0;
		FSWColormap *BaseColormap = nullptr;

		void SetColormap(double visibility, int shade, FDynamicColormap *basecolormap, bool fullbright, bool invertColormap, bool fadeToBlack);
	};

	inline int R_ActualExtraLight(bool fog) { return fog ? 0 : extralight << 4; }
}
