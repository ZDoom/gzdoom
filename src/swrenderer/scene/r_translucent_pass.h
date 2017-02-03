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
	class RenderThread;
	class VisibleSprite;
	struct DrawSegment;

	class RenderTranslucentPass
	{
	public:
		RenderTranslucentPass(RenderThread *thread);

		void Deinit();
		void Clear();
		void Render();

		bool ClipSpriteColumnWithPortals(int x, VisibleSprite *spr);

		RenderThread *Thread = nullptr;

	private:
		void CollectPortals();
		void DrawMaskedSingle(bool renew);

		TArray<DrawSegment *> portaldrawsegs;
	};
}
