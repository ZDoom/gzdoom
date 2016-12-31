
#pragma once

#include "r_visiblesprite.h"

namespace swrenderer
{
	void R_DrawPlayerSprites();
	void R_DrawPSprite(DPSprite *pspr, AActor *owner, float bobx, float boby, double wx, double wy, double ticfrac);
	void R_DrawRemainingPlayerSprites();
}
