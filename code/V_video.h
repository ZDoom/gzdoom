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
//		Gamma correction LUT.
//		Functions to draw patches (by post) directly to screen.
//		Functions to blit a block to the screen.
//
//-----------------------------------------------------------------------------


#ifndef __V_VIDEO__
#define __V_VIDEO__

#include "doomtype.h"

#include "doomdef.h"

// Needed because we are refering to patches.
#include "r_data.h"

//
// VIDEO
//

#define CENTERY 				(SCREENHEIGHT>>1)


// Screen 0 is the screen updated by I_Update screen.
// Screen 1 is an extra buffer.



extern	byte*	screens[5];

extern	int 	dirtybox[4];

extern	byte	newgamma[256];
extern	double	gammalevel;

extern	byte*	TransTable;			// Translucency tables (minus 65536)

extern	int		CleanWidth, CleanHeight, CleanXfac, CleanYfac;

// Allocates buffer screens, call before R_Init.
void V_Init (void);

void V_Archive (void *f);

void
V_CopyRect
( int			srcx,
  int			srcy,
  int			srcscrn,
  int			width,
  int			height,
  int			destx,
  int			desty,
  int			destscrn );

void
V_DrawPatch
( int			x,
  int			y,
  int			scrn,
  patch_t*		patch);

void
V_DrawLucentPatch
( int			x,
  int			y,
  int			scrn,
  patch_t*		patch);

void
V_DrawPatchDirect
( int			x,
  int			y,
  int			scrn,
  patch_t*		patch );

void
V_DrawTranslatedPatchStretched
( int			x,
  int			y,
  int			scrn,
  patch_t*		patch,
  byte*			map,
  int			destwidth,
  int			destheight );

void
V_DrawPatchStretched
( int			x,
  int			y,
  int			scrn,
  patch_t*		patch,
  int			destwidth,
  int			destheight );

void
V_DrawPatchIndirect
( int			x,
  int			y,
  int			scrn,
  patch_t*		patch );

void
V_DrawPatchClean
( int			x,
  int			y,
  int			scrn,
  patch_t*		patch );

void
V_DrawPatchCleanNoMove
( int			x,
  int			y,
  int			scrn,
  patch_t*		patch );

void
V_DrawTranslatedPatchClean
( int			x,
  int			y,
  int			scrn,
  patch_t*		patch,
  byte*			map );

void
V_DrawTranslatedPatchCleanNoMove
( int			x,
  int			y,
  int			scrn,
  patch_t*		patch,
  byte*			map );


// Draw a linear block of pixels into the view buffer.
void
V_DrawBlock
( int			x,
  int			y,
  int			scrn,
  int			width,
  int			height,
  byte* 		src );

// Reads a linear block of pixels into the view buffer.
void
V_GetBlock
( int			x,
  int			y,
  int			scrn,
  int			width,
  int			height,
  byte* 		dest );

// Darken the entire screen
void V_DimScreen (int screen);

void
V_MarkRect
( int			x,
  int			y,
  int			width,
  int			height );

// BestColor
byte BestColor (const byte *palette, const int r, const int g, const int b);
// Returns the closest color to the one desired. String
// should be of the form "rrrr gggg bbbb".
int V_GetColorFromString (const byte *palette, const char *colorstring);

// Output a line of text using the console font
void V_PrintStr (int x, int y, byte *str, int count, byte ormask);
void V_PrintStr2 (int x, int y, byte *str, int count, byte ormask);

// Output some text with wad heads-up font
void V_DrawText (int x, int y, byte *str);
void V_DrawWhiteText (int x, int y, byte *str);
void V_DrawRedText (int x, int y, byte *str);
void V_DrawTextClean (int x, int y, byte *str);		// <- Does not adjust x and y
void V_DrawWhiteTextClean (int x, int y, byte *str);
void V_DrawRedTextClean (int x, int y, byte *str);

boolean V_SetResolution (int width, int height);

#ifdef USEASM
void ASM_PatchRowBytes (int bytes);
#endif

#endif
//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
