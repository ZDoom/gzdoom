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

#pragma once

#include "swrenderer/line/r_line.h"
#include "swrenderer/scene/r_opaque_pass.h"
#include "swrenderer/things/r_visiblespritelist.h"

#define MINZ double((2048*4) / double(1 << 20))

struct particle_t;
struct FVoxel;

namespace swrenderer
{
	class VisibleSprite
	{
	public:
		virtual bool IsParticle() const { return false; }
		virtual bool IsVoxel() const { return false; }
		virtual bool IsWallSprite() const { return false; }

		virtual void Render(short *cliptop, short *clipbottom, int minZ, int maxZ) = 0;

		FTexture *pic;

		short x1, x2;
		float gzb, gzt; // global bottom / top for silhouette clipping

		double floorclip;

		double texturemid; // floorclip
		float yscale; // voxel and floorclip

		sector_t *heightsec; // height sector for underwater/fake ceiling
		WaterFakeSide FakeFlatStat; // which side of fake/floor ceiling sprite is on

		F3DFloor *fakefloor; // 3d floor clipping
		F3DFloor *fakeceiling;

		FVector3 gpos; // origin in world coordinates
		sector_t *sector; // sector this sprite is in

		// Light shared calculation?
		visstyle_t Style;
		bool foggy;
		short renderflags;

		float depth; // Sort (draw segments), also light

		float deltax, deltay; // Sort (2d/voxel version)
		float idepth; // Sort (non-voxel version)

		int CurrentPortalUniq; // to identify the portal that this thing is in. used for clipping.
	};
}
