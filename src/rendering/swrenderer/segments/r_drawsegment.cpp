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

#include <stdlib.h>
#include "templates.h"
#include "doomdef.h"
#include "m_bbox.h"

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

		TranslucentSegments.Clear();
		StartTranslucentIndices.Clear();
		StartTranslucentIndices.Push(0);
	}

	void DrawSegmentList::PushPortal()
	{
		StartIndices.Push(Segments.Size());
		StartTranslucentIndices.Push(TranslucentSegments.Size());
	}

	void DrawSegmentList::PopPortal()
	{
		Segments.Resize(StartIndices.Last());
		StartIndices.Pop();

		TranslucentSegments.Resize(StartTranslucentIndices.Last());
		StartTranslucentIndices.Pop();
	}

	void DrawSegmentList::Push(DrawSegment *segment)
	{
		Segments.Push(segment);
	}

	void DrawSegmentList::PushTranslucent(DrawSegment *segment)
	{
		TranslucentSegments.Push(segment);
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
			group.neardepth = MIN(ds->WallC.sz1, ds->WallC.sz2);
			group.fardepth = MAX(ds->WallC.sz1, ds->WallC.sz2);

			for (unsigned int groupIndex = group.BeginIndex + 1; groupIndex < group.EndIndex; groupIndex++)
			{
				ds = Segment(groupIndex);
				group.x1 = MIN(group.x1, ds->x1);
				group.x2 = MAX(group.x2, ds->x2);
				group.neardepth = MIN(group.neardepth, ds->WallC.sz1);
				group.neardepth = MIN(group.neardepth, ds->WallC.sz2);
				group.fardepth = MAX(ds->WallC.sz1, group.fardepth);
				group.fardepth = MAX(ds->WallC.sz2, group.fardepth);
			}

			for (int x = group.x1; x < group.x2; x++)
			{
				cliptop[x] = 0;
				clipbottom[x] = viewheight;
			}

			for (unsigned int groupIndex = group.BeginIndex; groupIndex < group.EndIndex; groupIndex++)
			{
				ds = Segment(groupIndex);

				if (ds->drawsegclip.silhouette & SIL_BOTTOM)
				{
					short *clip1 = clipbottom + ds->x1;
					const short *clip2 = ds->drawsegclip.sprbottomclip + ds->x1;
					int i = ds->x2 - ds->x1;
					do
					{
						if (*clip1 > *clip2)
							*clip1 = *clip2;
						clip1++;
						clip2++;
					} while (--i);
				}

				if (ds->drawsegclip.silhouette & SIL_TOP)
				{
					short *clip1 = cliptop + ds->x1;
					const short *clip2 = ds->drawsegclip.sprtopclip + ds->x1;
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

	/////////////////////////////////////////////////////////////////////////

	void DrawSegmentClipInfo::SetTopClip(RenderThread* thread, int x1, int x2, const short* ceilingclip)
	{
		short* clip = thread->FrameMemory->AllocMemory<short>(x2 - x1);
		memcpy(clip, ceilingclip + x1, (x2 - x1) * sizeof(short));
		sprtopclip = clip - x1;
	}

	void DrawSegmentClipInfo::SetTopClip(RenderThread* thread, int x1, int x2, short value)
	{
		short* clip = thread->FrameMemory->AllocMemory<short>(x2 - x1);
		for (int i = 0; i < x2 - x1; i++)
			clip[i] = value;
		sprtopclip = clip - x1;
	}

	void DrawSegmentClipInfo::SetBottomClip(RenderThread* thread, int x1, int x2, const short* floorclip)
	{
		short* clip = thread->FrameMemory->AllocMemory<short>(x2 - x1);
		memcpy(clip, floorclip + x1, (x2 - x1) * sizeof(short));
		sprbottomclip = clip - x1;
	}

	void DrawSegmentClipInfo::SetBottomClip(RenderThread* thread, int x1, int x2, short value)
	{
		short* clip = thread->FrameMemory->AllocMemory<short>(x2 - x1);
		for (int i = 0; i < x2 - x1; i++)
			clip[i] = value;
		sprbottomclip = clip - x1;
	}

	void DrawSegmentClipInfo::SetBackupClip(RenderThread* thread, int x1, int x2, const short* ceilingclip)
	{
		short* clip = thread->FrameMemory->AllocMemory<short>(x2 - x1);
		memcpy(clip, ceilingclip + x1, (x2 - x1) * sizeof(short));
		bkup = clip - x1;
	}

	void DrawSegmentClipInfo::SetRangeDrawn(int x1, int x2)
	{
		sprclipped = true;
		fillshort(const_cast<short*>(sprtopclip) + x1, x2 - x1, viewheight);
	}

	void DrawSegmentClipInfo::SetRangeUndrawn(int x1, int x2)
	{
		if (sprclipped)
		{
			sprclipped = false;
			memcpy(const_cast<short*>(sprtopclip) + x1, bkup + x1, (x2 - x1) * sizeof(short));
		}
	}
}
