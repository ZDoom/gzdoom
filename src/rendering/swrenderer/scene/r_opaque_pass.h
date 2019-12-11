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

#include "tarray.h"
#include <stddef.h>
#include "r_defs.h"
#include "swrenderer/line/r_line.h"
#include "swrenderer/scene/r_3dfloors.h"
#include <set>

struct FVoxelDef;

namespace swrenderer
{
	class RenderThread;
	struct VisiblePlane;

	// The 32 below is just an arbitrary value picked to avoid
	// drawing lines the player is too close to that would overflow
	// the texture calculations.
	#define TOO_CLOSE_Z (32.0 / (1 << 12))

	enum class WaterFakeSide
	{
		Center,
		BelowFloor,
		AboveCeiling
	};

	struct ThingSprite
	{
		DVector3 pos;
		int spritenum;
		FTexture *tex;
		FVoxelDef *voxel;
		FTextureID picnum;
		DVector2 spriteScale;
		int renderflags;
	};

	class RenderOpaquePass
	{
	public:
		RenderOpaquePass(RenderThread *thread);

		void ClearClip();
		void RenderScene(FLevelLocals *Level);

		void ResetFakingUnderwater() { r_fakingunderwater = false; }
		sector_t *FakeFlat(sector_t *sec, sector_t *tempsec, int *floorlightlevel, int *ceilinglightlevel, seg_t *backline, int backx1, int backx2, double frontcz1, double frontcz2);
		
		void ClearSeenSprites() { SeenSpriteSectors.clear(); SeenActors.clear(); }

		uint32_t GetSubsectorDepth(int index) const { return SubsectorDepths[index]; }

		short floorclip[MAXWIDTH];
		short ceilingclip[MAXWIDTH];

		RenderThread *Thread = nullptr;

	private:
		void RenderBSPNode(void *node);
		void RenderSubsector(subsector_t *sub);
		bool CheckBBox(float *bspcoord);

		void AddPolyobjs(subsector_t *sub);

		void Add3DFloorPlanes(subsector_t *sub, sector_t *frontsector, FDynamicColormap *basecolormap, bool foggy, int adjusted_ceilinglightlevel, int adjusted_floorlightlevel);
		void FakeDrawLoop(subsector_t *sub, sector_t *frontsector, VisiblePlane *floorplane, VisiblePlane *ceilingplane, Fake3DOpaque opaque3dfloor);
		void Add3DFloorLine(seg_t *line, sector_t *frontsector);

		void AddSprites(sector_t *sec, int lightlevel, WaterFakeSide fakeside, bool foggy, FDynamicColormap *basecolormap);
		bool IsPotentiallyVisible(AActor *thing);
		bool GetThingSprite(AActor *thing, ThingSprite &sprite);

		subsector_t *InSubsector = nullptr;
		WaterFakeSide FakeSide = WaterFakeSide::Center;
		bool r_fakingunderwater = false;

		SWRenderLine renderline;
		std::set<sector_t*> SeenSpriteSectors;
		std::set<AActor*> SeenActors;
		std::vector<uint32_t> PvsSubsectors;
		std::vector<uint32_t> SubsectorDepths;
	};
}
