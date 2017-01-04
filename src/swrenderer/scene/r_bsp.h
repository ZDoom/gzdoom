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

#include "tarray.h"
#include <stddef.h>
#include "r_defs.h"
#include "swrenderer/line/r_line.h"

namespace swrenderer
{
	struct visplane_t;

	// The 3072 below is just an arbitrary value picked to avoid
	// drawing lines the player is too close to that would overflow
	// the texture calculations.
	#define TOO_CLOSE_Z (3072.0 / (1<<12))

	enum
	{
		FAKED_Center,
		FAKED_BelowFloor,
		FAKED_AboveCeiling
	};

	class RenderBSP
	{
	public:
		static RenderBSP *Instance();

		void ClearClip();
		void RenderScene();

		void ResetFakingUnderwater() { r_fakingunderwater = false; }
		sector_t *FakeFlat(sector_t *sec, sector_t *tempsec, int *floorlightlevel, int *ceilinglightlevel, seg_t *backline, int backx1, int backx2, double frontcz1, double frontcz2);

		short floorclip[MAXWIDTH];
		short ceilingclip[MAXWIDTH];

	private:
		void RenderBSPNode(void *node);
		void RenderSubsector(subsector_t *sub);

		bool CheckBBox(float *bspcoord);
		void AddPolyobjs(subsector_t *sub);
		void FakeDrawLoop(subsector_t *sub, visplane_t *floorplane, visplane_t *ceilingplane);

		subsector_t *InSubsector;
		sector_t *frontsector;
		uint8_t FakeSide;
		bool r_fakingunderwater;

		SWRenderLine renderline;
	};
}
