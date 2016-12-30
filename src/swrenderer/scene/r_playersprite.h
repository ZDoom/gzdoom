
#pragma once

#include "r_visible_sprite.h"

namespace swrenderer
{
	void R_DrawPlayerSprites();
	void R_DrawPSprite(DPSprite *pspr, AActor *owner, float bobx, float boby, double wx, double wy, double ticfrac);
	void R_DrawRemainingPlayerSprites();
}
