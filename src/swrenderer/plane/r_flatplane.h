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
#include "swrenderer/viewport/r_spandrawer.h"

namespace swrenderer
{
	class RenderThread;
	struct VisiblePlaneLight;

	class RenderFlatPlane : PlaneRenderer
	{
	public:
		RenderFlatPlane(RenderThread *thread);
		void Render(VisiblePlane *pl, double _xscale, double _yscale, fixed_t alpha, bool additive, bool masked, FDynamicColormap *basecolormap, FTexture *texture);

		RenderThread *Thread = nullptr;

	private:
		void RenderLine(int y, int x1, int x2) override;

		int minx;
		double planeheight;
		bool plane_shade;
		int planeshade;
		double GlobVis;
		FDynamicColormap *basecolormap;
		double pviewx, pviewy;
		double xstepscale, ystepscale;
		double basexfrac, baseyfrac;
		VisiblePlaneLight *light_list;

		SpanDrawerArgs drawerargs;
	};

	class RenderColoredPlane : PlaneRenderer
	{
	public:
		RenderColoredPlane(RenderThread *thread);
		void Render(VisiblePlane *pl);
		
		RenderThread *Thread = nullptr;

	private:
		void RenderLine(int y, int x1, int x2) override;

		SpanDrawerArgs drawerargs;
	};
}
