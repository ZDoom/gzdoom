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
//		Gamma correction LUT stuff.
//		Functions to draw patches (by post) directly to screen.
//		Functions to blit a block to the screen.
//
//-----------------------------------------------------------------------------


static const char
rcsid[] = "$Id: v_video.c,v 1.5 1997/02/03 22:45:13 b1 Exp $";

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <malloc.h>
#include <math.h>

#include "i_system.h"
#include "i_video.h"
#include "r_local.h"
#include "r_draw.h"
#include "r_plane.h"
#include "r_state.h"

#include "doomdef.h"
#include "doomdata.h"
#include "doomstat.h"

#include "hu_stuff.h"

#include "m_argv.h"
#include "m_bbox.h"
#include "m_swap.h"

#include "v_video.h"

#include "w_wad.h"
#include "z_zone.h"

#include "c_cvars.h"

static byte ConChars[] =
#include "conchars.h"
;

#ifdef USEASM
extern byte *MutantCodeStart, *MutantCodeEnd;
#endif

byte	*WhiteMap;

// Each screen is [SCREENWIDTH*SCREENHEIGHT]; 
byte*	screens[5];
 
int 	dirtybox[4];

cvar_t *vid_defwidth, *vid_defheight;

byte	*TransTable;

byte	newgamma[256];
double	gammalevel = 0.0;


void V_Archive (void *f)
{
	// Archive gamma correction level
	fprintf ((FILE *)f, "gamma %g\n", gammalevel);
}

void Cmd_Gamma (void *plyr, int argc, char **argv)
{
	static boolean firsttime = true;
	double invgamma;
	int i;

	if (argc > 1) {
		invgamma = atof (argv[1]);
		if (gammalevel != invgamma && invgamma != 0.0) {
			// Only recalculate the gamma table if the new gamma
			// value is different from the old one and non-zero.
			
			// I found this formula on the web at
			// http://panda.mostang.com/sane/sane-gamma.html

			gammalevel = invgamma;
			invgamma = 1.0 / invgamma;

			for (i = 0; i < 256; i++) {
				newgamma[i] = (byte)(255.0 * pow (i / 255.0, invgamma));
			}
			I_SetPalette (NULL);
		}
	}

	if (!firsttime)
		Printf ("Gamma correction level: %g\n", gammalevel);

	firsttime = false;
}
						 
//
// V_MarkRect 
// 
void
V_MarkRect
( int			x,
  int			y,
  int			width,
  int			height ) 
{ 
	M_AddToBox (dirtybox, x, y); 
	M_AddToBox (dirtybox, x+width-1, y+height-1); 
} 
 

//
// V_CopyRect 
// 
void
V_CopyRect
( int			srcx,
  int			srcy,
  int			srcscrn,
  int			width,
  int			height,
  int			destx,
  int			desty,
  int			destscrn ) 
{ 
	byte*		src;
	byte*		dest; 
		 
#ifdef RANGECHECK 
	if (srcx<0
		||srcx+width >SCREENWIDTH
		|| srcy<0
		|| srcy+height>SCREENHEIGHT 
		||destx<0||destx+width >SCREENWIDTH
		|| desty<0
		|| desty+height>SCREENHEIGHT 
		|| (unsigned)srcscrn>4
		|| (unsigned)destscrn>4)
	{
		I_Error ("Bad V_CopyRect");
	}
#endif 
	V_MarkRect (destx, desty, width, height); 
		 
	src = screens[srcscrn]+SCREENPITCH*srcy+srcx; 
	dest = screens[destscrn]+SCREENPITCH*desty+destx; 

	for ( ; height>0 ; height--) 
	{ 
		memcpy (dest, src, width); 
		src += SCREENPITCH; 
		dest += SCREENPITCH; 
	} 
} 
 

//
// V_DrawPatch
// Masks a column based masked pic to the screen. 
//
void
V_DrawPatch
( int			x,
  int			y,
  int			scrn,
  patch_t*		patch ) 
{ 

	int 		count;
	int 		col; 
	column_t*	column; 
	byte*		desttop;
	byte*		dest;
	byte*		source; 
	int 		w; 
		 

	y -= SHORT(patch->topoffset); 
	x -= SHORT(patch->leftoffset); 
#ifdef RANGECHECK 
	if (x<0
		||x+SHORT(patch->width) >SCREENWIDTH
		|| y<0
		|| y+SHORT(patch->height)>SCREENHEIGHT 
		|| (unsigned)scrn>4)
	{
	  // Printf ("Patch at %d,%d exceeds LFB\n", x,y );
	  // No I_Error abort - what is up with TNT.WAD?
	  Printf ("V_DrawPatch: bad patch (ignored)\n");
	  return;
	}
#endif 
 
	if (!scrn)
		V_MarkRect (x, y, SHORT(patch->width), SHORT(patch->height)); 

	col = 0; 
	desttop = screens[scrn]+y*SCREENWIDTH+x; 
		 
	w = SHORT(patch->width); 

	for ( ; col<w ; x++, col++, desttop++)
	{ 
		column = (column_t *)((byte *)patch + LONG(patch->columnofs[col])); 
 
		// step through the posts in a column 
		while (column->topdelta != 0xff ) 
		{ 
			source = (byte *)column + 3; 
			dest = desttop + column->topdelta*SCREENWIDTH; 
			count = column->length; 
						 
			while (count--) 
			{ 
				*dest = *source++; 
				dest += SCREENPITCH; 
			} 
			column = (column_t *)(	(byte *)column + column->length 
									+ 4 ); 
		} 
	}					 
} 

void V_DrawTranslatedPatch (int x, int y, int scrn, patch_t *patch, byte *map)
{ 

	int 		count;
	int 		col; 
	column_t*	column; 
	byte*		desttop;
	byte*		dest;
	byte*		source; 
	int 		w; 
		 

	y -= SHORT(patch->topoffset); 
	x -= SHORT(patch->leftoffset); 
#ifdef RANGECHECK 
	if (x<0
		||x+SHORT(patch->width) >SCREENWIDTH
		|| y<0
		|| y+SHORT(patch->height)>SCREENHEIGHT 
		|| (unsigned)scrn>4)
	{
	  Printf ("V_DrawPatch: bad patch (ignored)\n");
	  return;
	}
#endif 
 
	if (!scrn)
		V_MarkRect (x, y, SHORT(patch->width), SHORT(patch->height)); 

	col = 0; 
	desttop = screens[scrn]+y*SCREENWIDTH+x; 
		 
	w = SHORT(patch->width); 

	for ( ; col<w ; x++, col++, desttop++) { 
		column = (column_t *)((byte *)patch + LONG(patch->columnofs[col])); 
 
		// step through the posts in a column 
		while (column->topdelta != 0xff ) { 
			source = (byte *)column + 3; 
			dest = desttop + column->topdelta*SCREENWIDTH; 
			count = column->length; 
						 
			while (count--) { 
				*dest = *(map + *source++);
				dest += SCREENPITCH; 
			} 
			column = (column_t *)(	(byte *)column + column->length 
									+ 4 ); 
		} 
	}					 
} 


//
// V_DrawPatchFlipped 
// Masks a column based masked pic to the screen.
// Flips horizontally, e.g. to mirror face.
//
void
V_DrawPatchFlipped
( int			x,
  int			y,
  int			scrn,
  patch_t*		patch ) 
{ 

	int 		count;
	int 		col; 
	column_t*	column; 
	byte*		desttop;
	byte*		dest;
	byte*		source; 
	int 		w; 
		 
	y -= SHORT(patch->topoffset); 
	x -= SHORT(patch->leftoffset); 
#ifdef RANGECHECK 
	if (x<0
		||x+SHORT(patch->width) >SCREENWIDTH
		|| y<0
		|| y+SHORT(patch->height)>SCREENHEIGHT 
		|| (unsigned)scrn>4)
	{
	  // fprintf( stderr, "Patch origin %d,%d exceeds LFB\n", x,y );
	  Printf ("Bad V_DrawPatch in V_DrawPatchFlipped (ignored)\n");
	}
#endif 
 
	if (!scrn)
		V_MarkRect (x, y, SHORT(patch->width), SHORT(patch->height)); 

	col = 0; 
	desttop = screens[scrn]+y*SCREENPITCH+x; 
		 
	w = SHORT(patch->width); 

	for ( ; col<w ; x++, col++, desttop++) 
	{ 
		column = (column_t *)((byte *)patch + LONG(patch->columnofs[w-1-col])); 
 
		// step through the posts in a column 
		while (column->topdelta != 0xff ) 
		{ 
			source = (byte *)column + 3; 
			dest = desttop + column->topdelta*SCREENPITCH; 
			count = column->length; 
						 
			while (count--) 
			{ 
				*dest = *source++; 
				dest += SCREENPITCH; 
			} 
			column = (column_t *)(	(byte *)column + column->length 
									+ 4 ); 
		} 
	}					 
} 
 


//
// V_DrawPatchDirect
// Draws directly to the screen on the pc. 
//
void
V_DrawPatchDirect
( int			x,
  int			y,
  int			scrn,
  patch_t*		patch ) 
{
	V_DrawPatch (x,y,scrn, patch); 
} 


//
// V_DrawPatchStretched
// Masks a column based masked pic to the screen
// stretching it to fot the given dimensions.
//
void
V_DrawPatchStretched
( int			x0,
  int			y0,
  int			scrn,
  patch_t*		patch,
  int			destwidth,
  int			destheight )
{
	
	int 		count;
	column_t*	column; 
	byte*		desttop;
	byte*		dest;
	byte*		source; 

	unsigned	xinc, yinc, col, w, c, ymul, xmul;

	if (destwidth == patch->width && destheight == patch->height) {
		V_DrawPatch (x0, y0, scrn, patch);
		return;
	}

	xinc = (patch->width << 16) / destwidth;
	yinc = (patch->height << 16) / destheight;
	xmul = (destwidth << 16) / patch->width;
	ymul = (destheight << 16) / patch->height;

	y0 -= (SHORT(patch->topoffset) * xmul) >> 16;
	x0 -= (SHORT(patch->leftoffset) * ymul) >> 16; 
#ifdef RANGECHECK 
	if (x0<0
		|| x0+SHORT(destwidth) >SCREENWIDTH
		|| y0<0
		|| y0+SHORT(destheight)>SCREENHEIGHT 
		|| (unsigned)scrn>4)
	{
	  // Printf ("Patch at %d,%d exceeds LFB\n", x0,y0 );
	  Printf ("V_DrawPatchStretched: bad patch (ignored)\n");
	  return;
	}
#endif 
 
	if (!scrn)
		V_MarkRect (x0, y0, SHORT(destwidth), SHORT(destheight));

	col = 0;
	desttop = screens[scrn]+y0*SCREENPITCH+x0;
		 
	w = destwidth * xinc;

	for ( ; col<w ; col += xinc, desttop++)
	{
		column = (column_t *)((byte *)patch + LONG(patch->columnofs[col >> 16]));

		// step through the posts in a column
		while (column->topdelta != 0xff ) 
		{ 
			source = (byte *)column + 3;
			dest = desttop + ((column->topdelta * ymul) >> 16) * SCREENPITCH;
			count = (column->length * ymul) >> 16;
			c = 0;
						 
			while (count--) 
			{
				*dest = source[c >> 16]; 
				dest += SCREENPITCH;
				c += yinc;
			}
			column = (column_t *)(	(byte *)column + column->length 
									+ 4 ); 
		}
	}					 
}


//
// V_DrawBlock
// Draw a linear block of pixels into the view buffer.
//
void
V_DrawBlock
( int			x,
  int			y,
  int			scrn,
  int			width,
  int			height,
  byte* 		src ) 
{ 
	byte*		dest; 
		 
#ifdef RANGECHECK 
	if (x<0
		||x+width >SCREENWIDTH
		|| y<0
		|| y+height>SCREENHEIGHT 
		|| (unsigned)scrn>4 )
	{
		I_Error ("Bad V_DrawBlock");
	}
#endif 
 
	V_MarkRect (x, y, width, height);
 
	dest = screens[scrn] + y*SCREENPITCH+x;

	if (width == SCREENWIDTH && SCREENWIDTH == SCREENPITCH) {
		memcpy (dest, src, width * height);
	} else {
		while (height--)
		{
			memcpy (dest, src, width);
			src += width;
			dest += SCREENPITCH;
		}
	}
} 
 


//
// V_GetBlock
// Gets a linear block of pixels from the view buffer.
//
void
V_GetBlock
( int			x,
  int			y,
  int			scrn,
  int			width,
  int			height,
  byte* 		dest ) 
{ 
	byte*		src; 
		 
#ifdef RANGECHECK 
	if (x<0
		||x+width >SCREENWIDTH
		|| y<0
		|| y+height>SCREENHEIGHT 
		|| (unsigned)scrn>4 )
	{
		I_Error ("Bad V_DrawBlock");
	}
#endif 
 
	src = screens[scrn] + y*SCREENPITCH+x; 

	while (height--) 
	{ 
		memcpy (dest, src, width); 
		src += SCREENPITCH; 
		dest += width; 
	} 
} 

//
// V_PrintStr
// Print a line of text using the console font
//
void V_PrintStr (int x, int y, byte *str, int count, byte ormask)
{
	byte *temp;
	long *writepos;
	long *charimg;
	int z;
	
	if (y > (SCREENHEIGHT - 8))
		return;

	if (x < 0) {
		int skip;

		skip = -(x - 7) / 8;
		x += skip * 8;
		if (count <= skip) {
			return;
		} else {
			count -= skip;
			str += skip;
		}
	}

	x &= ~3;

	while (count && x <= (SCREENWIDTH - 8)) {
		temp = screens[0] + x + y * SCREENPITCH;
		writepos = (long *)temp;
		charimg = (long *)&ConChars[((*str) | ormask) * 128];
		for (z = 0; z < 8; z++) {
			*writepos = (*writepos & charimg[2]) ^ charimg[0];
			writepos++;
			*writepos = (*writepos & charimg[3]) ^ charimg[1];
			writepos += (SCREENPITCH >> 2) - 1;
			charimg += 4;
		}
		str++;
		count--;
		x += 8;
	}
}

//
//		Write a string using the hu_font
//
extern patch_t *hu_font[HU_FONTSIZE];

void V_DrawText (int x, int y, byte *string)
{
	int 		w;
	byte*		ch;
	int 		c;
	int 		cx;
	int 		cy;
	byte*		map;
				

	ch = string;
	cx = x;
	cy = y;
		
	while(1) {
		c = *ch++;
		if (!c)
			break;
		if (c & 0x80) {
			map = NULL;
			c &= 0x7f;
		} else {
			map = WhiteMap;
		}

		if (c == '\n') {
			cx = x;
			cy += 12;
			continue;
		}

		c = toupper(c) - HU_FONTSTART;
		if (c < 0 || c>= HU_FONTSIZE) {
			cx += 4;
			continue;
		}
				
		w = SHORT (hu_font[c]->width);
		if (cx+w > SCREENWIDTH)
			break;
		if (!map) {
			V_DrawPatch (cx, cy, 0, hu_font[c]);
		} else {
			V_DrawTranslatedPatch (cx, cy, 0, hu_font[c], map);
		}
		cx+=w;
	}
}

void V_DrawWhiteText (int x, int y, byte *string)
{
	int 		w;
	byte*		ch;
	int 		c;
	int 		cx;
	int 		cy;
				

	ch = string;
	cx = x;
	cy = y;
		
	while(1) {
		c = *ch++;
		if (!c)
			break;
		c &= 0x7f;

		if (c == '\n') {
			cx = x;
			cy += 12;
			continue;
		}

		c = toupper(c) - HU_FONTSTART;
		if (c < 0 || c>= HU_FONTSIZE) {
			cx += 4;
			continue;
		}
				
		w = SHORT (hu_font[c]->width);
		if (cx+w > SCREENWIDTH)
			break;
		V_DrawTranslatedPatch (cx, cy, 0, hu_font[c], WhiteMap);
		cx+=w;
	}
}

void V_DrawRedText (int x, int y, byte *string)
{
	int 		w;
	byte*		ch;
	int 		c;
	int 		cx;
	int 		cy;
				

	ch = string;
	cx = x;
	cy = y;
		
	while (1) {
		c = *ch++;
		if (!c)
			break;
		c &= 0x7f;

		if (c == '\n') {
			cx = x;
			cy += 12;
			continue;
		}

		c = toupper(c) - HU_FONTSTART;
		if (c < 0 || c>= HU_FONTSIZE) {
			cx += 4;
			continue;
		}
				
		w = SHORT (hu_font[c]->width);
		if (cx+w > SCREENWIDTH)
			break;
		V_DrawPatch (cx, cy, 0, hu_font[c]);
		cx+=w;
	}
}

void V_DimScreen (int screen)
{
	unsigned int *spot, s;
	byte *fadetable = colormaps + 14 * 256;
	int x, y;
	byte a, b, c, d;

	spot = (unsigned int *)screens[screen];
	for (y = 0; y < SCREENHEIGHT; y++) {
		for (x = 0; x < (SCREENWIDTH >> 2); x++) {
			s = *spot;
			a = fadetable[s & 0xff];
			b = fadetable[(s >> 8) & 0xff];
			c = fadetable[(s >> 16) & 0xff];
			d = fadetable[s >> 24];
			*spot++ = a | (b << 8) | (c << 16) | (d << 24);
		}
		spot += (SCREENPITCH - SCREENWIDTH) >> 2;
	}
}

/*
===============
BestColor
(borrowed from Quake2 source: utils3/qdata/images.c)
===============
*/
byte BestColor (const byte *palette, const int r, const int g, const int b)
{
	int		i;
	int		dr, dg, db;
	int		bestdistortion, distortion;
	int		bestcolor;

//
// let any color go to 0 as a last resort
//
	bestdistortion = 256*256*4;
	bestcolor = 0;

	for (i = 0; i <= 255; i++)
	{
		dr = r - (int)palette[0];
		dg = g - (int)palette[1];
		db = b - (int)palette[2];
		palette += 3;
		distortion = dr*dr + dg*dg + db*db;
		if (distortion < bestdistortion)
		{
			if (!distortion)
				return i;		// perfect match

			bestdistortion = distortion;
			bestcolor = i;
		}
	}

	return bestcolor;
}

void BuildTransTable (byte *transtab, byte *palette)
{
	int a, b, c;
	byte *trans, *mtrans;
	
	trans = transtab;
	for (a = 0; a < 256; a++) {
		mtrans = transtab + a;
		for (b = 0; b < a; b++) {
			c = BestColor (palette,
							(palette[a*3+0] + palette[b*3+0]) >> 1,
							(palette[a*3+1] + palette[b*3+1]) >> 1,
							(palette[a*3+2] + palette[b*3+2]) >> 1);
			*trans++ = c;
			*mtrans = c;
			mtrans += 256;
		}
		*trans = a;
		trans += 256 - a;
	}
}

//
// V_SetResolution
//
boolean V_SetResolution (int width, int height)
{
	int i;

	if (!I_CheckResolution (width, height)) {					// Try specified resolution
		if (!I_CheckResolution (SCREENWIDTH, SCREENHEIGHT)) {	// Try previous resolution (if any)
			if (!I_CheckResolution (320, 200)) {				// Try "standard" resolution
				if (!I_CheckResolution (640, 480)) {			// Try a resolution that *should* be available
					return false;
				} else {
					width = 640;
					height = 480;
				}
			} else {
				width = 320;
				height = 200;
			}
		} else {
			width = SCREENWIDTH;
			height = SCREENHEIGHT;
		}
	}

	SCREENPITCH = SCREENWIDTH = width;
	SCREENHEIGHT = height;

	if (screens[0])
		free (screens[0]);

	// stick these in low dos memory on PCs
	if (screens[0] = I_AllocLow (SCREENWIDTH*SCREENHEIGHT*4)) {
		for (i=1 ; i<4 ; i++)
			screens[i] = screens[i-1] + SCREENWIDTH*SCREENHEIGHT;
	} else {
		return false;
	}

	R_InitFuzzTable ();

	if (!R_PlaneInitData ())
		return false;

	if (screenheightarray)
		free (screenheightarray);
	if (!(screenheightarray = malloc (sizeof(short) * SCREENWIDTH)))
		return false;

	if (xtoviewangle)
		free (xtoviewangle);
	if (!(xtoviewangle = malloc (sizeof(angle_t) * SCREENWIDTH)))
		return false;

#ifdef USEASM
	ASM_PatchRowBytes (SCREENPITCH);
	/* The above routine does some code-patching. Ouch. */
#ifdef _WIN32
	FlushInstructionCache (GetCurrentProcess(), MutantCodeStart, MutantCodeEnd - MutantCodeStart);
#endif
#endif

	return true;
}

//
// V_Init
// 
void V_Init (void) 
{ 
	int 		i;

	int width, height;

	width = height = 0;

	if (i = M_CheckParm ("-width")) {
		width = atoi (myargv[i + 1]);
	}

	if (i = M_CheckParm ("-height")) {
		height = atoi (myargv[i + 1]);
	}

	if (width == 0) {
		if (height == 0) {
			width = (int)(vid_defwidth->value);
			height = (int)(vid_defheight->value);
		} else {
			width = (height * 8) / 6;
		}
	} else if (height == 0) {
		height = (width * 6) / 8;
	}

	if (!V_SetResolution (width, height)) {
		I_FatalError ("Could not set resolution to %d x %d", width, height);
	} else {
		Printf ("Resolution: %d x %d\n", SCREENWIDTH, SCREENHEIGHT);
	}

	if (!TransTable && !M_CheckParm ("-notrans")) {
		Printf ("Creating translucency table\n");
		// Align TransTable on a 64k boundary
		TransTable = Z_Malloc (65536+65535, PU_STATIC, 0);
		TransTable = (byte *)(((unsigned int)TransTable + 0xffff) & 0xffff0000);
		BuildTransTable (TransTable, W_CacheLumpName ("PLAYPAL", PU_CACHE));
	}
}
