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

#include "m_alloc.h"

#include "doomdef.h"

#include "i_system.h"
#include "z_zone.h"
#include "w_wad.h"

#include "r_local.h"

// Needs access to LFB (guess what).
#include "v_video.h"

// State.
#include "doomstat.h"

#include "st_stuff.h"

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
int 			viewwidth;
int 			scaledviewwidth;
int 			viewheight;
int 			viewwindowx;
int 			viewwindowy;
byte**			ylookup;
int* 			columnofs;

int				realviewwidth;		// [RH] Physical width of view window
int				realviewheight;		// [RH] Physical height of view window
int				detailxshift;		// [RH] X shift for horizontal detail level
int				detailyshift;		// [RH] Y shift for vertical detail level

// [RH] Pointers to the different column drawers.
//		These get changed depending on the current
//		screen depth.
void (*R_DrawColumnHoriz)(void);
void (*R_DrawColumn)(void);
void (*R_DrawFuzzColumn)(void);
void (*R_DrawTranslucentColumn)(void);
void (*R_DrawTranslatedColumn)(void);




//
// R_DrawColumn
// Source is the top of the column to scale.
//
int						dc_pitch=0x12345678;	// [RH] Distance between rows

lighttable_t*			dc_colormap; 
int 					dc_x; 
int 					dc_yl; 
int 					dc_yh; 
fixed_t 				dc_iscale; 
fixed_t 				dc_texturemid;
int						dc_color;				// [RH] Color for column filler

// first pixel in a column (possibly virtual) 
byte*					dc_source;				

// [RH] Tutti-Frutti fix
unsigned int			dc_mask;

// just for profiling 
int 					dccount;


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
	if (dc_x >= screen.width
		|| dc_yl < 0
		|| dc_yh >= screen.height) {
		Printf (PRINT_HIGH, "R_DrawColumnP_C: %i to %i at %i\n", dc_yl, dc_yh, dc_x);
		return;
	}
#endif

	// Framebuffer destination address.
	// Use ylookup LUT to avoid multiply with ScreenWidth.
	// Use columnofs LUT for subwindows?
	dest = ylookup[dc_yl] + columnofs[dc_x];

	// Determine scaling,
	//	which is the only mapping to be done.
	fracstep = dc_iscale; 
	frac = dc_texturemid + (dc_yl-centery)*fracstep;

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
	if (dc_x >= screen.width
		|| dc_yl < 0
		|| dc_yh >= screen.height) {
		Printf (PRINT_HIGH, "R_StretchColumnP_C: %i to %i at %i\n", dc_yl, dc_yh, dc_x);
		return;
	}
#endif

	dest = ylookup[dc_yl] + columnofs[dc_x];
	fracstep = dc_iscale; 
	frac = dc_texturemid + (dc_yl-centery)*fracstep;

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
	if (dc_x >= screen.width
		|| dc_yl < 0
		|| dc_yh >= screen.height) {
		Printf (PRINT_HIGH, "R_StretchColumnP_C: %i to %i at %i\n", dc_yl, dc_yh, dc_x);
		return;
	}
#endif

	dest = ylookup[dc_yl] + columnofs[dc_x];

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
#define FUZZTABLE	64
#define FUZZOFF		(screen.pitch)


int 	fuzzoffset[FUZZTABLE];
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
	1,-1, 1,-1, 1, 1,-1, 1,
	1,-1, 1, 1, 1,-1, 1, 1,
	1,-1,-1,-1,-1, 1,-1,-1,
	1, 1, 1, 1,-1, 1,-1, 1,
	1,-1,-1, 1, 1,-1,-1,-1,
   -1, 1, 1, 1, 1,-1, 1, 1,
   -1, 1, 1, 1,-1, 1, 1, 1,
   -1, 1, 1,-1, 1, 1,-1, 1
};

int 	fuzzpos = 0; 

void R_InitFuzzTable (void)
{
	int i;
	int fuzzoff;

	V_LockScreen (&screen);
	fuzzoff = FUZZOFF << detailyshift;
	V_UnlockScreen (&screen);

	for (i = 0; i < FUZZTABLE; i++)
		fuzzoffset[i] = fuzzinit[i] * fuzzoff;
}

#ifndef USEASM
//
// Framebuffer postprocessing.
// Creates a fuzzy image by copying pixels
//	from adjacent ones to left and right.
// Used with an all black colormap, this
//	could create the SHADOW effect,
//	i.e. spectres and invisible players.
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
	if (dc_x >= screen.width
		|| dc_yl < 0 || dc_yh >= screen.height)
	{
		I_Error ("R_DrawFuzzColumnP_C: %i to %i at %i",
				 dc_yl, dc_yh, dc_x);
	}
#endif


	dest = ylookup[dc_yl] + columnofs[dc_x];

	// Looks like an attempt at dithering,
	//	using the colormap #6 (of 0-31, a bit
	//	brighter than average).
	{
		// [RH] Make local copies of global vars to try and improve
		//		the optimizations made by the compiler.
		int pitch = dc_pitch;
		int fuzz = fuzzpos;
		byte *map = DefaultPalette->maps.colormaps + 6*256;

		do 
		{
			// Lookup framebuffer, and retrieve
			//	a pixel that is either one column
			//	left or right of the current one.
			// Add index from colormap to index.
			*dest = map[dest[fuzzoffset[fuzz]]]; 

			// Clamp table lookup index.
			fuzz = (fuzz + 1) & (FUZZTABLE - 1);
			
			dest += pitch;
		} while (--count);

		fuzzpos = (fuzz + 3) & (FUZZTABLE - 1);
	}
} 
#endif	// USEASM

//
// R_DrawTranlucentColumn
//
byte *dc_transmap;

#ifndef USEASM
void R_DrawTranslucentColumnP_C (void)
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
	if (dc_x >= screen.width
		|| dc_yl < 0
		|| dc_yh >= screen.height)
	{
		I_Error ( "R_DrawTranslucentColumnP_C: %i to %i at %i",
				  dc_yl, dc_yh, dc_x);
	}
	
#endif 

	dest = ylookup[dc_yl] + columnofs[dc_x];

	fracstep = dc_iscale;
	frac = dc_texturemid + (dc_yl-centery)*fracstep;

	{
		byte *transmap = dc_transmap;
		byte *colormap = dc_colormap;
		byte *source = dc_source;
		int mask = dc_mask;
		int pitch = dc_pitch;

		do
		{
			*dest = transmap[colormap[source[(frac>>FRACBITS)&mask]] | ((*dest)<<8)];
			dest += pitch;

			frac += fracstep;
		} while (--count);
	}
}
#endif	// USEASM 

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
	if (dc_x >= screen.width
		|| dc_yl < 0
		|| dc_yh >= screen.height)
	{
		I_Error ( "R_DrawTranslatedColumnP_C: %i to %i at %i",
				  dc_yl, dc_yh, dc_x);
	}
	
#endif 

	dest = ylookup[dc_yl] + columnofs[dc_x];

	// Looks familiar.
	fracstep = dc_iscale;
	frac = dc_texturemid + (dc_yl-centery)*fracstep;

	// Here we do an additional index re-mapping.
	{
		// [RH] Local copies of global vars to improve compiler optimizations
		byte *colormap = dc_colormap;
		byte *translation = dc_translation;
		byte *source = dc_source;
		int pitch = dc_pitch;
		int mask = dc_mask;

		do
		{
			// Translation tables are used
			//	to map certain colorramps to other ones,
			//	used with PLAY sprites.
			// Thus the "green" ramp of the player 0 sprite
			//	is mapped to gray, red, black/indigo. 
			*dest = colormap[translation[source[(frac>>FRACBITS) & mask]]];
			dest += pitch;

			frac += fracstep;
		} while (--count);
	}
}

// [RH] Draw a column that is both translated and translucent
void R_DrawTlatedLucentColumnP_C (void)
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
	if (dc_x >= screen.width
		|| dc_yl < 0
		|| dc_yh >= screen.height)
	{
		I_Error ( "R_DrawTlatedLucentColumnP_C: %i to %i at %i",
				  dc_yl, dc_yh, dc_x);
	}
	
#endif 

	dest = ylookup[dc_yl] + columnofs[dc_x];

	fracstep = dc_iscale;
	frac = dc_texturemid + (dc_yl-centery)*fracstep;

	{
		byte *translation = dc_translation;
		byte *transmap = dc_transmap;
		byte *colormap = dc_colormap;
		byte *source = dc_source;
		int mask = dc_mask;
		int pitch = dc_pitch;

		do
		{
			*dest = transmap[colormap[translation[source[(frac>>FRACBITS)&mask]]] | ((*dest)<<8)];
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
int						ds_colsize=0x12345678;	// [RH] Distance between columns
int						ds_colshift=1;			// [RH] (1<<ds_colshift)=ds_colsize
int						ds_color;				// [RH] color for non-textured spans

int 					ds_y; 
int 					ds_x1; 
int 					ds_x2;

lighttable_t*			ds_colormap; 

fixed_t 				ds_xfrac; 
fixed_t 				ds_yfrac; 
fixed_t 				ds_xstep; 
fixed_t 				ds_ystep;

// start of a 64*64 tile image 
byte*					ds_source;		

// just for profiling
int 					dscount;


//
// Draws the actual span.
void R_DrawSpanP (void) 
{ 
	fixed_t 			xfrac;
	fixed_t 			yfrac; 
	byte*				dest; 
	int 				count;
	int 				spot; 

#ifdef RANGECHECK 
	if (ds_x2 < ds_x1
		|| ds_x1<0
		|| ds_x2>=screen.width  
		|| ds_y>screen.height)
	{
		I_Error( "R_DrawSpan: %i to %i at %i",
				 ds_x1,ds_x2,ds_y);
	}
//		dscount++; 
#endif 

	
	xfrac = ds_xfrac; 
	yfrac = ds_yfrac; 
		 
	dest = ylookup[ds_y] + columnofs[ds_x1];

	// We do not check for zero spans here?
	count = ds_x2 - ds_x1 + 1; 

	/* [Petteri] Do optimized loop if enough to do: */
#ifdef USEASM    
	if ( count > 1 ) {
		DrawSpan8Loop(xfrac, yfrac, count & (~1), dest);
		if ( (count & 1) == 0 )
			return;
		xfrac += ds_xstep * (count & (~1));
		yfrac += ds_ystep * (count & (~1));
		dest += (count & (~1)) << ds_colshift;
//		count = 1;
	}
#else
	do {
#endif
		// Current texture index in u,v.
		spot = ((yfrac>>(16-6))&(63*64)) + ((xfrac>>16)&63);

		// Lookup pixel from flat texture tile,
		//  re-index using light/colormap.
		*dest = ds_colormap[ds_source[spot]];
#ifndef USEASM
		dest += ds_colsize;

		// Next step in u,v.
		xfrac += ds_xstep; 
		yfrac += ds_ystep;
	} while (--count);
#endif
} 

// [RH] Just fill a span with a color
void R_FillSpan (void) 
{ 
	byte *dest;
	int count;
	int color = ds_color;
	int colsize = ds_colsize;

#ifdef RANGECHECK 
	if (ds_x2 < ds_x1
		|| ds_x1<0
		|| ds_x2>=screen.width  
		|| ds_y>screen.height)
	{
		I_Error( "R_FillSpan: %i to %i at %i",
				 ds_x1,ds_x2,ds_y);
	}
//		dscount++; 
#endif 

	dest = ylookup[ds_y] + columnofs[ds_x1];

	count = ds_x2 - ds_x1 + 1; 

	do {
		*dest = color;
		dest += colsize;
	} while (--count);
} 



/****************************************/
/*										*/
/* [RH] ARGB8888 drawers (C versions)	*/
/*										*/
/****************************************/

#define dc_shademap ((unsigned int *)dc_colormap)

void R_DrawColumnD_C (void) 
{ 
	int 				count;
	unsigned int*		dest;
	fixed_t 			frac;
	fixed_t 			fracstep;

	count = dc_yh - dc_yl;

	// Zero length, column does not exceed a pixel.
	if (count < 0)
		return;
	count++;

#ifdef RANGECHECK 
	if (dc_x >= screen.width
		|| dc_yl < 0
		|| dc_yh >= screen.height) {
		Printf (PRINT_HIGH, "R_DrawColumnD_C: %i to %i at %i\n", dc_yl, dc_yh, dc_x);
		return;
	}
#endif

	dest = (unsigned int *)(ylookup[dc_yl] + columnofs[dc_x]);

	fracstep = dc_iscale;
	frac = dc_texturemid + (dc_yl-centery)*fracstep;

	{
		unsigned int *shademap = dc_shademap;
		byte *source = dc_source;
		int mask = dc_mask;
		int pitch = dc_pitch >> 2;

		do
		{
			*dest = shademap[source[(frac>>FRACBITS)&mask]];

			dest += pitch;
			frac += fracstep;

		} while (--count);
	}
}

void R_DrawFuzzColumnD_C (void)
{
	int 				count;
	unsigned int*		dest;

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
	if (dc_x >= screen.width
		|| dc_yl < 0 || dc_yh >= screen.height)
	{
		I_Error ("R_DrawFuzzColumnD_C: %i to %i at %i",
				 dc_yl, dc_yh, dc_x);
	}
#endif

	dest = (unsigned int *)(ylookup[dc_yl] + columnofs[dc_x]);

	// [RH] This is actually slightly brighter than
	//		the indexed version, but it's close enough.
	{
		int fuzz = fuzzpos;
		int pitch = dc_pitch >> 2;

		do
		{
			unsigned int work = dest[fuzzoffset[fuzz]>>2];
			*dest = work - ((work >> 2) & 0x3f3f3f);

			// Clamp table lookup index.
			fuzz = (fuzz + 1) & (FUZZTABLE - 1);
			
			dest += pitch;
		} while (--count);

		fuzzpos = (fuzz + 3) & (FUZZTABLE - 1);
	}
}

void R_DrawTranslucentColumnD_C (void)
{
	int 				count;
	unsigned int*		dest;
	fixed_t 			frac;
	fixed_t 			fracstep;

	count = dc_yh - dc_yl;
	if (count < 0)
		return;
	count++;

#ifdef RANGECHECK
	if (dc_x >= screen.width
		|| dc_yl < 0
		|| dc_yh >= screen.height)
	{
		I_Error ( "R_DrawTranslucentColumnD_C: %i to %i at %i",
				  dc_yl, dc_yh, dc_x);
	}
	
#endif 

	dest = (unsigned int *)(ylookup[dc_yl] + columnofs[dc_x]);

	fracstep = dc_iscale;
	frac = dc_texturemid + (dc_yl-centery)*fracstep;

	{
		unsigned int *shademap = dc_shademap;
		byte *source = dc_source;
		int mask = dc_mask;
		int pitch = dc_pitch >> 2;

		do
		{
			*dest = ((*dest >> 1) & 0x7f7f7f) +
					((shademap[source[(frac>>FRACBITS)&mask]] >> 1) & 0x7f7f7f);
			dest += pitch;

			frac += fracstep;
		} while (--count);
	}
}

void R_DrawTranslatedColumnD_C (void)
{
	int 				count;
	unsigned int*		dest;
	fixed_t 			frac;
	fixed_t 			fracstep;

	count = dc_yh - dc_yl;
	if (count < 0)
		return;
	count++;

#ifdef RANGECHECK
	if (dc_x >= screen.width
		|| dc_yl < 0
		|| dc_yh >= screen.height)
	{
		I_Error ( "R_DrawTranslatedColumnD_C: %i to %i at %i",
				  dc_yl, dc_yh, dc_x);
	}
	
#endif


	dest = (unsigned int *)(ylookup[dc_yl] + columnofs[dc_x]);

	fracstep = dc_iscale;
	frac = dc_texturemid + (dc_yl-centery)*fracstep;

	// Here we do an additional index re-mapping.
	{
		byte *source = dc_source;
		unsigned int *shademap = dc_shademap;
		byte *translation = dc_translation;
		int mask = dc_mask;
		int pitch = dc_pitch >> 2;

		do
		{
			*dest = shademap[translation[source[(frac>>FRACBITS) & mask]]];
			dest += pitch;
			
			frac += fracstep;
		} while (--count);
	}
}

void R_DrawSpanD (void)
{ 
	fixed_t 			xfrac;
	fixed_t 			yfrac;
	unsigned int*		dest;
	int 				count;
	int 				spot;

#ifdef RANGECHECK 
	if (ds_x2 < ds_x1
		|| ds_x1<0
		|| ds_x2>=screen.width
		|| ds_y>screen.height)
	{
		I_Error( "R_DrawSpan: %i to %i at %i",
				 ds_x1,ds_x2,ds_y);
	}
//		dscount++;
#endif

	
	xfrac = ds_xfrac;
	yfrac = ds_yfrac;

	dest = (unsigned int *)(ylookup[ds_y] + columnofs[ds_x1]);

	count = ds_x2 - ds_x1 + 1;

	{
		byte *source = ds_source;
		unsigned int *shademap = (unsigned int *)ds_colormap;
		int colsize = ds_colsize >> 2;
		int xstep = ds_xstep;
		int ystep = ds_ystep;

		do {
			spot = ((yfrac>>(16-6))&(63*64)) + ((xfrac>>16)&63);

			// Lookup pixel from flat texture tile,
			//  re-index using light/colormap.
			*dest = shademap[source[spot]];
			dest += colsize;

			// Next step in u,v.
			xfrac += xstep; 
			yfrac += ystep;
		} while (--count);
	}
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
extern byte *Ranges;

void R_InitTranslationTables (void)
{
	static const char ranges[11][8] = {
		"CRBRICK",
		"CRTAN",
		"CRGRAY",
		"CRGREEN",
		"CRBROWN",
		"CRGOLD",
		"CRRED",
		"CRBLUE2",
		"CRORANGE",
		"CRYELLOW",
		"CRBLUE"
	};
	int i;
		
	translationtables = Z_Malloc (256*(MAXPLAYERS+3+11)+255, PU_STATIC, 0);
	translationtables = (byte *)(( (int)translationtables + 255 )& ~255);
	
	// [RH] Each player now gets their own translation table
	//		(soon to be palettes). These are set up during
	//		netgame arbitration and as-needed rather than
	//		in here. We do, however load some text translation
	//		tables from our PWAD (ala BOOM).

	for (i = 0; i < 256; i++)
		translationtables[i] = i;

	for (i = 1; i < MAXPLAYERS+3; i++)
		memcpy (translationtables + i*256, translationtables, 256);

	// create translation tables for dehacked patches that expect them
	for (i = 0x70; i < 0x80; i++) {
		// map green ramp to gray, brown, red
		translationtables[i+(MAXPLAYERS+0)*256] = 0x60 + (i&0xf);
		translationtables[i+(MAXPLAYERS+1)*256] = 0x40 + (i&0xf);
		translationtables[i+(MAXPLAYERS+2)*256] = 0x20 + (i&0xf);
	}

	Ranges = translationtables + (MAXPLAYERS+3)*256;
	for (i = 0; i < 11; i++)
		W_ReadLump (W_GetNumForName (ranges[i]), Ranges + 256 * i);

}

// [RH] Create a player's translation table based on
//		a given mid-range color.
void R_BuildPlayerTranslation (int player, int color)
{
	palette_t *pal = GetDefaultPalette();
	byte *table = &translationtables[player * 256];
	int i;
	float r = (float)RPART(color) / 255.0f;
	float g = (float)GPART(color) / 255.0f;
	float b = (float)BPART(color) / 255.0f;
	float h, s, v;
	float sdelta, vdelta;

	RGBtoHSV (r, g, b, &h, &s, &v);

	s -= 0.23f;
	if (s < 0.0f)
		s = 0.0f;
	sdelta = 0.014375f;

	v += 0.1f;
	if (v > 1.0f)
		v = 1.0f;
	vdelta = -0.05882f;

	for (i = 0x70; i < 0x80; i++) {
		HSVtoRGB (&r, &g, &b, h, s, v);
		table[i] = BestColor (pal->basecolors,
							  (int)(r * 255.0f),
							  (int)(g * 255.0f),
							  (int)(b * 255.0f),
							  pal->numcolors);
		s += sdelta;
		if (s > 1.0f) {
			s = 1.0f;
			sdelta = 0.0f;
		}

		v += vdelta;
		if (v < 0.0f) {
			v = 0.0f;
			vdelta = 0.0f;
		}
	}
}


//
// R_InitBuffer 
// Creats lookup tables that avoid
//	multiplies and other hassles
//	for getting the framebuffer address
//	of a pixel to draw.
//
void R_InitBuffer (int width, int height)
{
	int 		i;
	byte		*buffer;
	int			pitch;
	int			xshift;

	// Handle resize,
	//	e.g. smaller view windows
	//	with border and/or status bar.
	viewwindowx = (screen.width-(width<<detailxshift))>>1;

	// [RH] Adjust column offset according to bytes per pixel
	//		and detail mode
	xshift = (screen.is8bit) ? 0 : 2;
	xshift += detailxshift;

	// Column offset. For windows
	for (i = 0; i < width; i++)
		columnofs[i] = viewwindowx + (i << xshift);

	// Same with base row offset.
	if ((width<<detailxshift) == screen.width)
		viewwindowy = 0;
	else
		viewwindowy = (ST_Y-(height<<detailyshift)) >> 1;

	V_LockScreen (&screen);
	buffer = screen.buffer;
	pitch = screen.pitch;
	V_UnlockScreen (&screen);

	// Precalculate all row offsets.
	for (i=0 ; i<height ; i++)
		ylookup[i] = buffer + ((i<<detailyshift)+viewwindowy)*pitch;
}


void R_DrawBorder (int x1, int y1, int x2, int y2)
{
	int lump;

	lump = R_FlatNumForName ((gamemode == commercial) ? "GRNROCK" : "FLOOR7_2");
	V_FlatFill (x1 & ~63, y1, x2, y2, &screen,
				W_CacheLumpNum (lump + firstflat, PU_CACHE));

}


/*
==================
=
= R_DrawViewBorder
=
= Draws the border around the view for different size windows
==================
*/

BOOL BorderNeedRefresh;

void V_MarkRect (int x, int y, int width, int height);

void R_DrawViewBorder (void)
{
	int x, y;

	// [RH] Redraw the status bar if SCREENWIDTH > status bar width.
	// Will draw borders around itself, too.
	if (screen.width > 320)
	{
		SB_state = -1;
	}

	if (realviewwidth == screen.width) {
		return;
	}

	R_DrawBorder (0, 0, screen.width, viewwindowy);
	R_DrawBorder (0, viewwindowy, viewwindowx, realviewheight + viewwindowy);
	R_DrawBorder (viewwindowx + realviewwidth, viewwindowy, screen.width, realviewheight + viewwindowy);
	R_DrawBorder (0, viewwindowy + realviewheight, screen.width, ST_Y);

	for (x = viewwindowx; x < viewwindowx + realviewwidth; x += 8)
	{
		V_DrawPatch (x, viewwindowy - 8, &screen, W_CacheLumpName ("brdr_t", PU_CACHE));
		V_DrawPatch (x, viewwindowy + realviewheight, &screen, W_CacheLumpName ("brdr_b", PU_CACHE));
	}
	for (y = viewwindowy; y < viewwindowy + realviewheight; y += 8)
	{
		V_DrawPatch (viewwindowx - 8, y, &screen, W_CacheLumpName ("brdr_l", PU_CACHE));
		V_DrawPatch (viewwindowx + realviewwidth, y, &screen, W_CacheLumpName ("brdr_r", PU_CACHE));
	}
	// Draw beveled edge.
	V_DrawPatch (viewwindowx-8,
				 viewwindowy-8,
				 &screen,
				 W_CacheLumpName ("brdr_tl",PU_CACHE));
	
	V_DrawPatch (viewwindowx+realviewwidth,
				 viewwindowy-8,
				 &screen,
				 W_CacheLumpName ("brdr_tr",PU_CACHE));
	
	V_DrawPatch (viewwindowx-8,
				 viewwindowy+realviewheight,
				 &screen,
				 W_CacheLumpName ("brdr_bl",PU_CACHE));
	
	V_DrawPatch (viewwindowx+realviewwidth,
				 viewwindowy+realviewheight,
				 &screen,
				 W_CacheLumpName ("brdr_br",PU_CACHE));

	V_MarkRect (0,0,screen.width, ST_Y);
}

/*
==================
=
= R_DrawTopBorder
=
= Draws the top border around the view for different size windows
==================
*/

BOOL BorderTopRefresh;

void R_DrawTopBorder (void)
{
	int x, y;
	
	if (realviewwidth == screen.width)
		return;

	R_DrawBorder (0, 0, screen.width, 34);

	if (viewwindowy < 35)
	{
		for (x = viewwindowx; x < viewwindowx + realviewwidth; x += 8)
		{
			V_DrawPatch (x, viewwindowy-8, &screen, W_CacheLumpName("brdr_t", PU_CACHE));
		}
		for (y = viewwindowy; y < 35; y += 8)
		{
			V_DrawPatch(viewwindowx-8, y, &screen,
				W_CacheLumpName ("brdr_l", PU_CACHE));
			V_DrawPatch(viewwindowx+realviewwidth, y, &screen,
				W_CacheLumpName("brdr_r", PU_CACHE));
		}

		V_DrawPatch(viewwindowx-8, viewwindowy-8, &screen,
			W_CacheLumpName("brdr_tl", PU_CACHE));
		V_DrawPatch(viewwindowx+realviewwidth, viewwindowy-8, &screen,
			W_CacheLumpName("brdr_tr", PU_CACHE));
	}
}


// [RH] Double pixels in the view window horizontally
//		and/or vertically (or not at all).
void R_DetailDouble (void)
{
	switch ((detailxshift << 1) | detailyshift) {
		case 1:		// y-double
		{
			int rowsize = realviewwidth << ((screen.is8bit) ? 0 : 2);
			int pitch = screen.pitch;
			int y;
			byte *line;

			line = screen.buffer + viewwindowy*pitch + viewwindowx;
			for (y = 0; y < viewheight; y++, line += pitch<<1) {
				memcpy (line+pitch, line, rowsize);
			}
		}
		break;

		case 2:		// x-double
		{
			int rowsize = realviewwidth >> 2;
			int pitch = screen.pitch >> (2-detailyshift);
			int y,x;
			unsigned *line,a,b;

			line = (unsigned *)(screen.buffer + viewwindowy*screen.pitch + viewwindowx);
			for (y = 0; y < viewheight; y++, line += pitch) {
				for (x = 0; x < rowsize; x += 2) {
					a = line[x+0];
					b = line[x+1];
					a &= 0x00ff00ff;
					b &= 0x00ff00ff;
					line[x+0] = a | (a << 8);
					line[x+1] = b | (b << 8);
				}
			}
		}
		break;

		case 3:		// x- and y-double
		{
			int rowsize = realviewwidth >> 2;
			int pitch = screen.pitch >> (2-detailyshift);
			int realpitch = screen.pitch >> 2;
			int y,x;
			unsigned *line,a,b;

			line = (unsigned *)(screen.buffer + viewwindowy*screen.pitch + viewwindowx);
			for (y = 0; y < viewheight; y++, line += pitch) {
				for (x = 0; x < rowsize; x += 2) {
					a = line[x+0];
					b = line[x+1];
					a &= 0x00ff00ff;
					b &= 0x00ff00ff;
					line[x+0] = a | (a << 8);
					line[x+0+realpitch] = a | (a << 8);
					line[x+1] = b | (b << 8);
					line[x+1+realpitch] = b | (b << 8);
				}
			}
		}
		break;
	}
}

// [RH] Initialize the column drawer pointers
void R_InitColumnDrawers (BOOL is8bit)
{
	if (is8bit) {
#ifdef USEASM
		if (screen.height <= 240)
			R_DrawColumn		= R_DrawColumnP_Unrolled;
		else
			R_DrawColumn		= R_DrawColumnP_ASM;
		R_DrawColumnHoriz		= R_DrawColumnHorizP_ASM;
		R_DrawFuzzColumn		= R_DrawFuzzColumnP_ASM;
		R_DrawTranslucentColumn = R_DrawTranslucentColumnP_ASM;
		R_DrawTranslatedColumn	= R_DrawTranslatedColumnP_C;
		if (screen.width <= 320)
			R_DrawSpan			= R_DrawSpanP_Unrolled;
		else
			R_DrawSpan			= R_DrawSpanP;
#else
		R_DrawColumnHoriz		= R_DrawColumnHorizP_C;
		R_DrawColumn			= R_DrawColumnP_C;
		R_DrawFuzzColumn		= R_DrawFuzzColumnP_C;
		R_DrawTranslucentColumn = R_DrawTranslucentColumnP_C;
		R_DrawTranslatedColumn	= R_DrawTranslatedColumnP_C;
		R_DrawSpan				= R_DrawSpanP;
#endif
	} else {
#ifdef USEASM
		R_DrawColumnHoriz		= R_DrawColumnHorizP_ASM;
		R_DrawColumn			= R_DrawColumnD_C;
		R_DrawFuzzColumn		= R_DrawFuzzColumnD_C;
		R_DrawTranslucentColumn = R_DrawTranslucentColumnD_C;
		R_DrawTranslatedColumn	= R_DrawTranslatedColumnD_C;
#else
		R_DrawColumnHoriz		= R_DrawColumnHorizP_C;
		R_DrawColumn			= R_DrawColumnD_C;
		R_DrawFuzzColumn		= R_DrawFuzzColumnD_C;
		R_DrawTranslucentColumn = R_DrawTranslucentColumnD_C;
		R_DrawTranslatedColumn	= R_DrawTranslatedColumnD_C;
#endif
		R_DrawSpan				= R_DrawSpanD;
	}
}