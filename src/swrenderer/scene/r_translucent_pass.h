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
		void DrawMaskedSingle(bool renew, Fake3DTranslucent clip3DFloor);

		TArray<DrawSegment *> portaldrawsegs;
	};
}
