#pragma once

#include "r_visiblesprite.h"

namespace swrenderer
{
	class RenderSprite : public VisibleSprite
	{
	public:
		static void Project(RenderThread *thread, AActor *thing, const DVector3 &pos, FTexture *tex, const DVector2 &spriteScale, int renderflags, WaterFakeSide fakeside, F3DFloor *fakefloor, F3DFloor *fakeceiling, sector_t *current_sector, int spriteshade, bool foggy, FDynamicColormap *basecolormap);

	protected:
		void Render(RenderThread *thread, short *cliptop, short *clipbottom, int minZ, int maxZ, Fake3DTranslucent clip3DFloor) override;

	private:
		fixed_t xscale = 0;
		fixed_t	startfrac = 0; // horizontal position of x1
		fixed_t	xiscale = 0; // negative if flipped

		uint32_t Translation = 0;
		uint32_t FillColor = 0;
		
		uint32_t dynlightcolor = 0;
	};
}
