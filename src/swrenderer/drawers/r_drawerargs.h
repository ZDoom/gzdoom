
#pragma once

#include "templates.h"
#include "doomtype.h"
#include "doomdef.h"
#include "r_defs.h"
#include "r_draw.h"
#include "v_video.h"
#include "r_data/colormaps.h"
#include "r_data/r_translate.h"
#include "swrenderer/scene/r_light.h"

struct FSWColormap;
struct FLightNode;
struct TriLight;

namespace swrenderer
{
	class SWPixelFormatDrawers;
	class DrawerArgs;
	struct ShadeConstants;

	class DrawerArgs
	{
	public:
		void SetColorMapLight(FSWColormap *base_colormap, float light, int shade);
		void SetTranslationMap(lighttable_t *translation);

		uint8_t *Colormap() const;
		uint8_t *TranslationMap() const { return mTranslation; }

		ShadeConstants ColormapConstants() const;
		fixed_t Light() const { return LIGHTSCALE(mLight, mShade); }

	protected:
		static SWPixelFormatDrawers *Drawers();

	private:
		FSWColormap *mBaseColormap = nullptr;
		float mLight = 0.0f;
		int mShade = 0;
		uint8_t *mTranslation = nullptr;
	};

	class SkyDrawerArgs : public DrawerArgs
	{
	public:
		void SetDest(int x, int y);
		void SetCount(int count) { dc_count = count; }
		void SetFrontTexture(FTexture *texture, uint32_t column);
		void SetBackTexture(FTexture *texture, uint32_t column);
		void SetTextureVPos(uint32_t texturefrac) { dc_texturefrac = texturefrac; }
		void SetTextureVStep(uint32_t iscale) { dc_iscale = iscale; }
		void SetSolidTop(uint32_t color) { solid_top = color; }
		void SetSolidBottom(uint32_t color) { solid_bottom = color; }
		void SetFadeSky(bool enable) { fadeSky = enable; }

		uint8_t *Dest() const { return dc_dest; }
		int DestY() const { return dc_dest_y; }
		int Count() const { return dc_count; }

		uint32_t TextureVPos() const { return dc_texturefrac; }
		uint32_t TextureVStep() const { return dc_iscale; }

		uint32_t SolidTopColor() const { return solid_top; }
		uint32_t SolidBottomColor() const { return solid_bottom; }
		bool FadeSky() const { return fadeSky; }

		const uint8_t *FrontTexturePixels() const { return dc_source; }
		const uint8_t *BackTexturePixels() const { return dc_source2; }
		int FrontTextureHeight() const { return dc_sourceheight; }
		int BackTextureHeight() const { return dc_sourceheight; }

		void DrawSingleSkyColumn();
		void DrawDoubleSkyColumn();

	private:
		uint8_t *dc_dest = nullptr;
		int dc_dest_y = 0;
		int dc_count = 0;
		const uint8_t *dc_source;
		const uint8_t *dc_source2;
		uint32_t dc_sourceheight;
		uint32_t dc_sourceheight2;
		uint32_t dc_texturefrac;
		uint32_t dc_iscale;
		uint32_t solid_top;
		uint32_t solid_bottom;
		bool fadeSky;
	};

	class SpanDrawerArgs : public DrawerArgs
	{
	public:
		SpanDrawerArgs();

		void SetStyle(bool masked, bool additive, fixed_t alpha);
		void SetDestY(int y) { ds_y = y; }
		void SetDestX1(int x) { ds_x1 = x; }
		void SetDestX2(int x) { ds_x2 = x; }
		void SetTexture(FTexture *tex);
		void SetTextureLOD(double lod) { ds_lod = lod; }
		void SetTextureUPos(dsfixed_t xfrac) { ds_xfrac = xfrac; }
		void SetTextureVPos(dsfixed_t yfrac) { ds_yfrac = yfrac; }
		void SetTextureUStep(dsfixed_t xstep) { ds_xstep = xstep; }
		void SetTextureVStep(dsfixed_t vstep) { ds_ystep = vstep; }
		void SetSolidColor(int colorIndex) { ds_color = colorIndex; }

		void DrawSpan();
		void DrawTiltedSpan(int y, int x1, int x2, const FVector3 &plane_sz, const FVector3 &plane_su, const FVector3 &plane_sv, bool plane_shade, int planeshade, float planelightfloat, fixed_t pviewx, fixed_t pviewy, FDynamicColormap *basecolormap);
		void DrawColoredSpan(int y, int x1, int x2);
		void DrawFogBoundaryLine(int y, int x1, int x2);

		uint32_t *SrcBlend() const { return dc_srcblend; }
		uint32_t *DestBlend() const { return dc_destblend; }
		fixed_t SrcAlpha() const { return dc_srcalpha; }
		fixed_t DestAlpha() const { return dc_destalpha; }
		int DestY() const { return ds_y; }
		int DestX1() const { return ds_x1; }
		int DestX2() const { return ds_x2; }
		dsfixed_t TextureUPos() const { return ds_xfrac; }
		dsfixed_t TextureVPos() const { return ds_yfrac; }
		dsfixed_t TextureUStep() const { return ds_xstep; }
		dsfixed_t TextureVStep() const { return ds_ystep; }
		int SolidColor() const { return ds_color; }
		int TextureWidthBits() const { return ds_xbits; }
		int TextureHeightBits() const { return ds_ybits; }
		const uint8_t *TexturePixels() const { return ds_source; }
		bool MipmappedTexture() const { return ds_source_mipmapped; }
		double TextureLOD() const { return ds_lod; }

		FVector3 dc_normal;
		FVector3 dc_viewpos;
		FVector3 dc_viewpos_step;
		TriLight *dc_lights = nullptr;
		int dc_num_lights = 0;

	private:
		typedef void(SWPixelFormatDrawers::*SpanDrawerFunc)(const SpanDrawerArgs &args);
		SpanDrawerFunc spanfunc;

		int ds_y;
		int ds_x1;
		int ds_x2;
		int ds_xbits;
		int ds_ybits;
		const uint8_t *ds_source;
		bool ds_source_mipmapped;
		dsfixed_t ds_xfrac;
		dsfixed_t ds_yfrac;
		dsfixed_t ds_xstep;
		dsfixed_t ds_ystep;
		uint32_t *dc_srcblend;
		uint32_t *dc_destblend;
		fixed_t dc_srcalpha;
		fixed_t dc_destalpha;
		int ds_color = 0;
		double ds_lod;
	};

	class WallDrawerArgs : public DrawerArgs
	{
	public:
		void SetStyle(bool masked, bool additive, fixed_t alpha);
		void SetDest(int x, int y);

		bool IsMaskedDrawer() const;

		void DrawColumn();

		uint8_t *Dest() const { return dc_dest; }
		int DestY() const { return dc_dest_y; }

		uint32_t *dc_srcblend;
		uint32_t *dc_destblend;
		fixed_t dc_srcalpha;
		fixed_t dc_destalpha;

		fixed_t dc_iscale;
		fixed_t dc_texturefrac;
		uint32_t dc_texturefracx;
		uint32_t dc_textureheight;
		const uint8_t *dc_source;
		const uint8_t *dc_source2;
		int dc_count;

		int dc_wall_fracbits;

		FVector3 dc_normal;
		FVector3 dc_viewpos;
		FVector3 dc_viewpos_step;
		TriLight *dc_lights = nullptr;
		int dc_num_lights = 0;

	private:
		uint8_t *dc_dest = nullptr;
		int dc_dest_y = 0;

		typedef void(SWPixelFormatDrawers::*WallDrawerFunc)(const WallDrawerArgs &args);
		WallDrawerFunc wallfunc = nullptr;
	};

	class SpriteDrawerArgs : public DrawerArgs
	{
	public:
		SpriteDrawerArgs();

		bool SetPatchStyle(FRenderStyle style, fixed_t alpha, int translation, uint32_t color, FDynamicColormap *&basecolormap, fixed_t shadedlightshade = 0);
		bool SetPatchStyle(FRenderStyle style, float alpha, int translation, uint32_t color, FDynamicColormap *&basecolormap, fixed_t shadedlightshade = 0);

		void DrawMaskedColumn(int x, fixed_t iscale, FTexture *texture, fixed_t column, double spryscale, double sprtopscreen, bool sprflipvert, const short *mfloorclip, const short *mceilingclip, bool unmasked = false);
		void FillColumn();

		void SetDest(int x, int y);

		uint8_t *Dest() const { return dc_dest; }
		int DestY() const { return dc_dest_y; }

		int dc_x;
		int dc_yl;
		int dc_yh;

		fixed_t dc_iscale;
		fixed_t dc_texturefrac;
		uint32_t dc_texturefracx;
		uint32_t dc_textureheight;
		const uint8_t *dc_source;
		const uint8_t *dc_source2;
		int dc_count;

		int dc_color = 0;
		uint32_t dc_srccolor;
		uint32_t dc_srccolor_bgra;

		uint32_t *dc_srcblend;
		uint32_t *dc_destblend;
		fixed_t dc_srcalpha;
		fixed_t dc_destalpha;

	private:
		bool SetBlendFunc(int op, fixed_t fglevel, fixed_t bglevel, int flags);
		static fixed_t GetAlpha(int type, fixed_t alpha);
		void DrawMaskedColumnBgra(int x, fixed_t iscale, FTexture *tex, fixed_t column, double spryscale, double sprtopscreen, bool sprflipvert, const short *mfloorclip, const short *mceilingclip, bool unmasked);

		uint8_t *dc_dest = nullptr;
		int dc_dest_y = 0;
		bool drawer_needs_pal_input = false;

		typedef void(SWPixelFormatDrawers::*SpriteDrawerFunc)(const SpriteDrawerArgs &args);
		SpriteDrawerFunc colfunc;
	};

	struct ShadeConstants
	{
		uint16_t light_alpha;
		uint16_t light_red;
		uint16_t light_green;
		uint16_t light_blue;
		uint16_t fade_alpha;
		uint16_t fade_red;
		uint16_t fade_green;
		uint16_t fade_blue;
		uint16_t desaturate;
		bool simple_shade;
	};
}
