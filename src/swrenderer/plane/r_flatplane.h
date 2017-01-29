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

#include "r_planerenderer.h"
#include "swrenderer/drawers/r_drawerargs.h"

namespace swrenderer
{
	struct VisiblePlaneLight;

	class RenderFlatPlane : PlaneRenderer
	{
	public:
		void Render(VisiblePlane *pl, double _xscale, double _yscale, fixed_t alpha, bool additive, bool masked, FDynamicColormap *basecolormap, FTexture *texture);

		static void SetupSlope();

	private:
		void RenderLine(int y, int x1, int x2) override;
		void StepColumn() override;

		double planeheight;
		bool plane_shade;
		int planeshade;
		double GlobVis;
		FDynamicColormap *basecolormap;
		fixed_t pviewx, pviewy;
		fixed_t xscale, yscale;
		double xstepscale, ystepscale;
		double basexfrac, baseyfrac;
		VisiblePlaneLight *light_list;

		DrawerArgs drawerargs;

		static float yslope[MAXHEIGHT];
	};

	class RenderColoredPlane : PlaneRenderer
	{
	public:
		void Render(VisiblePlane *pl);

	private:
		void RenderLine(int y, int x1, int x2) override;

		DrawerArgs drawerargs;
	};
}
