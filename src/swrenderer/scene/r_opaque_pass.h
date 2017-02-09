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

	// The 3072 below is just an arbitrary value picked to avoid
	// drawing lines the player is too close to that would overflow
	// the texture calculations.
	#define TOO_CLOSE_Z (3072.0 / (1<<12))

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
		void RenderScene();

		void ResetFakingUnderwater() { r_fakingunderwater = false; }
		sector_t *FakeFlat(sector_t *sec, sector_t *tempsec, int *floorlightlevel, int *ceilinglightlevel, seg_t *backline, int backx1, int backx2, double frontcz1, double frontcz2);
		
		void ClearSeenSprites() { SeenSpriteSectors.clear(); SeenActors.clear(); }

		short floorclip[MAXWIDTH];
		short ceilingclip[MAXWIDTH];

		RenderThread *Thread = nullptr;

	private:
		void RenderBSPNode(void *node);
		void RenderSubsector(subsector_t *sub);

		bool CheckBBox(float *bspcoord);
		void AddPolyobjs(subsector_t *sub);
		void FakeDrawLoop(subsector_t *sub, VisiblePlane *floorplane, VisiblePlane *ceilingplane, bool foggy, FDynamicColormap *basecolormap);

		void AddSprites(sector_t *sec, int lightlevel, WaterFakeSide fakeside, bool foggy, FDynamicColormap *basecolormap);

		bool IsPotentiallyVisible(AActor *thing);
		static bool GetThingSprite(AActor *thing, ThingSprite &sprite);

		subsector_t *InSubsector = nullptr;
		sector_t *frontsector = nullptr;
		WaterFakeSide FakeSide = WaterFakeSide::Center;
		bool r_fakingunderwater = false;

		SWRenderLine renderline;
		std::set<sector_t*> SeenSpriteSectors;
		std::set<AActor*> SeenActors;
	};
}
