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



extern "C" byte*		ylookup[MAXHEIGHT];

extern "C" int			dc_pitch;		// [RH] Distance between rows

extern "C" lighttable_t*dc_colormap;
extern "C" int			dc_x;
extern "C" int			dc_yl;
extern "C" int			dc_yh;
extern "C" fixed_t		dc_iscale;
extern "C" fixed_t		dc_texturemid;
extern "C" fixed_t		dc_texturefrac;
extern "C" int			dc_color;		// [RH] For flat colors (no texturing)
extern "C" DWORD		*dc_srcblend;
extern "C" DWORD		*dc_destblend;

// first pixel in a column
extern "C" byte*		dc_source;

extern "C" byte*		dc_dest;
extern "C" int			dc_count;

extern "C" DWORD		vplce[4];
extern "C" DWORD		vince[4];
extern "C" BYTE*		palookupoffse[4];
extern "C" BYTE*		bufplce[4];

// [RH] Temporary buffer for column drawing
extern "C" byte			dc_temp[MAXHEIGHT*4];
extern "C" unsigned int	dc_tspans[4][256];
extern "C" unsigned int	*dc_ctspan[4];
extern "C" unsigned int	horizspans[4];


// [RH] Pointers to the different column and span drawers...

// The span blitting interface.
// Hook in assembler or system specific BLT here.
extern void (*R_DrawColumn)(void);

extern DWORD (STACK_ARGS *dovline1) ();
extern DWORD (STACK_ARGS *doprevline1) ();
extern void (STACK_ARGS *dovline4) ();
extern void setupvline (int);

// The Spectre/Invisibility effect.
extern void (*R_DrawFuzzColumn)(void);

// [RH] Draw shaded column
extern void (*R_DrawShadedColumn)(void);

// Draw with color translation tables, for player sprite rendering,
//	Green/Red/Blue/Indigo shirts.
extern void (*R_DrawTranslatedColumn)(void);

// Span blitting for rows, floor/ceiling. No Sepctre effect needed.
extern void (*R_DrawSpan)(void);

// [RH] Span blit into an interleaved intermediate buffer
extern void (*R_DrawColumnHoriz)(void);
void R_DrawMaskedColumnHoriz (column_t *column);
void R_DrawMaskedColumnHoriz2 (column2_t *column);

// [RH] Initialize the above pointers
void R_InitColumnDrawers ();

// [RH] Moves data from the temporary buffer to the screen.
void rt_copy1col_c (int hx, int sx, int yl, int yh);
void rt_copy4cols_c (int sx, int yl, int yh);
void rt_map1col_c (int hx, int sx, int yl, int yh);
void rt_map4cols_c (int sx, int yl, int yh);
void rt_add1col (int hx, int sx, int yl, int yh);
void rt_add4cols (int sx, int yl, int yh);
void rt_tlate1col (int hx, int sx, int yl, int yh);
void rt_tlate4cols (int sx, int yl, int yh);
void rt_tlateadd1col (int hx, int sx, int yl, int yh);
void rt_tlateadd4cols (int sx, int yl, int yh);
void rt_shaded1col (int hx, int sx, int yl, int yh);
void rt_shaded4cols (int sx, int yl, int yh);
void rt_addclamp1col (int hx, int sx, int yl, int yh);
void rt_addclamp4cols (int sx, int yl, int yh);
void rt_tlateaddclamp1col (int hx, int sx, int yl, int yh);
void rt_tlateaddclamp4cols (int sx, int yl, int yh);

extern "C" void rt_copy1col_asm (int hx, int sx, int yl, int yh);
extern "C" void rt_copy4cols_asm (int sx, int yl, int yh);
extern "C" void rt_map1col_asm (int hx, int sx, int yl, int yh);
extern "C" void rt_map4cols_asm1 (int sx, int yl, int yh);
extern "C" void rt_map4cols_asm2 (int sx, int yl, int yh);

extern void (*rt_map4cols)(int sx, int yl, int yh);

#ifdef USEASM
#define rt_copy1col		rt_copy1col_asm
#define rt_copy4cols	rt_copy4cols_asm
#define rt_map1col		rt_map1col_asm
#else
#define rt_copy1col		rt_copy1col_c
#define rt_copy4cols	rt_copy4cols_c
#define rt_map1col		rt_map1col_c
#endif

void rt_draw4cols (int sx);

// [RH] Preps the temporary horizontal buffer.
void rt_initcols (void);

void R_DrawFogBoundary (int x1, int x2, short *uclip, short *dclip);


#ifndef USEASM
void	R_DrawColumnHorizP_C (void);
void	R_DrawColumnP_C (void);
void	R_DrawFuzzColumnP_C (void);
void	R_DrawTranslatedColumnP_C (void);
void	R_DrawShadedColumnP_C (void);
void	R_DrawSpanP_C (void);

#else	/* USEASM */

extern "C" void	R_DrawColumnP_Unrolled (void);

extern "C" void	R_DrawColumnHorizP_ASM (void);
extern "C" void	R_DrawColumnP_ASM (void);
extern "C" void	R_DrawFuzzColumnP_ASM (void);
void	R_DrawTranslatedColumnP_C (void);
void	R_DrawShadedColumnP_C (void);
extern "C" void	R_DrawSpanP_ASM (void);

#endif

void	R_DrawTlatedLucentColumnP_C (void);
#define R_DrawTlatedLucentColumn R_DrawTlatedLucentColumnP_C
void	R_StretchColumnP_C (void);
#define R_StretchColumn R_StretchColumnP_C

void	R_FillColumnP (void);
void	R_FillColumnHorizP (void);
void	R_FillSpan (void);

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

// start of a 64*64 tile image
extern "C" byte*			ds_source;

extern "C" int				ds_color;		// [RH] For flat color (no texturing)

enum
{
	TRANSLATION_Shaded,
	TRANSLATION_Players,
	TRANSLATION_PlayersExtra,
	TRANSLATION_Standard,
	TRANSLATION_LevelScripted,
	TRANSLATION_Decals,
	TRANSLATION_PlayerCorpses,

	NUM_TRANSLATION_TABLES
};

extern byte*			translationtables[NUM_TRANSLATION_TABLES];
extern byte*			dc_translation;

inline WORD TRANSLATION(BYTE a, BYTE b)
{
	return (a<<8) | b;
}

const int MAX_ACS_TRANSLATIONS = 32;

inline void R_CopyTranslation (WORD to, WORD from)
{
	memcpy (&translationtables[to>>8][(to&255)*256],
		&translationtables[from>>8][(from&255)*256], 256);
}


// [RH] Double view pixels by detail mode
void R_DetailDouble (void);


// Initialize color translation tables,
//	for player rendering etc.
void R_InitTranslationTables (void);

// [RH] Actually create a player's translation table.
void R_BuildPlayerTranslation (int player);


// If the view size is not full screen, draws a border around it.
void R_DrawViewBorder (void);
void R_DrawTopBorder (void);
void R_DrawBorder (int x1, int y1, int x2, int y2);

// [RH] Added for muliresolution support
void R_InitFuzzTable (int fuzzoff);

// [RH] Consolidate column drawer selection
enum ESPSResult
{
	DontDraw,	// not useful to draw this
	DoDraw0,	// draw this as if r_columnmethod is 0
	DoDraw1,	// draw this as if r_columnmethod is 1
};
ESPSResult R_SetPatchStyle (int style, fixed_t alpha, int translation, DWORD color);

// Call this after finished drawing the current thing, in case its
// style was STYLE_Shade
void R_FinishSetPatchStyle ();

extern byte *R_GetColumn (int tex, int col);
void wallscan (int x1, int x2, short *uwal, short *dwal, fixed_t *swal, fixed_t *lwal, byte *(*getcol)(int tex, int col)=R_GetColumn);

#endif
