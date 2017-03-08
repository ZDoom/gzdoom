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

#include "swrenderer/segments/r_drawsegment.h"

namespace swrenderer
{
	class RenderThread;

	class RenderDrawSegment
	{
	public:
		RenderDrawSegment(RenderThread *thread);
		void Render(DrawSegment *ds, int x1, int x2);

		RenderThread *Thread = nullptr;

	private:
		void ClipMidtex(int x1, int x2);
		void RenderFakeWall(DrawSegment *ds, int x1, int x2, F3DFloor *rover, int wallshade, FDynamicColormap *basecolormap);
		void RenderFakeWallRange(DrawSegment *ds, int x1, int x2, int wallshade);
		void GetMaskedWallTopBottom(DrawSegment *ds, double &top, double &bot);

		sector_t *frontsector = nullptr;
		sector_t *backsector = nullptr;

		seg_t *curline = nullptr;

		FWallCoords WallC;
		FWallTmapVals WallT;

		float rw_light = 0.0f;
		float rw_lightstep = 0.0f;
		fixed_t rw_offset = 0;
		FTexture *rw_pic = nullptr;

		ProjectedWallLine wallupper;
		ProjectedWallLine walllower;
	};
}
