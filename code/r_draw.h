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


#ifdef __GNUG__
#pragma interface
#endif


extern lighttable_t*	dc_colormap;
extern int				dc_x;
extern int				dc_yl;
extern int				dc_yh;
extern fixed_t			dc_iscale;
extern fixed_t			dc_texturemid;

// first pixel in a column
extern byte*			dc_source;				


#ifndef USEASM
// The span blitting interface.
// Hook in assembler or system specific BLT
//	here.
void	R_DrawColumn_C (void);

// The Spectre/Invisibility effect.
void	R_DrawFuzzColumn_C (void);

// Draw translucent column;
void	R_DrawTranslucentColumn_C (void);

// Draw with color translation tables,
//	for player sprite rendering,
//	Green/Red/Blue/Indigo shirts.
void	R_DrawTranslatedColumn_C (void);

#define R_DrawColumn			R_DrawColumn_C
#define R_DrawFuzzColumn		R_DrawFuzzColumn_C
#define R_DrawTranslucentColumn	R_DrawTranslucentColumn_C
#define R_DrawTranslatedColumn	R_DrawTranslatedColumn_C

#else	/* USEASM */

void	R_DrawColumn_ASM (void);
void	R_DrawFuzzColumn_ASM (void);
void	R_DrawTranslucentColumn_ASM (void);
void	R_DrawTranslatedColumn_C (void);

#define R_DrawColumn			R_DrawColumn_ASM
#define R_DrawFuzzColumn		R_DrawFuzzColumn_ASM
#define R_DrawTranslucentColumn	R_DrawTranslucentColumn_ASM
#define R_DrawTranslatedColumn	R_DrawTranslatedColumn_C

#endif

void
R_VideoErase
( unsigned		ofs,
  int			count );

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


// Span blitting for rows, floor/ceiling.
// No Sepctre effect needed.
void	R_DrawSpan (void);

/* [Petteri] R_DrawSpan8() optimized inner loop (does two pixels
   per cycle) */
void __cdecl DrawSpan8Loop (fixed_t xfrac, fixed_t yfrac, int count,
							byte *dest);

// Low resolution mode, 160x200?
void	R_DrawSpanLow (void);


void
R_InitBuffer
( int			width,
  int			height );


// Initialize color translation tables,
//	for player rendering etc.
void	R_InitTranslationTables (void);



// Rendering function.
void R_FillBackScreen (void);

// If the view size is not full screen, draws a border around it.
void R_DrawViewBorder (void);


// Added for muliresolution support
void R_InitFuzzTable (void);

#endif
//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
