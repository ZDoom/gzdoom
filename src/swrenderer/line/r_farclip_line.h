//-----------------------------------------------------------------------------
//
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
//

#pragma once

#include "r_line.h"

namespace swrenderer
{
	class FarClipLine : VisibleSegmentRenderer
	{
	public:
		FarClipLine(RenderThread *thread);
		void Render(seg_t *line, subsector_t *subsector, VisiblePlane *linefloorplane, VisiblePlane *lineceilingplane);

		RenderThread *Thread = nullptr;

	private:
		bool RenderWallSegment(int x1, int x2) override;

		void ClipSegmentTopBottom(int x1, int x2);
		void MarkCeilingPlane(int x1, int x2);
		void MarkFloorPlane(int x1, int x2);

		subsector_t *mSubsector;
		sector_t *mFrontSector;
		seg_t *mLineSegment;
		VisiblePlane *mFloorPlane;
		VisiblePlane *mCeilingPlane;

		double mFrontCeilingZ1;
		double mFrontCeilingZ2;
		double mFrontFloorZ1;
		double mFrontFloorZ2;

		FWallCoords WallC;

		bool mPrepped;

		ProjectedWallLine walltop;
		ProjectedWallLine wallbottom;
	};
}
