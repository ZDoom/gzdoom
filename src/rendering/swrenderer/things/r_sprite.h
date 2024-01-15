#pragma once

#include "r_visiblesprite.h"

namespace swrenderer
{
	class RenderSprite : public VisibleSprite
	{
	public:
		static void Project(RenderThread *thread, AActor *thing, const DVector3 &pos, FSoftwareTexture *tex, const DVector2 &spriteScale, int renderflags, WaterFakeSide fakeside, F3DFloor *fakefloor, F3DFloor *fakeceiling, sector_t *current_sector, int lightlevel, bool foggy, FDynamicColormap *basecolormap, bool isSpriteShadow = false);

	protected:
		void Render(RenderThread *thread, short *cliptop, short *clipbottom, int minZ, int maxZ, Fake3DTranslucent clip3DFloor) override;

	private:
		FWallCoords wallc;
		double SpriteScale;

		FTranslationID Translation = NO_TRANSLATION;
		uint32_t FillColor = 0;
		
		uint32_t dynlightcolor = 0;
	};
}
