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
#include "swrenderer/drawers/r_draw.h"
#include "swrenderer/scene/r_3dfloors.h"
#include "a_sharedglobal.h"
#include "g_level.h"
#include "p_effect.h"
#include "doomstat.h"
#include "r_state.h"
#include "swrenderer/scene/r_opaque_pass.h"
#include "v_palette.h"
#include "r_sky.h"
#include "po_man.h"
#include "r_data/colormaps.h"
#include "swrenderer/segments/r_portalsegment.h"
#include "swrenderer/r_memory.h"
#include "swrenderer/r_renderthread.h"

namespace swrenderer
{
	PortalDrawseg::PortalDrawseg(RenderThread *thread, line_t *linedef, int x1, int x2, const short *topclip, const short *bottomclip) : x1(x1), x2(x2)
	{
		src = linedef;
		dst = linedef->special == Line_Mirror ? linedef : linedef->getPortalDestination();
		len = x2 - x1;

		ceilingclip = thread->FrameMemory->AllocMemory<short>(len);
		floorclip = thread->FrameMemory->AllocMemory<short>(len);
		memcpy(ceilingclip, topclip, len * sizeof(short));
		memcpy(floorclip, bottomclip, len * sizeof(short));

		for (int i = 0; i < x2 - x1; i++)
		{
			if (ceilingclip[i] < 0)
				ceilingclip[i] = 0;
			if (ceilingclip[i] >= viewheight)
				ceilingclip[i] = viewheight - 1;
			if (floorclip[i] < 0)
				floorclip[i] = 0;
			if (floorclip[i] >= viewheight)
				floorclip[i] = viewheight - 1;
		}

		mirror = linedef->special == Line_Mirror;
	}
}
