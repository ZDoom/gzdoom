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

#define BASEXCENTER (160)
#define BASEYCENTER (100)

namespace swrenderer
{
	class RenderPlayerSprite
	{
	public:
		static void SetupSpriteScale();

		static void RenderPlayerSprites();
		static void RenderRemainingPlayerSprites();

	private:
		static void Render(DPSprite *pspr, AActor *owner, float bobx, float boby, double wx, double wy, double ticfrac, int spriteshade);

		// Used to store a psprite's drawing information if it needs to be drawn later.
		struct vispsp_t
		{
			vissprite_t *vis;
			FDynamicColormap *basecolormap;
			int	 x1;
		};

		static TArray<vispsp_t> vispsprites;
		static unsigned int vispspindex;

		static double pspritexscale;
		static double pspritexiscale;
		static double pspriteyscale;

		static TArray<vissprite_t> avis;
	};
}
