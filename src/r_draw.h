// Emacs style mode select	 -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// DESCRIPTION:
//		System specific interface stuff.
//
//-----------------------------------------------------------------------------


#ifndef __R_DRAW__
#define __R_DRAW__

#include "r_defs.h"

struct FSWColormap;

EXTERN_CVAR(Bool, r_multithreaded);
EXTERN_CVAR(Bool, r_magfilter);
EXTERN_CVAR(Bool, r_minfilter);
EXTERN_CVAR(Bool, r_mipmap);
EXTERN_CVAR(Float, r_lod_bias);

namespace swrenderer       
{

// Spectre/Invisibility.
#define FUZZTABLE	50
extern "C" int 	fuzzoffset[FUZZTABLE + 1];	// [RH] +1 for the assembly routine
extern "C" int 	fuzzpos;
extern "C" int	fuzzviewheight;

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

extern "C" int			ylookup[MAXHEIGHT];

extern "C" int			dc_pitch;		// [RH] Distance between rows

extern "C" lighttable_t*dc_colormap;
extern "C" FSWColormap	*dc_fcolormap;
extern "C" ShadeConstants dc_shade_constants;
extern "C" fixed_t		dc_light;
extern "C" int			dc_x;
extern "C" int			dc_yl;
extern "C" int			dc_yh;
extern "C" fixed_t		dc_iscale;
extern     double		dc_texturemid;
extern "C" fixed_t		dc_texturefrac;
extern "C" uint32_t		dc_textureheight;
extern "C" int			dc_color;		// [RH] For flat colors (no texturing)
extern "C" DWORD		dc_srccolor;
extern "C" uint32_t		dc_srccolor_bgra;
extern "C" DWORD		*dc_srcblend;
extern "C" DWORD		*dc_destblend;
extern "C" fixed_t		dc_srcalpha;
extern "C" fixed_t		dc_destalpha;

// first pixel in a column
extern "C" const BYTE*	dc_source;
extern "C" const BYTE*	dc_source2;
extern "C" uint32_t		dc_texturefracx;

extern "C" BYTE			*dc_dest, *dc_destorg;
extern "C" int			dc_destheight;
extern "C" int			dc_count;

extern "C" DWORD		vplce[4];
extern "C" DWORD		vince[4];
extern "C" BYTE*		palookupoffse[4];
extern "C" fixed_t		palookuplight[4];
extern "C" const BYTE*	bufplce[4];
extern "C" const BYTE*	bufplce2[4];
extern "C" uint32_t		buftexturefracx[4];
extern "C" uint32_t		bufheight[4];

// [RH] Temporary buffer for column drawing
extern "C" BYTE			*dc_temp;
extern "C" unsigned int	dc_tspans[4][MAXHEIGHT];
extern "C" unsigned int	*dc_ctspan[4];
extern "C" unsigned int	horizspans[4];

// [RH] Pointers to the different column and span drawers...

// The span blitting interface.
// Hook in assembler or system specific BLT here.
extern void (*R_DrawColumn)(void);

extern DWORD (*dovline1) ();
extern DWORD (*doprevline1) ();
extern void (*dovline4) ();
extern void setupvline (int);

extern DWORD (*domvline1) ();
extern void (*domvline4) ();
extern void setupmvline (int);

extern void setuptmvline (int);

// The Spectre/Invisibility effect.
extern void (*R_DrawFuzzColumn)(void);

// [RH] Draw shaded column
extern void (*R_DrawShadedColumn)(void);

// Draw with color translation tables, for player sprite rendering,
//	Green/Red/Blue/Indigo shirts.
extern void (*R_DrawTranslatedColumn)(void);

// Span drawing for rows, floor/ceiling. No Spectre effect needed.
extern void (*R_DrawSpan)(void);
void R_SetupSpanBits(FTexture *tex);
void R_SetSpanColormap(FDynamicColormap *colormap, int shade);
void R_SetSpanSource(FTexture *tex);

// Span drawing for masked textures.
extern void (*R_DrawSpanMasked)(void);

// Span drawing for translucent textures.
extern void (*R_DrawSpanTranslucent)(void);

// Span drawing for masked, translucent textures.
extern void (*R_DrawSpanMaskedTranslucent)(void);

// Span drawing for translucent, additive textures.
extern void (*R_DrawSpanAddClamp)(void);

// Span drawing for masked, translucent, additive textures.
extern void (*R_DrawSpanMaskedAddClamp)(void);

// [RH] Span blit into an interleaved intermediate buffer
extern void (*R_DrawColumnHoriz)(void);

// [RH] Initialize the above pointers
void R_InitColumnDrawers ();

// [RH] Moves data from the temporary buffer to the screen.
extern "C"
{
void rt_copy1col_c (int hx, int sx, int yl, int yh);
void rt_copy4cols_c (int sx, int yl, int yh);

void rt_shaded1col_c (int hx, int sx, int yl, int yh);
void rt_shaded4cols_c (int sx, int yl, int yh);
void rt_shaded4cols_asm (int sx, int yl, int yh);

void rt_map1col_c (int hx, int sx, int yl, int yh);
void rt_add1col_c (int hx, int sx, int yl, int yh);
void rt_addclamp1col_c (int hx, int sx, int yl, int yh);
void rt_subclamp1col_c (int hx, int sx, int yl, int yh);
void rt_revsubclamp1col_c (int hx, int sx, int yl, int yh);

void rt_tlate1col_c (int hx, int sx, int yl, int yh);
void rt_tlateadd1col_c (int hx, int sx, int yl, int yh);
void rt_tlateaddclamp1col_c (int hx, int sx, int yl, int yh);
void rt_tlatesubclamp1col_c (int hx, int sx, int yl, int yh);
void rt_tlaterevsubclamp1col_c (int hx, int sx, int yl, int yh);

void rt_map4cols_c (int sx, int yl, int yh);
void rt_add4cols_c (int sx, int yl, int yh);
void rt_addclamp4cols_c (int sx, int yl, int yh);
void rt_subclamp4cols_c (int sx, int yl, int yh);
void rt_revsubclamp4cols_c (int sx, int yl, int yh);

void rt_tlate4cols_c (int sx, int yl, int yh);
void rt_tlateadd4cols_c (int sx, int yl, int yh);
void rt_tlateaddclamp4cols_c (int sx, int yl, int yh);
void rt_tlatesubclamp4cols_c (int sx, int yl, int yh);
void rt_tlaterevsubclamp4cols_c (int sx, int yl, int yh);

void rt_copy1col_asm (int hx, int sx, int yl, int yh);
void rt_map1col_asm (int hx, int sx, int yl, int yh);

void rt_copy4cols_asm (int sx, int yl, int yh);
void rt_map4cols_asm1 (int sx, int yl, int yh);
void rt_map4cols_asm2 (int sx, int yl, int yh);
void rt_add4cols_asm (int sx, int yl, int yh);
void rt_addclamp4cols_asm (int sx, int yl, int yh);
}

extern void (*rt_copy1col)(int hx, int sx, int yl, int yh);
extern void (*rt_copy4cols)(int sx, int yl, int yh);

extern void (*rt_shaded1col)(int hx, int sx, int yl, int yh);
extern void (*rt_shaded4cols)(int sx, int yl, int yh);

extern void (*rt_map1col)(int hx, int sx, int yl, int yh);
extern void (*rt_add1col)(int hx, int sx, int yl, int yh);
extern void (*rt_addclamp1col)(int hx, int sx, int yl, int yh);
extern void (*rt_subclamp1col)(int hx, int sx, int yl, int yh);
extern void (*rt_revsubclamp1col)(int hx, int sx, int yl, int yh);

extern void (*rt_tlate1col)(int hx, int sx, int yl, int yh);
extern void (*rt_tlateadd1col)(int hx, int sx, int yl, int yh);
extern void (*rt_tlateaddclamp1col)(int hx, int sx, int yl, int yh);
extern void (*rt_tlatesubclamp1col)(int hx, int sx, int yl, int yh);
extern void (*rt_tlaterevsubclamp1col)(int hx, int sx, int yl, int yh);

extern void (*rt_map4cols)(int sx, int yl, int yh);
extern void (*rt_add4cols)(int sx, int yl, int yh);
extern void (*rt_addclamp4cols)(int sx, int yl, int yh);
extern void (*rt_subclamp4cols)(int sx, int yl, int yh);
extern void (*rt_revsubclamp4cols)(int sx, int yl, int yh);

extern void (*rt_tlate4cols)(int sx, int yl, int yh);
extern void (*rt_tlateadd4cols)(int sx, int yl, int yh);
extern void (*rt_tlateaddclamp4cols)(int sx, int yl, int yh);
extern void (*rt_tlatesubclamp4cols)(int sx, int yl, int yh);
extern void (*rt_tlaterevsubclamp4cols)(int sx, int yl, int yh);

extern void (*rt_initcols)(BYTE *buffer);
extern void (*rt_span_coverage)(int x, int start, int stop);

void rt_flip_posts();
void rt_draw4cols (int sx);

// [RH] Preps the temporary horizontal buffer.
void rt_initcols_pal (BYTE *buffer);

void rt_span_coverage_pal(int x, int start, int stop);

extern void (*R_DrawFogBoundary)(int x1, int x2, short *uclip, short *dclip);

void R_DrawFogBoundary_C (int x1, int x2, short *uclip, short *dclip);


#ifdef X86_ASM

extern "C" void	R_DrawColumnP_Unrolled (void);
extern "C" void	R_DrawColumnHorizP_ASM (void);
extern "C" void	R_DrawColumnP_ASM (void);
extern "C" void	R_DrawFuzzColumnP_ASM (void);
		   void R_DrawTranslatedColumnP_C (void);
		   void R_DrawShadedColumnP_C (void);
extern "C" void	R_DrawSpanP_ASM (void);
extern "C" void R_DrawSpanMaskedP_ASM (void);

void	R_DrawColumnHorizP_C(void);

#else

void	R_DrawColumnP_C (void);
void	R_DrawFuzzColumnP_C (void);
void	R_DrawTranslatedColumnP_C (void);
void	R_DrawShadedColumnP_C (void);
void	R_DrawSpanP_C (void);
void	R_DrawSpanMaskedP_C (void);

#endif

void	R_DrawSpanTranslucentP_C (void);
void	R_DrawSpanMaskedTranslucentP_C (void);

void	R_DrawTlatedLucentColumnP_C (void);
#define R_DrawTlatedLucentColumn R_DrawTlatedLucentColumnP_C

extern void(*R_FillColumn)(void);
extern void(*R_FillAddColumn)(void);
extern void(*R_FillAddClampColumn)(void);
extern void(*R_FillSubClampColumn)(void);
extern void(*R_FillRevSubClampColumn)(void);
extern void(*R_DrawAddColumn)(void);
extern void(*R_DrawTlatedAddColumn)(void);
extern void(*R_DrawAddClampColumn)(void);
extern void(*R_DrawAddClampTranslatedColumn)(void);
extern void(*R_DrawSubClampColumn)(void);
extern void(*R_DrawSubClampTranslatedColumn)(void);
extern void(*R_DrawRevSubClampColumn)(void);
extern void(*R_DrawRevSubClampTranslatedColumn)(void);

extern void(*R_FillSpan)(void);
extern void(*R_FillColumnHoriz)(void);

void	R_FillColumnP_C (void);

void	R_FillColumnHorizP_C (void);
void	R_FillSpan_C (void);

extern void(*R_SetupDrawSlab)(FSWColormap *base_colormap, float light, int shade);
extern void(*R_DrawSlab)(int dx, fixed_t v, int dy, fixed_t vi, const BYTE *vptr, BYTE *p);

#ifdef X86_ASM
extern "C" void R_SetupDrawSlabA(const BYTE *colormap);
extern "C" void R_DrawSlabA(int dx, fixed_t v, int dy, fixed_t vi, const BYTE *vptr, BYTE *p);
#else
extern "C" void R_SetupDrawSlabC(const BYTE *colormap);
extern "C" void R_DrawSlabC(int dx, fixed_t v, int dy, fixed_t vi, const BYTE *vptr, BYTE *p);
#endif

extern "C" int				ds_y;
extern "C" int				ds_x1;
extern "C" int				ds_x2;

extern "C" FSWColormap*		ds_fcolormap;
extern "C" lighttable_t*	ds_colormap;
extern "C" ShadeConstants	ds_shade_constants;
extern "C" dsfixed_t		ds_light;

extern "C" dsfixed_t		ds_xfrac;
extern "C" dsfixed_t		ds_yfrac;
extern "C" dsfixed_t		ds_xstep;
extern "C" dsfixed_t		ds_ystep;
extern "C" int				ds_xbits;
extern "C" int				ds_ybits;
extern "C" fixed_t			ds_alpha;
extern "C" double			ds_lod;

// start of a 64*64 tile image
extern "C" const BYTE*		ds_source;
extern "C" bool				ds_source_mipmapped;

extern "C" int				ds_color;		// [RH] For flat color (no texturing)

extern BYTE shadetables[/*NUMCOLORMAPS*16*256*/];
extern FDynamicColormap ShadeFakeColormap[16];
extern BYTE identitymap[256];
extern FDynamicColormap identitycolormap;
extern BYTE *dc_translation;

// [RH] Added for muliresolution support
void R_InitShadeMaps();
void R_InitFuzzTable (int fuzzoff);

// [RH] Consolidate column drawer selection
enum ESPSResult
{
	DontDraw,	// not useful to draw this
	DoDraw0,	// draw this as if r_columnmethod is 0
	DoDraw1,	// draw this as if r_columnmethod is 1
};
ESPSResult R_SetPatchStyle (FRenderStyle style, fixed_t alpha, int translation, DWORD color);
inline ESPSResult R_SetPatchStyle(FRenderStyle style, float alpha, int translation, DWORD color)
{
	return R_SetPatchStyle(style, FLOAT2FIXED(alpha), translation, color);
}

// Call this after finished drawing the current thing, in case its
// style was STYLE_Shade
void R_FinishSetPatchStyle ();

extern fixed_t(*tmvline1_add)();
extern void(*tmvline4_add)();
extern fixed_t(*tmvline1_addclamp)();
extern void(*tmvline4_addclamp)();
extern fixed_t(*tmvline1_subclamp)();
extern void(*tmvline4_subclamp)();
extern fixed_t(*tmvline1_revsubclamp)();
extern void(*tmvline4_revsubclamp)();

// transmaskwallscan calls this to find out what column drawers to use
bool R_GetTransMaskDrawers (fixed_t (**tmvline1)(), void (**tmvline4)());

// Retrieve column data for wallscan. Should probably be removed
// to just use the texture's GetColumn() method. It just exists
// for double-layer skies.
const BYTE *R_GetColumn (FTexture *tex, int col);
void wallscan (int x1, int x2, short *uwal, short *dwal, float *swal, fixed_t *lwal, double yrepeat, const BYTE *(*getcol)(FTexture *tex, int col)=R_GetColumn);

// maskwallscan is exactly like wallscan but does not draw anything where the texture is color 0.
void maskwallscan (int x1, int x2, short *uwal, short *dwal, float *swal, fixed_t *lwal, double yrepeat, const BYTE *(*getcol)(FTexture *tex, int col)=R_GetColumn);

// transmaskwallscan is like maskwallscan, but it can also blend to the background
void transmaskwallscan (int x1, int x2, short *uwal, short *dwal, float *swal, fixed_t *lwal, double yrepeat, const BYTE *(*getcol)(FTexture *tex, int col)=R_GetColumn);

// Sets dc_colormap and dc_light to their appropriate values depending on the output format (pal vs true color)
void R_SetColorMapLight(FSWColormap *base_colormap, float light, int shade);

// Same as R_SetColorMapLight, but for ds_colormap and ds_light
void R_SetDSColorMapLight(FSWColormap *base_colormap, float light, int shade);

void R_SetTranslationMap(lighttable_t *translation);

void R_DrawSingleSkyCol1(uint32_t solid_top, uint32_t solid_bottom);
void R_DrawSingleSkyCol4(uint32_t solid_top, uint32_t solid_bottom);
void R_DrawDoubleSkyCol1(uint32_t solid_top, uint32_t solid_bottom);
void R_DrawDoubleSkyCol4(uint32_t solid_top, uint32_t solid_bottom);

void R_DrawSingleSkyCol1_rgba(uint32_t solid_top, uint32_t solid_bottom);
void R_DrawSingleSkyCol4_rgba(uint32_t solid_top, uint32_t solid_bottom);
void R_DrawDoubleSkyCol1_rgba(uint32_t solid_top, uint32_t solid_bottom);
void R_DrawDoubleSkyCol4_rgba(uint32_t solid_top, uint32_t solid_bottom);

extern bool r_swtruecolor;

}

#endif
