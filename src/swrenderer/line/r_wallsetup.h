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


#pragma once

#include "r_defs.h"

namespace swrenderer
{
	struct FWallCoords;
	struct FWallTmapVals;

	enum class ProjectedWallCull
	{
		Visible,
		OutsideAbove,
		OutsideBelow
	};

	class ProjectedWallLine
	{
	public:
		short ScreenY[MAXWIDTH];

		ProjectedWallCull Project(RenderViewport *viewport, double z1, double z2, const FWallCoords *wallc);
		ProjectedWallCull Project(RenderViewport *viewport, const secplane_t &plane, const FWallCoords *wallc, seg_t *line, bool xflip);
		ProjectedWallCull Project(RenderViewport *viewport, double z, const FWallCoords *wallc);
	};

	class ProjectedWallTexcoords
	{
	public:
		float VStep[MAXWIDTH]; // swall
		fixed_t UPos[MAXWIDTH]; // lwall

		void Project(RenderViewport *viewport, double walxrepeat, int x1, int x2, const FWallTmapVals &WallT);
		void ProjectPos(RenderViewport *viewport, double walxrepeat, int x1, int x2, const FWallTmapVals &WallT);
	};

	class ProjectedWallLight
	{
	public:
		int GetLightLevel() const { return lightlevel; }
		bool GetFoggy() const { return foggy; }
		FDynamicColormap *GetBaseColormap() const { return basecolormap; }

		float GetLightPos(int x) const { return lightleft + lightstep * (x - x1); }
		float GetLightStep() const { return lightstep; }

		void SetColormap(const sector_t *frontsector, seg_t *lineseg, lightlist_t *lit = nullptr);

		void SetLightLeft(float left, float step, int startx) { lightleft = left; lightstep = step; x1 = startx; }
		void SetLightLeft(RenderThread *thread, const FWallCoords &wallc);

	private:
		int lightlevel;
		bool foggy;
		FDynamicColormap *basecolormap;

		int x1;
		float lightleft;
		float lightstep;
	};
}
