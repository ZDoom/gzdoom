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

#include "r_visiblesprite.h"

namespace swrenderer
{
	class RenderSprite
	{
	public:
		static void Project(AActor *thing, const DVector3 &pos, FTexture *tex, const DVector2 &spriteScale, int renderflags, WaterFakeSide fakeside, F3DFloor *fakefloor, F3DFloor *fakeceiling, sector_t *current_sector, int spriteshade);
		static void Render(vissprite_t *vis, const short *mfloorclip, const short *mceilingclip);
	};
}
