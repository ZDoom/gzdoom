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


static const char
rcsid[] = "$Id: r_draw.c,v 1.4 1997/02/03 16:47:55 b1 Exp $";

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


// ?
#define MAXWIDTH						1120
#define MAXHEIGHT						832

// status bar height at bottom of screen
#define SBARHEIGHT				32

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

// Color tables for different players,
//	translate a limited part to another
//	(color ramps used for  suit colors).
//
byte			translations[3][256];	
 
 


//
// R_DrawColumn
// Source is the top of the column to scale.
//
lighttable_t*			dc_colormap; 
int 					dc_x; 
int 					dc_yl; 
int 					dc_yh; 
fixed_t 				dc_iscale; 
fixed_t 				dc_texturemid;

// first pixel in a column (possibly virtual) 
byte*					dc_source;				

// just for profiling 
int 					dccount;

#ifndef	USEASM
//
// A column is a vertical slice/span from a wall texture that,
//	given the DOOM style restrictions on the view orientation,
//	will always have constant z depth.
// Thus a special case loop for very fast rendering can
//	be used. It has also been used with Wolfenstein 3D.
// 
void R_DrawColumn_C (void) 
{ 
	int 				count; 
	byte*				dest; 
	fixed_t 			frac;
	fixed_t 			fracstep;		 
 
	count = dc_yh - dc_yl; 

	// Zero length, column does not exceed a pixel.
	if (count < 0) 
		return; 
								 
#ifdef RANGECHECK 
	if (dc_x >= SCREENWIDTH
		|| dc_yl < 0
		|| dc_yh >= SCREENHEIGHT) {
		Printf ("R_DrawColumn: %i to %i at %i\n", dc_yl, dc_yh, dc_x);
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

	// Inner loop that does the actual texture mapping,
	//	e.g. a DDA-lile scaling.
	// This is as fast as it gets.
	do 
	{
		// Re-map color indices from wall texture column
		//	using a lighting/special effects LUT.
		*dest = dc_colormap[dc_source[(frac>>FRACBITS)&127]];
		
		dest += SCREENPITCH; 
		frac += fracstep;
		
	} while (count--); 
} 
#endif	// USEASM

//
// Spectre/Invisibility.
//
// [RH] FUZZTABLE changed from 50 to 64
#define FUZZTABLE	64
#define FUZZOFF		(SCREENWIDTH)


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

	for (i = 0; i < FUZZTABLE; i++)
		fuzzoffset[i] = fuzzinit[i] * FUZZOFF;
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
void R_DrawFuzzColumn_C (void) 
{ 
	int 				count; 
	byte*				dest;
	byte*				map;

	// Adjust borders. Low... 
	if (!dc_yl) 
		dc_yl = 1;

	// .. and high.
	if (dc_yh == viewheight-1) 
		dc_yh = viewheight - 2; 
				 
	count = dc_yh - dc_yl; 

	// Zero length.
	if (count < 0) 
		return; 

	map = colormaps + 6*256;
	
#ifdef RANGECHECK 
	if (dc_x >= SCREENWIDTH
		|| dc_yl < 0 || dc_yh >= SCREENHEIGHT)
	{
		I_Error ("R_DrawFuzzColumn: %i to %i at %i",
				 dc_yl, dc_yh, dc_x);
	}
#endif


	// Does not work with blocky mode.
	dest = ylookup[dc_yl] + columnofs[dc_x];

	// Looks like an attempt at dithering,
	//	using the colormap #6 (of 0-31, a bit
	//	brighter than average).
	do 
	{
		// Lookup framebuffer, and retrieve
		//	a pixel that is either one column
		//	left or right of the current one.
		// Add index from colormap to index.
		*dest = map[dest[fuzzoffset[fuzzpos]]]; 

		// Clamp table lookup index.
		fuzzpos = (fuzzpos + 1) & (FUZZTABLE - 1);
		
		dest += SCREENPITCH;
	} while (count--);

	fuzzpos += 3;
} 
#endif	// USEASM

//
// R_DrawTranlucentColumn
//
byte *dc_transmap;

#ifndef USEASM
void R_DrawTranslucentColumn_C (void) 
{ 
	int 				count; 
	byte*				dest; 
	fixed_t 			frac;
	fixed_t 			fracstep;		 
 
	count = dc_yh - dc_yl; 
	if (count < 0) 
		return; 
								 
#ifdef RANGECHECK 
	if (dc_x >= SCREENWIDTH
		|| dc_yl < 0
		|| dc_yh >= SCREENHEIGHT)
	{
		I_Error ( "R_DrawTranslucentColumn: %i to %i at %i",
				  dc_yl, dc_yh, dc_x);
	}
	
#endif 


	// FIXME. As above.
	dest = ylookup[dc_yl] + columnofs[dc_x]; 

	// Looks familiar.
	fracstep = dc_iscale; 
	frac = dc_texturemid + (dc_yl-centery)*fracstep; 

	do 
	{
		*dest = dc_transmap[dc_colormap[dc_source[(frac>>FRACBITS)&0x7f]] + ((*dest)<<8)];
		dest += SCREENPITCH;
		
		frac += fracstep; 
	} while (count--); 
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

extern byte *WhiteMap;

void R_DrawTranslatedColumn_C (void) 
{ 
	int 				count; 
	byte*				dest; 
	fixed_t 			frac;
	fixed_t 			fracstep;		 
 
	count = dc_yh - dc_yl; 
	if (count < 0) 
		return; 
								 
#ifdef RANGECHECK 
	if (dc_x >= SCREENWIDTH
		|| dc_yl < 0
		|| dc_yh >= SCREENHEIGHT)
	{
		I_Error ( "R_DrawTranslatedColumn: %i to %i at %i",
				  dc_yl, dc_yh, dc_x);
	}
	
#endif 


	// FIXME. As above.
	dest = ylookup[dc_yl] + columnofs[dc_x]; 

	// Looks familiar.
	fracstep = dc_iscale; 
	frac = dc_texturemid + (dc_yl-centery)*fracstep; 

	// Here we do an additional index re-mapping.
	do 
	{
		// Translation tables are used
		//	to map certain colorramps to other ones,
		//	used with PLAY sprites.
		// Thus the "green" ramp of the player 0 sprite
		//	is mapped to gray, red, black/indigo. 
		*dest = dc_colormap[dc_translation[dc_source[frac>>FRACBITS]]];
		dest += SCREENPITCH;
		
		frac += fracstep; 
	} while (count--); 
} 




//
// R_InitTranslationTables
// Creates the translation tables to map
//	the green color ramp to gray, brown, red.
// Assumes a given structure of the PLAYPAL.
// Could be read from a lump instead.
//
void R_InitTranslationTables (void)
{
	int 		i;
		
	translationtables = Z_Malloc (256*4+255, PU_STATIC, 0);
	translationtables = (byte *)(( (int)translationtables + 255 )& ~255);
	
	// translate just the 16 green colors
	for (i=0 ; i<256 ; i++)
	{
		if (i >= 0x70 && i<= 0x7f)
		{
			// map green ramp to gray, brown, red
			translationtables[i] = 0x60 + (i&0xf);
			translationtables [i+256] = 0x40 + (i&0xf);
			translationtables [i+512] = 0x20 + (i&0xf);
		}
		else
		{
			// Keep all other colors as is.
			translationtables[i] = translationtables[i+256] 
				= translationtables[i+512] = i;
		}
	}

	// Create the red->white table for text routines
	WhiteMap = translationtables + 768;

	for (i = 0; i < 256; i++) {
		if (i >= 176 && i <= 191) {
			if (i < 186) {
				WhiteMap[i] = i - 83;
			} else {
				WhiteMap[i] = i - 80;
			}
		} else {
			WhiteMap[i] = i;
		}
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
void R_DrawSpan (void) 
{ 
	fixed_t 			xfrac;
	fixed_t 			yfrac; 
	byte*				dest; 
	int 				count;
	int 				spot; 

#ifdef RANGECHECK 
	if (ds_x2 < ds_x1
		|| ds_x1<0
		|| ds_x2>=SCREENWIDTH  
		|| ds_y>SCREENHEIGHT)
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
	count = ds_x2 - ds_x1; 

	/* [Petteri] Do optimized loop if enough to do: */
#ifdef USEASM    
	if ( count > 2 ) {
		count++; /* the do-loop below does count+1 pixels */
		DrawSpan8Loop(xfrac, yfrac, count & (~1), dest);
		if ( (count & 1) == 0 )
			return;
		xfrac += ds_xstep * (count & (~1));
		yfrac += ds_ystep * (count & (~1));
		dest += (count & (~1));
		count = 0;
	}
#endif

	do {
		// Current texture index in u,v.
		spot = ((yfrac>>(16-6))&(63*64)) + ((xfrac>>16)&63);

		// Lookup pixel from flat texture tile,
		//  re-index using light/colormap.
		*dest++ = ds_colormap[ds_source[spot]];

		// Next step in u,v.
		xfrac += ds_xstep; 
		yfrac += ds_ystep;

	} while (count--);
} 


//
// R_InitBuffer 
// Creats lookup tables that avoid
//	multiplies and other hazzles
//	for getting the framebuffer address
//	of a pixel to draw.
//
void
R_InitBuffer
( int			width,
  int			height ) 
{ 
	int 		i; 

	// Handle resize,
	//	e.g. smaller view windows
	//	with border and/or status bar.
	viewwindowx = (SCREENWIDTH-width) >> 1; 

	// Column offset. For windows.
	for (i=0 ; i<width ; i++) 
		columnofs[i] = viewwindowx + i;

	// Same with base row offset.
	if (width == SCREENWIDTH) 
		viewwindowy = 0; 
	else 
		viewwindowy = (SCREENHEIGHT-SBARHEIGHT-height) >> 1; 

	// Preclaculate all row offsets.
	for (i=0 ; i<height ; i++) 
		ylookup[i] = screens[0] + (i+viewwindowy)*SCREENPITCH; 
} 
 
 


//
// R_FillBackScreen
// Fills the back screen with a pattern
//	for variable screen sizes
// Also draws a beveled edge.
//
void R_FillBackScreen (void) 
{ 
	byte*		src;
	byte*		dest; 
	int 		x;
	int 		y; 
	patch_t*	patch;

	// DOOM border patch.
	//char		name1[] = "FLOOR7_2";

	// DOOM II border patch.
	//char		name2[] = "GRNROCK";	

	char*		name;

	// [RH] Always draws something now.

	if ( gamemode == commercial)
		name = "GRNROCK";
	else
		name = "FLOOR7_2";
	
	src = W_CacheLumpName (name, PU_CACHE); 
	dest = screens[1]; 

	for (y=0; y<SCREENHEIGHT; y++) 
	{ 
		for (x=0 ; x<SCREENWIDTH/64 ; x++) 
		{ 
			memcpy (dest, src+((y&63)<<6), 64); 
			dest += 64; 
		} 

		if (SCREENWIDTH&63) 
		{ 
			memcpy (dest, src+((y&63)<<6), SCREENWIDTH&63); 
			dest += (SCREENWIDTH&63); 
		} 
		dest += SCREENPITCH - SCREENWIDTH;
	} 

	if (scaledviewwidth == SCREENWIDTH)
		return;

	patch = W_CacheLumpName ("brdr_b",PU_CACHE);

	for (x=0 ; x<scaledviewwidth ; x+=8)
		V_DrawPatch (viewwindowx+x,viewwindowy+viewheight,1,patch);

	patch = W_CacheLumpName ("brdr_t",PU_CACHE);

	for (x=0 ; x<scaledviewwidth ; x+=8)
		V_DrawPatch (viewwindowx+x,viewwindowy-8,1,patch);
	patch = W_CacheLumpName ("brdr_l",PU_CACHE);

	for (y=0 ; y<viewheight ; y+=8)
		V_DrawPatch (viewwindowx-8,viewwindowy+y,1,patch);
	patch = W_CacheLumpName ("brdr_r",PU_CACHE);

	for (y=0 ; y<viewheight ; y+=8)
		V_DrawPatch (viewwindowx+scaledviewwidth,viewwindowy+y,1,patch);


	// Draw beveled edge. 
	V_DrawPatch (viewwindowx-8,
				 viewwindowy-8,
				 1,
				 W_CacheLumpName ("brdr_tl",PU_CACHE));
	
	V_DrawPatch (viewwindowx+scaledviewwidth,
				 viewwindowy-8,
				 1,
				 W_CacheLumpName ("brdr_tr",PU_CACHE));
	
	V_DrawPatch (viewwindowx-8,
				 viewwindowy+viewheight,
				 1,
				 W_CacheLumpName ("brdr_bl",PU_CACHE));
	
	V_DrawPatch (viewwindowx+scaledviewwidth,
				 viewwindowy+viewheight,
				 1,
				 W_CacheLumpName ("brdr_br",PU_CACHE));
} 
 

//
// Copy a screen buffer.
//
void
R_VideoErase
( unsigned		ofs,
  int			count ) 
{ 
  // LFB copy.
  // This might not be a good idea if memcpy
  //  is not optiomal, e.g. byte by byte on
  //  a 32bit CPU, as GNU GCC/Linux libc did
  //  at one point.
	memcpy (screens[0]+ofs, screens[1]+ofs, count); 
} 


//
// R_DrawViewBorder
// Draws the border around the view
//	for different size windows?
//
void
V_MarkRect
( int			x,
  int			y,
  int			width,
  int			height ); 
 
void R_DrawViewBorder (void) 
{ 
	int 		top;
	int 		side;
	int 		ofs;
	int 		i; 
 
	// [RH] Redraw the status bar if SCREENWIDTH > status bar width.
	// Will draw borders around itself, too.
	if (SCREENWIDTH > 320) {
		ST_Drawer (viewheight == SCREENHEIGHT, true);
	}

	if (scaledviewwidth == SCREENWIDTH) {
		return; 
	}
  
	top = ((SCREENHEIGHT-SBARHEIGHT)-viewheight)/2; 
	side = (SCREENWIDTH-scaledviewwidth)/2; 
 
	// copy top and one line of left side 
	R_VideoErase (0, top*SCREENPITCH+side); 
 
	// copy one line of right side and bottom 
	ofs = (viewheight+top)*SCREENPITCH-side; 
	R_VideoErase (ofs, top*SCREENPITCH+side); 
 
	// copy sides using wraparound 
	ofs = top*SCREENPITCH + SCREENWIDTH-side; 
	side <<= 1;
	
	for (i=1 ; i<viewheight ; i++) 
	{ 
		R_VideoErase (ofs, side); 
		ofs += SCREENPITCH; 
	} 

	// ? 
	V_MarkRect (0,0,SCREENWIDTH, SCREENHEIGHT-SBARHEIGHT); 
} 
 
 
