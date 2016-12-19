
#pragma once

#include "r_defs.h"

EXTERN_CVAR(Bool, r_multithreaded);
EXTERN_CVAR(Int, r_drawfuzz);
EXTERN_CVAR(Bool, r_drawtrans);
EXTERN_CVAR(Float, transsouls);
EXTERN_CVAR(Int, r_columnmethod);

namespace swrenderer
{
	struct vissprite_t;

	extern double dc_texturemid;

	namespace drawerargs
	{
		extern int dc_pitch;
		extern lighttable_t *dc_colormap;
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

	// Spectre/Invisibility.
	#define FUZZTABLE 50
	extern int fuzzoffset[FUZZTABLE + 1];
	extern int fuzzpos;
	extern int fuzzviewheight;

	void R_InitColumnDrawers();
	void R_InitShadeMaps();
	void R_InitFuzzTable(int fuzzoff);

	enum ESPSResult
	{
		DontDraw,	// not useful to draw this
		DoDraw0,	// draw this as if r_columnmethod is 0
		DoDraw1,	// draw this as if r_columnmethod is 1
	};

	ESPSResult R_SetPatchStyle(FRenderStyle style, fixed_t alpha, int translation, uint32_t color);
	ESPSResult R_SetPatchStyle(FRenderStyle style, float alpha, int translation, uint32_t color);
	void R_FinishSetPatchStyle(); // Call this after finished drawing the current thing, in case its style was STYLE_Shade
	bool R_GetTransMaskDrawers(void(**drawCol1)(), void(**drawCol4)());

	const uint8_t *R_GetColumn(FTexture *tex, int col);
	
	void rt_initcols(uint8_t *buffer = nullptr);
	void rt_span_coverage(int x, int start, int stop);
	void rt_draw4cols(int sx);
	void rt_flip_posts();
	void rt_copy1col(int hx, int sx, int yl, int yh);
	void rt_copy4cols(int sx, int yl, int yh);
	void rt_shaded1col(int hx, int sx, int yl, int yh);
	void rt_shaded4cols(int sx, int yl, int yh);
	void rt_map1col(int hx, int sx, int yl, int yh);
	void rt_add1col(int hx, int sx, int yl, int yh);
	void rt_addclamp1col(int hx, int sx, int yl, int yh);
	void rt_subclamp1col(int hx, int sx, int yl, int yh);
	void rt_revsubclamp1col(int hx, int sx, int yl, int yh);
	void rt_tlate1col(int hx, int sx, int yl, int yh);
	void rt_tlateadd1col(int hx, int sx, int yl, int yh);
	void rt_tlateaddclamp1col(int hx, int sx, int yl, int yh);
	void rt_tlatesubclamp1col(int hx, int sx, int yl, int yh);
	void rt_tlaterevsubclamp1col(int hx, int sx, int yl, int yh);
	void rt_map4cols(int sx, int yl, int yh);
	void rt_add4cols(int sx, int yl, int yh);
	void rt_addclamp4cols(int sx, int yl, int yh);
	void rt_subclamp4cols(int sx, int yl, int yh);
	void rt_revsubclamp4cols(int sx, int yl, int yh);
	void rt_tlate4cols(int sx, int yl, int yh);
	void rt_tlateadd4cols(int sx, int yl, int yh);
	void rt_tlateaddclamp4cols(int sx, int yl, int yh);
	void rt_tlatesubclamp4cols(int sx, int yl, int yh);
	void rt_tlaterevsubclamp4cols(int sx, int yl, int yh);
	void R_DrawColumnHoriz();
	void R_DrawColumn();
	void R_DrawFuzzColumn();
	void R_DrawTranslatedColumn();
	void R_DrawShadedColumn();
	void R_FillColumn();
	void R_FillAddColumn();
	void R_FillAddClampColumn();
	void R_FillSubClampColumn();
	void R_FillRevSubClampColumn();
	void R_DrawAddColumn();
	void R_DrawTlatedAddColumn();
	void R_DrawAddClampColumn();
	void R_DrawAddClampTranslatedColumn();
	void R_DrawSubClampColumn();
	void R_DrawSubClampTranslatedColumn();
	void R_DrawRevSubClampColumn();
	void R_DrawRevSubClampTranslatedColumn();
	void R_DrawSpan();
	void R_DrawSpanMasked();
	void R_DrawSpanTranslucent();
	void R_DrawSpanMaskedTranslucent();
	void R_DrawSpanAddClamp();
	void R_DrawSpanMaskedAddClamp();
	void R_FillSpan();
	void R_DrawTiltedSpan(int y, int x1, int x2, const FVector3 &plane_sz, const FVector3 &plane_su, const FVector3 &plane_sv, bool plane_shade, int planeshade, float planelightfloat, fixed_t pviewx, fixed_t pviewy);
	void R_DrawColoredSpan(int y, int x1, int x2);
	void R_SetupDrawSlab(uint8_t *colormap);
	void R_DrawSlab(int dx, fixed_t v, int dy, fixed_t vi, const uint8_t *vptr, uint8_t *p);
	void R_DrawFogBoundary(int x1, int x2, short *uclip, short *dclip);
	void R_FillColumnHoriz();
	void R_FillSpan();

	void R_DrawWallCol1();
	void R_DrawWallCol4();
	void R_DrawWallMaskedCol1();
	void R_DrawWallMaskedCol4();
	void R_DrawWallAddCol1();
	void R_DrawWallAddCol4();
	void R_DrawWallAddClampCol1();
	void R_DrawWallAddClampCol4();
	void R_DrawWallSubClampCol1();
	void R_DrawWallSubClampCol4();
	void R_DrawWallRevSubClampCol1();
	void R_DrawWallRevSubClampCol4();

	void R_DrawSingleSkyCol1(uint32_t solid_top, uint32_t solid_bottom);
	void R_DrawSingleSkyCol4(uint32_t solid_top, uint32_t solid_bottom);
	void R_DrawDoubleSkyCol1(uint32_t solid_top, uint32_t solid_bottom);
	void R_DrawDoubleSkyCol4(uint32_t solid_top, uint32_t solid_bottom);

	void R_SetColorMapLight(lighttable_t *base_colormap, float light, int shade);
	void R_SetColorMapLight(FDynamicColormap *base_colormap, float light, int shade);
	void R_SetDSColorMapLight(lighttable_t *base_colormap, float light, int shade);
	void R_SetDSColorMapLight(FDynamicColormap *base_colormap, float light, int shade);
	void R_SetTranslationMap(lighttable_t *translation);

	void R_SetupSpanBits(FTexture *tex);
	void R_SetSpanColormap(lighttable_t *colormap);
	void R_SetSpanSource(FTexture *tex);

	void R_MapTiltedPlane(int y, int x1);
	void R_MapColoredPlane(int y, int x1);
	void R_DrawParticle(vissprite_t *);
}
