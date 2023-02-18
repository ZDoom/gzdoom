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

#include "r_planerenderer.h"
#include "swrenderer/viewport/r_spandrawer.h"

namespace swrenderer
{
	class RenderThread;

	class RenderSlopePlane : PlaneRenderer
	{
	public:
		RenderSlopePlane(RenderThread *thread);
		void Render(VisiblePlane *pl, double _xscale, double _yscale, fixed_t alpha, bool additive, bool masked, FDynamicColormap *basecolormap, FSoftwareTexture *texture);

		RenderThread *Thread = nullptr;

	private:
		void RenderLine(int y, int x1, int x2) override;

		FVector3 plane_sz, plane_su, plane_sv;
		float planelightfloat;
		bool plane_shade;
		int lightlevel;
		bool foggy;
		fixed_t pviewx, pviewy;
		fixed_t xscale, yscale;
		FDynamicColormap *basecolormap;
		SpanDrawerArgs drawerargs;

		DVector3 planeNormal;
		double planeD;
		VisiblePlaneLight* light_list;
	};
}
