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
	struct DrawSegmentClipInfo
	{
		void SetTopClip(RenderThread* thread, int x1, int x2, const short* ceilingclip);
		void SetTopClip(RenderThread* thread, int x1, int x2, short value);
		void SetBottomClip(RenderThread* thread, int x1, int x2, const short* floorclip);
		void SetBottomClip(RenderThread* thread, int x1, int x2, short value);
		void SetBackupClip(RenderThread* thread, int x1, int x2, const short* ceilingclip);
		void SetRangeDrawn(int x1, int x2);
		void SetRangeUndrawn(int x1, int x2);

		uint8_t silhouette = 0; // 0=none, 1=bottom, 2=top, 3=both
		int CurrentPortalUniq = 0; // [ZZ] to identify the portal that this drawseg is in. used for sprite clipping.
		int SubsectorDepth;

		// Pointers to lists for sprite clipping, all three adjusted so [x1] is first value.
		const short* sprtopclip = nullptr;
		const short* sprbottomclip = nullptr;

	private:
		bool sprclipped = false; // True if draw segment was used for clipping sprites
		const short* bkup = nullptr;
	};

	struct DrawSegment
	{
		seg_t *curline;
		short x1, x2;

		FWallCoords WallC;
		ProjectedWallTexcoords texcoords;

		DrawSegmentClipInfo drawsegclip;

		bool HasFogBoundary() const { return (flags & 8) != 0; }
		bool Has3DFloorWalls() const { return (flags & 3) != 0; }
		bool Has3DFloorFrontSectorWalls() const { return (flags & 2) != 0; }
		bool Has3DFloorBackSectorWalls() const { return (flags & 1) != 0; }
		bool HasTranslucentMidTexture() const { return (flags & 4) != 0; }

		void SetHasFogBoundary() { flags |= 8; }
		void SetHas3DFloorFrontSectorWalls() { flags |= 2; }
		void SetHas3DFloorBackSectorWalls() { flags |= 1; }
		void SetHasTranslucentMidTexture() { flags |= 4; }

	private:
		int flags = 0; // 1=backsector, 2=frontsector, 4=midtexture, 8=fogboundary
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
