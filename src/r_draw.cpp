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
// $Log:$
//
// DESCRIPTION:
//		The actual span/column drawing functions.
//		Here find the main potential for optimization,
//		 e.g. inline assembly, different algorithms.
//
//-----------------------------------------------------------------------------

#include <stddef.h>

#include "templates.h"
#include "m_alloc.h"
#include "doomdef.h"
#include "i_system.h"
#include "w_wad.h"
#include "r_local.h"
#include "v_video.h"
#include "doomstat.h"
#include "st_stuff.h"
#include "a_hexenglobal.h"
#include "g_game.h"
#include "g_level.h"

#include "gi.h"
#include "stats.h"

#undef RANGECHECK

// status bar height at bottom of screen
// [RH] status bar position at bottom of screen
extern	int		ST_Y;

//
// All drawing to the view buffer is accomplished in this file.
// The other refresh files only know about ccordinates,
//	not the architecture of the frame buffer.
// Conveniently, the frame buffer is a linear one,
//	and we need only the base address,
//	and the total size == width*height*depth/8.,
//

extern BYTE decorate_translations[];

BYTE*			viewimage;
extern "C" {
int 			viewwidth;
int				halfviewwidth;
int 			viewheight;
int				ylookup[MAXHEIGHT];
BYTE			*dc_destorg;
}
int 			scaledviewwidth;
int 			viewwindowx;
int 			viewwindowy;

extern "C" {
int				realviewwidth;		// [RH] Physical width of view window
int				realviewheight;		// [RH] Physical height of view window
int				detailxshift;		// [RH] X shift for horizontal detail level
int				detailyshift;		// [RH] Y shift for vertical detail level
}

#ifdef USEASM
extern "C" void STACK_ARGS DoubleHoriz_MMX (int height, int width, BYTE *dest, int pitch);
extern "C" void STACK_ARGS DoubleHorizVert_MMX (int height, int width, BYTE *dest, int pitch);
extern "C" void STACK_ARGS DoubleVert_ASM (int height, int width, BYTE *dest, int pitch);
#endif

// [RH] Pointers to the different column drawers.
//		These get changed depending on the current
//		screen depth and asm/no asm.
void (*R_DrawColumnHoriz)(void);
void (*R_DrawColumn)(void);
void (*R_DrawFuzzColumn)(void);
void (*R_DrawTranslatedColumn)(void);
void (*R_DrawShadedColumn)(void);
void (*R_DrawSpan)(void);
void (*R_DrawSpanMasked)(void);
void (*R_DrawSpanTranslucent)(void);
void (*R_DrawSpanMaskedTranslucent)(void);
void (*rt_map4cols)(int,int,int);

//
// R_DrawColumn
// Source is the top of the column to scale.
//
extern "C" {
int				dc_pitch=0xABadCafe;	// [RH] Distance between rows

lighttable_t*	dc_colormap; 
int 			dc_x; 
int 			dc_yl; 
int 			dc_yh; 
fixed_t 		dc_iscale; 
fixed_t 		dc_texturemid;
fixed_t			dc_texturefrac;
int				dc_color;				// [RH] Color for column filler
DWORD			*dc_srcblend;			// [RH] Source and destination
DWORD			*dc_destblend;			// blending lookups

// first pixel in a column (possibly virtual) 
const BYTE*		dc_source;				

BYTE*			dc_dest;
int				dc_count;

DWORD			vplce[4];
DWORD			vince[4];
BYTE*			palookupoffse[4];
const BYTE*		bufplce[4];

// just for profiling 
int 			dccount;
}

cycle_t			DetailDoubleCycles;

int dc_fillcolor;
BYTE *dc_translation;
BYTE *translationtables[NUM_TRANSLATION_TABLES];

/************************************/
/*									*/
/* Palettized drawers (C versions)	*/
/*									*/
/************************************/

#ifndef	USEASM
//
// A column is a vertical slice/span from a wall texture that,
//	given the DOOM style restrictions on the view orientation,
//	will always have constant z depth.
// Thus a special case loop for very fast rendering can
//	be used. It has also been used with Wolfenstein 3D.
// 
void R_DrawColumnP_C (void)
{
	int 				count;
	BYTE*				dest;
	fixed_t 			frac;
	fixed_t 			fracstep;

	count = dc_count;

	// Zero length, column does not exceed a pixel.
	if (count <= 0)
		return;

	// Framebuffer destination address.
	dest = dc_dest;

	// Determine scaling,
	//	which is the only mapping to be done.
	fracstep = dc_iscale; 
	frac = dc_texturefrac;

	{
		// [RH] Get local copies of these variables so that the compiler
		//		has a better chance of optimizing this well.
		BYTE *colormap = dc_colormap;
		const BYTE *source = dc_source;
		int pitch = dc_pitch;

		// Inner loop that does the actual texture mapping,
		//	e.g. a DDA-lile scaling.
		// This is as fast as it gets.
		do
		{
			// Re-map color indices from wall texture column
			//	using a lighting/special effects LUT.
			*dest = colormap[source[frac>>FRACBITS]];

			dest += pitch;
			frac += fracstep;

		} while (--count);
	}
} 
#endif	// USEASM

// [RH] Just fills a column with a color
void R_FillColumnP (void)
{
	int 				count;
	BYTE*				dest;

	count = dc_count;

	if (count <= 0)
		return;

	dest = dc_dest;

	{
		int pitch = dc_pitch;
		BYTE color = dc_color;

		do
		{
			*dest = color;
			dest += pitch;
		} while (--count);
	}
}

void R_FillAddColumn (void)
{
	int count;
	BYTE *dest;

	count = dc_count;
	if (count <= 0)
		return;

	dest = dc_dest;
	DWORD *bg2rgb;
	DWORD fg;

	bg2rgb = dc_destblend;
	fg = dc_srcblend[dc_color];
	int pitch = dc_pitch;

	do
	{
		DWORD bg;
		bg = (fg + bg2rgb[*dest]) | 0x1f07c1f;
		*dest = RGB32k[0][0][bg & (bg>>15)];
		dest += pitch; 
	} while (--count);

}

//
// Spectre/Invisibility.
//
#define FUZZTABLE	50

extern "C"
{
int 	fuzzoffset[FUZZTABLE+1];	// [RH] +1 for the assembly routine
int 	fuzzpos = 0; 
int		fuzzviewheight;
}
/*
	FUZZOFF,-FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,
	FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,
	FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,
	FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,
	FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,
	FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,
	FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF 
*/

static const signed char fuzzinit[FUZZTABLE] = {
	1,-1, 1,-1, 1, 1,-1,
	1, 1,-1, 1, 1, 1,-1,
	1, 1, 1,-1,-1,-1,-1,
	1,-1,-1, 1, 1, 1, 1,-1,
	1,-1, 1, 1,-1,-1, 1,
	1,-1,-1,-1,-1, 1, 1,
	1, 1,-1, 1, 1,-1, 1 
};

void R_InitFuzzTable (int fuzzoff)
{
	int i;

	for (i = 0; i < FUZZTABLE; i++)
	{
		fuzzoffset[i] = fuzzinit[i] * fuzzoff;
	}
}

#ifndef USEASM
//
// Creates a fuzzy image by copying pixels from adjacent ones above and below.
// Used with an all black colormap, this could create the SHADOW effect,
// i.e. spectres and invisible players.
//
void R_DrawFuzzColumnP_C (void)
{
	int count;
	BYTE *dest;

	// Adjust borders. Low...
	if (dc_yl == 0)
		dc_yl = 1;

	// .. and high.
	if (dc_yh > fuzzviewheight)
		dc_yh = fuzzviewheight;

	count = dc_yh - dc_yl;

	// Zero length.
	if (count < 0)
		return;

	count++;

	dest = ylookup[dc_yl] + dc_x + dc_destorg;

	// colormap #6 is used for shading (of 0-31, a bit brighter than average)
	{
		// [RH] Make local copies of global vars to try and improve
		//		the optimizations made by the compiler.
		int pitch = dc_pitch;
		int fuzz = fuzzpos;
		int cnt;
		BYTE *map = &NormalLight.Maps[6*256];

		// [RH] Split this into three separate loops to minimize
		// the number of times fuzzpos needs to be clamped.
		if (fuzz)
		{
			cnt = MIN(FUZZTABLE-fuzz,count);
			count -= cnt;
			do
			{
				*dest = map[dest[fuzzoffset[fuzz++]]];
				dest += pitch;
			} while (--cnt);
		}
		if (fuzz == FUZZTABLE || count > 0)
		{
			while (count >= FUZZTABLE)
			{
				fuzz = 0;
				cnt = FUZZTABLE;
				count -= FUZZTABLE;
				do
				{
					*dest = map[dest[fuzzoffset[fuzz++]]];
					dest += pitch;
				} while (--cnt);
			}
			fuzz = 0;
			if (count > 0)
			{
				do
				{
					*dest = map[dest[fuzzoffset[fuzz++]]];
					dest += pitch;
				} while (--count);
			}
		}
		fuzzpos = fuzz;
	}
} 
#endif	// USEASM

//
// R_DrawTranlucentColumn
//

/*
[RH] This translucency algorithm is based on DOSDoom 0.65's, but uses
a 32k RGB table instead of an 8k one. At least on my machine, it's
slightly faster (probably because it uses only one shift instead of
two), and it looks considerably less green at the ends of the
translucency range. The extra size doesn't appear to be an issue.

The following note is from DOSDoom 0.65:

New translucency algorithm, by Erik Sandberg:

Basically, we compute the red, green and blue values for each pixel, and
then use a RGB table to check which one of the palette colours that best
represents those RGB values. The RGB table is 8k big, with 4 R-bits,
5 G-bits and 4 B-bits. A 4k table gives a bit too bad precision, and a 32k
table takes up more memory and results in more cache misses, so an 8k
table seemed to be quite ultimate.

The computation of the RGB for each pixel is accelerated by using two
1k tables for each translucency level.
The xth element of one of these tables contains the r, g and b values for
the colour x, weighted for the current translucency level (for example,
the weighted rgb values for background colour at 75% translucency are 1/4
of the original rgb values). The rgb values are stored as three
low-precision fixed point values, packed into one long per colour:
Bit 0-4:   Frac part of blue  (5 bits)
Bit 5-8:   Int  part of blue  (4 bits)
Bit 9-13:  Frac part of red   (5 bits)
Bit 14-17: Int  part of red   (4 bits)
Bit 18-22: Frac part of green (5 bits)
Bit 23-27: Int  part of green (5 bits)
Bit 28-31: All zeros          (4 bits)

The point of this format is that the two colours now can be added, and
then be converted to a RGB table index very easily: First, we just set
all the frac bits and the four upper zero bits to 1. It's now possible
to get the RGB table index by anding the current value >> 5 with the
current value >> 19. When asm-optimised, this should be the fastest
algorithm that uses RGB tables.

*/

void R_DrawAddColumnP_C (void)
{
	int count;
	BYTE *dest;
	fixed_t frac;
	fixed_t fracstep;

	count = dc_count;
	if (count <= 0)
		return;

	dest = dc_dest;

	fracstep = dc_iscale;
	frac = dc_texturefrac;

	{
		DWORD *fg2rgb = dc_srcblend;
		DWORD *bg2rgb = dc_destblend;
		BYTE *colormap = dc_colormap;
		const BYTE *source = dc_source;
		int pitch = dc_pitch;

		do
		{
			DWORD fg = colormap[source[frac>>FRACBITS]];
			DWORD bg = *dest;

			fg = fg2rgb[fg];
			bg = bg2rgb[bg];
			fg = (fg+bg) | 0x1f07c1f;
			*dest = RGB32k[0][0][fg & (fg>>15)];
			dest += pitch;
			frac += fracstep;
		} while (--count);
	}
}

//
// R_DrawTranslatedColumn
// Used to draw player sprites with the green colorramp mapped to others.
// Could be used with different translation tables, e.g. the lighter colored
// version of the BaronOfHell, the HellKnight, uses	identical sprites, kinda
// brightened up.
//

void R_DrawTranslatedColumnP_C (void)
{ 
	int 				count;
	BYTE*				dest;
	fixed_t 			frac;
	fixed_t 			fracstep;

	count = dc_count;
	if (count <= 0) 
		return;

	dest = dc_dest;

	fracstep = dc_iscale;
	frac = dc_texturefrac;

	{
		// [RH] Local copies of global vars to improve compiler optimizations
		BYTE *colormap = dc_colormap;
		BYTE *translation = dc_translation;
		const BYTE *source = dc_source;
		int pitch = dc_pitch;

		do
		{
			*dest = colormap[translation[source[frac>>FRACBITS]]];
			dest += pitch;

			frac += fracstep;
		} while (--count);
	}
}

// Draw a column that is both translated and translucent
void R_DrawTlatedAddColumnP_C (void)
{
	int count;
	BYTE *dest;
	fixed_t frac;
	fixed_t fracstep;

	count = dc_count;
	if (count <= 0)
		return;

	dest = dc_dest;

	fracstep = dc_iscale;
	frac = dc_texturefrac;

	{
		DWORD *fg2rgb = dc_srcblend;
		DWORD *bg2rgb = dc_destblend;
		BYTE *translation = dc_translation;
		BYTE *colormap = dc_colormap;
		const BYTE *source = dc_source;
		int pitch = dc_pitch;

		do
		{
			DWORD fg = colormap[translation[source[frac>>FRACBITS]]];
			DWORD bg = *dest;

			fg = fg2rgb[fg];
			bg = bg2rgb[bg];
			fg = (fg+bg) | 0x1f07c1f;
			*dest = RGB32k[0][0][fg & (fg>>15)];
			dest += pitch;
			frac += fracstep;
		} while (--count);
	}
}

// Draw a column whose "color" values are actually translucency
// levels for a base color stored in dc_color.
void R_DrawShadedColumnP_C (void)
{
	int  count;
	BYTE *dest;
	fixed_t frac, fracstep;

	count = dc_count;

	if (count <= 0)
		return;

	dest = dc_dest;

	fracstep = dc_iscale; 
	frac = dc_texturefrac;

	{
		const BYTE *source = dc_source;
		BYTE *colormap = dc_colormap;
		int pitch = dc_pitch;
		DWORD *fgstart = &Col2RGB8[0][dc_color];

		do
		{
			DWORD val = colormap[source[frac>>FRACBITS]];
			DWORD fg = fgstart[val<<8];
			val = (Col2RGB8[64-val][*dest] + fg) | 0x1f07c1f;
			*dest = RGB32k[0][0][val & (val>>15)];

			dest += pitch;
			frac += fracstep;
		} while (--count);
	}
} 

// Add source to destination, clamping it to white
void R_DrawAddClampColumnP_C ()
{
	int count;
	BYTE *dest;
	fixed_t frac;
	fixed_t fracstep;

	count = dc_count;
	if (count <= 0)
		return;

	dest = dc_dest;

	fracstep = dc_iscale;
	frac = dc_texturefrac;

	{
		BYTE *colormap = dc_colormap;
		const BYTE *source = dc_source;
		int pitch = dc_pitch;
		DWORD *fg2rgb = dc_srcblend;
		DWORD *bg2rgb = dc_destblend;

		do
		{
			DWORD a = fg2rgb[colormap[source[frac>>FRACBITS]]]
				+ bg2rgb[*dest];
			DWORD b = a;

			a |= 0x01f07c1f;
			b &= 0x40100400;
			a &= 0x3fffffff;
			b = b - (b >> 5);
			a |= b;
			*dest = RGB32k[0][0][a & (a>>15)];
			dest += pitch;
			frac += fracstep;
		} while (--count);
	}
}

// Add source to destination, clamping it to white
void R_DrawAddClampTranslatedColumnP_C ()
{
	int count;
	BYTE *dest;
	fixed_t frac;
	fixed_t fracstep;

	count = dc_count;
	if (count <= 0)
		return;

	dest = dc_dest;

	fracstep = dc_iscale;
	frac = dc_texturefrac;

	{
		BYTE *translation = dc_translation;
		BYTE *colormap = dc_colormap;
		const BYTE *source = dc_source;
		int pitch = dc_pitch;
		DWORD *fg2rgb = dc_srcblend;
		DWORD *bg2rgb = dc_destblend;

		do
		{
			DWORD a = fg2rgb[colormap[translation[source[frac>>FRACBITS]]]]
				+ bg2rgb[*dest];
			DWORD b = a;

			a |= 0x01f07c1f;
			b &= 0x40100400;
			a &= 0x3fffffff;
			b = b - (b >> 5);
			a |= b;
			*dest = RGB32k[0][0][(a>>15) & a];
			dest += pitch;
			frac += fracstep;
		} while (--count);
	}
}



//
// R_DrawSpan 
// With DOOM style restrictions on view orientation,
//	the floors and ceilings consist of horizontal slices
//	or spans with constant z depth.
// However, rotation around the world z axis is possible,
//	thus this mapping, while simpler and faster than
//	perspective correct texture mapping, has to traverse
//	the texture at an angle in all but a few cases.
// In consequence, flats are not stored by column (like walls),
//	and the inner loop has to step in texture space u and v.
//
// [RH] I'm not sure who wrote this, but floor/ceiling mapping
// *is* perspective correct for spans of constant z depth, which
// Doom guarantees because it does not let you change your pitch.
// Also, because of the new texture system, flats *are* stored by
// column to make it easy to use them on walls too. To accomodate
// this, the use of x/u and y/v in R_DrawSpan just needs to be
// swapped.
//
extern "C" {
int						ds_color;				// [RH] color for non-textured spans

int 					ds_y;
int 					ds_x1;
int 					ds_x2;

lighttable_t*			ds_colormap;

dsfixed_t 				ds_xfrac;
dsfixed_t 				ds_yfrac;
dsfixed_t 				ds_xstep;
dsfixed_t 				ds_ystep;
int						ds_xbits;
int						ds_ybits;

// start of a floor/ceiling tile image 
const BYTE*				ds_source;

// just for profiling
int 					dscount;
}

//
// Draws the actual span.
#if !defined(USEASM)
void R_DrawSpanP_C (void)
{
	dsfixed_t			xfrac;
	dsfixed_t			yfrac;
	dsfixed_t			xstep;
	dsfixed_t			ystep;
	BYTE*				dest;
	const BYTE*			source = ds_source;
	const BYTE*			colormap = ds_colormap;
	int 				count;
	int 				spot;

#ifdef RANGECHECK 
	if (ds_x2 < ds_x1 || ds_x1 < 0
		|| ds_x2 >= screen->width || ds_y > screen->height)
	{
		I_Error ("R_DrawSpan: %i to %i at %i", ds_x1, ds_x2, ds_y);
	}
//		dscount++;
#endif

	xfrac = ds_xfrac;
	yfrac = ds_yfrac;

	dest = ylookup[ds_y] + ds_x1 + dc_destorg;

	count = ds_x2 - ds_x1 + 1;

	xstep = ds_xstep;
	ystep = ds_ystep;

	if (ds_xbits == 6 && ds_ybits == 6)
	{
		// 64x64 is the most common case by far, so special case it.
		do
		{
			// Current texture index in u,v.
			spot = ((xfrac>>(32-6-6))&(63*64)) + (yfrac>>(32-6));

			// Lookup pixel from flat texture tile,
			//  re-index using light/colormap.
			*dest++ = colormap[source[spot]];

			// Next step in u,v.
			xfrac += xstep;
			yfrac += ystep;
		} while (--count);
	}
	else
	{
		BYTE yshift = 32 - ds_ybits;
		BYTE xshift = yshift - ds_xbits;
		int xmask = ((1 << ds_xbits) - 1) << ds_ybits;

		do
		{
			// Current texture index in u,v.
			spot = ((xfrac >> xshift) & xmask) + (yfrac >> yshift);

			// Lookup pixel from flat texture tile,
			//  re-index using light/colormap.
			*dest++ = colormap[source[spot]];

			// Next step in u,v.
			xfrac += xstep;
			yfrac += ystep;
		} while (--count);
	}
}

// [RH] Draw a span with holes
void R_DrawSpanMaskedP_C (void)
{
	dsfixed_t			xfrac;
	dsfixed_t			yfrac;
	dsfixed_t			xstep;
	dsfixed_t			ystep;
	BYTE*				dest;
	const BYTE*			source = ds_source;
	const BYTE*			colormap = ds_colormap;
	int 				count;
	int 				spot;

	xfrac = ds_xfrac;
	yfrac = ds_yfrac;

	dest = ylookup[ds_y] + ds_x1 + dc_destorg;

	count = ds_x2 - ds_x1 + 1;

	xstep = ds_xstep;
	ystep = ds_ystep;

	if (ds_xbits == 6 && ds_ybits == 6)
	{
		// 64x64 is the most common case by far, so special case it.
		do
		{
			BYTE texdata;

			spot = ((xfrac>>(32-6-6))&(63*64)) + (yfrac>>(32-6));
			texdata = source[spot];
			if (texdata != 0)
			{
				*dest = colormap[texdata];
			}
			dest++;
			xfrac += xstep;
			yfrac += ystep;
		} while (--count);
	}
	else
	{
		BYTE yshift = 32 - ds_ybits;
		BYTE xshift = yshift - ds_xbits;
		int xmask = ((1 << ds_xbits) - 1) << ds_ybits;
		do
		{
			BYTE texdata;
		
			spot = ((xfrac >> xshift) & xmask) + (yfrac >> yshift);
			texdata = source[spot];
			if (texdata != 0)
			{
				*dest = colormap[texdata];
			}
			dest++;
			xfrac += xstep;
			yfrac += ystep;
		} while (--count);
	}
}
#endif

void R_DrawSpanTranslucentP_C (void)
{
	dsfixed_t			xfrac;
	dsfixed_t			yfrac;
	dsfixed_t			xstep;
	dsfixed_t			ystep;
	BYTE*				dest;
	const BYTE*			source = ds_source;
	const BYTE*			colormap = ds_colormap;
	int 				count;
	int 				spot;
	DWORD *fg2rgb = dc_srcblend;
	DWORD *bg2rgb = dc_destblend;

	xfrac = ds_xfrac;
	yfrac = ds_yfrac;

	dest = ylookup[ds_y] + ds_x1 + dc_destorg;

	count = ds_x2 - ds_x1 + 1;

	xstep = ds_xstep;
	ystep = ds_ystep;

	if (ds_xbits == 6 && ds_ybits == 6)
	{
		// 64x64 is the most common case by far, so special case it.
		do
		{
			spot = ((xfrac>>(32-6-6))&(63*64)) + (yfrac>>(32-6));
			DWORD fg = colormap[source[spot]];
			DWORD bg = *dest;
			fg = fg2rgb[fg];
			bg = bg2rgb[bg];
			fg = (fg+bg) | 0x1f07c1f;
			*dest++ = RGB32k[0][0][fg & (fg>>15)];
			xfrac += xstep;
			yfrac += ystep;
		} while (--count);
	}
	else
	{
		BYTE yshift = 32 - ds_ybits;
		BYTE xshift = yshift - ds_xbits;
		int xmask = ((1 << ds_xbits) - 1) << ds_ybits;
		do
		{
			spot = ((xfrac >> xshift) & xmask) + (yfrac >> yshift);
			DWORD fg = colormap[source[spot]];
			DWORD bg = *dest;
			fg = fg2rgb[fg];
			bg = bg2rgb[bg];
			fg = (fg+bg) | 0x1f07c1f;
			*dest++ = RGB32k[0][0][fg & (fg>>15)];
			xfrac += xstep;
			yfrac += ystep;
		} while (--count);
	}
}

void R_DrawSpanMaskedTranslucentP_C (void)
{
	dsfixed_t			xfrac;
	dsfixed_t			yfrac;
	dsfixed_t			xstep;
	dsfixed_t			ystep;
	BYTE*				dest;
	const BYTE*			source = ds_source;
	const BYTE*			colormap = ds_colormap;
	int 				count;
	int 				spot;
	DWORD *fg2rgb = dc_srcblend;
	DWORD *bg2rgb = dc_destblend;

	xfrac = ds_xfrac;
	yfrac = ds_yfrac;

	dest = ylookup[ds_y] + ds_x1 + dc_destorg;

	count = ds_x2 - ds_x1 + 1;

	xstep = ds_xstep;
	ystep = ds_ystep;

	if (ds_xbits == 6 && ds_ybits == 6)
	{
		// 64x64 is the most common case by far, so special case it.
		do
		{
			BYTE texdata;

			spot = ((xfrac>>(32-6-6))&(63*64)) + (yfrac>>(32-6));
			texdata = source[spot];
			if (texdata != 0)
			{
				DWORD fg = colormap[texdata];
				DWORD bg = *dest;
				fg = fg2rgb[fg];
				bg = bg2rgb[bg];
				fg = (fg+bg) | 0x1f07c1f;
				*dest = RGB32k[0][0][fg & (fg>>15)];
			}
			dest++;
			xfrac += xstep;
			yfrac += ystep;
		} while (--count);
	}
	else
	{
		BYTE yshift = 32 - ds_ybits;
		BYTE xshift = yshift - ds_xbits;
		int xmask = ((1 << ds_xbits) - 1) << ds_ybits;
		do
		{
			BYTE texdata;
		
			spot = ((xfrac >> xshift) & xmask) + (yfrac >> yshift);
			texdata = source[spot];
			if (texdata != 0)
			{
				DWORD fg = colormap[texdata];
				DWORD bg = *dest;
				fg = fg2rgb[fg];
				bg = bg2rgb[bg];
				fg = (fg+bg) | 0x1f07c1f;
				*dest = RGB32k[0][0][fg & (fg>>15)];
			}
			dest++;
			xfrac += xstep;
			yfrac += ystep;
		} while (--count);
	}
}

// [RH] Just fill a span with a color
void R_FillSpan (void)
{
	memset (ylookup[ds_y] + ds_x1 + dc_destorg, ds_color, ds_x2 - ds_x1 + 1);
}

/****************************************************/
/****************************************************/

// wallscan stuff, in C

#ifndef USEASM
static DWORD STACK_ARGS vlinec1 ();
static void STACK_ARGS vlinec4 ();
static int vlinebits;

DWORD (STACK_ARGS *dovline1)() = vlinec1;
DWORD (STACK_ARGS *doprevline1)() = vlinec1;
void (STACK_ARGS *dovline4)() = vlinec4;

static DWORD STACK_ARGS mvlinec1();
static void STACK_ARGS mvlinec4();
static int mvlinebits;

DWORD (STACK_ARGS *domvline1)() = mvlinec1;
void (STACK_ARGS *domvline4)() = mvlinec4;

#else

extern "C"
{
DWORD STACK_ARGS vlineasm1 ();
DWORD STACK_ARGS prevlineasm1 ();
DWORD STACK_ARGS vlinetallasm1 ();
DWORD STACK_ARGS prevlinetallasm1 ();
void STACK_ARGS vlineasm4 ();
void STACK_ARGS vlinetallasm4 ();
void STACK_ARGS vlinetallasmathlon4 ();
void STACK_ARGS setupvlineasm (int);
void STACK_ARGS setupvlinetallasm (int);

DWORD STACK_ARGS mvlineasm1();
void STACK_ARGS mvlineasm4();
void STACK_ARGS setupmvlineasm (int);
}

DWORD (STACK_ARGS *dovline1)() = vlinetallasm1;
DWORD (STACK_ARGS *doprevline1)() = prevlinetallasm1;
void (STACK_ARGS *dovline4)() = vlinetallasm4;

DWORD (STACK_ARGS *domvline1)() = mvlineasm1;
void (STACK_ARGS *domvline4)() = mvlineasm4;
#endif

void setupvline (int fracbits)
{
#ifdef USEASM
	if (CPU.Family <= 5)
	{
		if (fracbits >= 24)
		{
			setupvlineasm (fracbits);
			dovline4 = vlineasm4;
			dovline1 = vlineasm1;
			doprevline1 = prevlineasm1;
		}
		else
		{
			setupvlinetallasm (fracbits);
			dovline1 = vlinetallasm1;
			doprevline1 = prevlinetallasm1;
			dovline4 = vlinetallasm4;
		}
	}
	else
	{
		setupvlinetallasm (fracbits);
		if (CPU.bIsAMD && CPU.AMDFamily >= 7)
		{
			dovline4 = vlinetallasmathlon4;
		}
	}
#else
	vlinebits = fracbits;
#endif
}

#ifndef USEASM
DWORD STACK_ARGS vlinec1 ()
{
	DWORD fracstep = dc_iscale;
	DWORD frac = dc_texturefrac;
	BYTE *colormap = dc_colormap;
	int count = dc_count;
	const BYTE *source = dc_source;
	BYTE *dest = dc_dest;
	int bits = vlinebits;
	int pitch = dc_pitch;

	do
	{
		*dest = colormap[source[frac>>bits]];
		frac += fracstep;
		dest += pitch;
	} while (--count);

	return frac;
}

void STACK_ARGS vlinec4 ()
{
	BYTE *dest = dc_dest;
	int count = dc_count;
	int bits = vlinebits;
	DWORD place;

	do
	{
		dest[0] = palookupoffse[0][bufplce[0][(place=vplce[0])>>bits]]; vplce[0] = place+vince[0];
		dest[1] = palookupoffse[1][bufplce[1][(place=vplce[1])>>bits]]; vplce[1] = place+vince[1];
		dest[2] = palookupoffse[2][bufplce[2][(place=vplce[2])>>bits]]; vplce[2] = place+vince[2];
		dest[3] = palookupoffse[3][bufplce[3][(place=vplce[3])>>bits]]; vplce[3] = place+vince[3];
		dest += dc_pitch;
	} while (--count);
}
#endif

void setupmvline (int fracbits)
{
#if defined(USEASM)
	setupmvlineasm (fracbits);
	domvline1 = mvlineasm1;
	domvline4 = mvlineasm4;
#else
	mvlinebits = fracbits;
#endif
}

#ifndef USEASM
DWORD STACK_ARGS mvlinec1 ()
{
	DWORD fracstep = dc_iscale;
	DWORD frac = dc_texturefrac;
	BYTE *colormap = dc_colormap;
	int count = dc_count;
	const BYTE *source = dc_source;
	BYTE *dest = dc_dest;
	int bits = mvlinebits;
	int pitch = dc_pitch;

	do
	{
		BYTE pix = source[frac>>bits];
		if (pix != 0)
		{
			*dest = colormap[pix];
		}
		frac += fracstep;
		dest += pitch;
	} while (--count);

	return frac;
}

void STACK_ARGS mvlinec4 ()
{
	BYTE *dest = dc_dest;
	int count = dc_count;
	int bits = mvlinebits;
	DWORD place;

	do
	{
		BYTE pix;

		pix = bufplce[0][(place=vplce[0])>>bits]; if(pix) dest[0] = palookupoffse[0][pix]; vplce[0] = place+vince[0];
		pix = bufplce[1][(place=vplce[1])>>bits]; if(pix) dest[1] = palookupoffse[1][pix]; vplce[1] = place+vince[1];
		pix = bufplce[2][(place=vplce[2])>>bits]; if(pix) dest[2] = palookupoffse[2][pix]; vplce[2] = place+vince[2];
		pix = bufplce[3][(place=vplce[3])>>bits]; if(pix) dest[3] = palookupoffse[3][pix]; vplce[3] = place+vince[3];
		dest += dc_pitch;
	} while (--count);
}
#endif

extern "C" short spanend[MAXHEIGHT];
extern fixed_t rw_light;
extern fixed_t rw_lightstep;
extern int wallshade;

static void R_DrawFogBoundarySection (int y, int y2, int x1)
{
	BYTE *colormap = dc_colormap;
	BYTE *dest = ylookup[y] + dc_destorg;

	for (; y < y2; ++y)
	{
		int x2 = spanend[y];
		int x = x1;
		do
		{
			dest[x] = colormap[dest[x]];
		} while (++x <= x2);
		dest += dc_pitch;
	}
}

static void R_DrawFogBoundaryLine (int y, int x)
{
	int x2 = spanend[y];
	BYTE *colormap = dc_colormap;
	BYTE *dest = ylookup[y] + dc_destorg;
	do
	{
		dest[x] = colormap[dest[x]];
	} while (++x <= x2);
}

void R_DrawFogBoundary (int x1, int x2, short *uclip, short *dclip)
{
	// This is essentially the same as R_MapVisPlane but with an extra step
	// to create new horizontal spans whenever the light changes enough that
	// we need to use a new colormap.

	fixed_t lightstep = rw_lightstep;
	fixed_t light = rw_light+lightstep*(x2-x1);
	int x = x2;
	int t2 = uclip[x];
	int b2 = dclip[x];
	int rcolormap = GETPALOOKUP (light, wallshade);
	int lcolormap;

	if (b2 > t2)
	{
		clearbufshort (spanend+t2, b2-t2, x);
	}

	dc_colormap = basecolormap + (rcolormap << COLORMAPSHIFT);

	for (--x; x >= x1; --x)
	{
		int t1 = uclip[x];
		int b1 = dclip[x];
		const int xr = x+1;
		int stop;

		light -= rw_lightstep;
		lcolormap = GETPALOOKUP (light, wallshade);
		if (lcolormap != rcolormap)
		{
			if (t2 < b2 && rcolormap != 0)
			{ // Colormap 0 is always the identity map, so rendering it is
			  // just a waste of time.
				R_DrawFogBoundarySection (t2, b2, xr);
			}
			if (t1 < t2) t2 = t1;
			if (b1 > b2) b2 = b1;
			if (t2 < b2)
			{
				clearbufshort (spanend+t2, b2-t2, x);
			}
			rcolormap = lcolormap;
			dc_colormap = basecolormap + (lcolormap << COLORMAPSHIFT);
		}
		else
		{
			if (dc_colormap != basecolormap)
			{
				stop = MIN (t1, b2);
				while (t2 < stop)
				{
					R_DrawFogBoundaryLine (t2++, xr);
				}
				stop = MAX (b1, t2);
				while (b2 > stop)
				{
					R_DrawFogBoundaryLine (--b2, xr);
				}
			}
			else
			{
				t2 = MAX (t2, MIN (t1, b2));
				b2 = MIN (b2, MAX (b1, t2));
			}

			stop = MIN (t2, b1);
			while (t1 < stop)
			{
				spanend[t1++] = x;
			}
			stop = MAX (b2, t2);
			while (b1 > stop)
			{
				spanend[--b1] = x;
			}
		}

		t2 = uclip[x];
		b2 = dclip[x];
	}
	if (t2 < b2 && rcolormap != 0)
	{
		R_DrawFogBoundarySection (t2, b2, x1);
	}
}

int tmvlinebits;

void setuptmvline (int bits)
{
	tmvlinebits = bits;
}

fixed_t tmvline1_add ()
{
	DWORD fracstep = dc_iscale;
	DWORD frac = dc_texturefrac;
	BYTE *colormap = dc_colormap;
	int count = dc_count;
	const BYTE *source = dc_source;
	BYTE *dest = dc_dest;
	int bits = tmvlinebits;
	int pitch = dc_pitch;

	DWORD *fg2rgb = dc_srcblend;
	DWORD *bg2rgb = dc_destblend;

	do
	{
		BYTE pix = source[frac>>bits];
		if (pix != 0)
		{
			DWORD fg = fg2rgb[colormap[pix]];
			DWORD bg = bg2rgb[*dest];
			fg = (fg+bg) | 0x1f07c1f;
			*dest = RGB32k[0][0][fg & (fg>>15)];
		}
		frac += fracstep;
		dest += pitch;
	} while (--count);

	return frac;
}

void tmvline4_add ()
{
	BYTE *dest = dc_dest;
	int count = dc_count;
	int bits = tmvlinebits;

	DWORD *fg2rgb = dc_srcblend;
	DWORD *bg2rgb = dc_destblend;

	do
	{
		for (int i = 0; i < 4; ++i)
		{
			BYTE pix = bufplce[i][vplce[i] >> bits];
			if (pix != 0)
			{
				DWORD fg = fg2rgb[palookupoffse[i][pix]];
				DWORD bg = bg2rgb[dest[i]];
				fg = (fg+bg) | 0x1f07c1f;
				dest[i] = RGB32k[0][0][fg & (fg>>15)];
			}
			vplce[i] += vince[i];
		}
		dest += dc_pitch;
	} while (--count);
}

fixed_t tmvline1_addclamp ()
{
	DWORD fracstep = dc_iscale;
	DWORD frac = dc_texturefrac;
	BYTE *colormap = dc_colormap;
	int count = dc_count;
	const BYTE *source = dc_source;
	BYTE *dest = dc_dest;
	int bits = tmvlinebits;
	int pitch = dc_pitch;

	DWORD *fg2rgb = dc_srcblend;
	DWORD *bg2rgb = dc_destblend;

	do
	{
		BYTE pix = source[frac>>bits];
		if (pix != 0)
		{
			DWORD a = fg2rgb[colormap[pix]] + bg2rgb[*dest];
			DWORD b = a;

			a |= 0x01f07c1f;
			b &= 0x40100400;
			a &= 0x3fffffff;
			b = b - (b >> 5);
			a |= b;
			*dest = RGB32k[0][0][a & (a>>15)];
		}
		frac += fracstep;
		dest += pitch;
	} while (--count);

	return frac;
}

void tmvline4_addclamp ()
{
	BYTE *dest = dc_dest;
	int count = dc_count;
	int bits = tmvlinebits;

	DWORD *fg2rgb = dc_srcblend;
	DWORD *bg2rgb = dc_destblend;

	do
	{
		for (int i = 0; i < 4; ++i)
		{
			BYTE pix = bufplce[i][vplce[i] >> bits];
			if (pix != 0)
			{
				DWORD a = fg2rgb[palookupoffse[i][pix]] + bg2rgb[dest[i]];
				DWORD b = a;

				a |= 0x01f07c1f;
				b &= 0x40100400;
				a &= 0x3fffffff;
				b = b - (b >> 5);
				a |= b;
				dest[i] = RGB32k[0][0][a & (a>>15)];
			}
			vplce[i] += vince[i];
		}
		dest += dc_pitch;
	} while (--count);
}

/****************************************************/
/****************************************************/

//
// R_InitTranslationTables
// Creates the translation tables to map the green color ramp to gray,
// brown, red. Assumes a given structure of the PLAYPAL.
//
void R_InitTranslationTables ()
{
	static BYTE MainTranslationTables[256*
		(NUMCOLORMAPS*16			// Shaded
		 +MAXPLAYERS*2+1			// Players + PlayersExtra + Menu player
		 +8							// Standard	(7 for Strife, 3 for the rest)
		 +MAX_ACS_TRANSLATIONS		// LevelScripted
		 +BODYQUESIZE				// PlayerCorpses
		 )];
	int i, j;

	// Diminishing translucency tables for shaded actors. Not really
	// translation tables, but putting them here was convenient, particularly
	// since translationtables[0] would otherwise be wasted.
	translationtables[0] = MainTranslationTables;

	// Player translations, one for each player
	translationtables[TRANSLATION_Players] =
		translationtables[0] + NUMCOLORMAPS*16*256;

	// Extra player translations, one for each player, unused by Doom
	translationtables[TRANSLATION_PlayersExtra] =
		translationtables[TRANSLATION_Players] + (MAXPLAYERS+1)*256;

	// The three standard translations from Doom or Heretic (seven for Strife),
	// plus the generic ice translation.
	translationtables[TRANSLATION_Standard] =
		translationtables[TRANSLATION_PlayersExtra] + MAXPLAYERS*256;

	translationtables[TRANSLATION_LevelScripted] =
		translationtables[TRANSLATION_Standard] + 8*256;

	translationtables[TRANSLATION_PlayerCorpses] =
		translationtables[TRANSLATION_LevelScripted] + MAX_ACS_TRANSLATIONS*256;

	translationtables[TRANSLATION_Decorate] = decorate_translations;
	translationtables[TRANSLATION_Blood] = decorate_translations + MAX_DECORATE_TRANSLATIONS*256;

	// [RH] Each player now gets their own translation table. These are set
	//		up during netgame arbitration and as-needed rather than in here.

	for (i = 0; i < 256; ++i)
	{
		translationtables[0][i] = i;
	}
	for (i = 0; i < MAXPLAYERS; ++i)
	{
		memcpy (translationtables[TRANSLATION_Players] + i*256, translationtables[0], 256);
		memcpy (translationtables[TRANSLATION_PlayersExtra] + i*256, translationtables[0], 256);
	}

	// Create the standard translation tables
	for (i = 0; i < 7; ++i)
	{
		memcpy (translationtables[TRANSLATION_Standard] + i*256, translationtables[0], 256);
	}
	if (gameinfo.gametype == GAME_Doom)
	{
		for (i = 0x70; i < 0x80; i++)
		{ // map green ramp to gray, brown, red
			translationtables[TRANSLATION_Standard][i    ] = 0x60 + (i&0xf);
			translationtables[TRANSLATION_Standard][i+256] = 0x40 + (i&0xf);
			translationtables[TRANSLATION_Standard][i+512] = 0x20 + (i&0xf);
		}
	}
	else if (gameinfo.gametype == GAME_Heretic)
	{
		for (i = 225; i <= 240; i++)
		{
			translationtables[TRANSLATION_Standard][i    ] = 114+(i-225); // yellow
			translationtables[TRANSLATION_Standard][i+256] = 145+(i-225); // red
			translationtables[TRANSLATION_Standard][i+512] = 190+(i-225); // blue
		}
	}
	else if (gameinfo.gametype == GAME_Strife)
	{
		for (i = 0x20; i <= 0x3F; ++i)
		{
			translationtables[TRANSLATION_Standard][i      ] = i - 0x20;
			translationtables[TRANSLATION_Standard][i+1*256] = i - 0x20;
			translationtables[TRANSLATION_Standard][i+2*256] = 0xD0 + (i&0xf);
			translationtables[TRANSLATION_Standard][i+3*256] = 0xD0 + (i&0xf);
			translationtables[TRANSLATION_Standard][i+4*256] = i - 0x20;
			translationtables[TRANSLATION_Standard][i+5*256] = i - 0x20;
			translationtables[TRANSLATION_Standard][i+6*256] = i - 0x20;
		}
		for (i = 0x50; i <= 0x5F; ++i)
		{
			// Merchant hair
			translationtables[TRANSLATION_Standard][i+4*256] = 0x80 + (i&0xf);
			translationtables[TRANSLATION_Standard][i+5*256] = 0x10 + (i&0xf);
			translationtables[TRANSLATION_Standard][i+6*256] = 0x40 + (i&0xf);
		}
		for (i = 0x80; i <= 0x8F; ++i)
		{
			translationtables[TRANSLATION_Standard][i      ] = 0x40 + (i&0xf); // red
			translationtables[TRANSLATION_Standard][i+1*256] = 0xB0 + (i&0xf); // rust
			translationtables[TRANSLATION_Standard][i+2*256] = 0x10 + (i&0xf); // gray
			translationtables[TRANSLATION_Standard][i+3*256] = 0x30 + (i&0xf); // dark green
			translationtables[TRANSLATION_Standard][i+4*256] = 0x50 + (i&0xf); // gold
			translationtables[TRANSLATION_Standard][i+5*256] = 0x60 + (i&0xf); // bright green
			translationtables[TRANSLATION_Standard][i+6*256] = 0x90 + (i&0xf); // blue
		}
		for (i = 0xC0; i <= 0xCF; ++i)
		{
			translationtables[TRANSLATION_Standard][i+4*256] = 0xA0 + (i&0xf);
			translationtables[TRANSLATION_Standard][i+5*256] = 0x20 + (i&0xf);
			translationtables[TRANSLATION_Standard][i+6*256] = (i&0xf);
		}
		translationtables[TRANSLATION_Standard][0xC0+6*256] = 1;
		for (i = 0xD0; i <= 0xDF; ++i)
		{
			translationtables[TRANSLATION_Standard][i+4*256] = 0xB0 + (i&0xf);
			translationtables[TRANSLATION_Standard][i+5*256] = 0x30 + (i&0xf);
			translationtables[TRANSLATION_Standard][i+6*256] = 0x10 + (i&0xf);
		}
		for (i = 0xF1; i <= 0xF6; ++i)
		{
			translationtables[TRANSLATION_Standard][i      ] = 0xDF + (i&0xf);
		}
		for (i = 0xF7; i <= 0xFB; ++i)
		{
			translationtables[TRANSLATION_Standard][i      ] = i - 6;
		}
	}

	// Create the ice translation table, based on Hexen's. Alas, the standard
	// Doom palette has no good substitutes for these bluish-tinted grays, so
	// they will just look gray unless you use a different PLAYPAL with Doom.

	static const BYTE IcePalette[16][3] =
	{
		{  10,  8, 18 },
		{  15, 15, 26 },
		{  20, 16, 36 },
		{  30, 26, 46 },
		{  40, 36, 57 },
		{  50, 46, 67 },
		{  59, 57, 78 },
		{  69, 67, 88 },
		{  79, 77, 99 },
		{  89, 87,109 },
		{  99, 97,120 },
		{ 109,107,130 },
		{ 118,118,141 },
		{ 128,128,151 },
		{ 138,138,162 },
		{ 148,148,172 }
	};
	BYTE IcePaletteRemap[16];
	for (i = 0; i < 16; ++i)
	{
		IcePaletteRemap[i] = ColorMatcher.Pick (IcePalette[i][0], IcePalette[i][1], IcePalette[i][2]);
	}
	for (i = 0; i < 256; ++i)
	{
		int r = GPalette.BaseColors[i].r;
		int g = GPalette.BaseColors[i].g;
		int b = GPalette.BaseColors[i].b;
		int v = (r*77 + g*143 + b*37) >> 12;
		translationtables[TRANSLATION_Standard][7*256+i] = IcePaletteRemap[v];
	}

	// set up shading tables for shaded columns
	// 16 colormap sets, progressing from full alpha to minimum visible alpha

	BYTE *table = translationtables[TRANSLATION_Shaded];

	// Full alpha
	for (i = 0; i < 16; ++i)
	{
		for (j = 0; j < NUMCOLORMAPS; ++j)
		{
			int a = (NUMCOLORMAPS - j) * 256 / NUMCOLORMAPS * (16-i);
			for (int k = 0; k < 256; ++k)
			{
				BYTE v = (((k+2) * a) + 256) >> 14;
				table[k] = MIN<BYTE> (v, 64);
			}
			table += 256;
		}
	}
}

// [RH] Create a player's translation table based on a given mid-range color.
// [GRB] Split to 2 functions (because of player setup menu)
static void R_CreatePlayerTranslation (float h, float s, float v, FPlayerSkin *skin, BYTE *table, BYTE *alttable)
{
	int i;
	BYTE start = skin->range0start;
	BYTE end = skin->range0end;
	float r, g, b;
	float bases, basev;
	float sdelta, vdelta;
	float range;

	// Set up the base translation for this skin. If the skin was created
	// for the current game, then this is just an identity translation.
	// Otherwise, it remaps the colors from the skin's original palette to
	// the current one.
	if (skin->othergame)
	{
		memcpy (table, OtherGameSkinRemap, 256);
	}
	else
	{
		for (i = 0; i < 256; ++i)
		{
			table[i] = i;
		}
	}

	// [GRB] Don't translate skins with color range 0-0 (APlayerPawn default)
	if (start == 0 && end == 0)
		return;

	range = (float)(end-start+1);

	bases = s;
	basev = v;

	if (gameinfo.gametype == GAME_Doom || gameinfo.gametype == GAME_Strife)
	{
		// Build player sprite translation
		s -= 0.23f;
		v += 0.1f;
		sdelta = 0.23f / range;
		vdelta = -0.94112f / range;

		for (i = start; i <= end; i++)
		{
			float uses, usev;
			uses = clamp (s, 0.f, 1.f);
			usev = clamp (v, 0.f, 1.f);
			HSVtoRGB (&r, &g, &b, h, uses, usev);
			table[i] = ColorMatcher.Pick (
				clamp ((int)(r * 255.f), 0, 255),
				clamp ((int)(g * 255.f), 0, 255),
				clamp ((int)(b * 255.f), 0, 255));
			s += sdelta;
			v += vdelta;
		}
	}
	else if (gameinfo.gametype == GAME_Heretic)
	{
		float vdelta = 0.418916f / range;

		// Build player sprite translation
		for (i = start; i <= end; i++)
		{
			v = vdelta * (float)(i - start) + basev - 0.2352937f;
			v = clamp (v, 0.f, 1.f);
			HSVtoRGB (&r, &g, &b, h, s, v);
			table[i] = ColorMatcher.Pick (
				clamp ((int)(r * 255.f), 0, 255),
				clamp ((int)(g * 255.f), 0, 255),
				clamp ((int)(b * 255.f), 0, 255));
		}

		// Build rain/lifegem translation
		if (alttable)
		{
			bases = MIN (bases*1.3f, 1.f);
			basev = MIN (basev*1.3f, 1.f);
			for (i = 145; i <= 168; i++)
			{
				s = MIN (bases, 0.8965f - 0.0962f*(float)(i - 161));
				v = MIN (1.f, (0.2102f + 0.0489f*(float)(i - 144)) * basev);
				HSVtoRGB (&r, &g, &b, h, s, v);
				alttable[i] = ColorMatcher.Pick (
					clamp ((int)(r * 255.f), 0, 255),
					clamp ((int)(g * 255.f), 0, 255),
					clamp ((int)(b * 255.f), 0, 255));
			}
		}
	}
	else if (gameinfo.gametype == GAME_Hexen)
	{
		if (memcmp (sprites[skin->sprite].name, "PLAY", 4) == 0)
		{ // The fighter is different! He gets a brown hairy loincloth, but the other
		  // two have blue capes.
			float vs[9] = { .28f, .32f, .38f, .42f, .47f, .5f, .58f, .71f, .83f };

			// Build player sprite translation
			//h = 45.f;
			v = MAX (0.1f, v);

			for (i = start; i <= end; i++)
			{
				HSVtoRGB (&r, &g, &b, h, s, vs[(i-start)*9/(int)range]*basev);
				table[i] = ColorMatcher.Pick (
					clamp ((int)(r * 255.f), 0, 255),
					clamp ((int)(g * 255.f), 0, 255),
					clamp ((int)(b * 255.f), 0, 255));
			}
		}
		else
		{
			float ms[18] = { .95f, .96f, .89f, .97f, .97f, 1.f, 1.f, 1.f, .97f, .99f, .87f, .77f, .69f, .62f, .57f, .47f, .43f };
			float mv[18] = { .16f, .19f, .22f, .25f, .31f, .35f, .38f, .41f, .47f, .54f, .60f, .65f, .71f, .77f, .83f, .89f, .94f, 1.f };

			// Build player sprite translation
			v = MAX (0.1f, v);

			for (i = start; i <= end; i++)
			{
				HSVtoRGB (&r, &g, &b, h, ms[(i-start)*18/(int)range]*bases, mv[(i-start)*18/(int)range]*basev);
				table[i] = ColorMatcher.Pick (
					clamp ((int)(r * 255.f), 0, 255),
					clamp ((int)(g * 255.f), 0, 255),
					clamp ((int)(b * 255.f), 0, 255));
			}
		}

		// Build lifegem translation
		if (alttable)
		{
			for (i = 164; i <= 185; ++i)
			{
				const PalEntry *base = &GPalette.BaseColors[i];
				float dummy;

				RGBtoHSV (base->r/255.f, base->g/255.f, base->b/255.f, &dummy, &s, &v);
				HSVtoRGB (&r, &g, &b, h, s*bases, v*basev);
				alttable[i] = ColorMatcher.Pick (
					clamp ((int)(r * 255.f), 0, 255),
					clamp ((int)(g * 255.f), 0, 255),
					clamp ((int)(b * 255.f), 0, 255));
			}
		}
	}
}

void R_BuildPlayerTranslation (int player)
{
	float h, s, v;

	D_GetPlayerColor (player, &h, &s, &v);

	R_CreatePlayerTranslation (h, s, v,
		&skins[players[player].userinfo.skin],
		&translationtables[TRANSLATION_Players][player*256],
		&translationtables[TRANSLATION_PlayersExtra][player*256]);
}

void R_GetPlayerTranslation (int color, FPlayerSkin *skin, BYTE *table)
{
	float h, s, v;

	RGBtoHSV (RPART(color)/255.f, GPART(color)/255.f, BPART(color)/255.f,
		&h, &s, &v);

	R_CreatePlayerTranslation (h, s, v, skin, table, NULL);
}

void R_DrawBorder (int x1, int y1, int x2, int y2)
{
	int picnum;

	if (level.info != NULL)
	{
		picnum = TexMan.CheckForTexture (level.info->bordertexture, FTexture::TEX_Flat);
	}
	else
	{
		picnum = TexMan.CheckForTexture (gameinfo.borderFlat, FTexture::TEX_Flat);
	}

	if (picnum >= 0)
	{
		screen->FlatFill (x1, y1, x2, y2, TexMan(picnum));
	}
	else
	{
		screen->Clear (x1, y1, x2, y2, 0, 0);
	}
}


/*
==================
=
= R_DrawViewBorder
=
= Draws the border around the view for different size windows
==================
*/

int BorderNeedRefresh;

void V_MarkRect (int x, int y, int width, int height);
void M_DrawFrame (int x, int y, int width, int height);

void R_DrawViewBorder (void)
{
	// [RH] Redraw the status bar if SCREENWIDTH > status bar width.
	// Will draw borders around itself, too.
	if (SCREENWIDTH > 320)
	{
		SB_state = screen->GetPageCount ();
	}

	if (realviewwidth == SCREENWIDTH)
	{
		return;
	}

	R_DrawBorder (0, 0, SCREENWIDTH, viewwindowy);
	R_DrawBorder (0, viewwindowy, viewwindowx, realviewheight + viewwindowy);
	R_DrawBorder (viewwindowx + realviewwidth, viewwindowy, SCREENWIDTH, realviewheight + viewwindowy);
	R_DrawBorder (0, viewwindowy + realviewheight, SCREENWIDTH, ST_Y);

	M_DrawFrame (viewwindowx, viewwindowy, realviewwidth, realviewheight);
	V_MarkRect (0, 0, SCREENWIDTH, ST_Y);
}

/*
==================
=
= R_DrawTopBorder
=
= Draws the top border around the view for different size windows
==================
*/

int BorderTopRefresh;

void R_DrawTopBorder ()
{
	FTexture *p1, *p2;
	int x, y;
	int offset;
	int size;

	if (realviewwidth == SCREENWIDTH)
		return;

	R_DrawBorder (0, 0, SCREENWIDTH, 34);
	offset = gameinfo.border->offset;
	size = gameinfo.border->size;

	if (viewwindowy < 35)
	{
		p1 = TexMan(gameinfo.border->t);
		for (x = viewwindowx; x < viewwindowx + realviewwidth; x += size)
		{
			screen->DrawTexture (p1, x, viewwindowy - offset, TAG_DONE);
		}

		p1 = TexMan(gameinfo.border->l);
		p2 = TexMan(gameinfo.border->r);
		for (y = viewwindowy; y < 35; y += size)
		{
			screen->DrawTexture (p1, viewwindowx - offset, y, TAG_DONE);
			screen->DrawTexture (p2, viewwindowx + realviewwidth, y, TAG_DONE);
		}

		p1 = TexMan(gameinfo.border->tl);
		screen->DrawTexture (p1, viewwindowx-offset, viewwindowy - offset, TAG_DONE);

		p1 = TexMan(gameinfo.border->tr);
		screen->DrawTexture (p1, viewwindowx+realviewwidth, viewwindowy - offset, TAG_DONE);
	}
}


// [RH] Double pixels in the view window horizontally
//		and/or vertically (or not at all).
void R_DetailDouble ()
{
	if (!viewactive) return;
	DetailDoubleCycles = 0;
	clock (DetailDoubleCycles);

	switch ((detailxshift << 1) | detailyshift)
	{
	case 1:		// y-double
#ifdef USEASM
		DoubleVert_ASM (viewheight, viewwidth, dc_destorg, RenderTarget->GetPitch());
#else
		{
			int rowsize = realviewwidth;
			int pitch = RenderTarget->GetPitch();
			int y;
			BYTE *line;

			line = dc_destorg;
			for (y = viewheight; y != 0; --y, line += pitch<<1)
			{
				memcpy (line+pitch, line, rowsize);
			}
		}
#endif
		break;

	case 2:		// x-double
#ifdef USEASM
		if (CPU.bMMX && (viewwidth&15)==0)
		{
			DoubleHoriz_MMX (viewheight, viewwidth, dc_destorg+viewwidth, RenderTarget->GetPitch());
		}
		else
#endif
		{
			int rowsize = viewwidth;
			int pitch = RenderTarget->GetPitch();
			int y,x;
			BYTE *linefrom, *lineto;

			linefrom = dc_destorg;
			for (y = viewheight; y != 0; --y, linefrom += pitch)
			{
				lineto = linefrom - viewwidth;
				for (x = 0; x < rowsize; ++x)
				{
					BYTE c = linefrom[x];
					lineto[x*2] = c;
					lineto[x*2+1] = c;
				}
			}
		}
		break;

	case 3:		// x- and y-double
#ifdef USEASM
		if (CPU.bMMX && (viewwidth&15)==0 && 0)
		{
			DoubleHorizVert_MMX (viewheight, viewwidth, dc_destorg+viewwidth, RenderTarget->GetPitch());
		}
		else
#endif
		{
			int rowsize = viewwidth;
			int realpitch = RenderTarget->GetPitch();
			int pitch = realpitch << 1;
			int y,x;
			BYTE *linefrom, *lineto;

			linefrom = dc_destorg;
			for (y = viewheight; y != 0; --y, linefrom += pitch)
			{
				lineto = linefrom - viewwidth;
				for (x = 0; x < rowsize; ++x)
				{
					BYTE c = linefrom[x];
					lineto[x*2] = c;
					lineto[x*2+1] = c;
					lineto[x*2+realpitch] = c;
					lineto[x*2+realpitch+1] = c;
				}
			}
		}
		break;
	}

	unclock (DetailDoubleCycles);
}

ADD_STAT(detail)
{
	FString out;
	out.Format ("doubling = %04.1f ms", (double)DetailDoubleCycles * 1000 * SecondsPerCycle);
	return out;
}

// [RH] Initialize the column drawer pointers
void R_InitColumnDrawers ()
{
#ifdef USEASM
	R_DrawColumn				= R_DrawColumnP_ASM;
	R_DrawColumnHoriz			= R_DrawColumnHorizP_ASM;
	R_DrawFuzzColumn			= R_DrawFuzzColumnP_ASM;
	R_DrawTranslatedColumn		= R_DrawTranslatedColumnP_C;
	R_DrawShadedColumn			= R_DrawShadedColumnP_C;
	R_DrawSpan					= R_DrawSpanP_ASM;
	R_DrawSpanMasked			= R_DrawSpanMaskedP_ASM;
	if (CPU.Family <= 5)
	{
		rt_map4cols				= rt_map4cols_asm2;
	}
	else
	{
		rt_map4cols				= rt_map4cols_asm1;
	}
#else
	R_DrawColumnHoriz			= R_DrawColumnHorizP_C;
	R_DrawColumn				= R_DrawColumnP_C;
	R_DrawFuzzColumn			= R_DrawFuzzColumnP_C;
	R_DrawTranslatedColumn		= R_DrawTranslatedColumnP_C;
	R_DrawShadedColumn			= R_DrawShadedColumnP_C;
	R_DrawSpan					= R_DrawSpanP_C;
	R_DrawSpanMasked			= R_DrawSpanMaskedP_C;
	rt_map4cols					= rt_map4cols_c;
#endif
	R_DrawSpanTranslucent		= R_DrawSpanTranslucentP_C;
	R_DrawSpanMaskedTranslucent = R_DrawSpanMaskedTranslucentP_C;
}

// [RH] Choose column drawers in a single place
EXTERN_CVAR (Bool, r_drawfuzz)
EXTERN_CVAR (Float, transsouls)
CVAR (Bool, r_drawtrans, true, 0)

static BYTE *basecolormapsave;

// Convenience macros, to make the following look more like OpenGL/Direct3D
#define BL_ONE				FRACUNIT
#define BL_ZERO				0
#define BL_SRC_ALPHA		alpha
#define BL_INV_SRC_ALPHA	(BL_ONE-alpha)

static bool stencilling;

static bool R_SetBlendFunc (fixed_t fglevel, fixed_t bglevel)
{
	if (!r_drawtrans || (fglevel == BL_ONE && bglevel == BL_ZERO))
	{
		if (stencilling)
		{
			colfunc = R_FillColumnP;
			hcolfunc_post1 = rt_copy1col;
			hcolfunc_post4 = rt_copy4cols;
		}
		else if (dc_translation == NULL)
		{
			colfunc = basecolfunc;
			hcolfunc_post1 = rt_map1col;
			hcolfunc_post4 = rt_map4cols;
		}
		else
		{
			colfunc = transcolfunc;
			hcolfunc_post1 = rt_tlate1col;
			hcolfunc_post4 = rt_tlate4cols;
		}
		return true;
	}
	if (fglevel == BL_ZERO && bglevel == BL_ONE)
	{
		return false;
	}
	if (fglevel + bglevel <= BL_ONE)
	{ // Colors won't overflow when added
		dc_srcblend = Col2RGB8[fglevel>>10];
		dc_destblend = Col2RGB8[bglevel>>10];
		if (stencilling)
		{
			colfunc = R_FillAddColumn;
			hcolfunc_post1 = rt_add1col;
			hcolfunc_post4 = rt_add4cols;
		}
		else if (dc_translation == NULL)
		{
			colfunc = R_DrawAddColumnP_C;
			hcolfunc_post1 = rt_add1col;
			hcolfunc_post4 = rt_add4cols;
		}
		else
		{
			colfunc = R_DrawTlatedAddColumnP_C;
			hcolfunc_post1 = rt_tlateadd1col;
			hcolfunc_post4 = rt_tlateadd4cols;
		}
	}
	else
	{ // Colors might overflow when added
		dc_srcblend = Col2RGB8_LessPrecision[fglevel>>10];
		dc_destblend = Col2RGB8_LessPrecision[bglevel>>10];
		if (dc_translation == NULL)
		{
			colfunc = R_DrawAddClampColumnP_C;
			hcolfunc_post1 = rt_addclamp1col;
			hcolfunc_post4 = rt_addclamp4cols;
		}
		else
		{
			colfunc = R_DrawAddClampTranslatedColumnP_C;
			hcolfunc_post1 = rt_tlateaddclamp1col;
			hcolfunc_post4 = rt_tlateaddclamp4cols;
		}
	}
	return true;
}

ESPSResult R_SetPatchStyle (int style, fixed_t alpha, int translation, DWORD color)
{
	fixed_t fglevel, bglevel;

	if (style == STYLE_OptFuzzy)
	{
		style = (r_drawfuzz || !r_drawtrans) ? STYLE_Fuzzy : STYLE_Translucent;
	}
	else if (style == STYLE_SoulTrans)
	{
		style = STYLE_Translucent;
		alpha = (fixed_t)(FRACUNIT * transsouls);
	}

	alpha = clamp<fixed_t> (alpha, 0, FRACUNIT);

	if (translation != 0)
	{
		dc_translation = translationtables[(translation&0xff00)>>8]
			+ (translation&0x00ff)*256;
	}
	else
	{
		dc_translation = NULL;
	}
	basecolormapsave = basecolormap;
	stencilling = false;
	hcolfunc_pre = R_DrawColumnHoriz;

	switch (style)
	{
		// Special modes
	case STYLE_Fuzzy:
		colfunc = fuzzcolfunc;
		return DoDraw0;

	case STYLE_Shaded:
		// Shaded drawer only gets 16 levels because it saves memory.
		if ((alpha >>= 12) == 0)
			return DontDraw;
		colfunc = R_DrawShadedColumn;
		hcolfunc_post1 = rt_shaded1col;
		hcolfunc_post4 = rt_shaded4cols;
		dc_color = fixedcolormap ? fixedcolormap[APART(color)] : basecolormap[APART(color)];
		dc_colormap = basecolormap = &translationtables[TRANSLATION_Shaded][((16-alpha)*NUMCOLORMAPS)*256];
		if (fixedlightlev)
		{
			dc_colormap += fixedlightlev;
		}
		return r_columnmethod ? DoDraw1 : DoDraw0;

		// Standard modes
	case STYLE_Stencil:
		dc_color = APART(color);
		stencilling = true;
	case STYLE_Normal:
		fglevel = BL_ONE;
		bglevel = BL_ZERO;
		break;

	case STYLE_TranslucentStencil:
		dc_color = APART(color);
		stencilling = true;
	case STYLE_Translucent:
		fglevel = BL_SRC_ALPHA;
		bglevel = BL_INV_SRC_ALPHA;
		break;

	case STYLE_Add:
		fglevel = BL_SRC_ALPHA;
		bglevel = BL_ONE;
		break;

	default:
		return DontDraw;
	}

	if (stencilling)
	{
		hcolfunc_pre = R_FillColumnHorizP;
	}

	return R_SetBlendFunc (fglevel, bglevel) ?
		(r_columnmethod ? DoDraw1 : DoDraw0) : DontDraw;
}

void R_FinishSetPatchStyle ()
{
	basecolormap = basecolormapsave;
}

bool R_GetTransMaskDrawers (fixed_t (**tmvline1)(), void (**tmvline4)())
{
	if (colfunc == R_DrawAddColumnP_C)
	{
		*tmvline1 = tmvline1_add;
		*tmvline4 = tmvline4_add;
		return true;
	}
	if (colfunc == R_DrawAddClampColumnP_C)
	{
		*tmvline1 = tmvline1_addclamp;
		*tmvline4 = tmvline4_addclamp;
		return true;
	}
	return false;
}
