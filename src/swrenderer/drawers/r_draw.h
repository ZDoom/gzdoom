
#pragma once

#include "r_defs.h"

struct FSWColormap;
struct FLightNode;
struct TriLight;

EXTERN_CVAR(Bool, r_multithreaded);
EXTERN_CVAR(Bool, r_magfilter);
EXTERN_CVAR(Bool, r_minfilter);
EXTERN_CVAR(Bool, r_mipmap);
EXTERN_CVAR(Float, r_lod_bias);
EXTERN_CVAR(Int, r_drawfuzz);
EXTERN_CVAR(Bool, r_drawtrans);
EXTERN_CVAR(Float, transsouls);
EXTERN_CVAR(Bool, r_dynlights);

namespace swrenderer
{
	struct vissprite_t;

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

	namespace drawerargs
	{
		extern int dc_pitch;
		extern lighttable_t *dc_colormap;
		extern FSWColormap *dc_fcolormap;
		extern ShadeConstants dc_shade_constants;
		extern fixed_t dc_light;
		extern int dc_x;
		extern int dc_yl;
		extern int dc_yh;
		extern fixed_t dc_iscale;
		extern fixed_t dc_texturefrac;
		extern uint32_t dc_textureheight;
		extern int dc_color;
		extern uint32_t dc_srccolor;
		extern uint32_t dc_srccolor_bgra;
		extern uint32_t *dc_srcblend;
		extern uint32_t *dc_destblend;
		extern fixed_t dc_srcalpha;
		extern fixed_t dc_destalpha;
		extern const uint8_t *dc_source;
		extern const uint8_t *dc_source2;
		extern uint32_t dc_texturefracx;
		extern uint8_t *dc_translation;
		extern uint8_t *dc_dest;
		extern uint8_t *dc_destorg;
		extern int dc_destheight;
		extern int dc_count;
		extern FVector3 dc_normal;
		extern FVector3 dc_viewpos;
		extern FVector3 dc_viewpos_step;
		extern TriLight *dc_lights;
		extern int dc_num_lights;

		extern bool drawer_needs_pal_input;

		extern uint32_t dc_wall_texturefrac[4];
		extern uint32_t dc_wall_iscale[4];
		extern uint8_t *dc_wall_colormap[4];
		extern fixed_t dc_wall_light[4];
		extern const uint8_t *dc_wall_source[4];
		extern const uint8_t *dc_wall_source2[4];
		extern uint32_t dc_wall_texturefracx[4];
		extern uint32_t dc_wall_sourceheight[4];
		extern int dc_wall_fracbits;

		extern int ds_y;
		extern int ds_x1;
		extern int ds_x2;
		extern lighttable_t * ds_colormap;
		extern FSWColormap *ds_fcolormap;
		extern ShadeConstants ds_shade_constants;
		extern dsfixed_t ds_light;
		extern dsfixed_t ds_xfrac;
		extern dsfixed_t ds_yfrac;
		extern dsfixed_t ds_xstep;
		extern dsfixed_t ds_ystep;
		extern int ds_xbits;
		extern int ds_ybits;
		extern fixed_t ds_alpha;
		extern double ds_lod;
		extern const uint8_t *ds_source;
		extern bool ds_source_mipmapped;
		extern int ds_color;

		extern unsigned int dc_tspans[4][MAXHEIGHT];
		extern unsigned int *dc_ctspan[4];
		extern unsigned int *horizspan[4];
	}

	extern int ylookup[MAXHEIGHT];
	extern uint8_t shadetables[/*NUMCOLORMAPS*16*256*/];
	extern FDynamicColormap ShadeFakeColormap[16];
	extern uint8_t identitymap[256];
	extern FDynamicColormap identitycolormap;

	// Constant arrays used for psprite clipping and initializing clipping.
	extern short zeroarray[MAXWIDTH];
	extern short screenheightarray[MAXWIDTH];

	// Spectre/Invisibility.
	#define FUZZTABLE 50
	extern int fuzzoffset[FUZZTABLE + 1];
	extern int fuzzpos;
	extern int fuzzviewheight;

	extern uint32_t particle_texture[16 * 16];

	extern bool r_swtruecolor;

	class SWPixelFormatDrawers
	{
	public:
		virtual ~SWPixelFormatDrawers() { }
		virtual void DrawWallColumn() = 0;
		virtual void DrawWallMaskedColumn() = 0;
		virtual void DrawWallAddColumn() = 0;
		virtual void DrawWallAddClampColumn() = 0;
		virtual void DrawWallSubClampColumn() = 0;
		virtual void DrawWallRevSubClampColumn() = 0;
		virtual void DrawSingleSkyColumn(uint32_t solid_top, uint32_t solid_bottom) = 0;
		virtual void DrawDoubleSkyColumn(uint32_t solid_top, uint32_t solid_bottom) = 0;
		virtual void DrawColumn() = 0;
		virtual void FillColumn() = 0;
		virtual void FillAddColumn() = 0;
		virtual void FillAddClampColumn() = 0;
		virtual void FillSubClampColumn() = 0;
		virtual void FillRevSubClampColumn() = 0;
		virtual void DrawFuzzColumn() = 0;
		virtual void DrawAddColumn() = 0;
		virtual void DrawTranslatedColumn() = 0;
		virtual void DrawTranslatedAddColumn() = 0;
		virtual void DrawShadedColumn() = 0;
		virtual void DrawAddClampColumn() = 0;
		virtual void DrawAddClampTranslatedColumn() = 0;
		virtual void DrawSubClampColumn() = 0;
		virtual void DrawSubClampTranslatedColumn() = 0;
		virtual void DrawRevSubClampColumn() = 0;
		virtual void DrawRevSubClampTranslatedColumn() = 0;
		virtual void DrawSpan() = 0;
		virtual void DrawSpanMasked() = 0;
		virtual void DrawSpanTranslucent() = 0;
		virtual void DrawSpanMaskedTranslucent() = 0;
		virtual void DrawSpanAddClamp() = 0;
		virtual void DrawSpanMaskedAddClamp() = 0;
		virtual void FillSpan() = 0;
		virtual void DrawTiltedSpan(int y, int x1, int x2, const FVector3 &plane_sz, const FVector3 &plane_su, const FVector3 &plane_sv, bool plane_shade, int planeshade, float planelightfloat, fixed_t pviewx, fixed_t pviewy, FDynamicColormap *basecolormap) = 0;
		virtual void DrawColoredSpan(int y, int x1, int x2) = 0;
		virtual void DrawFogBoundaryLine(int y, int x1, int x2) = 0;
	};

	typedef void(SWPixelFormatDrawers::*DrawerFunc)();

	SWPixelFormatDrawers *R_Drawers();

	void R_InitColumnDrawers();
	void R_InitShadeMaps();
	void R_InitFuzzTable(int fuzzoff);
	void R_InitParticleTexture();

	bool R_SetPatchStyle(FRenderStyle style, fixed_t alpha, int translation, uint32_t color, FDynamicColormap *&basecolormap);
	bool R_SetPatchStyle(FRenderStyle style, float alpha, int translation, uint32_t color, FDynamicColormap *&basecolormap);
	DrawerFunc R_GetTransMaskDrawer();

	void R_UpdateFuzzPos();

	// Sets dc_colormap and dc_light to their appropriate values depending on the output format (pal vs true color)
	void R_SetColorMapLight(FSWColormap *base_colormap, float light, int shade);
	void R_SetDSColorMapLight(FSWColormap *base_colormap, float light, int shade);
	void R_SetTranslationMap(lighttable_t *translation);

	void R_SetSpanTexture(FTexture *tex);
	void R_SetSpanColormap(FDynamicColormap *colormap, int shade);

	void R_DrawMaskedColumn(int x, fixed_t iscale, FTexture *texture, fixed_t column, double spryscale, double sprtopscreen, bool sprflipvert, const short *mfloorclip, const short *mceilingclip, bool unmasked = false);
	void R_DrawMaskedColumnBgra(int x, fixed_t iscale, FTexture *tex, fixed_t column, double spryscale, double sprtopscreen, bool sprflipvert, const short *mfloorclip, const short *mceilingclip, bool unmasked);

	extern DrawerFunc colfunc;
	extern DrawerFunc basecolfunc;
	extern DrawerFunc fuzzcolfunc;
	extern DrawerFunc transcolfunc;
	extern DrawerFunc spanfunc;
}
