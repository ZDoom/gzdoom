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
#include "z_zone.h"
#include "w_wad.h"
#include "r_local.h"
#include "v_video.h"
#include "doomstat.h"
#include "st_stuff.h"

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


byte*			viewimage;
extern "C" {
int 			viewwidth;
int				halfviewwidth;
int 			viewheight;
}
int 			scaledviewwidth;
int 			viewwindowx;
int 			viewwindowy;
byte*			ylookup[MAXHEIGHT];

extern "C" {
int				realviewwidth;		// [RH] Physical width of view window
int				realviewheight;		// [RH] Physical height of view window
int				detailxshift;		// [RH] X shift for horizontal detail level
int				detailyshift;		// [RH] Y shift for vertical detail level
}

#ifdef USEASM
extern "C" void STACK_ARGS DoubleHoriz_MMX (int height, int width, byte *dest, int pitch);
extern "C" void STACK_ARGS DoubleHorizVert_MMX (int height, int width, byte *dest, int pitch);
extern "C" void STACK_ARGS DoubleVert_ASM (int height, int width, byte *dest, int pitch);
#endif

// [RH] Pointers to the different column drawers.
//		These get changed depending on the current
//		screen depth.
void (*R_DrawColumnHoriz)(void);
void (*R_DrawColumn)(void);
void (*R_DrawFuzzColumn)(void);
void (*R_DrawTranslatedColumn)(void);
void (*R_DrawShadedColumn)(void);
void (*R_DrawSpan)(void);
void (*rt_map4cols)(int,int,int);


//
// R_DrawColumn
// Source is the top of the column to scale.
//
extern "C" {
int				dc_pitch=0x12345678;	// [RH] Distance between rows

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
byte*			dc_source;				

// [RH] Tutti-Frutti fix
unsigned int	dc_mask;

// just for profiling 
int 			dccount;
}

cycle_t			DetailDoubleCycles;

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
	byte*				dest;
	fixed_t 			frac;
	fixed_t 			fracstep;

	count = dc_yh - dc_yl;

	// Zero length, column does not exceed a pixel.
	if (count < 0)
		return;

	count++;

#ifdef RANGECHECK 
	if (dc_x >= screen->width
		|| dc_yl < 0
		|| dc_yh >= screen->height)
	{
		Printf ("R_DrawColumnP_C: %i to %i at %i\n", dc_yl, dc_yh, dc_x);
		return;
	}
#endif

	// Framebuffer destination address.
	// Use ylookup LUT to avoid multiply with ScreenWidth.
	dest = ylookup[dc_yl] + dc_x;

	// Determine scaling,
	//	which is the only mapping to be done.
	fracstep = dc_iscale; 
	frac = dc_texturefrac;

	{
		// [RH] Get local copies of these variables so that the compiler
		//		has a better chance of optimizing this well.
		byte *colormap = dc_colormap;
		int mask = dc_mask;
		byte *source = dc_source;
		int pitch = dc_pitch;

		// Inner loop that does the actual texture mapping,
		//	e.g. a DDA-lile scaling.
		// This is as fast as it gets.
		do
		{
			// Re-map color indices from wall texture column
			//	using a lighting/special effects LUT.
			*dest = colormap[source[(frac>>FRACBITS)&mask]];

			dest += pitch;
			frac += fracstep;

		} while (--count);
	}
} 
#endif	// USEASM


// [RH] Same as R_DrawColumnP_C except that it doesn't do any colormapping.
//		Used by the sky drawer because the sky is always fullbright.
void R_StretchColumnP_C (void)
{
	int 				count;
	byte*				dest;
	fixed_t 			frac;
	fixed_t 			fracstep;

	count = dc_yh - dc_yl;

	if (count < 0)
		return;

	count++;

#ifdef RANGECHECK 
	if (dc_x >= screen->width
		|| dc_yl < 0
		|| dc_yh >= screen->height)
	{
		Printf ("R_StretchColumnP_C: %i to %i at %i\n", dc_yl, dc_yh, dc_x);
		return;
	}
#endif

	dest = ylookup[dc_yl] + dc_x;
	fracstep = dc_iscale; 
	frac = dc_texturefrac;

	{
		int mask = dc_mask;
		byte *source = dc_source;
		int pitch = dc_pitch;

		do
		{
			*dest = source[(frac>>FRACBITS)&mask];
			dest += pitch;
			frac += fracstep;
		} while (--count);
	}
} 

// [RH] Just fills a column with a color
void R_FillColumnP (void)
{
	int 				count;
	byte*				dest;

	count = dc_yh - dc_yl;

	if (count < 0)
		return;

	count++;

#ifdef RANGECHECK 
	if (dc_x >= screen->width
		|| dc_yl < 0
		|| dc_yh >= screen->height)
	{
		Printf ("R_StretchColumnP_C: %i to %i at %i\n", dc_yl, dc_yh, dc_x);
		return;
	}
#endif

	dest = ylookup[dc_yl] + dc_x;

	{
		int pitch = dc_pitch;
		byte color = dc_color;

		do
		{
			*dest = color;
			dest += pitch;
		} while (--count);
	}
} 

//
// Spectre/Invisibility.
//
// [RH] FUZZTABLE changed from 50 to 64
#define FUZZTABLE	50

extern "C"
{
int 	fuzzoffset[FUZZTABLE+1];	// [RH] +1 for the assembly routine
int 	fuzzpos = 0; 
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
	byte *dest;

	// Adjust borders. Low...
	if (!dc_yl)
		dc_yl = 1;

	// .. and high.
	if (dc_yh == realviewheight-1)
		dc_yh = realviewheight - 2;

	count = dc_yh - dc_yl;

	// Zero length.
	if (count < 0)
		return;

	count++;

#ifdef RANGECHECK
	if (dc_x >= screen->width
		|| dc_yl < 0 || dc_yh >= screen->height)
	{
		I_Error ("R_DrawFuzzColumnP_C: %i to %i at %i",
				 dc_yl, dc_yh, dc_x);
	}
#endif


	dest = ylookup[dc_yl] + dc_x;

	// colormap #6 is used for shading (of 0-31, a bit brighter than average)
	{
		// [RH] Make local copies of global vars to try and improve
		//		the optimizations made by the compiler.
		int pitch = dc_pitch;
		int fuzz = fuzzpos;
		int cnt;
		byte *map = &NormalLight.Maps[6*256];

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
	byte *dest;
	fixed_t frac;
	fixed_t fracstep;

	count = dc_yh - dc_yl;
	if (count < 0)
		return;
	count++;

#ifdef RANGECHECK
	if (dc_x >= screen->width
		|| dc_yl < 0
		|| dc_yh >= screen->height)
	{
		I_Error ( "R_DrawTranslucentColumnP_C: %i to %i at %i",
				  dc_yl, dc_yh, dc_x);
	}
#endif 

	dest = ylookup[dc_yl] + dc_x;

	fracstep = dc_iscale;
	frac = dc_texturefrac;

	{
		DWORD *fg2rgb = dc_srcblend;
		DWORD *bg2rgb = dc_destblend;
		byte *colormap = dc_colormap;
		byte *source = dc_source;
		int mask = dc_mask;
		int pitch = dc_pitch;

		do
		{
			DWORD fg = colormap[source[(frac>>FRACBITS)&mask]];
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
// Used to draw player sprites
//	with the green colorramp mapped to others.
// Could be used with different translation
//	tables, e.g. the lighter colored version
//	of the BaronOfHell, the HellKnight, uses
//	identical sprites, kinda brightened up.
//
byte*	dc_translation;
byte*	translationtables;

void R_DrawTranslatedColumnP_C (void)
{ 
	int 				count;
	byte*				dest;
	fixed_t 			frac;
	fixed_t 			fracstep;

	count = dc_yh - dc_yl;
	if (count < 0) 
		return;
	count++;

#ifdef RANGECHECK 
	if (dc_x >= screen->width
		|| dc_yl < 0
		|| dc_yh >= screen->height)
	{
		I_Error ( "R_DrawTranslatedColumnP_C: %i to %i at %i",
				  dc_yl, dc_yh, dc_x);
	}
	
#endif 

	dest = ylookup[dc_yl] + dc_x;

	fracstep = dc_iscale;
	frac = dc_texturefrac;

	{
		// [RH] Local copies of global vars to improve compiler optimizations
		byte *colormap = dc_colormap;
		byte *translation = dc_translation;
		byte *source = dc_source;
		int pitch = dc_pitch;
		int mask = dc_mask;

		do
		{
			*dest = colormap[translation[source[(frac>>FRACBITS) & mask]]];
			dest += pitch;

			frac += fracstep;
		} while (--count);
	}
}

// Draw a column that is both translated and translucent
void R_DrawTlatedAddColumnP_C (void)
{
	int count;
	byte *dest;
	fixed_t frac;
	fixed_t fracstep;

	count = dc_yh - dc_yl;
	if (count < 0)
		return;
	count++;

#ifdef RANGECHECK
	if (dc_x >= screen->width
		|| dc_yl < 0
		|| dc_yh >= screen->height)
	{
		I_Error ( "R_DrawTlatedLucentColumnP_C: %i to %i at %i",
				  dc_yl, dc_yh, dc_x);
	}
	
#endif 

	dest = ylookup[dc_yl] + dc_x;

	fracstep = dc_iscale;
	frac = dc_texturefrac;

	{
		DWORD *fg2rgb = dc_srcblend;
		DWORD *bg2rgb = dc_destblend;
		byte *translation = dc_translation;
		byte *colormap = dc_colormap;
		byte *source = dc_source;
		int mask = dc_mask;
		int pitch = dc_pitch;

		do
		{
			DWORD fg = colormap[translation[source[(frac>>FRACBITS)&mask]]];
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
	byte *dest;
	fixed_t frac, fracstep;

	count = dc_yh - dc_yl;

	if (count < 0)
		return;

	count++;

#ifdef RANGECHECK 
	if (dc_x >= screen->width
		|| dc_yl < 0
		|| dc_yh >= screen->height) {
		Printf ("R_DrawShadedColumnP_C: %i to %i at %i\n", dc_yl, dc_yh, dc_x);
		return;
	}
#endif

	dest = ylookup[dc_yl] + dc_x;

	fracstep = dc_iscale; 
	frac = dc_texturefrac;

	{
		int mask = dc_mask;
		byte *source = dc_source;
		byte *colormap = dc_colormap;
		int pitch = dc_pitch;
		DWORD *fgstart = &Col2RGB8[0][dc_color];

		do
		{
			DWORD val = colormap[source[(frac>>FRACBITS)&mask]];
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
	byte *dest;
	fixed_t frac;
	fixed_t fracstep;

	count = dc_yh - dc_yl;
	if (count < 0)
		return;
	count++;

#ifdef RANGECHECK
	if (dc_x >= screen->width
		|| dc_yl < 0
		|| dc_yh >= screen->height)
	{
		I_Error ( "R_DrawAddColumnP_C: %i to %i at %i",
				  dc_yl, dc_yh, dc_x);
	}
#endif 

	dest = ylookup[dc_yl] + dc_x;

	fracstep = dc_iscale;
	frac = dc_texturefrac;

	{
		byte *colormap = dc_colormap;
		byte *source = dc_source;
		int mask = dc_mask;
		int pitch = dc_pitch;
		DWORD *fg2rgb = dc_srcblend;
		DWORD *bg2rgb = dc_destblend;

		do
		{
			DWORD a = fg2rgb[colormap[source[(frac>>FRACBITS)&mask]]]
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
	byte *dest;
	fixed_t frac;
	fixed_t fracstep;

	count = dc_yh - dc_yl;
	if (count < 0)
		return;
	count++;

#ifdef RANGECHECK
	if (dc_x >= screen->width
		|| dc_yl < 0
		|| dc_yh >= screen->height)
	{
		I_Error ( "R_DrawAddColumnP_C: %i to %i at %i",
				  dc_yl, dc_yh, dc_x);
	}
#endif 

	dest = ylookup[dc_yl] + dc_x;

	fracstep = dc_iscale;
	frac = dc_texturefrac;

	{
		byte *translation = dc_translation;
		byte *colormap = dc_colormap;
		byte *source = dc_source;
		int mask = dc_mask;
		int pitch = dc_pitch;
		DWORD *fg2rgb = dc_srcblend;
		DWORD *bg2rgb = dc_destblend;

		do
		{
			DWORD a = fg2rgb[colormap[translation[source[(frac>>FRACBITS)&mask]]]]
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

// start of a 64*64 tile image 
byte*					ds_source;		

// just for profiling
int 					dscount;
}

//
// Draws the actual span.
#ifndef USEASM
void R_DrawSpanP_C (void)
{
	dsfixed_t			xfrac;
	dsfixed_t			yfrac;
	dsfixed_t			xstep;
	dsfixed_t			ystep;
	byte*				dest;
	int 				count;
	int 				spot;

#ifdef RANGECHECK 
	if (ds_x2 < ds_x1
		|| ds_x1<0
		|| ds_x2>=screen->width
		|| ds_y>screen->height)
	{
		I_Error ("R_DrawSpan: %i to %i at %i",
				 ds_x1,ds_x2,ds_y);
	}
//		dscount++;
#endif

	
	xfrac = ds_xfrac;
	yfrac = ds_yfrac;

	dest = ylookup[ds_y] + ds_x1;

	// We do not check for zero spans here?
	count = ds_x2 - ds_x1 + 1;

	xstep = ds_xstep;
	ystep = ds_ystep;

	do {
		// Current texture index in u,v.
		spot = ((yfrac>>(32-6-6))&(63*64)) + (xfrac>>(32-6));

		// Lookup pixel from flat texture tile,
		//  re-index using light/colormap.
		*dest++ = ds_colormap[ds_source[spot]];

		// Next step in u,v.
		xfrac += xstep;
		yfrac += ystep;
	} while (--count);
}
#endif
// [RH] Just fill a span with a color
void R_FillSpan (void)
{
#ifdef RANGECHECK
	if (ds_x2 < ds_x1
		|| ds_x1<0
		|| ds_x2>=screen->width
		|| ds_y>screen->height)
	{
		I_Error( "R_FillSpan: %i to %i at %i",
				 ds_x1,ds_x2,ds_y);
	}
//		dscount++;
#endif

	memset (ylookup[ds_y] + ds_x1, ds_color, ds_x2 - ds_x1 + 1);
}

/****************************************************/
/****************************************************/

//
// R_InitTranslationTables
// Creates the translation tables to map
//	the green color ramp to gray, brown, red.
// Assumes a given structure of the PLAYPAL.
// Could be read from a lump instead.
//
void R_InitTranslationTables ()
{
	int i, j;
		
	translationtables = (byte *)Z_Malloc (256*(2*MAXPLAYERS+3+
		NUMCOLORMAPS*16)+255, PU_STATIC, NULL);
	translationtables = (byte *)(((ptrdiff_t)translationtables + 255) & ~255);
	
	// [RH] Each player now gets their own translation table. These are set
	//		up during netgame arbitration and as-needed rather than in here.

	for (i = 0; i < 256; i++)
		translationtables[i] = i;

	for (i = 1; i < MAXPLAYERS*2+3; i++)
		memcpy (translationtables + i*256, translationtables, 256);

	// create translation tables for dehacked patches that expect them
	if (gameinfo.gametype == GAME_Doom)
	{
		for (i = 0x70; i < 0x80; i++)
		{ // map green ramp to gray, brown, red
			translationtables[i+(MAXPLAYERS*2+0)*256] = 0x60 + (i&0xf);
			translationtables[i+(MAXPLAYERS*2+1)*256] = 0x40 + (i&0xf);
			translationtables[i+(MAXPLAYERS*2+2)*256] = 0x20 + (i&0xf);
		}
	}
	else if (gameinfo.gametype == GAME_Heretic)
	{
		for (i = 225; i <= 240; i++)
		{
			translationtables[i+(MAXPLAYERS*2+0)*256] = 114+(i-225); // yellow
			translationtables[i+(MAXPLAYERS*2+1)*256] = 145+(i-225); // red
			translationtables[i+(MAXPLAYERS*2+2)*256] = 190+(i-225); // blue
		}
	}

	// set up shading tables for shaded columns
	// 16 colormap sets, progressing from full alpha to minimum visible alpha

	BYTE *table = translationtables + (MAXPLAYERS*2+3)*256;

	// Full alpha
	for (i = 0; i < 16; ++i)
	{
		for (j = 0; j < NUMCOLORMAPS; ++j)
		{
			int a = (NUMCOLORMAPS - j) * (256 / NUMCOLORMAPS) * (16-i);
			for (int k = 0; k < 256; ++k)
			{
				BYTE v = ((k * a) + 256) >> 14;
				table[k] = MIN<BYTE> (v, 64);
			}
			table += 256;
		}
	}
}

// [RH] Create a player's translation table based on
//		a given mid-range color.
void R_BuildPlayerTranslation (int player, int color)
{
	byte *table = &translationtables[player*256*2];
	FPlayerSkin *skin = &skins[players[player].userinfo.skin];

	byte i;
	byte start = skin->range0start;
	byte end = skin->range0end;
	float r = (float)RPART(color) / 255.f;
	float g = (float)GPART(color) / 255.f;
	float b = (float)BPART(color) / 255.f;
	float h, s, v;
	float bases, basev;
	float sdelta, vdelta;
	float range;

	range = (float)(end-start+1);

	RGBtoHSV (r, g, b, &h, &s, &v);

	bases = s;
	basev = v;

	if (gameinfo.gametype == GAME_Doom)
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
		table += 256;
		bases = MIN (bases*1.3f, 1.f);
		basev = MIN (basev*1.3f, 1.f);
		for (i = 145; i <= 168; i++)
		{
			s = MIN (bases, 0.8965f - 0.0962f*(float)(i - 161));
			v = MIN (1.f, (0.2102f + 0.0489f*(float)(i - 144)) * basev);
			HSVtoRGB (&r, &g, &b, h, s, v);
			table[i] = ColorMatcher.Pick (
				clamp ((int)(r * 255.f), 0, 255),
				clamp ((int)(g * 255.f), 0, 255),
				clamp ((int)(b * 255.f), 0, 255));
		}
	}
}

void R_DrawBorder (int x1, int y1, int x2, int y2)
{
	int lump;

	lump = W_CheckNumForName (gameinfo.borderFlat, ns_flats);
	if (lump >= 0)
	{
		screen->FlatFill (x1 & ~63, y1, x2, y2,
			(byte *)W_CacheLumpNum (lump, PU_CACHE));
	}
	else
	{
		screen->Clear (x1, y1, x2, y2, 0);
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
		for (x = viewwindowx; x < viewwindowx + realviewwidth; x += size)
		{
			screen->DrawPatch ((patch_t *)W_CacheLumpName(gameinfo.border->t, PU_CACHE),
				x, viewwindowy - offset);
		}
		for (y = viewwindowy; y < 35; y += size)
		{
			screen->DrawPatch ((patch_t *)W_CacheLumpName (gameinfo.border->l, PU_CACHE),
				viewwindowx - offset, y);
			screen->DrawPatch ((patch_t *)W_CacheLumpName(gameinfo.border->r, PU_CACHE),
				viewwindowx + realviewwidth, y);
		}

		screen->DrawPatch ((patch_t *)W_CacheLumpName(gameinfo.border->tl, PU_CACHE),
			viewwindowx-offset, viewwindowy - offset);
		screen->DrawPatch ((patch_t *)W_CacheLumpName(gameinfo.border->tr, PU_CACHE),
			viewwindowx+realviewwidth, viewwindowy - offset);
	}
}


// [RH] Double pixels in the view window horizontally
//		and/or vertically (or not at all).
void R_DetailDouble ()
{
	DetailDoubleCycles = 0;
	clock (DetailDoubleCycles);

	switch ((detailxshift << 1) | detailyshift)
	{
	case 1:		// y-double
#ifdef USEASM
		DoubleVert_ASM (viewheight, viewwidth, ylookup[0], RenderTarget->GetPitch());
#else
		{
			int rowsize = realviewwidth;
			int pitch = RenderTarget->GetPitch();
			int y;
			byte *line;

			line = ylookup[0];
			for (y = viewheight; y != 0; --y, line += pitch<<1)
			{
				memcpy (line+pitch, line, rowsize);
			}
		}
#endif
		break;

	case 2:		// x-double
#ifdef USEASM
		if (UseMMX)
		{
			DoubleHoriz_MMX (viewheight, viewwidth, ylookup[0]+viewwidth, RenderTarget->GetPitch());
		}
		else
#endif
		{
			int rowsize = viewwidth;
			int pitch = RenderTarget->GetPitch();
			int y,x;
			byte *linefrom, *lineto;

			linefrom = ylookup[0];
			for (y = viewheight; y != 0; --y, linefrom += pitch)
			{
				lineto = linefrom - viewwidth;
				for (x = 0; x < rowsize; ++x)
				{
					byte c = linefrom[x];
					lineto[x*2] = c;
					lineto[x*2+1] = c;
				}
			}
		}
		break;

	case 3:		// x- and y-double
#ifdef USEASM
		if (UseMMX)
		{
			DoubleHorizVert_MMX (viewheight, viewwidth, ylookup[0]+viewwidth, RenderTarget->GetPitch());
		}
		else
#endif
		{
			int rowsize = viewwidth;
			int realpitch = RenderTarget->GetPitch();
			int pitch = realpitch << 1;
			int y,x;
			byte *linefrom, *lineto;

			linefrom = ylookup[0];
			for (y = viewheight; y != 0; --y, linefrom += pitch)
			{
				lineto = linefrom - viewwidth;
				for (x = 0; x < rowsize; ++x)
				{
					byte c = linefrom[x];
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

ADD_STAT(detail,out)
{
	sprintf (out, "doubling = %04.1f ms", (double)DetailDoubleCycles * 1000 * SecondsPerCycle);
}

// [RH] Initialize the column drawer pointers
void R_InitColumnDrawers ()
{
#ifdef USEASM
	R_DrawColumn			= R_DrawColumnP_ASM;
	R_DrawColumnHoriz		= R_DrawColumnHorizP_ASM;
	R_DrawFuzzColumn		= R_DrawFuzzColumnP_ASM;
	R_DrawTranslatedColumn	= R_DrawTranslatedColumnP_C;
	R_DrawShadedColumn		= R_DrawShadedColumnP_C;
	R_DrawSpan				= R_DrawSpanP_ASM;
	if (CPUFamily <= 5)
		rt_map4cols			= rt_map4cols_asm2;
	else
		rt_map4cols			= rt_map4cols_asm1;
#else
	R_DrawColumnHoriz		= R_DrawColumnHorizP_C;
	R_DrawColumn			= R_DrawColumnP_C;
	R_DrawFuzzColumn		= R_DrawFuzzColumnP_C;
	R_DrawTranslatedColumn	= R_DrawTranslatedColumnP_C;
	R_DrawShadedColumn		= R_DrawShadedColumnP_C;
	R_DrawSpan				= R_DrawSpanP_C;
	rt_map4cols				= rt_map4cols_c;
#endif
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

static bool R_SetBlendFunc (fixed_t fglevel, fixed_t bglevel)
{
	if (!r_drawtrans || (fglevel == BL_ONE && bglevel == BL_ZERO))
	{
		if (dc_translation == NULL)
		{
			colfunc = basecolfunc;
			hcolfunc_post1 = rt_map1col;
			hcolfunc_post2 = rt_map2cols;
			hcolfunc_post4 = rt_map4cols;
		}
		else
		{
			colfunc = transcolfunc;
			hcolfunc_post1 = rt_tlate1col;
			hcolfunc_post2 = rt_tlate2cols;
			hcolfunc_post4 = rt_tlate4cols;
		}
		r_MarkTrans = false;
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
		if (dc_translation == NULL)
		{
			colfunc = R_DrawAddColumnP_C;
			hcolfunc_post1 = rt_add1col;
			hcolfunc_post2 = rt_add2cols;
			hcolfunc_post4 = rt_add4cols;
		}
		else
		{
			colfunc = R_DrawTlatedAddColumnP_C;
			hcolfunc_post1 = rt_tlateadd1col;
			hcolfunc_post2 = rt_tlateadd2cols;
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
			hcolfunc_post2 = rt_addclamp2cols;
			hcolfunc_post4 = rt_addclamp4cols;
		}
		else
		{
			colfunc = R_DrawAddClampTranslatedColumnP_C;
			hcolfunc_post1 = rt_tlateaddclamp1col;
			hcolfunc_post2 = rt_tlateaddclamp2cols;
			hcolfunc_post4 = rt_tlateaddclamp4cols;
		}
	}
	r_MarkTrans = true;
	return true;
}

ESPSResult R_SetPatchStyle (int style, fixed_t alpha, BYTE *translation, DWORD color)
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

	dc_translation = translation;
	basecolormapsave = basecolormap;

	switch (style)
	{
		// Special modes
	case STYLE_Fuzzy:
		colfunc = fuzzcolfunc;
		r_MarkTrans = true;
		return DoDraw0;

	case STYLE_Shaded:
		// Shaded drawer only gets 16 levels because it saves memory.
		if ((alpha >>= 12) == 0)
			return DontDraw;
		colfunc = R_DrawShadedColumn;
		dc_color = fixedcolormap ? fixedcolormap[APART(color)] : basecolormap[APART(color)];
		dc_colormap = basecolormap = &translationtables[(MAXPLAYERS*2+3+(16-alpha)*NUMCOLORMAPS)*256];
		if (fixedlightlev)
		{
			dc_colormap += fixedlightlev;
		}
		hcolfunc_post1 = rt_shaded1col;
		hcolfunc_post2 = rt_shaded2cols;
		hcolfunc_post4 = rt_shaded4cols;
		r_MarkTrans = true;
		return r_columnmethod ? DoDraw1 : DoDraw0;

		// Standard modes
	case STYLE_Normal:
		fglevel = BL_ONE;
		bglevel = BL_ZERO;
		break;

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
	return R_SetBlendFunc (fglevel, bglevel) ?
		(r_columnmethod ? DoDraw1 : DoDraw0) : DontDraw;
}

void R_FinishSetPatchStyle ()
{
	basecolormap = basecolormapsave;
}
