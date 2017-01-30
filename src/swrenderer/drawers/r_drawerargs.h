
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

	typedef void(SWPixelFormatDrawers::*DrawerFunc)(const DrawerArgs &args);
	typedef void(SWPixelFormatDrawers::*WallDrawerFunc)(const WallDrawerArgs &args);
	typedef void(SWPixelFormatDrawers::*ColumnDrawerFunc)(const ColumnDrawerArgs &args);
	typedef void(SWPixelFormatDrawers::*SpanDrawerFunc)(const SpanDrawerArgs &args);

	class DrawerArgs
	{
	public:
		DrawerArgs();

		bool SetPatchStyle(FRenderStyle style, fixed_t alpha, int translation, uint32_t color, FDynamicColormap *&basecolormap, fixed_t shadedlightshade = 0);
		bool SetPatchStyle(FRenderStyle style, float alpha, int translation, uint32_t color, FDynamicColormap *&basecolormap, fixed_t shadedlightshade = 0);
		void SetSpanStyle(bool masked, bool additive, fixed_t alpha);

		void SetColorMapLight(FSWColormap *base_colormap, float light, int shade);
		void SetTranslationMap(lighttable_t *translation);

		uint8_t *Colormap() const;
		uint8_t *TranslationMap() const { return mTranslation; }

		ShadeConstants ColormapConstants() const;
		fixed_t Light() const { return LIGHTSCALE(mLight, mShade); }

		SWPixelFormatDrawers *Drawers() const;

		ColumnDrawerFunc colfunc;
		ColumnDrawerFunc basecolfunc;
		ColumnDrawerFunc fuzzcolfunc;
		ColumnDrawerFunc transcolfunc;
		SpanDrawerFunc spanfunc;

		uint32_t *dc_srcblend;
		uint32_t *dc_destblend;
		fixed_t dc_srcalpha;
		fixed_t dc_destalpha;

		int dc_color = 0;
		uint32_t dc_srccolor;
		uint32_t dc_srccolor_bgra;

	protected:
		bool drawer_needs_pal_input = false;

	private:
		bool SetBlendFunc(int op, fixed_t fglevel, fixed_t bglevel, int flags);
		static fixed_t GetAlpha(int type, fixed_t alpha);

		FSWColormap *mBaseColormap = nullptr;
		float mLight = 0.0f;
		int mShade = 0;
		uint8_t *mTranslation = nullptr;
	};

	class SkyDrawerArgs : public DrawerArgs
	{
	public:
		const uint8_t *dc_wall_source;
		const uint8_t *dc_wall_source2;
		uint32_t dc_wall_sourceheight[2];
		uint32_t dc_wall_texturefrac;
		uint32_t dc_wall_iscale;
		int dc_count;

		void SetDest(int x, int y);

		uint8_t *Dest() const { return dc_dest; }
		int DestY() const { return dc_dest_y; }

		void DrawSingleSkyColumn(uint32_t solid_top, uint32_t solid_bottom, bool fadeSky);
		void DrawDoubleSkyColumn(uint32_t solid_top, uint32_t solid_bottom, bool fadeSky);

	private:
		uint8_t *dc_dest = nullptr;
		int dc_dest_y = 0;
	};

	class WallDrawerArgs : public DrawerArgs
	{
	public:
		void SetDest(int x, int y);

		WallDrawerFunc GetTransMaskDrawer();

		uint8_t *Dest() const { return dc_dest; }
		int DestY() const { return dc_dest_y; }

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
	};

	class SpanDrawerArgs : public DrawerArgs
	{
	public:
		void SetSpanTexture(FTexture *tex);

		void DrawTiltedSpan(int y, int x1, int x2, const FVector3 &plane_sz, const FVector3 &plane_su, const FVector3 &plane_sv, bool plane_shade, int planeshade, float planelightfloat, fixed_t pviewx, fixed_t pviewy, FDynamicColormap *basecolormap);
		void DrawColoredSpan(int y, int x1, int x2);
		void DrawFogBoundaryLine(int y, int x1, int x2);

		int ds_y;
		int ds_x1;
		int ds_x2;
		dsfixed_t ds_xfrac;
		dsfixed_t ds_yfrac;
		dsfixed_t ds_xstep;
		dsfixed_t ds_ystep;
		int ds_xbits;
		int ds_ybits;
		fixed_t ds_alpha;
		double ds_lod;
		const uint8_t *ds_source;
		bool ds_source_mipmapped;
		int ds_color = 0;

		FVector3 dc_normal;
		FVector3 dc_viewpos;
		FVector3 dc_viewpos_step;
		TriLight *dc_lights = nullptr;
		int dc_num_lights = 0;
	};

	class ColumnDrawerArgs : public DrawerArgs
	{
	public:
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
		uint32_t dc_textureheight;
		const uint8_t *dc_source;
		const uint8_t *dc_source2;
		uint32_t dc_texturefracx;
		int dc_count;

	private:
		void DrawMaskedColumnBgra(int x, fixed_t iscale, FTexture *tex, fixed_t column, double spryscale, double sprtopscreen, bool sprflipvert, const short *mfloorclip, const short *mceilingclip, bool unmasked);

		uint8_t *dc_dest = nullptr;
		int dc_dest_y = 0;
	};

	void R_InitColumnDrawers();
}
