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



extern int				dc_pitch;		// [RH] Distance between rows

extern lighttable_t*	dc_colormap;
extern int				dc_x;
extern int				dc_yl;
extern int				dc_yh;
extern fixed_t			dc_iscale;
extern fixed_t			dc_texturemid;

// first pixel in a column
extern byte*			dc_source;				

// [RH] Tutti-Frutti fix
unsigned int			dc_mask;


// [RH] Pointers to the different column and span drawers...

// The span blitting interface.
// Hook in assembler or system specific BLT
//	here.
void (*R_DrawColumn)(void);

// The Spectre/Invisibility effect.
void (*R_DrawFuzzColumn)(void);

// [RH] Draw translucent column;
void (*R_DrawTranslucentColumn)(void);

// Draw with color translation tables,
//	for player sprite rendering,
//	Green/Red/Blue/Indigo shirts.
void (*R_DrawTranslatedColumn)(void);

// Span blitting for rows, floor/ceiling.
// No Sepctre effect needed.
void (*R_DrawSpan)(void);


// [RH] Initialize the above five pointers
void R_InitColumnDrawers (BOOL is8bit);


#ifndef USEASM
void	R_DrawColumnP_C (void);
void	R_DrawFuzzColumnP_C (void);
void	R_DrawTranslucentColumnP_C (void);
void	R_DrawTranslatedColumnP_C (void);
void	R_DrawSpanP (void);

void	R_DrawColumnD_C (void);
void	R_DrawFuzzColumnD_C (void);
void	R_DrawTranslucentColumnD_C (void);
void	R_DrawTranslatedColumnD_C (void);
void	R_DrawSpanD (void);

#else	/* USEASM */

void	R_DrawColumnP_ASM (void);
void	R_DrawFuzzColumnP_ASM (void);
void	R_DrawTranslucentColumnP_ASM (void);
void	R_DrawTranslatedColumnP_C (void);
void	R_DrawSpanP (void);

void	R_DrawColumnD_C (void);
void	R_DrawFuzzColumnD_C (void);
void	R_DrawTranslucentColumnD_C (void);
void	R_DrawTranslatedColumnD_C (void);
void	R_DrawSpanD (void);

#endif

void R_VideoErase (int x1, int y1, int x2, int y2);

extern int				ds_colsize;		// [RH] Distance between columns
extern int				ds_colshift;

extern int				ds_y;
extern int				ds_x1;
extern int				ds_x2;

extern lighttable_t*	ds_colormap;

extern fixed_t			ds_xfrac;
extern fixed_t			ds_yfrac;
extern fixed_t			ds_xstep;
extern fixed_t			ds_ystep;

// start of a 64*64 tile image
extern byte*			ds_source;

extern byte*			translationtables;
extern byte*			dc_translation;

extern byte*			dc_transmap;


/* [Petteri] R_DrawSpan8() optimized inner loop (does two pixels
   per cycle) */
void STACK_ARGS DrawSpan8Loop (fixed_t xfrac, fixed_t yfrac, int count, byte *dest);


// [RH] Double view pixels by detail mode
void R_DetailDouble (void);


void R_InitBuffer (int width, int height);


// Initialize color translation tables,
//	for player rendering etc.
void R_InitTranslationTables (void);

// [RH] Actually create a player's translation table.
void R_BuildPlayerTranslation (int player, int color);

// [RH] Build the same translation tables as org. Doom.
void R_BuildCompatiblePlayerTranslations (void);


// Rendering function.
void R_FillBackScreen (void);

// If the view size is not full screen, draws a border around it.
void R_DrawViewBorder (void);


// [RH] Added for muliresolution support
void R_InitFuzzTable (void);


#endif
//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
