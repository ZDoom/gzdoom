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
#include "doomdef.h"
#include "i_system.h"
#include "w_wad.h"
#include "r_local.h"
#include "v_video.h"
#include "doomstat.h"
#include "st_stuff.h"
#include "g_game.h"
#include "g_level.h"
#include "r_data/r_translate.h"
#include "v_palette.h"
#include "r_data/colormaps.h"

#include "gi.h"
#include "stats.h"
#include "x86.h"

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

BYTE*			viewimage;
extern "C" {
int				ylookup[MAXHEIGHT];
BYTE			*dc_destorg;
}
int 			scaledviewwidth;

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
void (*R_DrawSpanAddClamp)(void);
void (*R_DrawSpanMaskedAddClamp)(void);
void (*rt_map4cols)(int,int,int);

//
// R_DrawColumn
// Source is the top of the column to scale.
//
double 			dc_texturemid;
extern "C" {
int				dc_pitch=0xABadCafe;	// [RH] Distance between rows

lighttable_t*	dc_colormap; 
int 			dc_x; 
int 			dc_yl; 
int 			dc_yh; 
fixed_t 		dc_iscale; 
fixed_t			dc_texturefrac;
int				dc_color;				// [RH] Color for column filler
DWORD			dc_srccolor;
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

int dc_fillcolor;
BYTE *dc_translation;
BYTE shadetables[NUMCOLORMAPS*16*256];
FDynamicColormap ShadeFakeColormap[16];
BYTE identitymap[256];

EXTERN_CVAR (Int, r_columnmethod)


void R_InitShadeMaps()
{
	int i,j;
	// set up shading tables for shaded columns
	// 16 colormap sets, progressing from full alpha to minimum visible alpha

	BYTE *table = shadetables;

	// Full alpha
	for (i = 0; i < 16; ++i)
	{
		ShadeFakeColormap[i].Color = ~0u;
		ShadeFakeColormap[i].Desaturate = ~0u;
		ShadeFakeColormap[i].Next = NULL;
		ShadeFakeColormap[i].Maps = table;

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
	for (i = 0; i < NUMCOLORMAPS*16*256; ++i)
	{
		assert(shadetables[i] <= 64);
	}

	// Set up a guaranteed identity map
	for (i = 0; i < 256; ++i)
	{
		identitymap[i] = i;
	}
}

/************************************/
/*									*/
/* Palettized drawers (C versions)	*/
/*									*/
/************************************/

#ifndef	X86_ASM
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
#endif

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
	fg = dc_srccolor;
	int pitch = dc_pitch;

	do
	{
		DWORD bg;
		bg = (fg + bg2rgb[*dest]) | 0x1f07c1f;
		*dest = RGB32k.All[bg & (bg>>15)];
		dest += pitch; 
	} while (--count);

}

void R_FillAddClampColumn (void)
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
	fg = dc_srccolor;
	int pitch = dc_pitch;

	do
	{
		DWORD a = fg + bg2rgb[*dest];
		DWORD b = a;

		a |= 0x01f07c1f;
		b &= 0x40100400;
		a &= 0x3fffffff;
		b = b - (b >> 5);
		a |= b;
		*dest = RGB32k.All[a & (a>>15)];
		dest += pitch; 
	} while (--count);

}

void R_FillSubClampColumn (void)
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
	fg = dc_srccolor | 0x40100400;
	int pitch = dc_pitch;

	do
	{
		DWORD a = fg - bg2rgb[*dest];
		DWORD b = a;

		b &= 0x40100400;
		b = b - (b >> 5);
		a &= b;
		a |= 0x01f07c1f;
		*dest = RGB32k.All[a & (a>>15)];
		dest += pitch; 
	} while (--count);

}

void R_FillRevSubClampColumn (void)
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
	fg = dc_srccolor;
	int pitch = dc_pitch;

	do
	{
		DWORD a = (bg2rgb[*dest] | 0x40100400) - fg;
		DWORD b = a;

		b &= 0x40100400;
		b = b - (b >> 5);
		a &= b;
		a |= 0x01f07c1f;
		*dest = RGB32k.All[a & (a>>15)];
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

#ifndef X86_ASM
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
#endif

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
			*dest = RGB32k.All[fg & (fg>>15)];
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
			*dest = RGB32k.All[fg & (fg>>15)];
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
			*dest = RGB32k.All[val & (val>>15)];

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
			DWORD a = fg2rgb[colormap[source[frac>>FRACBITS]]] + bg2rgb[*dest];
			DWORD b = a;

			a |= 0x01f07c1f;
			b &= 0x40100400;
			a &= 0x3fffffff;
			b = b - (b >> 5);
			a |= b;
			*dest = RGB32k.All[a & (a>>15)];
			dest += pitch;
			frac += fracstep;
		} while (--count);
	}
}

// Add translated source to destination, clamping it to white
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
			DWORD a = fg2rgb[colormap[translation[source[frac>>FRACBITS]]]] + bg2rgb[*dest];
			DWORD b = a;

			a |= 0x01f07c1f;
			b &= 0x40100400;
			a &= 0x3fffffff;
			b = b - (b >> 5);
			a |= b;
			*dest = RGB32k.All[(a>>15) & a];
			dest += pitch;
			frac += fracstep;
		} while (--count);
	}
}

// Subtract destination from source, clamping it to black
void R_DrawSubClampColumnP_C ()
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
			DWORD a = (fg2rgb[colormap[source[frac>>FRACBITS]]] | 0x40100400) - bg2rgb[*dest];
			DWORD b = a;

			b &= 0x40100400;
			b = b - (b >> 5);
			a &= b;
			a |= 0x01f07c1f;
			*dest = RGB32k.All[a & (a>>15)];
			dest += pitch;
			frac += fracstep;
		} while (--count);
	}
}

// Subtract destination from source, clamping it to black
void R_DrawSubClampTranslatedColumnP_C ()
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
			DWORD a = (fg2rgb[colormap[translation[source[frac>>FRACBITS]]]] | 0x40100400) - bg2rgb[*dest];
			DWORD b = a;

			b &= 0x40100400;
			b = b - (b >> 5);
			a &= b;
			a |= 0x01f07c1f;
			*dest = RGB32k.All[(a>>15) & a];
			dest += pitch;
			frac += fracstep;
		} while (--count);
	}
}

// Subtract source from destination, clamping it to black
void R_DrawRevSubClampColumnP_C ()
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
			DWORD a = (bg2rgb[*dest] | 0x40100400) - fg2rgb[colormap[source[frac>>FRACBITS]]];
			DWORD b = a;

			b &= 0x40100400;
			b = b - (b >> 5);
			a &= b;
			a |= 0x01f07c1f;
			*dest = RGB32k.All[a & (a>>15)];
			dest += pitch;
			frac += fracstep;
		} while (--count);
	}
}

// Subtract source from destination, clamping it to black
void R_DrawRevSubClampTranslatedColumnP_C ()
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
			DWORD a = (bg2rgb[*dest] | 0x40100400) - fg2rgb[colormap[translation[source[frac>>FRACBITS]]]];
			DWORD b = a;

			b &= 0x40100400;
			b = b - (b >> 5);
			a &= b;
			a |= 0x01f07c1f;
			*dest = RGB32k.All[(a>>15) & a];
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

#ifdef X86_ASM
extern "C" void R_SetSpanSource_ASM (const BYTE *flat);
extern "C" void R_SetSpanSize_ASM (int xbits, int ybits);
extern "C" void R_SetSpanColormap_ASM (BYTE *colormap);
extern "C" BYTE *ds_curcolormap, *ds_cursource, *ds_curtiltedsource;
#endif
}

//==========================================================================
//
// R_SetSpanSource
//
// Sets the source bitmap for the span drawing routines.
//
//==========================================================================

void R_SetSpanSource(const BYTE *pixels)
{
	ds_source = pixels;
#ifdef X86_ASM
	if (ds_cursource != ds_source)
	{
		R_SetSpanSource_ASM(pixels);
	}
#endif
}

//==========================================================================
//
// R_SetSpanColormap
//
// Sets the colormap for the span drawing routines.
//
//==========================================================================

void R_SetSpanColormap(BYTE *colormap)
{
	ds_colormap = colormap;
#ifdef X86_ASM
	if (ds_colormap != ds_curcolormap)
	{
		R_SetSpanColormap_ASM (ds_colormap);
	}
#endif
}

//==========================================================================
//
// R_SetupSpanBits
//
// Sets the texture size for the span drawing routines.
//
//==========================================================================

void R_SetupSpanBits(FTexture *tex)
{
	tex->GetWidth ();
	ds_xbits = tex->WidthBits;
	ds_ybits = tex->HeightBits;
	if ((1 << ds_xbits) > tex->GetWidth())
	{
		ds_xbits--;
	}
	if ((1 << ds_ybits) > tex->GetHeight())
	{
		ds_ybits--;
	}
#ifdef X86_ASM
	R_SetSpanSize_ASM (ds_xbits, ds_ybits);
#endif
}

//
// Draws the actual span.
#ifndef X86_ASM
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
			*dest++ = RGB32k.All[fg & (fg>>15)];
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
			*dest++ = RGB32k.All[fg & (fg>>15)];
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
				*dest = RGB32k.All[fg & (fg>>15)];
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
				*dest = RGB32k.All[fg & (fg>>15)];
			}
			dest++;
			xfrac += xstep;
			yfrac += ystep;
		} while (--count);
	}
}

void R_DrawSpanAddClampP_C (void)
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
			DWORD a = fg2rgb[colormap[source[spot]]] + bg2rgb[*dest];
			DWORD b = a;

			a |= 0x01f07c1f;
			b &= 0x40100400;
			a &= 0x3fffffff;
			b = b - (b >> 5);
			a |= b;
			*dest++ = RGB32k.All[a & (a>>15)];
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
			DWORD a = fg2rgb[colormap[source[spot]]] + bg2rgb[*dest];
			DWORD b = a;

			a |= 0x01f07c1f;
			b &= 0x40100400;
			a &= 0x3fffffff;
			b = b - (b >> 5);
			a |= b;
			*dest++ = RGB32k.All[a & (a>>15)];
			xfrac += xstep;
			yfrac += ystep;
		} while (--count);
	}
}

void R_DrawSpanMaskedAddClampP_C (void)
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
				DWORD a = fg2rgb[colormap[texdata]] + bg2rgb[*dest];
				DWORD b = a;

				a |= 0x01f07c1f;
				b &= 0x40100400;
				a &= 0x3fffffff;
				b = b - (b >> 5);
				a |= b;
				*dest = RGB32k.All[a & (a>>15)];
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
				DWORD a = fg2rgb[colormap[texdata]] + bg2rgb[*dest];
				DWORD b = a;

				a |= 0x01f07c1f;
				b &= 0x40100400;
				a &= 0x3fffffff;
				b = b - (b >> 5);
				a |= b;
				*dest = RGB32k.All[a & (a>>15)];
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

// Draw a voxel slab
//
// "Build Engine & Tools" Copyright (c) 1993-1997 Ken Silverman
// Ken Silverman's official web site: "http://www.advsys.net/ken"
// See the included license file "BUILDLIC.TXT" for license info.

// Actually, this is just R_DrawColumn with an extra width parameter.

#ifndef X86_ASM
static const BYTE *slabcolormap;

extern "C" void R_SetupDrawSlabC(const BYTE *colormap)
{
	slabcolormap = colormap;
}

extern "C" void R_DrawSlabC(int dx, fixed_t v, int dy, fixed_t vi, const BYTE *vptr, BYTE *p)
{
	int x;
	const BYTE *colormap = slabcolormap;
	int pitch = dc_pitch;

	assert(dx > 0);

	if (dx == 1)
	{
		while (dy > 0)
		{
			*p = colormap[vptr[v >> FRACBITS]];
			p += pitch;
			v += vi;
			dy--;
		}
	}
	else if (dx == 2)
	{
		while (dy > 0)
		{
			BYTE color = colormap[vptr[v >> FRACBITS]];
			p[0] = color;
			p[1] = color;
			p += pitch;
			v += vi;
			dy--;
		}
	}
	else if (dx == 3)
	{
		while (dy > 0)
		{
			BYTE color = colormap[vptr[v >> FRACBITS]];
			p[0] = color;
			p[1] = color;
			p[2] = color;
			p += pitch;
			v += vi;
			dy--;
		}
	}
	else if (dx == 4)
	{
		while (dy > 0)
		{
			BYTE color = colormap[vptr[v >> FRACBITS]];
			p[0] = color;
			p[1] = color;
			p[2] = color;
			p[3] = color;
			p += pitch;
			v += vi;
			dy--;
		}
	}
	else while (dy > 0)
	{
		BYTE color = colormap[vptr[v >> FRACBITS]];
		// The optimizer will probably turn this into a memset call.
		// Since dx is not likely to be large, I'm not sure that's a good thing,
		// hence the alternatives above.
		for (x = 0; x < dx; x++)
		{
			p[x] = color;
		}
		p += pitch;
		v += vi;
		dy--;
	}
}
#endif


/****************************************************/
/****************************************************/

// wallscan stuff, in C

#ifndef X86_ASM
static DWORD vlinec1 ();
static int vlinebits;

DWORD (*dovline1)() = vlinec1;
DWORD (*doprevline1)() = vlinec1;

#ifdef X64_ASM
extern "C" void vlinetallasm4();
#define dovline4 vlinetallasm4
extern "C" void setupvlinetallasm (int);
#else
static void vlinec4 ();
void (*dovline4)() = vlinec4;
#endif

static DWORD mvlinec1();
static void mvlinec4();
static int mvlinebits;

DWORD (*domvline1)() = mvlinec1;
void (*domvline4)() = mvlinec4;

#else

extern "C"
{
DWORD vlineasm1 ();
DWORD prevlineasm1 ();
DWORD vlinetallasm1 ();
DWORD prevlinetallasm1 ();
void vlineasm4 ();
void vlinetallasmathlon4 ();
void vlinetallasm4 ();
void setupvlineasm (int);
void setupvlinetallasm (int);

DWORD mvlineasm1();
void mvlineasm4();
void setupmvlineasm (int);
}

DWORD (*dovline1)() = vlinetallasm1;
DWORD (*doprevline1)() = prevlinetallasm1;
void (*dovline4)() = vlinetallasm4;

DWORD (*domvline1)() = mvlineasm1;
void (*domvline4)() = mvlineasm4;
#endif

void setupvline (int fracbits)
{
#ifdef X86_ASM
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
#ifdef X64_ASM
	setupvlinetallasm(fracbits);
#endif
#endif
}

#if !defined(X86_ASM)
DWORD vlinec1 ()
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

void vlinec4 ()
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
#if defined(X86_ASM)
	setupmvlineasm (fracbits);
	domvline1 = mvlineasm1;
	domvline4 = mvlineasm4;
#else
	mvlinebits = fracbits;
#endif
}

#if !defined(X86_ASM)
DWORD mvlinec1 ()
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

void mvlinec4 ()
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
extern float rw_light;
extern float rw_lightstep;
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

	double lightstep = rw_lightstep;
	double light = rw_light + rw_lightstep*(x2-x1-1);
	int x = x2-1;
	int t2 = uclip[x];
	int b2 = dclip[x];
	int rcolormap = GETPALOOKUP(light, wallshade);
	int lcolormap;
	BYTE *basecolormapdata = basecolormap->Maps;

	if (b2 > t2)
	{
		clearbufshort (spanend+t2, b2-t2, x);
	}

	dc_colormap = basecolormapdata + (rcolormap << COLORMAPSHIFT);

	for (--x; x >= x1; --x)
	{
		int t1 = uclip[x];
		int b1 = dclip[x];
		const int xr = x+1;
		int stop;

		light -= rw_lightstep;
		lcolormap = GETPALOOKUP(light, wallshade);
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
			dc_colormap = basecolormapdata + (lcolormap << COLORMAPSHIFT);
		}
		else
		{
			if (dc_colormap != basecolormapdata)
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
			*dest = RGB32k.All[fg & (fg>>15)];
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
				dest[i] = RGB32k.All[fg & (fg>>15)];
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
			*dest = RGB32k.All[a & (a>>15)];
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
				dest[i] = RGB32k.All[a & (a>>15)];
			}
			vplce[i] += vince[i];
		}
		dest += dc_pitch;
	} while (--count);
}

fixed_t tmvline1_subclamp ()
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
			DWORD a = (fg2rgb[colormap[pix]] | 0x40100400) - bg2rgb[*dest];
			DWORD b = a;

			b &= 0x40100400;
			b = b - (b >> 5);
			a &= b;
			a |= 0x01f07c1f;
			*dest = RGB32k.All[a & (a>>15)];
		}
		frac += fracstep;
		dest += pitch;
	} while (--count);

	return frac;
}

void tmvline4_subclamp ()
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
				DWORD a = (fg2rgb[palookupoffse[i][pix]] | 0x40100400) - bg2rgb[dest[i]];
				DWORD b = a;

				b &= 0x40100400;
				b = b - (b >> 5);
				a &= b;
				a |= 0x01f07c1f;
				dest[i] = RGB32k.All[a & (a>>15)];
			}
			vplce[i] += vince[i];
		}
		dest += dc_pitch;
	} while (--count);
}

fixed_t tmvline1_revsubclamp ()
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
			DWORD a = (bg2rgb[*dest] | 0x40100400) - fg2rgb[colormap[pix]];
			DWORD b = a;

			b &= 0x40100400;
			b = b - (b >> 5);
			a &= b;
			a |= 0x01f07c1f;
			*dest = RGB32k.All[a & (a>>15)];
		}
		frac += fracstep;
		dest += pitch;
	} while (--count);

	return frac;
}

void tmvline4_revsubclamp ()
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
				DWORD a = (bg2rgb[dest[i]] | 0x40100400) - fg2rgb[palookupoffse[i][pix]];
				DWORD b = a;

				b &= 0x40100400;
				b = b - (b >> 5);
				a &= b;
				a |= 0x01f07c1f;
				dest[i] = RGB32k.All[a & (a>>15)];
			}
			vplce[i] += vince[i];
		}
		dest += dc_pitch;
	} while (--count);
}


//==========================================================================
//
// R_GetColumn
//
//==========================================================================

const BYTE *R_GetColumn (FTexture *tex, int col)
{
	int width;

	// If the texture's width isn't a power of 2, then we need to make it a
	// positive offset for proper clamping.
	if (col < 0 && (width = tex->GetWidth()) != (1 << tex->WidthBits))
	{
		col = width + (col % width);
	}
	return tex->GetColumn (col, NULL);
}


// [RH] Initialize the column drawer pointers
void R_InitColumnDrawers ()
{
#ifdef X86_ASM
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
	R_DrawSpanAddClamp			= R_DrawSpanAddClampP_C;
	R_DrawSpanMaskedAddClamp	= R_DrawSpanMaskedAddClampP_C;
}

// [RH] Choose column drawers in a single place
EXTERN_CVAR (Int, r_drawfuzz)
EXTERN_CVAR (Bool, r_drawtrans)
EXTERN_CVAR (Float, transsouls)

static FDynamicColormap *basecolormapsave;

static bool R_SetBlendFunc (int op, fixed_t fglevel, fixed_t bglevel, int flags)
{
	// r_drawtrans is a seriously bad thing to turn off. I wonder if I should
	// just remove it completely.
	if (!r_drawtrans || (op == STYLEOP_Add && fglevel == FRACUNIT && bglevel == 0 && !(flags & STYLEF_InvertSource)))
	{
		if (flags & STYLEF_ColorIsFixed)
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
	if (flags & STYLEF_InvertSource)
	{
		dc_srcblend = Col2RGB8_Inverse[fglevel>>10];
		dc_destblend = Col2RGB8_LessPrecision[bglevel>>10];
	}
	else if (op == STYLEOP_Add && fglevel + bglevel <= FRACUNIT)
	{
		dc_srcblend = Col2RGB8[fglevel>>10];
		dc_destblend = Col2RGB8[bglevel>>10];
	}
	else
	{
		dc_srcblend = Col2RGB8_LessPrecision[fglevel>>10];
		dc_destblend = Col2RGB8_LessPrecision[bglevel>>10];
	}
	switch (op)
	{
	case STYLEOP_Add:
		if (fglevel == 0 && bglevel == FRACUNIT)
		{
			return false;
		}
		if (fglevel + bglevel <= FRACUNIT)
		{ // Colors won't overflow when added
			if (flags & STYLEF_ColorIsFixed)
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
			if (flags & STYLEF_ColorIsFixed)
			{
				colfunc = R_FillAddClampColumn;
				hcolfunc_post1 = rt_addclamp1col;
				hcolfunc_post4 = rt_addclamp4cols;
			}
			else if (dc_translation == NULL)
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

	case STYLEOP_Sub:
		if (flags & STYLEF_ColorIsFixed)
		{
			colfunc = R_FillSubClampColumn;
			hcolfunc_post1 = rt_subclamp1col;
			hcolfunc_post4 = rt_subclamp4cols;
		}
		else if (dc_translation == NULL)
		{
			colfunc = R_DrawSubClampColumnP_C;
			hcolfunc_post1 = rt_subclamp1col;
			hcolfunc_post4 = rt_subclamp4cols;
		}
		else
		{
			colfunc = R_DrawSubClampTranslatedColumnP_C;
			hcolfunc_post1 = rt_tlatesubclamp1col;
			hcolfunc_post4 = rt_tlatesubclamp4cols;
		}
		return true;

	case STYLEOP_RevSub:
		if (fglevel == 0 && bglevel == FRACUNIT)
		{
			return false;
		}
		if (flags & STYLEF_ColorIsFixed)
		{
			colfunc = R_FillRevSubClampColumn;
			hcolfunc_post1 = rt_subclamp1col;
			hcolfunc_post4 = rt_subclamp4cols;
		}
		else if (dc_translation == NULL)
		{
			colfunc = R_DrawRevSubClampColumnP_C;
			hcolfunc_post1 = rt_revsubclamp1col;
			hcolfunc_post4 = rt_revsubclamp4cols;
		}
		else
		{
			colfunc = R_DrawRevSubClampTranslatedColumnP_C;
			hcolfunc_post1 = rt_tlaterevsubclamp1col;
			hcolfunc_post4 = rt_tlaterevsubclamp4cols;
		}
		return true;

	default:
		return false;
	}
}

static fixed_t GetAlpha(int type, fixed_t alpha)
{
	switch (type)
	{
	case STYLEALPHA_Zero:		return 0;
	case STYLEALPHA_One:		return OPAQUE;
	case STYLEALPHA_Src:		return alpha;
	case STYLEALPHA_InvSrc:		return OPAQUE - alpha;
	default:					return 0;
	}
}

ESPSResult R_SetPatchStyle (FRenderStyle style, fixed_t alpha, int translation, DWORD color)
{
	fixed_t fglevel, bglevel;

	style.CheckFuzz();

	if (style.BlendOp == STYLEOP_Shadow)
	{
		style = LegacyRenderStyles[STYLE_TranslucentStencil];
		alpha = TRANSLUC33;
		color = 0;
	}

	if (style.Flags & STYLEF_TransSoulsAlpha)
	{
		alpha = fixed_t(transsouls * OPAQUE);
	}
	else if (style.Flags & STYLEF_Alpha1)
	{
		alpha = FRACUNIT;
	}
	else
	{
		alpha = clamp<fixed_t> (alpha, 0, OPAQUE);
	}

	dc_translation = NULL;
	if (translation != 0)
	{
		FRemapTable *table = TranslationToTable(translation);
		if (table != NULL && !table->Inactive)
		{
			dc_translation = table->Remap;
		}
	}
	basecolormapsave = basecolormap;
	hcolfunc_pre = R_DrawColumnHoriz;

	// Check for special modes
	if (style.BlendOp == STYLEOP_Fuzz)
	{
		colfunc = fuzzcolfunc;
		return DoDraw0;
	}
	else if (style == LegacyRenderStyles[STYLE_Shaded])
	{
		// Shaded drawer only gets 16 levels of alpha because it saves memory.
		if ((alpha >>= 12) == 0)
			return DontDraw;
		colfunc = R_DrawShadedColumn;
		hcolfunc_post1 = rt_shaded1col;
		hcolfunc_post4 = rt_shaded4cols;
		dc_color = fixedcolormap ? fixedcolormap[APART(color)] : basecolormap->Maps[APART(color)];
		dc_colormap = (basecolormap = &ShadeFakeColormap[16-alpha])->Maps;
		if (fixedlightlev >= 0 && fixedcolormap == NULL)
		{
			dc_colormap += fixedlightlev;
		}
		return r_columnmethod ? DoDraw1 : DoDraw0;
	}

	fglevel = GetAlpha(style.SrcAlpha, alpha);
	bglevel = GetAlpha(style.DestAlpha, alpha);

	if (style.Flags & STYLEF_ColorIsFixed)
	{
		int x = fglevel >> 10;
		int r = RPART(color);
		int g = GPART(color);
		int b = BPART(color);
		// dc_color is used by the rt_* routines. It is indexed into dc_srcblend.
		dc_color = RGB32k.RGB[r>>3][g>>3][b>>3];
		if (style.Flags & STYLEF_InvertSource)
		{
			r = 255 - r;
			g = 255 - g;
			b = 255 - b;
		}
		// dc_srccolor is used by the R_Fill* routines. It is premultiplied
		// with the alpha.
		dc_srccolor = ((((r*x)>>4)<<20) | ((g*x)>>4) | ((((b)*x)>>4)<<10)) & 0x3feffbff;
		hcolfunc_pre = R_FillColumnHorizP;
		dc_colormap = identitymap;
	}

	if (!R_SetBlendFunc (style.BlendOp, fglevel, bglevel, style.Flags))
	{
		return DontDraw;
	}
	return r_columnmethod ? DoDraw1 : DoDraw0;
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
	if (colfunc == R_DrawSubClampColumnP_C)
	{
		*tmvline1 = tmvline1_subclamp;
		*tmvline4 = tmvline4_subclamp;
		return true;
	}
	if (colfunc == R_DrawRevSubClampColumnP_C)
	{
		*tmvline1 = tmvline1_revsubclamp;
		*tmvline4 = tmvline4_revsubclamp;
		return true;
	}
	return false;
}

