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

#include <stdio.h>
#include "m_alloc.h"
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
#include "c_dispatch.h"
#include "cmdlib.h"

static byte *ConChars;

byte	*WhiteMap;

// Each screen is [SCREENWIDTH*SCREENHEIGHT]; 
byte*	screens[5];
 
int 	dirtybox[4];

cvar_t *vid_defwidth, *vid_defheight;
cvar_t *dimamount, *dimcolor;

byte	*TransTable;

byte	newgamma[256];
double	gammalevel = 0.0;

// Stretch values for V_DrawPatchClean()
int CleanXfac, CleanYfac;
// Virtual screen sizes as perpetuated by V_DrawPatchClean()
int CleanWidth, CleanHeight;

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
void V_MarkRect (int x, int y, int width, int height)
{ 
	M_AddToBox (dirtybox, x, y); 
	M_AddToBox (dirtybox, x+width-1, y+height-1); 
} 
 

//
// V_CopyRect 
// 
void V_CopyRect (int srcx, int srcy, int srcscrn, int width, int height, int destx, int desty, int destscrn)
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
void V_DrawPatch (int x, int y, int scrn, patch_t *patch)
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

void V_DrawLucentPatch (int x, int y, int scrn, patch_t *patch)
{ 

	int 		count;
	int 		col; 
	column_t*	column; 
	byte*		desttop;
	byte*		dest;
	byte*		source;
	byte*		map;
	int 		w; 
		 
	if (!TransTable) {
		V_DrawPatch (x, y, scrn, patch);
		return;
	}

	map = TransTable + 65536 * 2;

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
				*dest = map[(*dest)|((*source++)<<8)]; 
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
// [RH] This is only called in f_finale.c, so I changed it to behave
// much like V_DrawPatchIndirect() instead of adding yet another function
// solely to handle virtual stretching of this function.
//
void V_DrawPatchFlipped (int x0, int y0, int scrn, patch_t *patch)
{
	// I must be dumb or something...
	V_DrawPatchIndirect (x0, y0, scrn, patch);

/*
	int 		count;
	column_t*	column; 
	byte*		desttop;
	byte*		dest;
	byte*		source; 

	int			xinc, yinc, col, w, c, ymul, xmul, destwidth, destheight;

	destwidth = (SCREENWIDTH * SHORT(patch->width)) / 320;
	destheight = (SCREENHEIGHT * SHORT(patch->height)) / 200;

	xinc = (SHORT(patch->width) << 16) / destwidth;
	yinc = (SHORT(patch->height) << 16) / destheight;
	xmul = (destwidth << 16) / patch->width;
	ymul = (destheight << 16) / patch->height;

	y0 -= (SHORT(patch->topoffset) * ymul) >> 16;
	x0 -= (SHORT(patch->leftoffset) * xmul) >> 16;

#ifdef RANGECHECK 
	if (x0<0
		|| x0+destwidth >SCREENWIDTH
		|| y0<0
		|| y0+destheight>SCREENHEIGHT 
		|| (unsigned)scrn>4)
	{
	  //Printf ("Patch at %d,%d exceeds LFB\n", x0,y0 );
	  Printf ("V_DrawPatchFlipped: bad patch (ignored)\n");
	  return;
	}
#endif 
 
	if (!scrn)
		V_MarkRect (x0, y0, destwidth, destheight);

	col = 0;
	desttop = screens[scrn]+y0*SCREENPITCH+x0;
		 
	w = destwidth * xinc;

	for ( ; col<w ; col += xinc, desttop++)
	{
		column = (column_t *)((byte *)patch + LONG(patch->columnofs[SHORT(patch->width) - 1 - (col >> 16)]));

		// step through the posts in a column
		while (column->topdelta != 0xff ) 
		{ 
			source = (byte *)column + 3;
			dest = desttop + ((column->topdelta * ymul) >> 16) * SCREENPITCH;
			count = (column->length * ymul) >> 16;
			c = 0;
						 
			do
			{
				*dest = source[c >> 16]; 
				dest += SCREENPITCH;
				c += yinc;
			} while (--count);
			column = (column_t *)(	(byte *)column + column->length 
									+ 4 ); 
		}
	}
*/
}



//
// V_DrawPatchDirect
// Draws directly to the screen on the pc. 
//
void V_DrawPatchDirect (int x, int y, int scrn, patch_t *patch)
{
	V_DrawPatch (x,y,scrn, patch); 
} 


void V_DrawTranslatedPatchStretched (int x0, int y0, int scrn, patch_t *patch, byte *map, int destwidth, int destheight)
{
	
	int 		count;
	column_t*	column; 
	byte*		desttop;
	byte*		dest;
	byte*		source; 

	int			xinc, yinc, col, w, c, ymul, xmul;

	if (destwidth == patch->width && destheight == patch->height) {
		V_DrawPatch (x0, y0, scrn, patch);
		return;
	}

	xinc = (SHORT(patch->width) << 16) / SHORT(destwidth);
	yinc = (SHORT(patch->height) << 16) / SHORT(destheight);
	xmul = (destwidth << 16) / patch->width;
	ymul = (destheight << 16) / patch->height;

	y0 -= (SHORT(patch->topoffset) * ymul) >> 16;
	x0 -= (SHORT(patch->leftoffset) * xmul) >> 16;

#ifdef RANGECHECK 
	if (x0<0
		|| x0+destwidth >SCREENWIDTH
		|| y0<0
		|| y0+destheight>SCREENHEIGHT 
		|| (unsigned)scrn>4)
	{
	  //Printf ("Patch at %d,%d exceeds LFB\n", x0,y0 );
	  Printf ("V_DrawPatchStretched: bad patch (ignored)\n");
	  return;
	}
#endif 
 
	if (!scrn)
		V_MarkRect (x0, y0, destwidth, destheight);

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
						 
			do
			{
				*dest = *(map + source[c>>16]);
				dest += SCREENPITCH;
				c += yinc;
			} while (--count);
			column = (column_t *)(	(byte *)column + column->length 
									+ 4 ); 
		}
	}					 
}

//
// V_DrawPatchStretched
// Masks a column based masked pic to the screen
// stretching it to fit the given dimensions.
//
void V_DrawPatchStretched (int x0, int y0, int scrn, patch_t *patch, int destwidth, int destheight)
{
	
	int 		count;
	column_t*	column; 
	byte*		desttop;
	byte*		dest;
	byte*		source; 

	int			xinc, yinc, col, w, c, ymul, xmul;

	if (destwidth == SHORT(patch->width) && destheight == SHORT(patch->height)) {
		V_DrawPatch (x0, y0, scrn, patch);
		return;
	}

	if (destwidth == SCREENWIDTH && destheight == SCREENHEIGHT &&
		SHORT(patch->width) == 320 && SHORT(patch->height) == 200) {
		// Special case: Drawing a full-screen patch, so use
		// F_DrawPatchCol in f_finale.c, since it's faster.
		extern F_DrawPatchCol (int, patch_t *, int, int);

		for (w = 0; w < 320; w++)
			F_DrawPatchCol (w, patch, w, scrn);
		return;
	}

	xinc = (SHORT(patch->width) << 16) / destwidth;
	yinc = (SHORT(patch->height) << 16) / destheight;
	xmul = (destwidth << 16) / patch->width;
	ymul = (destheight << 16) / patch->height;

	y0 -= (SHORT(patch->topoffset) * ymul) >> 16;
	x0 -= (SHORT(patch->leftoffset) * xmul) >> 16;

#ifdef RANGECHECK 
	if (x0<0
		|| x0+destwidth >SCREENWIDTH
		|| y0<0
		|| y0+destheight>SCREENHEIGHT 
		|| (unsigned)scrn>4)
	{
	  //Printf ("Patch at %d,%d exceeds LFB\n", x0,y0 );
	  Printf ("V_DrawPatchStretched: bad patch (ignored)\n");
	  return;
	}
#endif 
 
	if (!scrn)
		V_MarkRect (x0, y0, destwidth, destheight);

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
						 
			do
			{
				*dest = source[c >> 16]; 
				dest += SCREENPITCH;
				c += yinc;
			} while (--count);
			column = (column_t *)(	(byte *)column + column->length 
									+ 4 ); 
		}
	}
}

//
// V_DrawPatchIndirect
// Like V_DrawPatchDirect except it will stretch the patches as
// needed for non-320x200 screens.
//
void V_DrawPatchIndirect (int x0, int y0, int scrn, patch_t *patch)
{
	if (SCREENWIDTH == 320 && SCREENHEIGHT == 200)
		V_DrawPatch (x0, y0, scrn, patch);
	else
		V_DrawPatchStretched ((SCREENWIDTH * x0) / 320, (SCREENHEIGHT * y0) / 200, scrn, patch,
					(SCREENWIDTH * patch->width) / 320, (SCREENHEIGHT * patch->height) / 200);
}

//
// V_DrawPatchClean
// Like V_DrawPatchIndirect, except it only uses integral multipliers.
//
void V_DrawPatchClean (int x0, int y0, int scrn, patch_t *patch)
{
	if (CleanXfac == 1 && CleanYfac == 1) {
		V_DrawPatch ((x0-160) + (SCREENWIDTH/2), (y0-100) + (SCREENHEIGHT/2), scrn, patch);
	} else {
		V_DrawPatchStretched ((x0-160)*CleanXfac+(SCREENWIDTH/2),
							  (y0-100)*CleanYfac+(SCREENHEIGHT/2),
							  scrn, patch, patch->width * CleanXfac, patch->height * CleanYfac);
	}
}

//
// V_DrawPatchCleanNoMove
// Like V_DrawPatchClean, except it doesn't adjust the x and y coordinates.
//
void V_DrawPatchCleanNoMove (int x0, int y0, int scrn, patch_t *patch)
{
	if (CleanXfac == 1 && CleanYfac == 1)
		V_DrawPatch (x0, y0, scrn, patch);
	else
		V_DrawPatchStretched (x0, y0, scrn, patch, patch->width * CleanXfac, patch->height * CleanYfac);
}
		
void V_DrawTranslatedPatchClean (int x0, int y0, int scrn, patch_t *patch, byte *map)
{
	if (CleanXfac == 1 && CleanYfac == 1) {
		V_DrawTranslatedPatch ((x0-160) + (SCREENWIDTH/2), (y0-100) + (SCREENHEIGHT/2), scrn, patch, map);
	} else {
		V_DrawTranslatedPatchStretched ((x0-160)*CleanXfac+(SCREENWIDTH/2),
							  (y0-100)*CleanYfac+(SCREENHEIGHT/2),
							  scrn, patch, map, patch->width * CleanXfac, patch->height * CleanYfac);
	}
}

void V_DrawTranslatedPatchCleanNoMove (int x0, int y0, int scrn, patch_t *patch, byte *map)
{
	if (CleanXfac == 1 && CleanYfac == 1) {
		V_DrawTranslatedPatch (x0, y0, scrn, patch, map);
	} else {
		V_DrawTranslatedPatchStretched (x0, y0, scrn, patch, map, patch->width * CleanXfac, patch->height * CleanYfac);
	}
}


//
// V_DrawBlock
// Draw a linear block of pixels into the view buffer.
//
void V_DrawBlock (int x, int y, int scrn, int width, int height, byte *src)
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
void V_GetBlock (int x, int y, int scrn, int width, int height, byte *dest)
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
	long *charimg;
	
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
	temp = screens[0] + y * SCREENPITCH;

	while (count && x <= (SCREENWIDTH - 8)) {
		charimg = (long *)&ConChars[((*str) | ormask) * 128];
		{
#ifdef USEASM
			extern PrintChar1P (long *charimg, byte *dest, int screenpitch);

			PrintChar1P (charimg, temp + x, SCREENPITCH);
#else
			int z;
			long *writepos;

			writepos = (long *)(temp + x);
			for (z = 0; z < 8; z++) {
				*writepos = (*writepos & charimg[2]) ^ charimg[0];
				writepos++;
				*writepos = (*writepos & charimg[3]) ^ charimg[1];
				writepos += (SCREENPITCH >> 2) - 1;
				charimg += 4;
			}
#endif
		}
		str++;
		count--;
		x += 8;
	}
}

//
// V_PrintStr2
// Same as V_PrintStr but doubles the size of every character.
//
void V_PrintStr2 (int x, int y, byte *str, int count, byte ormask)
{
	byte *temp;
	long *charimg;
	
	if (y > (SCREENHEIGHT - 16))
		return;

	if (x < 0) {
		int skip;

		skip = -(x - 15) / 16;
		x += skip * 16;
		if (count <= skip) {
			return;
		} else {
			count -= skip;
			str += skip;
		}
	}

	x &= ~3;
	temp = screens[0] + y * SCREENPITCH;

	while (count && x <= (SCREENWIDTH - 16)) {
		charimg = (long *)&ConChars[((*str) | ormask) * 128];
#ifdef USEASM
		if (UseMMX) {
			extern PrintChar2P_MMX (long *charimg, byte *dest, int screenpitch);

			PrintChar2P_MMX (charimg, temp + x, SCREENPITCH);
		} else
#endif
		{
			int z;
			byte *buildmask, *buildbits, *image;
			unsigned int m1, s1;
			unsigned int *writepos;

			writepos = (long *)(temp + x);
			buildbits = (byte *)&s1;
			buildmask = (byte *)&m1;
			image = (byte *)charimg;

			for (z = 0; z < 8; z++) {
				buildmask[0] = buildmask[1] = image[8];
				buildmask[2] = buildmask[3] = image[9];
				buildbits[0] = buildbits[1] = image[0];
				buildbits[2] = buildbits[3] = image[1];
				writepos[0] = (writepos[0] & m1) ^ s1;
				writepos[SCREENPITCH/4] = (writepos[SCREENPITCH/4] & m1) ^ s1;

				buildmask[0] = buildmask[1] = image[10];
				buildmask[2] = buildmask[3] = image[11];
				buildbits[0] = buildbits[1] = image[2];
				buildbits[2] = buildbits[3] = image[3];
				writepos[1] = (writepos[1] & m1) ^ s1;
				writepos[1+SCREENPITCH/4] = (writepos[1+SCREENPITCH/4] & m1) ^ s1;

				buildmask[0] = buildmask[1] = image[12];
				buildmask[2] = buildmask[3] = image[13];
				buildbits[0] = buildbits[1] = image[4];
				buildbits[2] = buildbits[3] = image[5];
				writepos[2] = (writepos[2] & m1) ^ s1;
				writepos[2+SCREENPITCH/4] = (writepos[2+SCREENPITCH/4] & m1) ^ s1;

				buildmask[0] = buildmask[1] = image[14];
				buildmask[2] = buildmask[3] = image[15];
				buildbits[0] = buildbits[1] = image[6];
				buildbits[2] = buildbits[3] = image[7];
				writepos[3] = (writepos[3] & m1) ^ s1;
				writepos[3+SCREENPITCH/4] = (writepos[3+SCREENPITCH/4] & m1) ^ s1;

				writepos += SCREENPITCH >> 1;
				image += 16;
			}

		}
		str++;
		count--;
		x += 16;
	}
#ifdef USEASM
	EndMMX();
#endif
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

void V_DrawTextClean (int x, int y, byte *string)
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
			cy += 12 * CleanYfac;
			continue;
		}

		c = toupper(c) - HU_FONTSTART;
		if (c < 0 || c>= HU_FONTSIZE) {
			cx += 4 * CleanXfac;
			continue;
		}
				
		w = SHORT (hu_font[c]->width) * CleanXfac;
		if (cx+w > SCREENWIDTH)
			break;
		if (!map) {
			V_DrawPatchCleanNoMove (cx, cy, 0, hu_font[c]);
		} else {
			V_DrawTranslatedPatchCleanNoMove (cx, cy, 0, hu_font[c], map);
		}
		cx += w;
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

void V_DrawWhiteTextClean (int x, int y, byte *string)
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
		if (cx+w > 320)
			break;
		V_DrawTranslatedPatchClean (cx, cy, 0, hu_font[c], WhiteMap);
		cx+=w;
	}
}

void V_DrawRedTextClean (int x, int y, byte *string)
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
		if (cx+w > 320)
			break;
		V_DrawPatchClean (cx, cy, 0, hu_font[c]);
		cx+=w;
	}
}

void V_DimScreen (int screen)
{
	byte *fadetable;

	if (dimamount->value < 0.0)
		SetCVarFloat (dimamount, 0.0);
	else if (dimamount->value > 3.0)
		SetCVarFloat (dimamount, 3.0);

	if (dimamount->value == 0.0)
		return;

	if (!TransTable)
		fadetable = colormaps + 16 * 256;
	else
		fadetable = TransTable + 65536*(4-(int)dimamount->value) + 256*V_GetColorFromString (W_CacheLumpName ("PLAYPAL", PU_CACHE), dimcolor->string);

	{
#ifdef	USEASM
		extern DimScreenPLoop (byte *colormap, byte *screen, int width, int modulo, int height);

		DimScreenPLoop (fadetable, screens[screen], SCREENWIDTH, SCREENPITCH-SCREENWIDTH, SCREENHEIGHT);
#else
		unsigned int *spot, s;
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
#endif
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

int V_GetColorFromString (const byte *palette, const char *cstr)
{
	int c[3], i, p;
	char val[5];
	const char *s, *g;

	val[4] = 0;
	for (s = cstr, i = 0; i < 3; i++) {
		c[i] = 0;
		while ((*s <= ' ') && (*s != 0))
			s++;
		if (*s) {
			p = 0;
			while (*s > ' ') {
				if (p < 4) {
					val[p++] = *s;
				}
				s++;
			}
			g = val;
			while (p < 4) {
				val[p++] = *g++;
			}
			c[i] = ParseHex (val);
		}
	}
	return BestColor (palette, c[0]>>8, c[1]>>8, c[2]>>8);
}

char *V_GetColorStringByName (const char *name)
{
	/* Note: The X11R6RGB lump used by this function *MUST* end
	 * with a NULL byte. This is so that COM_Parse is able to
	 * detect the end of the lump.
	 */
	char *rgbNames, *data, descr[5*3];
	int c[3], step;

	if (!(rgbNames = W_CacheLumpName ("X11R6RGB", PU_CACHE))) {
		Printf ("X11R6RGB lump not found\n");
		return NULL;
	}

	// skip past the header line
	data = strchr (rgbNames, '\n');
	step = 0;

	while (data = COM_Parse (data)) {
		if (step < 3) {
			c[step++] = atoi (com_token);
		} else {
			step = 0;
			if (*data >= ' ') {		// In case this name contains a space...
				char *newchar = com_token + strlen(com_token);

				while (*data >= ' ') {
					*newchar++ = *data++;
				}
				*newchar = 0;
			}
			
			if (!stricmp (com_token, name)) {
				sprintf (descr, "%04x %04x %04x",
						 (c[0] << 8) | c[0],
						 (c[1] << 8) | c[1],
						 (c[2] << 8) | c[2]);
				return copystring (descr);
			}
		}
	}
	Printf ("Unknown color \"%s\"\n", name);
	return NULL;
}

void Cmd_SetColor (void *plyr, int argc, char **argv)
{
	char *desc, setcmd[256], *name;

	if (argc < 3) {
		Printf ("Usage: setcolor <cvar> <color>\n");
		return;
	}

	if (name = BuildString (argc - 2, argv + 2)) {
		if (desc = V_GetColorStringByName (name)) {
			sprintf (setcmd, "set %s \"%s\"", argv[1], desc);
			AddCommandString (setcmd);
			free (desc);
		}
		free (name);
	}
}

void BuildTransTable (byte *transtab, byte *palette)
{
	int a, b, c;
	byte *trans25, *trans50, *trans75, *mtrans, *trans;
	
	trans25 = transtab;
	trans50 = transtab + 65536;
	trans75 = transtab + 131072;

	// Build the 50% translucency table
	trans = trans50;
	for (a = 0; a < 256; a++) {
		mtrans = trans50 + a;
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

	// Build the 25% and 75% tables
	trans = trans75;
	for (a = 0; a < 256; a++) {
		for (b = 0; b < 256; b++) {
			c = BestColor (palette,
							(palette[a*3+0] + palette[b*3+0] * 3) >> 2,
							(palette[a*3+1] + palette[b*3+1] * 3) >> 2,
							(palette[a*3+2] + palette[b*3+2] * 3) >> 2);
			*trans++ = c;
			trans25[(b << 8) | a] = c;
		}
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

	CleanXfac = SCREENWIDTH / 320;
	CleanYfac = SCREENHEIGHT / 200;
	CleanWidth = SCREENWIDTH / CleanXfac;
	CleanHeight = SCREENHEIGHT / CleanYfac;

	if (screens[0])
		free (screens[0]);

	// stick these in low dos memory on PCs
	if (screens[0] = I_AllocLow (SCREENWIDTH*SCREENHEIGHT*4)) {
		for (i=1 ; i<4 ; i++)
			screens[i] = screens[i-1] + SCREENWIDTH*SCREENHEIGHT;
	} else {
		return false;
	}

	R_MultiresInit ();

	if (screenheightarray)
		free (screenheightarray);
	screenheightarray = Malloc (sizeof(short) * SCREENWIDTH);

	if (xtoviewangle)
		free (xtoviewangle);
	xtoviewangle = Malloc (sizeof(angle_t) * SCREENWIDTH);

#ifdef USEASM
	ASM_PatchRowBytes (SCREENPITCH);
#endif

	return true;
}

static void InitConChars (const byte transcolor)
{
	byte *d, *s, v, *src;
	patch_t *chars;
	int x, y, z, a;

	chars = W_CacheLumpName ("CONCHARS", PU_CACHE);
	{
		long *screen, fill;

		fill = (transcolor << 24) | (transcolor << 16) | (transcolor << 8) | transcolor;
		for (y = 0; y < 128; y++) {
			screen = (long *)(screens[1] + SCREENPITCH * y);
			for (x = 0; x < 128/4; x++) {
				*screen++ = fill;
			}
		}
		V_DrawPatch (0, 0, 1, chars);
	}
	src = screens[1];
	if (ConChars = Malloc (256*8*8*2)) {
		d = ConChars;
		for (y = 0; y < 16; y++) {
			for (x = 0; x < 16; x++) {
				s = src + x * 8 + (y * 8 * SCREENPITCH);
				for (z = 0; z < 8; z++) {
					for (a = 0; a < 8; a++) {
						v = s[a];
						if (v == transcolor) {
							d[a] = 0x00;
							d[a+8] = 0xff;
						} else {
							d[a] = v;
							d[a+8] = 0x00;
						}
					}
					d += 16;
					s += SCREENPITCH;
				}
			}
		}
	}
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

	InitConChars (0xf7);

	if (!M_CheckParm ("-notrans")) {
		char cachename[256];
		byte *palette;
		boolean isCached = false;
		FILE *cached;
		int i;

		// Align TransTable on a 64k boundary
		TransTable = Malloc (65536*3+65535);
		TransTable = (byte *)(((unsigned int)TransTable + 0xffff) & 0xffff0000);

		i = M_CheckParm("-transfile");
		if (i && i < myargc - 1) {
			strcpy (cachename, myargv[i+1]);
			DefaultExtension (cachename, ".cch");
		} else {
			sprintf (cachename, "%stranstab.tch", progdir);
		}
		palette = W_CacheLumpName ("PLAYPAL", PU_CACHE);

		{	// Check for cached translucency table
			byte cachepal[768];

			cached = fopen (cachename, "rb");
			if (cached) {
				if (fread (cachepal, 1, 768, cached) == 768) {
					if (memcmp (cachepal, palette, 768)) {
						Printf ("Translucency cache is from another IWAD\n");
					} else {
						isCached = (fread (TransTable, 1, 65536*3, cached) == 65536*3);
					}
				}
				if (!isCached) {
					Printf ("Bad translucency cache file\n");
				}
			}
		}

		if (!isCached) {
			Printf ("Creating translucency tables\n");
			BuildTransTable (TransTable, W_CacheLumpName ("PLAYPAL", PU_CACHE));
			if (!cached) {
				cached = fopen (cachename, "wb");
				if (cached) {
					fwrite (palette, 1, 768, cached);
					fwrite (TransTable, 1, 65536*3, cached);
				}
			}
		}
		if (cached)
			fclose (cached);

		TransTable -= 65536;
	}
}
