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

#include "swrenderer/viewport/r_walldrawer.h"
#include "r_line.h"

class FTexture;
struct FLightNode;
struct seg_t;
struct FLightNode;
struct FDynamicColormap;

namespace swrenderer
{
	class RenderThread;
	struct DrawSegment;
	struct FWallCoords;
	class ProjectedWallLine;
	class ProjectedWallTexcoords;
	struct WallSampler;

	class RenderWallPart
	{
	public:
		RenderWallPart(RenderThread *thread);

		void Render(
			const sector_t *lightsector,
			seg_t *curline,
			const FWallCoords &WallC,
			FSoftwareTexture *pic,
			int x1,
			int x2,
			const short *walltop,
			const short *wallbottom,
			const ProjectedWallTexcoords &texcoords,
			bool mask,
			bool additive,
			fixed_t alpha);

	private:
		void ProcessStripedWall(const short *uwal, const short *dwal, const ProjectedWallTexcoords& texcoords);
		void ProcessNormalWall(const short *uwal, const short *dwal, const ProjectedWallTexcoords& texcoords);
		FLightNode* GetLightList();

		RenderThread* Thread = nullptr;

		int x1 = 0;
		int x2 = 0;
		FSoftwareTexture *pic = nullptr;
		const sector_t *lightsector = nullptr;
		seg_t *curline = nullptr;
		FWallCoords WallC;

		ProjectedWallLight mLight;

		FLightNode *light_list = nullptr;
		bool mask = false;
		bool additive = false;
		fixed_t alpha = 0;
	};
}
