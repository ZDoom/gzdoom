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
#include "swrenderer/viewport/r_viewport.h"
#include "swrenderer/r_renderthread.h"

namespace swrenderer
{
	DrawSegmentList::DrawSegmentList(RenderThread *thread)
	{
		Thread = thread;
	}

	void DrawSegmentList::Clear()
	{
		Segments.Clear();
		StartIndices.Clear();
		StartIndices.Push(0);

		InterestingSegments.Clear();
		StartInterestingIndices.Clear();
		StartInterestingIndices.Push(0);
	}

	void DrawSegmentList::PushPortal()
	{
		StartIndices.Push(Segments.Size());
		StartInterestingIndices.Push(InterestingSegments.Size());
	}

	void DrawSegmentList::PopPortal()
	{
		Segments.Resize(StartIndices.Last());
		StartIndices.Pop();

		InterestingSegments.Resize(StartInterestingIndices.Last());
		StartInterestingIndices.Pop();
	}

	void DrawSegmentList::Push(DrawSegment *segment)
	{
		Segments.Push(segment);
	}

	void DrawSegmentList::PushInteresting(DrawSegment *segment)
	{
		InterestingSegments.Push(segment);
	}

	void DrawSegmentList::BuildSegmentGroups()
	{
		SegmentGroups.Clear();

		unsigned int groupSize = 100;
		for (unsigned int index = 0; index < SegmentsCount(); index += groupSize)
		{
			auto ds = Segment(index);

			DrawSegmentGroup group;
			group.BeginIndex = index;
			group.EndIndex = MIN(index + groupSize, SegmentsCount());
			group.x1 = ds->x1;
			group.x2 = ds->x2;
			group.neardepth = MIN(ds->sz1, ds->sz2);
			group.fardepth = MAX(ds->sz1, ds->sz2);

			for (unsigned int groupIndex = group.BeginIndex + 1; groupIndex < group.EndIndex; groupIndex++)
			{
				ds = Segment(groupIndex);
				group.x1 = MIN(group.x1, ds->x1);
				group.x2 = MAX(group.x2, ds->x2);
				group.neardepth = MIN(group.neardepth, ds->sz1);
				group.neardepth = MIN(group.neardepth, ds->sz2);
				group.fardepth = MAX(ds->sz1, group.fardepth);
				group.fardepth = MAX(ds->sz2, group.fardepth);
			}

			for (int x = group.x1; x < group.x2; x++)
			{
				cliptop[x] = 0;
				clipbottom[x] = viewheight;
			}

			for (unsigned int groupIndex = group.BeginIndex; groupIndex < group.EndIndex; groupIndex++)
			{
				ds = Segment(groupIndex);

				// kg3D - no clipping on fake segs
				if (ds->fake) continue;

				if (ds->silhouette & SIL_BOTTOM)
				{
					short *clip1 = clipbottom + ds->x1;
					const short *clip2 = ds->sprbottomclip;
					int i = ds->x2 - ds->x1;
					do
					{
						if (*clip1 > *clip2)
							*clip1 = *clip2;
						clip1++;
						clip2++;
					} while (--i);
				}

				if (ds->silhouette & SIL_TOP)
				{
					short *clip1 = cliptop + ds->x1;
					const short *clip2 = ds->sprtopclip;
					int i = ds->x2 - ds->x1;
					do
					{
						if (*clip1 < *clip2)
							*clip1 = *clip2;
						clip1++;
						clip2++;
					} while (--i);
				}
			}

			group.sprtopclip = Thread->FrameMemory->AllocMemory<short>(group.x2 - group.x1);
			group.sprbottomclip = Thread->FrameMemory->AllocMemory<short>(group.x2 - group.x1);
			memcpy(group.sprtopclip, cliptop + group.x1, (group.x2 - group.x1) * sizeof(short));
			memcpy(group.sprbottomclip, clipbottom + group.x1, (group.x2 - group.x1) * sizeof(short));

			SegmentGroups.Push(group);
		}
	}
}
