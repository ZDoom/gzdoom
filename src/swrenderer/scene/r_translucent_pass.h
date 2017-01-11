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

#define MINZ double((2048*4) / double(1 << 20))

struct particle_t;
struct FVoxel;

namespace swrenderer
{
	struct vissprite_t;
	struct drawseg_t;

	class RenderTranslucentPass
	{
	public:
		static void Deinit();
		static void Clear();
		static void Render();

		static bool DrewAVoxel;

		static bool ClipSpriteColumnWithPortals(int x, vissprite_t* spr);

	private:
		static void CollectPortals();
		static void DrawSprite(vissprite_t *spr);
		static void DrawMaskedSingle(bool renew);

		static TArray<drawseg_t *> portaldrawsegs;
	};
}
