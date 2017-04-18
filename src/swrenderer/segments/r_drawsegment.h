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

#pragma once

#include "swrenderer/line/r_line.h"

namespace swrenderer
{
	struct DrawSegment
	{
		seg_t *curline;
		float light, lightstep;
		float iscale, iscalestep;
		short x1, x2; // Same as sx1 and sx2, but clipped to the drawseg
		short sx1, sx2; // left, right of parent seg on screen
		float sz1, sz2; // z for left, right of parent seg on screen
		float siz1, siz2; // 1/z for left, right of parent seg on screen
		float cx, cy, cdx, cdy;
		float yscale;
		uint8_t silhouette; // 0=none, 1=bottom, 2=top, 3=both
		uint8_t bFogBoundary;
		uint8_t bFakeBoundary; // for fake walls
		int shade;
		bool foggy;

		// Pointers to lists for sprite clipping, all three adjusted so [x1] is first value.
		short *sprtopclip;
		short *sprbottomclip;
		fixed_t *maskedtexturecol;
		float *swall;
		short *bkup; // sprtopclip backup, for mid and fake textures
		
		FWallTmapVals tmapvals;
		
		int fake; // ident fake drawseg, don't draw and clip sprites backups
		int CurrentPortalUniq; // [ZZ] to identify the portal that this drawseg is in. used for sprite clipping.
	};

	struct DrawSegmentGroup
	{
		short x1, x2;
		float neardepth, fardepth;
		short *sprtopclip;
		short *sprbottomclip;
		unsigned int BeginIndex;
		unsigned int EndIndex;
	};

	class DrawSegmentList
	{
	public:
		DrawSegmentList(RenderThread *thread);

		TArray<DrawSegmentGroup> SegmentGroups;

		unsigned int SegmentsCount() const { return Segments.Size() - StartIndices.Last(); }
		DrawSegment *Segment(unsigned int index) const { return Segments[Segments.Size() - 1 - index]; }

		unsigned int InterestingSegmentsCount() const { return InterestingSegments.Size() - StartInterestingIndices.Last(); }
		DrawSegment *InterestingSegment(unsigned int index) const { return InterestingSegments[InterestingSegments.Size() - 1 - index]; }

		void Clear();
		void PushPortal();
		void PopPortal();
		void Push(DrawSegment *segment);
		void PushInteresting(DrawSegment *segment);

		void BuildSegmentGroups();

		RenderThread *Thread = nullptr;

	private:
		TArray<DrawSegment *> Segments;
		TArray<unsigned int> StartIndices;

		TArray<DrawSegment *> InterestingSegments; // drawsegs that have something drawn on them
		TArray<unsigned int> StartInterestingIndices;

		// For building segment groups
		short cliptop[MAXWIDTH];
		short clipbottom[MAXWIDTH];
	};
}
