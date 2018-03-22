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
		uint8_t silhouette = 0; // 0=none, 1=bottom, 2=top, 3=both
		bool bFogBoundary = false;
		int shade = 0;
		bool foggy = false;

		// Pointers to lists for sprite clipping, all three adjusted so [x1] is first value.
		short *sprtopclip = nullptr;
		short *sprbottomclip = nullptr;
		fixed_t *maskedtexturecol = nullptr;
		float *swall = nullptr;
		short *bkup = nullptr; // sprtopclip backup, for mid and fake textures
		bool sprclipped = false; // True if draw segment was used for clipping sprites
		
		FWallTmapVals tmapvals;
		
		int CurrentPortalUniq = 0; // [ZZ] to identify the portal that this drawseg is in. used for sprite clipping.

		bool Has3DFloorWalls() const { return b3DFloorBoundary != 0; }
		bool Has3DFloorFrontSectorWalls() const { return (b3DFloorBoundary & 2) == 2; }
		bool Has3DFloorBackSectorWalls() const { return (b3DFloorBoundary & 1) == 1; }
		bool Has3DFloorMidTexture() const { return (b3DFloorBoundary & 4) == 4; }

		void SetHas3DFloorFrontSectorWalls() { b3DFloorBoundary |= 2; }
		void SetHas3DFloorBackSectorWalls() { b3DFloorBoundary |= 1; }
		void SetHas3DFloorMidTexture() { b3DFloorBoundary |= 4; }

	private:
		uint8_t b3DFloorBoundary = 0; // 1=backsector, 2=frontsector, 4=midtexture
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

		unsigned int TranslucentSegmentsCount() const { return TranslucentSegments.Size() - StartTranslucentIndices.Last(); }
		DrawSegment *TranslucentSegment(unsigned int index) const { return TranslucentSegments[TranslucentSegments.Size() - 1 - index]; }

		void Clear();
		void PushPortal();
		void PopPortal();
		void Push(DrawSegment *segment);
		void PushTranslucent(DrawSegment *segment);

		void BuildSegmentGroups();

		RenderThread *Thread = nullptr;

	private:
		TArray<DrawSegment *> Segments;
		TArray<unsigned int> StartIndices;

		TArray<DrawSegment *> TranslucentSegments; // drawsegs that have something drawn on them
		TArray<unsigned int> StartTranslucentIndices;

		// For building segment groups
		short cliptop[MAXWIDTH];
		short clipbottom[MAXWIDTH];
	};
}
