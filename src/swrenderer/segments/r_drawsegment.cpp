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

#include <stdlib.h>
#include "templates.h"
#include "doomdef.h"
#include "m_bbox.h"
#include "i_system.h"
#include "p_lnspec.h"
#include "p_setup.h"
#include "a_sharedglobal.h"
#include "g_level.h"
#include "p_effect.h"
#include "doomstat.h"
#include "r_state.h"
#include "v_palette.h"
#include "r_sky.h"
#include "po_man.h"
#include "r_data/colormaps.h"
#include "d_net.h"
#include "swrenderer/r_memory.h"
#include "swrenderer/drawers/r_draw.h"
#include "swrenderer/scene/r_3dfloors.h"
#include "swrenderer/scene/r_opaque_pass.h"
#include "swrenderer/scene/r_portal.h"
#include "swrenderer/line/r_wallsetup.h"
#include "swrenderer/line/r_walldraw.h"
#include "swrenderer/line/r_fogboundary.h"
#include "swrenderer/segments/r_drawsegment.h"
#include "swrenderer/things/r_visiblesprite.h"
#include "swrenderer/scene/r_light.h"
#include "swrenderer/scene/r_viewport.h"

namespace swrenderer
{
	DrawSegmentList *DrawSegmentList::Instance()
	{
		static DrawSegmentList instance;
		return &instance;
	}

	void DrawSegmentList::Deinit()
	{
		if (drawsegs != nullptr)
		{
			M_Free(drawsegs);
			drawsegs = nullptr;
		}
	}

	void DrawSegmentList::Clear()
	{
		if (drawsegs == nullptr)
		{
			MaxDrawSegs = 256; // [RH] Default. Increased as needed.
			firstdrawseg = drawsegs = (DrawSegment *)M_Malloc (MaxDrawSegs * sizeof(DrawSegment));
		}
		FirstInterestingDrawseg = 0;
		InterestingDrawsegs.Clear ();
		ds_p = drawsegs;
	}

	DrawSegment *DrawSegmentList::Add()
	{
		if (ds_p == &drawsegs[MaxDrawSegs])
		{ // [RH] Grab some more drawsegs
			size_t newdrawsegs = MaxDrawSegs ? MaxDrawSegs * 2 : 32;
			ptrdiff_t firstofs = firstdrawseg - drawsegs;
			drawsegs = (DrawSegment *)M_Realloc(drawsegs, newdrawsegs * sizeof(DrawSegment));
			firstdrawseg = drawsegs + firstofs;
			ds_p = drawsegs + MaxDrawSegs;
			MaxDrawSegs = newdrawsegs;
			DPrintf(DMSG_NOTIFY, "MaxDrawSegs increased to %zu\n", MaxDrawSegs);
		}

		return ds_p++;
	}
}
