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
//

#pragma once

#include "swrenderer/segments/r_drawsegment.h"

namespace swrenderer
{
	class RenderThread;

	class RenderDrawSegment
	{
	public:
		RenderDrawSegment(RenderThread *thread);
		void Render(DrawSegment *ds, int x1, int x2, Fake3DTranslucent clip3DFloor);

		RenderThread *Thread = nullptr;

	private:
		void RenderWall(DrawSegment *ds, int x1, int x2);
		void Render3DFloorWall(DrawSegment *ds, int x1, int x2, F3DFloor *rover, double clipTop, double clipBottom, FSoftwareTexture *rw_pic);
		void Render3DFloorWallRange(DrawSegment *ds, int x1, int x2);
		void RenderFog(DrawSegment* ds, int x1, int x2);
		void ClipMidtex(DrawSegment* ds, int x1, int x2);
		void GetNoWrapMidTextureZ(DrawSegment* ds, FSoftwareTexture* tex, double& ceilZ, double& floorZ);
		void GetMaskedWallTopBottom(DrawSegment *ds, double &top, double &bot);

		sector_t *frontsector = nullptr;
		sector_t *backsector = nullptr;

		seg_t *curline = nullptr;
		Fake3DTranslucent m3DFloor;

		ProjectedWallLine wallupper;
		ProjectedWallLine walllower;
	};
}
