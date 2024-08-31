#pragma once

#include "r_visiblesprite.h"

namespace swrenderer
{
	class ProjectedWallTexcoords;
	class SpriteDrawerArgs;

	class RenderWallSprite : public VisibleSprite
	{
	public:
		static void Project(RenderThread *thread, AActor *thing, const DVector3 &pos, FSoftwareTexture *pic, const DVector2 &scale, int renderflags, int lightlevel, bool foggy, FDynamicColormap *basecolormap);

	protected:
		bool IsWallSprite() const override { return true; }
		void Render(RenderThread *thread, short *cliptop, short *clipbottom, int minZ, int maxZ, Fake3DTranslucent clip3DFloor) override;

	private:
		FWallCoords wallc;
		FTranslationID Translation = NO_TRANSLATION;
		uint32_t FillColor = 0;
	};
}
