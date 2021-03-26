
#pragma once

#include "r_draw.h"
#include "v_palette.h"
#include "r_thread.h"
#include "swrenderer/viewport/r_skydrawer.h"
#include "swrenderer/viewport/r_spandrawer.h"
#include "swrenderer/viewport/r_walldrawer.h"
#include "swrenderer/viewport/r_spritedrawer.h"
#include "swrenderer/r_swcolormaps.h"

struct FSWColormap;

namespace swrenderer
{
	class WallColumnDrawerArgs
	{
	public:
		void SetDest(int x, int y)
		{
			dc_dest = Viewport()->GetDest(x, y);
			dc_dest_y = y;
		}

		void SetCount(int count) { dc_count = count; }
		void SetTexture(const uint8_t* pixels, const uint8_t* pixels2, int height)
		{
			dc_source = pixels;
			dc_source2 = pixels2;
			dc_textureheight = height;
		}
		void SetTextureFracBits(int bits) { dc_wall_fracbits = bits; }
		void SetTextureUPos(uint32_t pos) { dc_texturefracx = pos; }
		void SetTextureVPos(fixed_t pos) { dc_texturefrac = pos; }
		void SetTextureVStep(fixed_t step) { dc_iscale = step; }

		void SetLight(float light, int shade) { mLight = light; mShade = shade; }

		uint8_t* Dest() const { return dc_dest; }
		int DestY() const { return dc_dest_y; }
		int Count() const { return dc_count; }

		uint32_t* SrcBlend() const { return wallargs->SrcBlend(); }
		uint32_t* DestBlend() const { return wallargs->DestBlend(); }
		fixed_t SrcAlpha() const { return wallargs->SrcAlpha(); }
		fixed_t DestAlpha() const { return wallargs->DestAlpha(); }

		uint32_t TextureUPos() const { return dc_texturefracx; }
		fixed_t TextureVPos() const { return dc_texturefrac; }
		fixed_t TextureVStep() const { return dc_iscale; }

		const uint8_t* TexturePixels() const { return dc_source; }
		const uint8_t* TexturePixels2() const { return dc_source2; }
		uint32_t TextureHeight() const { return dc_textureheight; }

		int TextureFracBits() const { return dc_wall_fracbits; }

		FVector3 dc_normal = { 0,0,0 };
		FVector3 dc_viewpos = { 0,0,0 };
		FVector3 dc_viewpos_step = { 0,0,0 };
		enum { MAX_DRAWER_LIGHTS = 16 };
		DrawerLight dc_lights[MAX_DRAWER_LIGHTS];
		int dc_num_lights = 0;

		RenderViewport* Viewport() const { return wallargs->Viewport(); }

		uint8_t* Colormap(RenderViewport* viewport) const
		{
			auto basecolormap = wallargs->BaseColormap();
			if (basecolormap)
			{
				if (viewport->RenderTarget->IsBgra())
					return basecolormap->Maps;
				else
					return basecolormap->Maps + (GETPALOOKUP(mLight, mShade) << COLORMAPSHIFT);
			}
			else
			{
				return wallargs->TranslationMap();
			}
		}

		uint8_t* TranslationMap() const { return wallargs->TranslationMap(); }

		ShadeConstants ColormapConstants() const { return wallargs->ColormapConstants(); }
		fixed_t Light() const { return LIGHTSCALE(mLight, mShade); }

		FLightNode* LightList() const { return wallargs->lightlist; }

		const WallDrawerArgs* wallargs;

	private:
		uint8_t* dc_dest = nullptr;
		int dc_dest_y = 0;
		int dc_count = 0;

		fixed_t dc_iscale = 0;
		fixed_t dc_texturefrac = 0;
		uint32_t dc_texturefracx = 0;
		uint32_t dc_textureheight = 0;
		const uint8_t* dc_source = nullptr;
		const uint8_t* dc_source2 = nullptr;
		int dc_wall_fracbits = 0;

		float mLight = 0.0f;
		int mShade = 0;
	};

	struct DrawWallModeNormal;
	struct DrawWallModeMasked;
	struct DrawWallModeAdd;
	struct DrawWallModeAddClamp;
	struct DrawWallModeSubClamp;
	struct DrawWallModeRevSubClamp;

	class SWPalDrawers : public SWPixelFormatDrawers
	{
	public:
		using SWPixelFormatDrawers::SWPixelFormatDrawers;
		
		void DrawWall(const WallDrawerArgs &args) override { DrawWallColumns<DrawWallModeNormal>(args); }
		void DrawWallMasked(const WallDrawerArgs &args) override { DrawWallColumns<DrawWallModeMasked>(args); }
		void DrawWallAdd(const WallDrawerArgs &args) override { DrawWallColumns<DrawWallModeAdd>(args); }
		void DrawWallAddClamp(const WallDrawerArgs &args) override { DrawWallColumns<DrawWallModeAddClamp>(args); }
		void DrawWallSubClamp(const WallDrawerArgs &args) override { DrawWallColumns<DrawWallModeSubClamp>(args); }
		void DrawWallRevSubClamp(const WallDrawerArgs &args) override { DrawWallColumns<DrawWallModeRevSubClamp>(args); }
		void DrawSingleSkyColumn(const SkyDrawerArgs& args) override;
		void DrawDoubleSkyColumn(const SkyDrawerArgs& args) override;
		void DrawColumn(const SpriteDrawerArgs& args) override;
		void FillColumn(const SpriteDrawerArgs& args) override;
		void FillAddColumn(const SpriteDrawerArgs& args) override;
		void FillAddClampColumn(const SpriteDrawerArgs& args) override;
		void FillSubClampColumn(const SpriteDrawerArgs& args) override;
		void FillRevSubClampColumn(const SpriteDrawerArgs& args) override;
		void DrawFuzzColumn(const SpriteDrawerArgs &args) override
		{
			if (r_fuzzscale)
				DrawScaledFuzzColumn(args);
			else
				DrawUnscaledFuzzColumn(args);
			R_UpdateFuzzPos(args);
		}
		void DrawAddColumn(const SpriteDrawerArgs& args) override;
		void DrawTranslatedColumn(const SpriteDrawerArgs& args) override;
		void DrawTranslatedAddColumn(const SpriteDrawerArgs& args) override;
		void DrawShadedColumn(const SpriteDrawerArgs& args) override;
		void DrawAddClampShadedColumn(const SpriteDrawerArgs& args) override;
		void DrawAddClampColumn(const SpriteDrawerArgs& args) override;
		void DrawAddClampTranslatedColumn(const SpriteDrawerArgs& args) override;
		void DrawSubClampColumn(const SpriteDrawerArgs& args) override;
		void DrawSubClampTranslatedColumn(const SpriteDrawerArgs& args) override;
		void DrawRevSubClampColumn(const SpriteDrawerArgs& args) override;
		void DrawRevSubClampTranslatedColumn(const SpriteDrawerArgs& args) override;
		void DrawVoxelBlocks(const SpriteDrawerArgs& args, const VoxelBlock* blocks, int blockcount) override;
		void DrawSpan(const SpanDrawerArgs& args) override;
		void DrawSpanMasked(const SpanDrawerArgs& args) override;
		void DrawSpanTranslucent(const SpanDrawerArgs& args) override;
		void DrawSpanMaskedTranslucent(const SpanDrawerArgs& args) override;
		void DrawSpanAddClamp(const SpanDrawerArgs& args) override;
		void DrawSpanMaskedAddClamp(const SpanDrawerArgs& args) override;
		void FillSpan(const SpanDrawerArgs& args) override;
		void DrawTiltedSpan(const SpanDrawerArgs& args, const FVector3& plane_sz, const FVector3& plane_su, const FVector3& plane_sv, bool plane_shade, int planeshade, float planelightfloat, fixed_t pviewx, fixed_t pviewy, FDynamicColormap* basecolormap) override;
		void DrawColoredSpan(const SpanDrawerArgs& args) override;
		void DrawFogBoundaryLine(const SpanDrawerArgs& args) override;

		void DrawParticleColumn(int x, int yl, int ycount, uint32_t fg, uint32_t alpha, uint32_t fracposx) override;

		void DrawScaledFuzzColumn(const SpriteDrawerArgs& args);
		void DrawUnscaledFuzzColumn(const SpriteDrawerArgs& args);

		inline static uint8_t AddLightsColumn(const DrawerLight* lights, int num_lights, float viewpos_z, uint8_t fg, uint8_t material);
		inline static uint8_t AddLightsSpan(const DrawerLight* lights, int num_lights, float viewpos_z, uint8_t fg, uint8_t material);
		inline static uint8_t AddLights(uint8_t fg, uint8_t material, uint32_t lit_r, uint32_t lit_g, uint32_t lit_b);

		void CalcTiltedLighting(double lstart, double lend, int width, int planeshade, uint8_t* basecolormapdata);

		template<typename DrawerT> void DrawWallColumns(const WallDrawerArgs& args);
		template<typename DrawerT> void DrawWallColumn8(WallColumnDrawerArgs& drawerargs, int x, int y1, int y2, uint32_t texelX, uint32_t texelY, uint32_t texelStepY);
		template<typename DrawerT> void DrawWallColumn(const WallColumnDrawerArgs& args);

		// Working buffer used by the tilted (sloped) span drawer
		const uint8_t* tiltlighting[MAXWIDTH];

		WallColumnDrawerArgs wallcolargs;
	};
}
