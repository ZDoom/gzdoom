
#pragma once

#include "r_visiblesprite.h"

namespace swrenderer
{
	void R_ProjectWallSprite(AActor *thing, const DVector3 &pos, FTextureID picnum, const DVector2 &scale, int renderflags);
	void R_DrawWallSprite(vissprite_t *spr);
	void R_WallSpriteColumn(int x, float maskedScaleY);
}
