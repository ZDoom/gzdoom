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
	void R_SetupPlayerSpriteScale();

	void R_DrawPlayerSprites();
	void R_DrawPSprite(DPSprite *pspr, AActor *owner, float bobx, float boby, double wx, double wy, double ticfrac);
	void R_DrawRemainingPlayerSprites();
}
