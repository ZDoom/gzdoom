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



extern "C" byte**		ylookup;
extern "C" int*			columnofs;

extern "C" int			dc_pitch;		// [RH] Distance between rows

extern "C" lighttable_t*	dc_colormap;
extern "C" unsigned int*	dc_shademap;	// [RH] For high/true color modes
extern "C" int			dc_x;
extern "C" int			dc_yl;
extern "C" int			dc_yh;
extern "C" fixed_t		dc_iscale;
extern "C" fixed_t		dc_texturemid;
extern "C" fixed_t		dc_texturefrac;
extern "C" int			dc_color;		// [RH] For flat colors (no texturing)

// first pixel in a column
extern "C" byte*			dc_source;

// [RH] Temporary buffer for column drawing
extern "C" byte			dc_temp[1200*4];
extern "C" unsigned int	dc_tspans[4][256];
extern "C" unsigned int	*dc_ctspan[4];
extern "C" unsigned int	horizspans[4];

// [RH] Tutti-Frutti fix
extern "C" unsigned int		dc_mask;


// [RH] Pointers to the different column and span drawers...

// The span blitting interface.
// Hook in assembler or system specific BLT here.
extern void (*R_DrawColumn)(void);

// The Spectre/Invisibility effect.
extern void (*R_DrawFuzzColumn)(void);

// [RH] Draw translucent column;
extern void (*R_DrawTranslucentColumn)(void);

// Draw with color translation tables,
//	for player sprite rendering,
//	Green/Red/Blue/Indigo shirts.
extern void (*R_DrawTranslatedColumn)(void);

// Span blitting for rows, floor/ceiling.
// No Sepctre effect needed.
extern void (*R_DrawSpan)(void);

// [RH] Span blit into an interleaved intermediate buffer
extern void (*R_DrawColumnHoriz)(void);
void R_DrawMaskedColumnHoriz (column_t *column);

// [RH] Initialize the above five pointers
void R_InitColumnDrawers (BOOL is8bit);

// [RH] Moves data from the temporary horizontal buffer to the screen.
void rt_copy1col_c (int hx, int sx, int yl, int yh);
void rt_copy2cols_c (int hx, int sx, int yl, int yh);
void rt_copy4cols_c (int sx, int yl, int yh);
void rt_map1col_c (int hx, int sx, int yl, int yh);
void rt_map2cols_c (int hx, int sx, int yl, int yh);
void rt_map4cols_c (int sx, int yl, int yh);
void rt_lucent1col (int hx, int sx, int yl, int yh);
void rt_lucent2cols (int hx, int sx, int yl, int yh);
void rt_lucent4cols (int sx, int yl, int yh);
void rt_tlate1col (int hx, int sx, int yl, int yh);
void rt_tlate2cols (int hx, int sx, int yl, int yh);
void rt_tlate4cols (int sx, int yl, int yh);
void rt_tlatelucent1col (int hx, int sx, int yl, int yh);
void rt_tlatelucent2cols (int hx, int sx, int yl, int yh);
void rt_tlatelucent4cols (int sx, int yl, int yh);
extern "C" void rt_copy1col_asm (int hx, int sx, int yl, int yh);
extern "C" void rt_copy2cols_asm (int hx, int sx, int yl, int yh);
extern "C" void rt_copy4cols_asm (int sx, int yl, int yh);
extern "C" void rt_map1col_asm (int hx, int sx, int yl, int yh);
extern "C" void rt_map2cols_asm (int hx, int sx, int yl, int yh);
extern "C" void rt_map4cols_asm1 (int sx, int yl, int yh);
extern "C" void rt_map4cols_asm2 (int sx, int yl, int yh);

extern void (*rt_map4cols)(int sx, int yl, int yh);

#ifdef USEASM
#define rt_copy1col		rt_copy1col_asm
#define rt_copy2cols	rt_copy2cols_asm
#define rt_copy4cols	rt_copy4cols_asm
#define rt_map1col		rt_map1col_asm
#define rt_map2cols		rt_map2cols_asm
#else
#define rt_copy1col		rt_copy1col_c
#define rt_copy2cols	rt_copy2cols_c
#define rt_copy4cols	rt_copy4cols_c
#define rt_map1col		rt_map1col_c
#define rt_map2cols		rt_map2cols_c
#endif

void rt_draw1col (int hx, int sx);
void rt_draw2cols (int hx, int sx);
void rt_draw4cols (int sx);

// [RH] Preps the temporary horizontal buffer.
void rt_initcols (void);


#ifndef USEASM
void	R_DrawColumnHorizP_C (void);
void	R_DrawColumnP_C (void);
void	R_DrawFuzzColumnP_C (void);
void	R_DrawTranslucentColumnP_C (void);
void	R_DrawTranslatedColumnP_C (void);
void	R_DrawSpanP_C (void);

void	R_DrawColumnD_C (void);
void	R_DrawFuzzColumnD_C (void);
void	R_DrawTranslucentColumnD_C (void);
void	R_DrawTranslatedColumnD_C (void);
void	R_DrawSpanD (void);

#else	/* USEASM */

extern "C" void	R_DrawColumnP_Unrolled (void);

extern "C" void	R_DrawColumnHorizP_ASM (void);
extern "C" void	R_DrawColumnP_ASM (void);
extern "C" void	R_DrawFuzzColumnP_ASM (void);
void	R_DrawTranslucentColumnP_C (void);
void	R_DrawTranslatedColumnP_C (void);
extern "C" void	R_DrawSpanP_ASM (void);

void	R_DrawColumnD_C (void);
void	R_DrawFuzzColumnD_C (void);
void	R_DrawTranslucentColumnD_C (void);
void	R_DrawTranslatedColumnD_C (void);
void	R_DrawSpanD (void);
#endif

void	R_DrawTlatedLucentColumnP_C (void);
#define R_DrawTlatedLucentColumn R_DrawTlatedLucentColumnP_C
void	R_StretchColumnP_C (void);
#define R_StretchColumn R_StretchColumnP_C

void	R_FillColumnP (void);
void	R_FillColumnHorizP (void);
void	R_FillSpan (void);

extern "C" int				ds_colsize;		// [RH] Distance between columns

extern "C" int				ds_y;
extern "C" int				ds_x1;
extern "C" int				ds_x2;

extern "C" lighttable_t*	ds_colormap;

extern "C" dsfixed_t		ds_xfrac;
extern "C" dsfixed_t		ds_yfrac;
extern "C" dsfixed_t		ds_xstep;
extern "C" dsfixed_t		ds_ystep;

// start of a 64*64 tile image
extern "C" byte*			ds_source;

extern "C" int				ds_color;		// [RH] For flat color (no texturing)

extern byte*			translationtables;
extern byte*			dc_translation;

extern fixed_t dc_translevel;


// [RH] Double view pixels by detail mode
void R_DetailDouble (void);


void R_InitBuffer (int width, int height);


// Initialize color translation tables,
//	for player rendering etc.
void R_InitTranslationTables (void);

// [RH] Actually create a player's translation table.
void R_BuildPlayerTranslation (int player, int color);


// If the view size is not full screen, draws a border around it.
void R_DrawViewBorder (void);
void R_DrawTopBorder (void);
void R_DrawBorder (int x1, int y1, int x2, int y2);

// [RH] Added for muliresolution support
void R_InitFuzzTable (void);


#endif
//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
