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
	struct visplane_t;

	class VisiblePlaneList
	{
	public:
		static VisiblePlaneList *Instance();

		void Init();
		void Deinit();
		void Clear(bool fullclear);

		visplane_t *FindPlane(const secplane_t &height, FTextureID picnum, int lightlevel, double Alpha, bool additive, const FTransform &xxform, int sky, FSectorPortal *portal, FDynamicColormap *basecolormap);
		visplane_t *GetRange(visplane_t *pl, int start, int stop);

		int Render();
		void RenderHeight(double height);

		enum { MAXVISPLANES = 128 }; // must be a power of 2
		visplane_t *visplanes[MAXVISPLANES + 1];
		visplane_t *freetail = nullptr;
		visplane_t **freehead = nullptr;

	private:
		VisiblePlaneList();
		visplane_t *Add(unsigned hash);

		static unsigned CalcHash(int picnum, int lightlevel, const secplane_t &height) { return (unsigned)((picnum) * 3 + (lightlevel)+(FLOAT2FIXED((height).fD())) * 7) & (MAXVISPLANES - 1); }
	};
}
