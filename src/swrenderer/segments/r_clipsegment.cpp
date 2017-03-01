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
#include "swrenderer/segments/r_clipsegment.h"

namespace swrenderer
{
	void RenderClipSegment::Clear(short left, short right)
	{
		solidsegs[0].first = -0x7fff;
		solidsegs[0].last = left;
		solidsegs[1].first = right;
		solidsegs[1].last = 0x7fff;
		newend = solidsegs+2;
	}

	bool RenderClipSegment::Check(int first, int last)
	{
		cliprange_t *start;

		// Find the first range that touches the range
		// (adjacent pixels are touching).
		start = solidsegs;
		while (start->last < first)
			start++;

		if (first < start->first)
		{
			return true;
		}

		// Bottom contained in start?
		if (last > start->last)
		{
			return true;
		}

		return false;
	}

	bool RenderClipSegment::IsVisible(int sx1, int sx2)
	{
		// Does not cross a pixel.
		if (sx2 <= sx1)
			return false;

		cliprange_t *start = solidsegs;
		while (start->last < sx2)
			start++;

		if (sx1 >= start->first && sx2 <= start->last)
		{
			// The clippost contains the new span.
			return false;
		}

		return true;
	}

	bool RenderClipSegment::Clip(int first, int last, bool solid, VisibleSegmentRenderer *visitor)
	{
		cliprange_t *next, *start;
		int i, j;
		bool res = false;

		// Find the first range that touches the range
		// (adjacent pixels are touching).
		start = solidsegs;
		while (start->last < first)
			start++;

		if (first < start->first)
		{
			res = true;
			if (last <= start->first)
			{
				// Post is entirely visible (above start).
				if (!visitor->RenderWallSegment(first, last))
					return true;

				// Insert a new clippost for solid walls.
				if (solid)
				{
					if (last == start->first)
					{
						start->first = first;
					}
					else
					{
						next = newend;
						newend++;
						while (next != start)
						{
							*next = *(next - 1);
							next--;
						}
						next->first = first;
						next->last = last;
					}
				}
				return true;
			}

			// There is a fragment above *start.
			if (visitor->RenderWallSegment(first, start->first) && solid)
			{
				start->first = first; // Adjust the clip size for solid walls
			}
		}

		// Bottom contained in start?
		if (last <= start->last)
			return res;

		bool clipsegment;
		next = start;
		while (last >= (next + 1)->first)
		{
			// There is a fragment between two posts.
			clipsegment = visitor->RenderWallSegment(next->last, (next + 1)->first);
			next++;

			if (last <= next->last)
			{
				// Bottom is contained in next.
				last = next->last;
				goto crunch;
			}
		}

		// There is a fragment after *next.
		clipsegment = visitor->RenderWallSegment(next->last, last);

	crunch:
		if (!clipsegment)
		{
			return true;
		}
		if (solid)
		{
			// Adjust the clip size.
			start->last = last;

			if (next != start)
			{
				// Remove start+1 to next from the clip list,
				// because start now covers their area.
				for (i = 1, j = (int)(newend - next); j > 0; i++, j--)
				{
					start[i] = next[i];
				}
				newend = start + i;
			}
		}
		return true;
	}
}
