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
//		Functions to draw patches (by post) directly to screen.
//		Functions to blit a block to the screen.
//
//-----------------------------------------------------------------------------



#include <stdio.h>

#include "minilzo.h"
// [RH] Output buffer size for LZO compression.
//		Extra space in case uncompressable.
#define OUT_LEN(a)		((a) + (a) / 64 + 16 + 3)

#include "m_alloc.h"

#include "i_system.h"
#include "i_video.h"
#include "r_local.h"
#include "r_draw.h"
#include "r_plane.h"
#include "r_state.h"

#include "doomdef.h"
#include "doomdata.h"
#include "doomstat.h"

#include "c_consol.h"
#include "hu_stuff.h"

#include "m_argv.h"
#include "m_bbox.h"
#include "m_swap.h"

#include "v_video.h"
#include "v_text.h"

#include "w_wad.h"
#include "z_zone.h"

#include "c_cvars.h"
#include "c_dispch.h"
#include "cmdlib.h"

extern void STACK_ARGS DimScreenPLoop (byte *colormap, byte *screen, int width, int modulo, int height);

extern char *IdStrings[22];
extern int DisplayID;

unsigned int Col2RGB8[65][256];
byte RGB8k[16][32][16];


// [RH] The framebuffer is no longer a mere byte array.
// There's also only one, not four.
screen_t screen;

int 	dirtybox[4];

cvar_t *vid_defwidth, *vid_defheight, *vid_defid;
cvar_t *dimamount, *dimcolor;

palette_t *DefaultPalette;


// [RH] Set true when vid_setmode command has been executed
BOOL	setmodeneeded = false;
// [RH] Resolution to change to when setmodeneeded is true
int		NewWidth, NewHeight, NewID;


//
// V_MarkRect 
// 
void V_MarkRect (int x, int y, int width, int height)
{ 
	M_AddToBox (dirtybox, x, y); 
	M_AddToBox (dirtybox, x+width-1, y+height-1); 
} 
 


// [RH] Fill an area with a 64x64 flat texture
//		right and bottom are one pixel *past* the boundaries they describe.
void V_FlatFill (int left, int top, int right, int bottom, screen_t *scrn, byte *src)
{
	int x, y;
	int advance;
	int width;

	width = right - left;
	right = width >> 6;

	if (scrn->is8bit) {
		byte *dest;

		advance = scrn->pitch - width;
		dest = scrn->buffer + top * scrn->pitch + left;

		for (y = top; y < bottom; y++)
		{
			for (x = 0; x < right; x++)
			{
				memcpy (dest, src + ((y&63)<<6), 64);
				dest += 64;
			}

			if (width & 63)
			{
				memcpy (dest, src + ((y&63)<<6), width & 63);
				dest += width & 63;
			}
			dest += advance;
		}
	} else {
		unsigned int *dest;
		int z;
		byte *l;

		advance = (scrn->pitch - (width << 2)) >> 2;
		dest = (unsigned int *)(scrn->buffer + top * scrn->pitch + (left << 2));

		for (y = top; y < bottom; y++) {
			l = src + ((y&63)<<6);
			for (x = 0; x < right; x++) {
				for (z = 0; z < 64; z += 4, dest += 4) {
					// Try and let the optimizer pair this on a Pentium
					// (even though VC++ doesn't anyway)
					dest[0] = V_Palette[l[z]];
					dest[1] = V_Palette[l[z+1]];
					dest[2] = V_Palette[l[z+2]];
					dest[3] = V_Palette[l[z+3]];
				}
			}

			if (width & 63) {
				// Do any odd pixel left over
				if (width & 1)
					*dest++ = V_Palette[l[0]];

				// Do the rest of the pixels
				for (z = 1; z < (width & 63); z += 2, dest += 2) {
					dest[0] = V_Palette[l[z]];
					dest[1] = V_Palette[l[z+1]];
				}
			}

			dest += advance;
		}
	}
}


// [RH] Set an area to a specified color
void V_Clear (int left, int top, int right, int bottom, screen_t *scrn, int color)
{
	int x, y;

	if (scrn->is8bit) {
		byte *dest;

		dest = scrn->buffer + top * scrn->pitch + left;
		x = right - left;
		for (y = top; y < bottom; y++) {
			memset (dest, color, x);
			dest += scrn->pitch;
		}
	} else {
		unsigned int *dest;

		dest = (unsigned int *)(scrn->buffer + top * scrn->pitch + (left << 2));
		right -= left;

		for (y = top; y < bottom; y++) {
			for (x = 0; x < right; x++) {
				dest[x] = color;
			}
			dest += scrn->pitch >> 2;
		}
	}
}


void V_DimScreen (screen_t *screen)
{
	if (dimamount->value < 0)
		SetCVarFloat (dimamount, 0);
	else if (dimamount->value > 1)
		SetCVarFloat (dimamount, 1);

	if (dimamount->value == 0)
		return;

	if (screen->is8bit) {
		unsigned int *bg2rgb;
		unsigned int fg;
		int gap;
		byte *spot;
		int x, y;

		{
			unsigned int *fg2rgb;
			fixed_t amount;

			amount = (fixed_t)(dimamount->value * 64);
			fg2rgb = Col2RGB8[amount];
			bg2rgb = Col2RGB8[64-amount];
			fg = fg2rgb[V_GetColorFromString (DefaultPalette->basecolors, dimcolor->string)];
		}

		spot = screen->buffer;
		gap = screen->pitch - screen->width;
		for (y = 0; y < screen->height; y++) {
			for (x = 0; x < screen->width; x++) {
				unsigned int bg = bg2rgb[*spot];
				bg = (fg+bg) | 0xf07c3e1f;
				*spot++ = RGB8k[0][0][(bg>>5) & (bg>>19)];
			}
			spot += gap;
		}
	} else {
		int x, y;
		int *line;
		int fill = V_GetColorFromString (NULL, dimcolor->string);

		line = (int *)(screen->buffer);

		if (dimamount->value == 1.0) {
			fill = (fill >> 2) & 0x3f3f3f;
			for (y = 0; y < screen->height; y++) {
				for (x = 0; x < screen->width; x++) {
					line[x] = (line[x] - ((line[x] >> 2) & 0x3f3f3f)) + fill;
				}
				line += screen->pitch >> 2;
			}
		} else if (dimamount->value == 2.0) {
			fill = (fill >> 1) & 0x7f7f7f;
			for (y = 0; y < screen->height; y++) {
				for (x = 0; x < screen->width; x++) {
					line[x] = ((line[x] >> 1) & 0x7f7f7f) + fill;
				}
				line += screen->pitch >> 2;
			}
		} else if (dimamount->value == 3.0) {
			fill = fill - ((fill >> 2) & 0x3f3f3f);
			for (y = 0; y < screen->height; y++) {
				for (x = 0; x < screen->width; x++) {
					line[x] = ((line[x] >> 2) & 0x3f3f3f) + fill;
				}
				line += screen->pitch >> 2;
			}
		}
	}
}

/*
===============
BestColor
(borrowed from Quake2 source: utils3/qdata/images.c)
===============
*/
byte BestColor (const unsigned int *palette, const int r, const int g, const int b, const int numcolors)
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

	for (i = 0; i < numcolors; i++)
	{
		dr = r - RPART(palette[i]);
		dg = g - GPART(palette[i]);
		db = b - BPART(palette[i]);
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

int V_GetColorFromString (const unsigned int *palette, const char *cstr)
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
	if (palette)
		return BestColor (palette, c[0]>>8, c[1]>>8, c[2]>>8, 256);
	else
		return ((c[0] << 8) & 0xff0000) |
			   ((c[1])      & 0x00ff00) |
			   ((c[2] >> 8));
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
		Printf (PRINT_HIGH, "X11R6RGB lump not found\n");
		return NULL;
	}

	// skip past the header line
	data = strchr (rgbNames, '\n');
	step = 0;

	while ( (data = COM_Parse (data)) ) {
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
	return NULL;
}

void Cmd_SetColor (void *plyr, int argc, char **argv)
{
	char *desc, setcmd[256], *name;

	if (argc < 3) {
		Printf (PRINT_HIGH, "Usage: setcolor <cvar> <color>\n");
		return;
	}

	if ( (name = BuildString (argc - 2, argv + 2)) ) {
		if ( (desc = V_GetColorStringByName (name)) ) {
			sprintf (setcmd, "set %s \"%s\"", argv[1], desc);
			AddCommandString (setcmd);
			free (desc);
		}
		free (name);
	}
}

// This is DOSDoom 0.65's translucency table code, cleaned up.
// I also removed the use of Allegro's RGB table code, because
// it just wasn't accurate enough.
void BuildTransTable (unsigned int *palette)
{
	{
		int r, g, b;

		// create the small RGB table
		for (r = 0; r < 16; r++)
			for (g = 0; g < 32; g++)
				for (b = 0; b < 16; b++)
					RGB8k[r][g][b] = BestColor (palette, r * 16, g * 8, b * 16, 256);
	}

	{
		int x, y;

		for (x = 0; x < 65; x++)
			for (y = 0; y < 256; y++)
				Col2RGB8[x][y] = (((RPART(palette[y])*x)>>5)<<9)  |
								 (((GPART(palette[y])*x)>>4)<<18) |
								  ((BPART(palette[y])*x)>>5);
	}
}

void V_LockScreen (screen_t *scrn)
{
	scrn->lockcount++;
	if (scrn->lockcount == 1) {
		I_LockScreen (scrn);

		if (scrn == &screen) {
			if (dc_pitch != scrn->pitch << detailyshift) {
				dc_pitch = scrn->pitch << detailyshift;
				R_InitFuzzTable ();
#ifdef USEASM
				ASM_PatchPitch ();
#endif
			}

			if ((scrn->is8bit ? 1 : 4) << detailxshift != ds_colsize) {
				ds_colsize = (scrn->is8bit ? 1 : 4) << detailxshift;
				ds_colshift = (scrn->is8bit ? 0 : 2) + detailxshift;
#ifdef USEASM
				ASM_PatchColSize ();
#endif
			}
		}
	}
}

void V_UnlockScreen (screen_t *screen)
{
	if (screen->lockcount)
		if (--screen->lockcount == 0)
			I_UnlockScreen (screen);
}

void V_Blit (screen_t *src, int srcx, int srcy, int srcwidth, int srcheight,
			 screen_t *dest, int destx, int desty, int destwidth, int destheight)
{
	I_Blit (src, srcx, srcy, srcwidth, srcheight, dest, destx, desty, destwidth, destheight);
}


//
// V_SetResolution
//
BOOL V_DoModeSetup (int width, int height, int id)
{
	int bpp;

	CleanXfac = width / 320;
	CleanYfac = height / 200;
	CleanWidth = width / CleanXfac;
	CleanHeight = height / CleanYfac;

	// Free the virtual framebuffer
	V_FreeScreen (&screen);

	I_SetMode (width, height, id);
	bpp = (id == 1010) ? 8 : 32;

	// Allocate a new virtual framebuffer
	I_AllocateScreen (&screen, width, height, bpp);

	V_ForceBlend (0,0,0,0);
	if (bpp == 8)
		RefreshPalettes ();

	R_InitColumnDrawers (screen.is8bit);
	R_MultiresInit ();

	return true;
}

BOOL V_SetResolution (int width, int height, int id)
{
	int oldwidth, oldheight;
	int oldID;

	if (screen.impdata) {
		oldwidth = screen.width;
		oldheight = screen.height;
		oldID = DisplayID;
	} else {
		// Harmless if screen wasn't allocated
		oldwidth = width;
		oldheight = height;
		oldID = id;
	}

	if (!I_CheckResolution (width, height, id)) {				// Try specified resolution
		if (!I_CheckResolution (oldwidth, oldheight, oldID)) {	// Try previous resolution (if any)
			if (!I_CheckResolution (320, 200, 1010)) {			// Try "standard" resolution
				if (!I_CheckResolution (640, 480, 1010)) {		// Try a resolution that *should* be available
					return false;
				} else {
					width = 640;
					height = 480;
					id = 1010;
				}
			} else {
				width = 320;
				height = 200;
				id = 1010;
			}
		} else {
			width = oldwidth;
			height = oldheight;
			id = oldID;
		}
	}

	return V_DoModeSetup (width, height, id);
}

void Cmd_Vid_SetMode (void *plyr, int argc, char **argv)
{
	BOOL	goodmode = false;
	int		width = 0, height = screen.height;
	int		id = DisplayID;

	if (argc > 1) {
		width = atoi (argv[1]);
		if (argc > 2) {
			height = atoi (argv[2]);
			if (argc > 3) {
				int i;

				for (i = 0; i < 22; i++)
					if (!stricmp (IdStrings[i], argv[3]))
						break;

				if (i < 22)
					id = 1000 + i;
			}
		}
	}

	if (width) {
		if (I_CheckResolution (width, height, id))
			goodmode = true;
	}

	if (goodmode) {
		// The actual change of resolution will take place
		// near the beginning of D_Display().
		if (gamestate != GS_STARTUP) {
			setmodeneeded = true;
			NewWidth = width;
			NewHeight = height;
			NewID = id;
		}
	} else if (width) {
		Printf (PRINT_HIGH, "Unknown resolution %d x %d (%s)\n", width, height, IdStrings[id-1000]);
	} else {
		Printf (PRINT_HIGH, "Usage: vid_setmode <width> <height> <mode>\n");
	}
}

//
// V_Init
//
extern char *IdStrings[22];
extern int DisplayID;

static int IdNameToId (char *name)
{
	int i;

	for (i = 0; i < 22; i++) {
		if (!stricmp (IdStrings[0], name))
			break;
	}

	if (i < 22)
		return i + 1000;
	else
		return 1010;	// INDEX8
}

void V_Init (void) 
{ 
	int i;
	int width, height, id;

	// [RH] Setup gamma callback
	gammalevel->u.callback = GammaCallback;
	GammaCallback (gammalevel);

	// [RH] Initialize palette subsystem
	if (!(DefaultPalette = InitPalettes ("PLAYPAL")))
		I_FatalError ("Could not initialize palette");

	width = height = id = 0;

	if ( (i = M_CheckParm ("-width")) )
		if (i < myargc - 1)
			width = atoi (myargv[i + 1]);

	if ( (i = M_CheckParm ("-height")) )
		if (i < myargc - 1)
			height = atoi (myargv[i + 1]);

	if ( (i = M_CheckParm ("-mode")) )
		if (i < myargc - 1)
			id = IdNameToId (myargv[i + 1]);

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

	if (id == 0) {
		id = IdNameToId (vid_defid->string);
	}

	if (!V_SetResolution (width, height, id)) {
		I_FatalError ("Could not set resolution to %d x %d (%s)", width, height, IdStrings[id-1000]);
	} else {
		Printf (PRINT_HIGH, "Resolution: %d x %d (%s)\n", screen.width, screen.height, IdStrings[id-1000]);
	}

	V_InitConChars (0xf7);
	C_InitConsole (screen.width, screen.height, true);

	V_Palette = DefaultPalette->colors;

	BuildTransTable (DefaultPalette->basecolors);
}

void V_AttachPalette (screen_t *scrn, palette_t *pal)
{
	if (scrn->palette == pal)
		return;

	V_DetachPalette (scrn);

	pal->usecount++;
	scrn->palette = pal;
}

void V_DetachPalette (screen_t *scrn)
{
	if (scrn->palette) {
		FreePalette (scrn->palette);
		scrn->palette = NULL;
	}
}