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


// [RH] Screens are no longer mere byte arrays.
screen_t screens[2];
 
int 	dirtybox[4];

cvar_t *vid_defwidth, *vid_defheight, *vid_defbpp;
cvar_t *dimamount, *dimcolor;

byte	*TransTable;

palette_t *DefaultPalette;


// [RH] Set true when vid_setmode command has been executed
BOOL	setmodeneeded = false;
// [RH] Resolution to change to when setmodeneeded is true
int		NewWidth, NewHeight, NewBpp;


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
	byte *fadetable;

	if (dimamount->value < 0.0)
		SetCVarFloat (dimamount, 0.0);
	else if (dimamount->value > 3.0)
		SetCVarFloat (dimamount, 3.0);

	if (dimamount->value == 0.0)
		return;

	if (screen->is8bit) {
		if (!TransTable)
			fadetable = DefaultPalette->maps.colormaps + (NUMCOLORMAPS/2) * 256;
		else
			fadetable = TransTable + 65536*(4-(int)dimamount->value) +
						256*V_GetColorFromString (DefaultPalette->basecolors, dimcolor->string);

		{
#ifdef	USEASM
			DimScreenPLoop (fadetable, screen->buffer, screen->width, screen->pitch-screen->width, screen->height);
#else
			unsigned int *spot, s;
			int x, y;
			byte a, b, c, d;

			spot = (unsigned int *)(screen->buffer);
			for (y = 0; y < screen->height; y++) {
				for (x = 0; x < (screen->width >> 2); x++) {
					s = *spot;
					a = fadetable[s & 0xff];
					b = fadetable[(s >> 8) & 0xff];
					c = fadetable[(s >> 16) & 0xff];
					d = fadetable[s >> 24];
					*spot++ = a | (b << 8) | (c << 16) | (d << 24);
				}
				spot += (screen->pitch - screen->width) >> 2;
			}
#endif
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
		Printf ("X11R6RGB lump not found\n");
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
		Printf ("Usage: setcolor <cvar> <color>\n");
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

void BuildTransTable (byte *transtab, unsigned int *palette)
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
							(RPART(palette[a]) + RPART(palette[b])) >> 1,
							(GPART(palette[a]) + GPART(palette[b])) >> 1,
							(BPART(palette[a]) + BPART(palette[b])) >> 1,
							256);
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
							(RPART(palette[a]) + RPART(palette[b]) * 3) >> 2,
							(GPART(palette[a]) + GPART(palette[b]) * 3) >> 2,
							(BPART(palette[a]) + BPART(palette[b]) * 3) >> 2,
							256);
			*trans++ = c;
			trans25[(b << 8) | a] = c;
		}
	}
}

void V_LockScreen (screen_t *screen)
{
	screen->lockcount++;
	if (screen->lockcount == 1) {
		I_LockScreen (screen);

		if (screen == &screens[0]) {
			if (dc_pitch != screen->pitch << detailyshift) {
				dc_pitch = screen->pitch << detailyshift;
				R_InitFuzzTable ();
#ifdef USEASM
				ASM_PatchPitch ();
#endif
			}

			if ((screen->is8bit ? 1 : 4) << detailxshift != ds_colsize) {
				ds_colsize = (screen->is8bit ? 1 : 4) << detailxshift;
				ds_colshift = (screen->is8bit ? 0 : 2) + detailxshift;
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
BOOL V_DoModeSetup (int width, int height, int bpp)
{
	int i;

	CleanXfac = width / 320;
	CleanYfac = height / 200;
	CleanWidth = width / CleanXfac;
	CleanHeight = height / CleanYfac;

	// [RH] Screens are no longer byte arrays
	for (i = 0; i < 2; i++)
		if (screens[i].impdata)
			V_FreeScreen (&screens[i]);

	I_SetMode (width, height, bpp);
	bpp = (bpp == 8) ? bpp : 32;

	for (i = 0; i < 2; i++) {
		if (!I_AllocateScreen (&screens[i], width, height, bpp))
			return false;
	}

	V_ForceBlend (0,0,0,0);
	if (bpp == 8)
		RefreshPalettes ();

	R_InitColumnDrawers (screens[0].is8bit);
	R_MultiresInit ();

	return true;
}

BOOL V_SetResolution (int width, int height, int Bpp)
{
	int oldwidth, oldheight;
	int oldBpp;

	if (screens[0].impdata) {
		oldwidth = screens[0].width;
		oldheight = screens[0].height;
		oldBpp = screens[0].Bpp * 8;
	} else {
		// Harmless if screens[0] wasn't allocated
		oldwidth = width;
		oldheight = height;
		oldBpp = Bpp;
	}

	if (!I_CheckResolution (width, height, Bpp)) {				// Try specified resolution
		if (!I_CheckResolution (oldwidth, oldheight, oldBpp)) {	// Try previous resolution (if any)
			if (!I_CheckResolution (320, 200, 8)) {				// Try "standard" resolution
				if (!I_CheckResolution (640, 480, 8)) {			// Try a resolution that *should* be available
					return false;
				} else {
					width = 640;
					height = 480;
					Bpp = 8;
				}
			} else {
				width = 320;
				height = 200;
				Bpp = 8;
			}
		} else {
			width = oldwidth;
			height = oldheight;
			Bpp = oldBpp;
		}
	}

	return V_DoModeSetup (width, height, Bpp);
}

void Cmd_Vid_SetMode (void *plyr, int argc, char **argv)
{
	BOOL	goodmode = false;
	int		width = 0, height = screens[0].height, bpp = screens[0].Bpp * 8;

	if (argc > 1) {
		width = atoi (argv[1]);
		if (argc > 2) {
			height = atoi (argv[2]);
			if (argc > 3)
				bpp = atoi (argv[3]);
		}
	}

	if (width) {
		if (I_CheckResolution (width, height, bpp))
			goodmode = true;
	}

	if (goodmode) {
		// The actual change of resolution will take place
		// near the beginning of D_Display().
		setmodeneeded = true;
		NewWidth = width;
		NewHeight = height;
		NewBpp = bpp;
	} else if (width) {
		Printf ("Unknown resolution %d x %d x %d\n", width, height, bpp);
	} else {
		Printf ("Usage: vid_setmode <width> <height> <bpp>\n");
	}
}

//
// V_Init
// 
void V_Init (void) 
{ 
	int i;
	int width, height, bpp;

	// [RH] Setup gamma callback
	gammalevel->u.callback = GammaCallback;
	GammaCallback (gammalevel);

	// [RH] Initialize palette subsystem
	Printf ("InitPalettes\n");
	if (!(DefaultPalette = InitPalettes ("PLAYPAL")))
		I_FatalError ("Could not start palette subsystem");

	width = height = bpp = 0;

	if ( (i = M_CheckParm ("-width")) )
		if (i < myargc - 1)
			width = atoi (myargv[i + 1]);

	if ( (i = M_CheckParm ("-height")) )
		if (i < myargc - 1)
		height = atoi (myargv[i + 1]);

	if ( (i = M_CheckParm ("-bpp")) )
		if (i < myargc - 1)
			bpp = atoi (myargv[i + 1]);

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

	if (bpp == 0) {
		bpp = (int)(vid_defbpp->value);
	}

	if (!V_SetResolution (width, height, bpp)) {
		I_FatalError ("Could not set resolution to %d x %d x %d", width, height, bpp);
	} else {
		Printf ("Resolution: %d x %d x %d\n", screens[0].width, screens[0].height, screens[0].Bpp * 8);
	}

	V_InitConChars (0xf7);

	{
		static int palpoop[256];
		byte *palette = W_CacheLumpName ("PLAYPAL", PU_CACHE);
		int i;

		for (i = 0; i < 256; i++) {
			palpoop[i] = (palette[0] << 16) |
						 (palette[1] << 8) |
						 (palette[2]);
			palette += 3;
		}

		V_Palette = palpoop;
	}

	if (!M_CheckParm ("-notrans")) {
		char cachename[256];
		byte *palette;
		BOOL isCached = false;
		FILE *cached;
		int i;

		// Align TransTable on a 64k boundary
		TransTable = Malloc (65536*3+65535);
		TransTable = (byte *)(((unsigned int)TransTable + 0xffff) & 0xffff0000);

		i = M_CheckParm("-transfile");
		if (i && i < myargc - 1) {
			strcpy (cachename, myargv[i+1]);
			DefaultExtension (cachename, ".tch");
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
						Printf ("Translucency cache was generated with a different palette\n");
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
			BuildTransTable (TransTable, DefaultPalette->basecolors);
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