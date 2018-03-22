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

#include <stddef.h>
#include "r_defs.h"

struct FSectorPortal;

namespace swrenderer
{
	class RenderThread;
	struct VisiblePlane;

	class VisiblePlaneList
	{
	public:
		VisiblePlaneList(RenderThread *thread);

		void Clear();
		void ClearKeepFakePlanes();

		VisiblePlane *FindPlane(const secplane_t &height, FTextureID picnum, int lightlevel, double Alpha, bool additive, const FTransform &xxform, int sky, FSectorPortal *portal, FDynamicColormap *basecolormap, Fake3DOpaque::Type fakeFloorType, fixed_t fakeAlpha);
		VisiblePlane *GetRange(VisiblePlane *pl, int start, int stop);

		bool HasPortalPlanes() const;
		VisiblePlane *PopFirstPortalPlane();
		void ClearPortalPlanes();

		int Render();
		void RenderHeight(double height);

		RenderThread *Thread = nullptr;

	private:
		VisiblePlaneList();
		VisiblePlane *Add(unsigned hash);

		enum { MAXVISPLANES = 128 }; // must be a power of 2
		VisiblePlane *visplanes[MAXVISPLANES + 1];

		static unsigned CalcHash(int picnum, int lightlevel, const secplane_t &height) { return (unsigned)((picnum) * 3 + (lightlevel)+(FLOAT2FIXED((height).fD())) * 7) & (MAXVISPLANES - 1); }
	};
}
