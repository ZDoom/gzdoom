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
	class RenderSprite : public VisibleSprite
	{
	public:
		static void Project(AActor *thing, const DVector3 &pos, FTexture *tex, const DVector2 &spriteScale, int renderflags, WaterFakeSide fakeside, F3DFloor *fakefloor, F3DFloor *fakeceiling, sector_t *current_sector, int spriteshade, bool foggy, FDynamicColormap *basecolormap);

	protected:
		void Render(short *cliptop, short *clipbottom, int minZ, int maxZ) override;

	private:
		fixed_t xscale;
		fixed_t	startfrac; // horizontal position of x1
		fixed_t	xiscale; // negative if flipped

		uint32_t Translation;
		uint32_t FillColor;

		friend class RenderPlayerSprite; // To do: detach sprite from playersprite!
	};
}
