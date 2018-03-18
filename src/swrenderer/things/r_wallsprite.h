#pragma once

#include "r_visiblesprite.h"

namespace swrenderer
{
	class ProjectedWallTexcoords;
	class SpriteDrawerArgs;

	class RenderWallSprite : public VisibleSprite
	{
	public:
		static void Project(RenderThread *thread, AActor *thing, const DVector3 &pos, FTexture *pic, const DVector2 &scale, int renderflags, int spriteshade, bool foggy, FDynamicColormap *basecolormap);

	protected:
		bool IsWallSprite() const override { return true; }
		void Render(RenderThread *thread, short *cliptop, short *clipbottom, int minZ, int maxZ, Fake3DTranslucent clip3DFloor) override;

	private:
		static void DrawColumn(RenderThread *thread, SpriteDrawerArgs &drawerargs, int x, FTexture *WallSpriteTile, const ProjectedWallTexcoords &walltexcoords, double texturemid, float maskedScaleY, bool sprflipvert, const short *mfloorclip, const short *mceilingclip, FRenderStyle style);

		FWallCoords wallc;
		uint32_t Translation = 0;
		uint32_t FillColor = 0;
	};
}
