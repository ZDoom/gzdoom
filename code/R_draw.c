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


// ?
#define MAXWIDTH						1120
#define MAXHEIGHT						832

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

// Color tables for different players,
//	translate a limited part to another
//	(color ramps used for suit colors)
//
byte			translations[3][256];


// [RH] Pointers to the different column drawers.
//		These get changed depending on the current
//		screen depth.
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
								 
#ifdef RANGECHECK 
	if (dc_x >= screens[0].width
		|| dc_yl < 0
		|| dc_yh >= screens[0].height) {
		Printf ("R_DrawColumnP_C: %i to %i at %i\n", dc_yl, dc_yh, dc_x);
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
		*dest = dc_colormap[dc_source[(frac>>FRACBITS)&dc_mask]];
		
		dest += dc_pitch; 
		frac += fracstep;
		
	} while (count--); 
} 
#endif	// USEASM

//
// Spectre/Invisibility.
//
// [RH] FUZZTABLE changed from 50 to 64
#define FUZZTABLE	64
#define FUZZOFF		(screens[0].pitch)


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

	V_LockScreen (&screens[0]);
	fuzzoff = FUZZOFF << detailyshift;
	V_UnlockScreen (&screens[0]);

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
	int 				count; 
	byte*				dest;
	byte*				map;

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

	map = DefaultPalette->maps.colormaps + 6*256;
	
#ifdef RANGECHECK 
	if (dc_x >= screens[0].width
		|| dc_yl < 0 || dc_yh >= screens[0].height)
	{
		I_Error ("R_DrawFuzzColumnP_C: %i to %i at %i",
				 dc_yl, dc_yh, dc_x);
	}
#endif


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
		
		dest += dc_pitch;
	} while (count--);

	fuzzpos = (fuzzpos + 3) & (FUZZTABLE - 1);
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
								 
#ifdef RANGECHECK 
	if (dc_x >= screens[0].width
		|| dc_yl < 0
		|| dc_yh >= screens[0].height)
	{
		I_Error ( "R_DrawTranslucentColumnP_C: %i to %i at %i",
				  dc_yl, dc_yh, dc_x);
	}
	
#endif 


	dest = ylookup[dc_yl] + columnofs[dc_x]; 

	// Looks familiar.
	fracstep = dc_iscale; 
	frac = dc_texturemid + (dc_yl-centery)*fracstep; 

	do 
	{
		*dest = dc_transmap[dc_colormap[dc_source[(frac>>FRACBITS)&dc_mask]] + ((*dest)<<8)];
		dest += dc_pitch;
		
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

void R_DrawTranslatedColumnP_C (void) 
{ 
	int 				count; 
	byte*				dest; 
	fixed_t 			frac;
	fixed_t 			fracstep;		 
 
	count = dc_yh - dc_yl; 
	if (count < 0) 
		return; 
								 
#ifdef RANGECHECK 
	if (dc_x >= screens[0].width
		|| dc_yl < 0
		|| dc_yh >= screens[0].height)
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
	do 
	{
		// Translation tables are used
		//	to map certain colorramps to other ones,
		//	used with PLAY sprites.
		// Thus the "green" ramp of the player 0 sprite
		//	is mapped to gray, red, black/indigo. 
		*dest = dc_colormap[dc_translation[dc_source[(frac>>FRACBITS) & dc_mask]]];
		dest += dc_pitch;
		
		frac += fracstep; 
	} while (count--); 
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
		|| ds_x2>=screens[0].width  
		|| ds_y>screens[0].height)
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
								 
#ifdef RANGECHECK 
	if (dc_x >= screens[0].width
		|| dc_yl < 0
		|| dc_yh >= screens[0].height) {
		Printf ("R_DrawColumnD_C: %i to %i at %i\n", dc_yl, dc_yh, dc_x);
		return;
	}
#endif 

	dest = (unsigned int *)(ylookup[dc_yl] + columnofs[dc_x]);

	fracstep = dc_iscale; 
	frac = dc_texturemid + (dc_yl-centery)*fracstep; 

	do 
	{
		*dest = dc_shademap[dc_source[(frac>>FRACBITS)&dc_mask]];
		
		dest += dc_pitch >> 2; 
		frac += fracstep;
		
	} while (count--); 
} 

void R_DrawFuzzColumnD_C (void) 
{ 
	int 				count;
	unsigned int*		dest;
	unsigned int		work;

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
	
#ifdef RANGECHECK
	if (dc_x >= screens[0].width
		|| dc_yl < 0 || dc_yh >= screens[0].height)
	{
		I_Error ("R_DrawFuzzColumnD_C: %i to %i at %i",
				 dc_yl, dc_yh, dc_x);
	}
#endif

	dest = (unsigned int *)(ylookup[dc_yl] + columnofs[dc_x]);

	// [RH] This is actually slightly brighter than
	//		the indexed version, but it's close enough.
	do
	{
		work = dest[fuzzoffset[fuzzpos]>>2];
		*dest = work - ((work >> 2) & 0x3f3f3f);

		// Clamp table lookup index.
		fuzzpos = (fuzzpos + 1) & (FUZZTABLE - 1);
		
		dest += dc_pitch >> 2;
	} while (count--);

	fuzzpos = (fuzzpos + 3) & (FUZZTABLE - 1);
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

#ifdef RANGECHECK 
	if (dc_x >= screens[0].width
		|| dc_yl < 0
		|| dc_yh >= screens[0].height)
	{
		I_Error ( "R_DrawTranslucentColumnD_C: %i to %i at %i",
				  dc_yl, dc_yh, dc_x);
	}
	
#endif 

	dest = (unsigned int *)(ylookup[dc_yl] + columnofs[dc_x]);

	fracstep = dc_iscale;
	frac = dc_texturemid + (dc_yl-centery)*fracstep; 

	do 
	{
		*dest = ((*dest >> 1) & 0x7f7f7f) +
				((dc_shademap[dc_source[(frac>>FRACBITS)&dc_mask]] >> 1) & 0x7f7f7f);
		dest += dc_pitch >> 2;
		
		frac += fracstep;
	} while (count--);
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

#ifdef RANGECHECK
	if (dc_x >= screens[0].width
		|| dc_yl < 0
		|| dc_yh >= screens[0].height)
	{
		I_Error ( "R_DrawTranslatedColumnD_C: %i to %i at %i",
				  dc_yl, dc_yh, dc_x);
	}
	
#endif


	dest = (unsigned int *)(ylookup[dc_yl] + columnofs[dc_x]);

	fracstep = dc_iscale;
	frac = dc_texturemid + (dc_yl-centery)*fracstep;

	// Here we do an additional index re-mapping.
	do
	{
		*dest = dc_shademap[dc_translation[dc_source[(frac>>FRACBITS) & dc_mask]]];
		dest += dc_pitch >> 2;
		
		frac += fracstep;
	} while (count--);
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
		|| ds_x2>=screens[0].width  
		|| ds_y>screens[0].height)
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

	do {
		spot = ((yfrac>>(16-6))&(63*64)) + ((xfrac>>16)&63);

		// Lookup pixel from flat texture tile,
		//  re-index using light/colormap.
		*dest = ((unsigned int *)ds_colormap)[ds_source[spot]];
		dest += ds_colsize >> 2;

		// Next step in u,v.
		xfrac += ds_xstep; 
		yfrac += ds_ystep;
	} while (--count);
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
extern byte *WhiteMap;
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
		"CRBLUE",
		"CRORANGE",
		"CRYELLOW",
		"CRBLUE2"
	};
	int i;
		
	translationtables = Z_Malloc (256*(MAXPLAYERS+11)+255, PU_STATIC, 0);
	translationtables = (byte *)(( (int)translationtables + 255 )& ~255);
	
	// [RH] Each player now gets their own translation table
	//		(soon to be palettes). These are set up during
	//		netgame arbitration and as-needed rather than
	//		in here. We do, however load some text translation
	//		tables from our PWAD (ala BOOM).

	for (i = 0; i < 256; i++)
		translationtables[i] = i;

	for (i = 1; i < MAXPLAYERS; i++)
		memcpy (translationtables + i*256, translationtables, 256);

	// Create the red->white table for text routines
	Ranges = translationtables + MAXPLAYERS*256;

	for (i = 0; i < 11; i++)
		W_ReadLump (W_GetNumForName (ranges[i]), Ranges + 256 * i);

	WhiteMap = Ranges + 2 * 256;
}

// [RH] Create player translations using the old method.
void R_BuildCompatiblePlayerTranslations (void)
{
	int i;

	// translate just the 16 green colors
	for (i = 0x70; i < 0x80; i++) {
		// map green ramp to gray, brown, red
		translationtables[i] = i;
		translationtables[i+256] = 0x60 + (i&0xf);
		translationtables [i+512] = 0x40 + (i&0xf);
		translationtables [i+768] = 0x20 + (i&0xf);
	}
}

// [RH] Create a player's translation table based on
//		a given mid-range color.
void R_BuildPlayerTranslation (int player, int color)
{
	if (olddemo) {
		R_BuildCompatiblePlayerTranslations ();
	} else {
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
}


//
// R_InitBuffer 
// Creats lookup tables that avoid
//	multiplies and other hazzles
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
	viewwindowx = (screens[0].width-(width<<detailxshift)) >> 1;

	// [RH] Adjust column offset according to bytes per pixel
	//		and detail mode
	xshift = (screens[0].is8bit) ? 0 : 2;
	xshift += detailxshift;

	// Column offset. For windows
	for (i=0 ; i<width ; i++)
		columnofs[i] = (viewwindowx + i) << xshift;

	// Same with base row offset.
	if ((width<<detailxshift) == screens[0].width)
		viewwindowy = 0;
	else
		viewwindowy = (ST_Y-(height<<detailyshift)) >> 1;

	V_LockScreen (&screens[0]);
	buffer = screens[0].buffer;
	pitch = screens[0].pitch;
	V_UnlockScreen (&screens[0]);

	// Precalculate all row offsets.
	for (i=0 ; i<height ; i++)
		ylookup[i] = buffer + ((i<<detailyshift)+viewwindowy)*pitch;
}




//
// R_FillBackScreen
// Fills the back screen with a pattern
//	for variable screen sizes
// Also draws a beveled edge.
//
void R_FillBackScreen (void)
{
	int 		x;
	int 		y;
	patch_t*	patch;
	
	V_LockScreen (&screens[1]);

	{
		int lump = R_FlatNumForName ((gamemode == commercial) ? "GRNROCK" : "FLOOR7_2");
		V_FlatFill (0,0,screens[1].width,screens[1].height,&screens[1],
					W_CacheLumpNum (lump + firstflat, PU_CACHE));
	}


	if (realviewwidth != screens[0].width) {
		patch = W_CacheLumpName ("brdr_b",PU_CACHE);

		for (x=0 ; x<realviewwidth ; x+=8)
			V_DrawPatch (viewwindowx+x,viewwindowy+realviewheight,&screens[1],patch);

		patch = W_CacheLumpName ("brdr_t",PU_CACHE);

		for (x=0 ; x<realviewwidth ; x+=8)
			V_DrawPatch (viewwindowx+x,viewwindowy-8,&screens[1],patch);
		patch = W_CacheLumpName ("brdr_l",PU_CACHE);

		for (y=0 ; y<realviewheight ; y+=8)
			V_DrawPatch (viewwindowx-8,viewwindowy+y,&screens[1],patch);
		patch = W_CacheLumpName ("brdr_r",PU_CACHE);

		for (y=0 ; y<realviewheight ; y+=8)
			V_DrawPatch (viewwindowx+realviewwidth,viewwindowy+y,&screens[1],patch);


		// Draw beveled edge.
		V_DrawPatch (viewwindowx-8,
					 viewwindowy-8,
					 &screens[1],
					 W_CacheLumpName ("brdr_tl",PU_CACHE));
		
		V_DrawPatch (viewwindowx+realviewwidth,
					 viewwindowy-8,
					 &screens[1],
					 W_CacheLumpName ("brdr_tr",PU_CACHE));
		
		V_DrawPatch (viewwindowx-8,
					 viewwindowy+realviewheight,
					 &screens[1],
					 W_CacheLumpName ("brdr_bl",PU_CACHE));
		
		V_DrawPatch (viewwindowx+realviewwidth,
					 viewwindowy+realviewheight,
					 &screens[1],
					 W_CacheLumpName ("brdr_br",PU_CACHE));
	}

	V_UnlockScreen (&screens[1]);
}
 

//
// Copy a screen buffer.
// [RH] R_VideoErase() now copies blocks instead of arrays
//
void R_VideoErase (int x1, int y1, int x2, int y2)
{
	V_Blit (&screens[1], x1, y1, x2 - x1, y2 - y1,
			&screens[0], x1, y1, x2 - x1, y2 - y1);
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

	// [RH] Redraw the status bar if SCREENWIDTH > status bar width.
	// Will draw borders around itself, too.
	if (screens[0].width > 320) {
		ST_Drawer (realviewheight == screens[0].height, true);
	}

	if (realviewwidth == screens[0].width) {
		return;
	}

	top = (ST_Y-realviewheight)>>1;
	side = (screens[0].width-realviewwidth)>>1;

	// copy top
	R_VideoErase (0, 0, screens[0].width, top);

	// copy bottom
	R_VideoErase (0, realviewheight+top, screens[0].width, ST_Y);

	// copy left
	R_VideoErase (0, top, side, realviewheight+top);

	// copy right
	R_VideoErase (viewwindowx+realviewwidth, top, screens[0].width, realviewheight+top);

	// ? 
	V_MarkRect (0,0,screens[0].width, ST_Y); 
}


// [RH] Double pixels in the view window horizontally
//		and/or vertically (or not at all).
void R_DetailDouble (void)
{
	if (detailxshift) {
		int rowsize = realviewwidth >> 2;
		int pitch = screens[0].pitch >> (2-detailyshift);
		int y,x;
		unsigned *line,a,b;

		line = (unsigned *)(screens[0].buffer + viewwindowy*screens[0].pitch + viewwindowx);
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

	if (detailyshift) {
		int rowsize = realviewwidth << ((screens[0].is8bit) ? 0 : 2);
		int y;
		byte *line;

		line = screens[0].buffer + viewwindowy*screens[0].pitch + viewwindowx;
		for (y = 0; y < viewheight; y++, line += screens[0].pitch<<1) {
			memcpy (line + screens[0].pitch, line, rowsize);
		}
	}
}

// [RH] Initialize the column drawer pointers
void R_InitColumnDrawers (BOOL is8bit)
{
	if (is8bit) {
#ifdef USEASM
		R_DrawColumn			= R_DrawColumnP_ASM;
		R_DrawFuzzColumn		= R_DrawFuzzColumnP_ASM;
		R_DrawTranslucentColumn = R_DrawTranslucentColumnP_ASM;
		R_DrawTranslatedColumn	= R_DrawTranslatedColumnP_C;
#else
		R_DrawColumn			= R_DrawColumnP_C;
		R_DrawFuzzColumn		= R_DrawFuzzColumnP_C;
		R_DrawTranslucentColumn = R_DrawTranslucentColumnP_C;
		R_DrawTranslatedColumn	= R_DrawTranslatedColumnP_C;
#endif
		R_DrawSpan				= R_DrawSpanP;
	} else {
#ifdef USEASM
		R_DrawColumn			= R_DrawColumnD_C;
		R_DrawFuzzColumn		= R_DrawFuzzColumnD_C;
		R_DrawTranslucentColumn = R_DrawTranslucentColumnD_C;
		R_DrawTranslatedColumn	= R_DrawTranslatedColumnD_C;
#else
		R_DrawColumn			= R_DrawColumnD_C;
		R_DrawFuzzColumn		= R_DrawFuzzColumnD_C;
		R_DrawTranslucentColumn = R_DrawTranslucentColumnD_C;
		R_DrawTranslatedColumn	= R_DrawTranslatedColumnD_C;
#endif
		R_DrawSpan				= R_DrawSpanD;
	}
}