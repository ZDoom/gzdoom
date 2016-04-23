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

extern "C" int			ylookup[MAXHEIGHT];

extern "C" int			dc_pitch;		// [RH] Distance between rows

extern "C" lighttable_t*dc_colormap;
extern "C" int			dc_x;
extern "C" int			dc_yl;
extern "C" int			dc_yh;
extern "C" fixed_t		dc_iscale;
extern     double		dc_texturemid;
extern "C" fixed_t		dc_texturefrac;
extern "C" int			dc_color;		// [RH] For flat colors (no texturing)
extern "C" DWORD		dc_srccolor;
extern "C" DWORD		*dc_srcblend;
extern "C" DWORD		*dc_destblend;

// first pixel in a column
extern "C" const BYTE*	dc_source;

extern "C" BYTE			*dc_dest, *dc_destorg;
extern "C" int			dc_count;

extern "C" DWORD		vplce[4];
extern "C" DWORD		vince[4];
extern "C" BYTE*		palookupoffse[4];
extern "C" const BYTE*	bufplce[4];

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
#ifdef X64_ASM
#define dovline4 vlinetallasm4
extern "C" void vlinetallasm4();
#else
extern void (*dovline4) ();
#endif
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
void R_SetSpanColormap(BYTE *colormap);
void R_SetSpanSource(const BYTE *pixels);

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
void R_DrawMaskedColumnHoriz (const BYTE *column, const FTexture::Span *spans);

// [RH] Initialize the above pointers
void R_InitColumnDrawers ();

// [RH] Moves data from the temporary buffer to the screen.
extern "C"
{
void rt_copy1col_c (int hx, int sx, int yl, int yh);
void rt_copy4cols_c (int sx, int yl, int yh);

void rt_shaded1col (int hx, int sx, int yl, int yh);
void rt_shaded4cols_c (int sx, int yl, int yh);
void rt_shaded4cols_asm (int sx, int yl, int yh);

void rt_map1col_c (int hx, int sx, int yl, int yh);
void rt_add1col (int hx, int sx, int yl, int yh);
void rt_addclamp1col (int hx, int sx, int yl, int yh);
void rt_subclamp1col (int hx, int sx, int yl, int yh);
void rt_revsubclamp1col (int hx, int sx, int yl, int yh);

void rt_tlate1col (int hx, int sx, int yl, int yh);
void rt_tlateadd1col (int hx, int sx, int yl, int yh);
void rt_tlateaddclamp1col (int hx, int sx, int yl, int yh);
void rt_tlatesubclamp1col (int hx, int sx, int yl, int yh);
void rt_tlaterevsubclamp1col (int hx, int sx, int yl, int yh);

void rt_map4cols_c (int sx, int yl, int yh);
void rt_add4cols_c (int sx, int yl, int yh);
void rt_addclamp4cols_c (int sx, int yl, int yh);
void rt_subclamp4cols (int sx, int yl, int yh);
void rt_revsubclamp4cols (int sx, int yl, int yh);

void rt_tlate4cols (int sx, int yl, int yh);
void rt_tlateadd4cols (int sx, int yl, int yh);
void rt_tlateaddclamp4cols (int sx, int yl, int yh);
void rt_tlatesubclamp4cols (int sx, int yl, int yh);
void rt_tlaterevsubclamp4cols (int sx, int yl, int yh);

void rt_copy1col_asm (int hx, int sx, int yl, int yh);
void rt_map1col_asm (int hx, int sx, int yl, int yh);

void rt_copy4cols_asm (int sx, int yl, int yh);
void rt_map4cols_asm1 (int sx, int yl, int yh);
void rt_map4cols_asm2 (int sx, int yl, int yh);
void rt_add4cols_asm (int sx, int yl, int yh);
void rt_addclamp4cols_asm (int sx, int yl, int yh);
}

extern void (*rt_map4cols)(int sx, int yl, int yh);

#ifdef X86_ASM
#define rt_copy1col			rt_copy1col_asm
#define rt_copy4cols		rt_copy4cols_asm
#define rt_map1col			rt_map1col_asm
#define rt_shaded4cols		rt_shaded4cols_asm
#define rt_add4cols			rt_add4cols_asm
#define rt_addclamp4cols	rt_addclamp4cols_asm
#else
#define rt_copy1col			rt_copy1col_c
#define rt_copy4cols		rt_copy4cols_c
#define rt_map1col			rt_map1col_c
#define rt_shaded4cols		rt_shaded4cols_c
#define rt_add4cols			rt_add4cols_c
#define rt_addclamp4cols	rt_addclamp4cols_c
#endif

void rt_draw4cols (int sx);

// [RH] Preps the temporary horizontal buffer.
void rt_initcols (BYTE *buffer=NULL);

void R_DrawFogBoundary (int x1, int x2, short *uclip, short *dclip);


#ifdef X86_ASM

extern "C" void	R_DrawColumnP_Unrolled (void);
extern "C" void	R_DrawColumnHorizP_ASM (void);
extern "C" void	R_DrawColumnP_ASM (void);
extern "C" void	R_DrawFuzzColumnP_ASM (void);
		   void R_DrawTranslatedColumnP_C (void);
		   void R_DrawShadedColumnP_C (void);
extern "C" void	R_DrawSpanP_ASM (void);
extern "C" void R_DrawSpanMaskedP_ASM (void);

#else

void	R_DrawColumnHorizP_C (void);
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

void	R_FillColumnP (void);
void	R_FillColumnHorizP (void);
void	R_FillSpan (void);

#ifdef X86_ASM
#define R_SetupDrawSlab R_SetupDrawSlabA
#define R_DrawSlab R_DrawSlabA
#else
#define R_SetupDrawSlab R_SetupDrawSlabC
#define R_DrawSlab R_DrawSlabC
#endif

extern "C" void			   R_SetupDrawSlab(const BYTE *colormap);
extern "C" void R_DrawSlab(int dx, fixed_t v, int dy, fixed_t vi, const BYTE *vptr, BYTE *p);

extern "C" int				ds_y;
extern "C" int				ds_x1;
extern "C" int				ds_x2;

extern "C" lighttable_t*	ds_colormap;

extern "C" dsfixed_t		ds_xfrac;
extern "C" dsfixed_t		ds_yfrac;
extern "C" dsfixed_t		ds_xstep;
extern "C" dsfixed_t		ds_ystep;
extern "C" int				ds_xbits;
extern "C" int				ds_ybits;
extern "C" fixed_t			ds_alpha;

// start of a 64*64 tile image
extern "C" const BYTE*		ds_source;

extern "C" int				ds_color;		// [RH] For flat color (no texturing)

extern BYTE shadetables[/*NUMCOLORMAPS*16*256*/];
extern FDynamicColormap ShadeFakeColormap[16];
extern BYTE identitymap[256];
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

#endif
