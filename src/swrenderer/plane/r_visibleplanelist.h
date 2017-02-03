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

		VisiblePlane *FindPlane(const secplane_t &height, FTextureID picnum, int lightlevel, double Alpha, bool additive, const FTransform &xxform, int sky, FSectorPortal *portal, FDynamicColormap *basecolormap);
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
