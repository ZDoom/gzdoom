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
//		Functions to draw patches (by post) directly to screen.
//		Functions to blit a block to the screen.
//
//-----------------------------------------------------------------------------


#ifndef __V_VIDEO__
#define __V_VIDEO__

#include "doomtype.h"

#include "v_palett.h"

#include "doomdef.h"

// Needed because we are refering to patches.
#include "r_data.h"

//
// VIDEO
//
// [RH] Made screens more implementation-independant:
//
// V_LockScreen() must be called before drawing on a
// screen, and V_UnlockScreen() must be called when
// drawing is finished. As far as ZDoom is concerned,
// there are only two display depths: 8-bit indexed and
// 32-bit RGBA. The video driver is expected to be able
// to convert these to a format supported by the hardware.
// As such, the Bpp field is only used as an indicator
// of the output display depth and not as the screen's
// display depth (use is8bit for that).
struct screen_s {
	void	   *impdata;
	int			Bpp;		// *Bytes* per pixel
	byte	   *buffer;
	int			width;
	int			height;
	int			pitch;
	BOOL		is8bit;
	int			lockcount;
	palette_t  *palette;
};
typedef struct screen_s screen_t;

extern palette_t *DefaultPalette;

// Screen 0 is the screen updated by I_Update screen.
// Screen 1 is used to be an extra buffer for wipes.
extern	screen_t screens[1];

extern	int 	dirtybox[4];

extern	byte	newgamma[256];
extern	cvar_t	*gammalevel;

extern	byte*	TransTable;			// Translucency tables (minus 65536)

extern	int		CleanWidth, CleanHeight, CleanXfac, CleanYfac;

// Allocates buffer screens, call before R_Init.
void V_Init (void);

// [RH] Separated V_DrawPatch and all its derivatives into a
//		wrapper and the individual column drawers.
typedef void (*vdrawfunc) (byte *source, byte *dest, int count, int pitch);
typedef void (*vdrawsfunc) (byte *source, byte *dest, int count, int pitch, int yinc);

// Palettized versions of the column drawers
extern vdrawfunc vdrawPfuncs[6];
extern vdrawsfunc vdrawsPfuncs[6];

// Direct (true-color) versions of the column drawers
extern vdrawfunc vdrawDfuncs[6];
extern vdrawsfunc vdrawsDfuncs[6];

void V_DrawWrapper (int drawer, int x, int y, screen_t *scrn, patch_t *patch);
void V_DrawSWrapper (int drawer, int x, int y, screen_t *scrn, patch_t *patch, int destwidth, int destheight);
void V_DrawIWrapper (int drawer, int x, int y, screen_t *scrn, patch_t *patch);
void V_DrawCWrapper (int drawer, int x, int y, screen_t *scrn, patch_t *patch);
void V_DrawCNMWrapper (int drawer, int x, int y, screen_t *scrn, patch_t *patch);

// Just draws the patch as is
#define V_DRAWPATCH				0
// Mixes the patch with the background
#define V_DRAWLUCENTPATCH		1
// Color translates the patch and draws it
#define V_DRAWTRANSLATEDPATCH	2
// Translates the patch and mixes it with the background
#define V_DRAWTLATEDLUCENTPATCH	3
// Fills the patch area with a solid color
#define V_DRAWCOLOREDPATCH		4
// Mixes a solid color in the patch area with the background
#define V_DRAWCOLORLUCENTPATCH	5

// The color to fill with for #4 and #5 above
extern int V_ColorFill;

// The color map for #1 and #2 above
extern byte *V_ColorMap;

// The palette lookup table to be used with for direct modes
extern int *V_Palette;


#define V_DrawPatch(x,y,s,p) \
		V_DrawWrapper (V_DRAWPATCH, x, y, s, p)
#define V_DrawPatchStretched(x,y,s,p) \
		V_DrawSWrapper (V_DRAWPATCH, x, y, s, p)
#define V_DrawPatchDirect(x,y,s,p) \
		V_DrawWrapper (V_DRAWPATCH, x, y, s, p)
#define V_DrawPatchIndirect(x,y,s,p) \
		V_DrawIWrapper (V_DRAWPATCH, x, y, s, p)
#define V_DrawPatchClean(x,y,s,p) \
		V_DrawCWrapper (V_DRAWPATCH, x, y, s, p)
#define V_DrawPatchCleanNoMove(x,y,s,p) \
		V_DrawCNMWrapper (V_DRAWPATCH, x, y, s, p)

#define V_DrawLucentPatch(x,y,s,p) \
		V_DrawWrapper (V_DRAWLUCENTPATCH, x, y, s, p)
#define V_DrawLucentPatchStretched(x,y,s,p) \
		V_DrawSWrapper (V_DRAWLUCENTPATCH, x, y, s, p)
#define V_DrawLucentPatchIndirect(x,y,s,p) \
		V_DrawIWrapper (V_DRAWLUCENTPATCH, x, y, s, p)
#define V_DrawLucentPatchClean(x,y,s,p) \
		V_DrawWrapper (V_DRAWLUCENTPATCH, x, y, s, p)
#define V_DrawLucentPatchCleanNoMove(x,y,s,p) \
		V_DrawWrapper (V_DRAWLUCENTPATCH, x, y, s, p)

#define V_DrawTranslatedPatch(x,y,s,p) \
		V_DrawWrapper (V_DRAWTRANSLATEDPATCH, x, y, s, p)
#define V_DrawTranslatedPatchStretched(x,y,s,p) \
		V_DrawSWrapper (V_DRAWTRANSLATEDPATCH, x, y, s, p)
#define V_DrawTranslatedPatchIndirect(x,y,s,p) \
		V_DrawIWrapper (V_DRAWTRANLATEDPATCH, x, y, s, p)
#define V_DrawTranslatedPatchClean(x,y,s,p) \
		V_DrawCWrapper (V_DRAWTRANSLATEDPATCH, x, y, s, p)
#define V_DrawTranslatedPatchCleanNoMove(x,y,s,p) \
		V_DrawCNMWrapper (V_DRAWTRANSLATEDPATCH, x, y, s, p)

#define V_DrawTranslatedLucentPatch(x,y,s,p) \
		V_DrawWrapper (V_DRAWTLATEDLUCENTPATCH, x, y, s, p)
#define V_DrawTranslatedLucentPatchStretched(x,y,s,p) \
		V_DrawSWrapper (V_DRAWTLATEDLUCENTPATCH, x, y, s, p)
#define V_DrawTranslatedLucentPatchIndirect(x,y,s,p) \
		V_DrawIWrapper (V_DRAWTLATEDLUCENTPATCH, x, y, s, p)
#define V_DrawTranslatedLucentPatchClean(x,y,s,p) \
		V_DrawCWrapper (V_DRAWTLATEDLUCENTPATCH, x, y, s, p)
#define V_DrawTranslatedLucentPatchCleanNoMove(x,y,s,p) \
		V_DrawCNMWrapper (V_DRAWTLATEDLUCENTPATCH, x, y, s, p)

#define V_DrawColoredPatch(x,y,s,p) \
		V_DrawWrapper (V_DRAWCOLOREDPATCH, x, y, s, p)
#define V_DrawColoredPatchStretched(x,y,s,p) \
		V_DrawSWrapper (V_DRAWCOLOREDPATCH, x, y, s, p)
#define V_DrawColoredPatchIndirect(x,y,s,p) \
		V_DrawIWrapper (V_DRAWCOLOREDPATCH, x, y, s, p)
#define V_DrawColoredPatchClean(x,y,s,p) \
		V_DrawCWrapper (V_DRAWCOLOREDPATCH, x, y, s, p)
#define V_DrawColoredPatchCleanNoMove(x,y,s,p) \
		V_DrawCNMWrapper (V_DRAWCOLOREDPATCH, x, y, s, p)

#define V_DrawColoredLucentPatch(x,y,s,p) \
		V_DrawWrapper (V_DRAWCOLORLUCENTPATCH, x, y, s, p)
#define V_DrawColoredLucentPatchStretched(x,y,s,p) \
		V_DrawSWrapper (V_DRAWCOLORLUCENTPATCH, x, y, s, p)
#define V_DrawColoredLucentPatchIndirect(x,y,s,p) \
		V_DrawIWrapper (V_DRAWCOLORLUCENTPATCH, x, y, s, p)
#define V_DrawColoredLucentPatchClean(x,y,s,p) \
		V_DrawCWrapper (V_DRAWCOLORLUCENTPATCH, x, y, s, p)
#define V_DrawColoredLucentPatchCleanNoMove(x,y,s,p) \
		V_DrawCNMWrapper (V_DRAWCOLORLUCENTPATCH, x, y, s, p)


void V_CopyRect
( int			srcx,
  int			srcy,
  screen_t	   *srcscrn,
  int			width,
  int			height,
  int			destx,
  int			desty,
  screen_t	   *destscrn );


// Draw a linear block of pixels into the view buffer.
void
V_DrawBlock
( int			x,
  int			y,
  screen_t	   *scrn,
  int			width,
  int			height,
  byte* 		src );

// Reads a linear block of pixels into the view buffer.
void
V_GetBlock
( int			x,
  int			y,
  screen_t	   *scrn,
  int			width,
  int			height,
  byte* 		dest );

// Darken the entire screen
void V_DimScreen (screen_t *screen);

void
V_MarkRect
( int			x,
  int			y,
  int			width,
  int			height );

// BestColor
byte BestColor (const unsigned int *palette, const int r, const int g, const int b, const int numcolors);
// Returns the closest color to the one desired. String
// should be of the form "rrrr gggg bbbb".
int V_GetColorFromString (const unsigned int *palette, const char *colorstring);
// Scans through the X11R6RGB lump for a matching color
// and returns a color string suitable for V_GetColorFromString.
char *V_GetColorStringByName (const char *name);

// [RH] Fill an area with a 64x64 flat texture
void V_FlatFill (int left, int top, int right, int bottom, screen_t *scrn, byte *src);
// [RH] Set an area to a specified color
void V_Clear (int left, int top, int right, int bottom, screen_t *scrn, int color);

BOOL V_SetResolution (int width, int height, int bpp);

void V_LockScreen (screen_t *screen);
void V_UnlockScreen (screen_t *screen);
void V_Blit (screen_t *src, int srcx, int srcy, int srcwidth, int srcheight,
			 screen_t *dest, int destx, int desty, int destwidth, int destheight);

// Only used when blitting when at least on of the screens is 8-bit.
void V_AttachPalette (screen_t *screen, palette_t *pal);
// Converse
void V_DetachPalette (screen_t *screen);

BOOL I_AllocateScreen (screen_t *scrn, int width, int height, int bpp);
void I_FreeScreen (screen_t *scrn);
// #defines for consistancy
#define V_AllocScreen(s,x,y,d)	I_AllocateScreen(s,x,y,d)
#define V_FreeScreen(s)			I_FreeScreen(s)

#ifdef USEASM
void ASM_PatchPitch (void);
void ASM_PatchColSize (void);
#endif

#endif
//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
