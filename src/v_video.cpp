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
//		Functions to draw patches (by post) directly to screen->
//		Functions to blit a block to the screen->
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

#include "c_console.h"
#include "hu_stuff.h"

#include "m_argv.h"
#include "m_bbox.h"
#include "m_swap.h"
#include "m_menu.h"

#include "i_video.h"
#include "v_video.h"
#include "v_text.h"

#include "w_wad.h"

#include "c_cvars.h"
#include "c_dispatch.h"
#include "cmdlib.h"
#include "gi.h"
#include "templates.h"

IMPLEMENT_ABSTRACT_CLASS (DCanvas)
IMPLEMENT_ABSTRACT_CLASS (DFrameBuffer)

// SimpleCanvas is not really abstract, but this macro does not
// try to generate a CreateNew() function.
IMPLEMENT_ABSTRACT_CLASS (DSimpleCanvas)

int DisplayWidth, DisplayHeight, DisplayBits;

FFont *SmallFont, *SmallFont2, *BigFont, *ConFont;

extern "C" {
DWORD *Col2RGB8_LessPrecision[65];
DWORD Col2RGB8[65][256];
byte RGB32k[32][32][32];
}

static DWORD Col2RGB8_2[63][256];

// [RH] The framebuffer is no longer a mere byte array.
// There's also only one, not four.
DFrameBuffer *screen;

CVAR (Int, vid_defwidth, 640, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Int, vid_defheight, 480, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Int, vid_defbits, 8, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Bool, vid_fps, false, 0)
CVAR (Bool, ticker, false, 0)
CVAR (Bool, vid_showpalette, false, 0)

CUSTOM_CVAR (Float, dimamount, 0.2f, CVAR_ARCHIVE)
{
	if (self < 0.f)
	{
		self = 0.f;
	}
	else if (self > 1.f)
	{
		self = 1.f;
	}
}
CVAR (Color, dimcolor, 0xffd700, CVAR_ARCHIVE)

// [RH] Set true when vid_setmode command has been executed
BOOL	setmodeneeded = false;
// [RH] Resolution to change to when setmodeneeded is true
int		NewWidth, NewHeight, NewBits;


//
// V_MarkRect 
// 
void V_MarkRect (int x, int y, int width, int height)
{
}

DCanvas *DCanvas::CanvasChain = NULL;

DCanvas::DCanvas (int _width, int _height)
{
	// Init member vars
	Buffer = NULL;
	Font = NULL;
	LockCount = 0;
	Width = _width;
	Height = _height;

	// Add to list of active canvases
	Next = CanvasChain;
	CanvasChain = this;
}

DCanvas::~DCanvas ()
{
	// Remove from list of active canvases
	DCanvas *probe = CanvasChain, **prev;

	prev = &CanvasChain;
	probe = CanvasChain;

	while (probe != NULL)
	{
		if (probe == this)
		{
			*prev = probe->Next;
			break;
		}
		prev = &probe->Next;
		probe = probe->Next;
	}
}

bool DCanvas::IsValid ()
{
	// A nun-subclassed DCanvas is never valid
	return false;
}

// [RH] Fill an area with a 64x64 flat texture
//		right and bottom are one pixel *past* the boundaries they describe.
void DCanvas::FlatFill (int left, int top, int right, int bottom, FTexture *src)
{/*
	int x, y;
	int advance;
	int width;

	width = right - left;
	right = width >> 6;

	byte *dest;

	advance = Pitch - width;
	dest = Buffer + top * Pitch + left;

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
	}*/
	int w = src->GetWidth();
	int h = src->GetHeight();

	// Repeatedly draw the texture, left-to-right, top-to-bottom. The
	// texture is positioned so that no matter what coordinates you pass
	// to FlatFill, the origin of the repeating pattern is always (0,0).
	for (int y = top / h * h; y < bottom; y += h)
	{
		for (int x = left / w * w; x < right; x += w)
		{
			DrawTexture (src, x, y,
				DTA_ClipLeft, left,
				DTA_ClipRight, right,
				DTA_ClipTop, top,
				DTA_ClipBottom, bottom,
				TAG_DONE);
		}
	}
}


// [RH] Set an area to a specified color
void DCanvas::Clear (int left, int top, int right, int bottom, int color) const
{
	int x, y;
	byte *dest;

	dest = Buffer + top * Pitch + left;
	x = right - left;
	for (y = top; y < bottom; y++)
	{
		memset (dest, color, x);
		dest += Pitch;
	}
}

void DCanvas::Dim () const
{
	float amount = dimamount;

	if (gameinfo.gametype == GAME_Hexen && gamestate == GS_DEMOSCREEN)
	{ // On the Hexen title screen, the default dimming is not
		// enough to make the menus readable.
		amount = MIN<float> (1.f, amount*2.f);
	}
	Dim (PalEntry(dimcolor), amount, 0, 0, Width, Height);
}

void DCanvas::Dim (PalEntry color, float damount, int x1, int y1, int w, int h) const
{
	if (damount == 0.f)
		return;

	DWORD *bg2rgb;
	DWORD fg;
	int gap;
	byte *spot;
	int x, y;

	{
		int amount;

		amount = (int)(damount * 64);
		bg2rgb = Col2RGB8[64-amount];

		fg = (((color.r * amount) >> 4) << 20) |
			  ((color.g * amount) >> 4) |
			 (((color.b * amount) >> 4) << 10);
	}

	spot = Buffer + x1 + y1*Pitch;
	gap = Pitch - w;
	for (y = h; y != 0; y--)
	{
		for (x = w; x != 0; x--)
		{
			DWORD bg;

			bg = bg2rgb[(*spot)&0xff];
			bg = (fg+bg) | 0x1f07c1f;
			*spot = RGB32k[0][0][bg&(bg>>15)];
			spot++;
		}
		spot += gap;
	}
}

int V_GetColorFromString (const DWORD *palette, const char *cstr)
{
	int c[3], i, p;
	char val[5];
	const char *s, *g;

	val[4] = 0;
	for (s = cstr, i = 0; i < 3; i++)
	{
		c[i] = 0;
		while ((*s <= ' ') && (*s != 0))
			s++;
		if (*s)
		{
			p = 0;
			while (*s > ' ')
			{
				if (p < 4)
				{
					val[p++] = *s;
				}
				s++;
			}
			g = val;
			while (p < 4)
			{
				val[p++] = *g++;
			}
			c[i] = ParseHex (val);
		}
	}
	if (palette)
		return ColorMatcher.Pick (c[0]>>8, c[1]>>8, c[2]>>8);
	else
		return ((c[0] << 8) & 0xff0000) |
			   ((c[1])      & 0x00ff00) |
			   ((c[2] >> 8));
}

char *V_GetColorStringByName (const char *name)
{
	FMemLump rgbNames;
	char *rgbEnd;
	char *rgb, *endp;
	char descr[5*3];
	int rgblump;
	int c[3], step;
	size_t namelen;

	rgblump = Wads.CheckNumForName ("X11R6RGB");
	if (rgblump == -1)
	{
		Printf ("X11R6RGB lump not found\n");
		return NULL;
	}

	rgbNames = Wads.ReadLump (rgblump);
	rgb = (char *)rgbNames.GetMem();
	rgbEnd = rgb + Wads.LumpLength (rgblump);
	step = 0;
	namelen = strlen (name);

	while (rgb < rgbEnd)
	{
		// Skip white space
		if (*rgb <= ' ')
		{
			do
			{
				rgb++;
			} while (rgb < rgbEnd && *rgb <= ' ');
		}
		else if (step == 0 && *rgb == '!')
		{ // skip comment lines
			do
			{
				rgb++;
			} while (rgb < rgbEnd && *rgb != '\n');
		}
		else if (step < 3)
		{ // collect RGB values
			c[step++] = strtoul (rgb, &endp, 10);
			if (endp == rgb)
			{
				break;
			}
			rgb = endp;
		}
		else
		{ // Check color name
			endp = rgb;
			// Find the end of the line
			while (endp < rgbEnd && *endp != '\n')
				endp++;
			// Back up over any whitespace
			while (endp > rgb && *endp <= ' ')
				endp--;
			if (endp == rgb)
			{
				break;
			}
			size_t checklen = ++endp - rgb;
			if (checklen == namelen && strnicmp (rgb, name, checklen) == 0)
			{
				sprintf (descr, "%02x %02x %02x", c[0], c[1], c[2]);
				return copystring (descr);
			}
			rgb = endp;
			step = 0;
		}
	}
	if (rgb < rgbEnd)
	{
		Printf ("X11R6RGB lump is corrupt\n");
	}
	return NULL;
}

int V_GetColor (const DWORD *palette, const char *str)
{
	char *string = V_GetColorStringByName (str);
	int res;

	if (string != NULL)
	{
		res = V_GetColorFromString (palette, string);
		delete[] string;
	}
	else
	{
		res = V_GetColorFromString (palette, str);
	}
	return res;
}

CCMD (setcolor)
{
	char *desc, setcmd[256];

	if (argv.argc() < 3)
	{
		Printf ("Usage: setcolor <cvar> <color>\n");
		return;
	}

	if ( (desc = V_GetColorStringByName (argv[2])) )
	{
		sprintf (setcmd, "set %s \"%s\"", argv[1], desc);
		C_DoCommand (setcmd);
		delete[] desc;
	}
}

// Build the tables necessary for blending
static void BuildTransTable (const PalEntry *palette)
{
	int r, g, b;

	// create the RGB555 lookup table
	for (r = 0; r < 32; r++)
		for (g = 0; g < 32; g++)
			for (b = 0; b < 32; b++)
				RGB32k[r][g][b] = ColorMatcher.Pick ((r<<3)|(r>>2), (g<<3)|(g>>2), (b<<3)|(b>>2));

	int x, y;

	// create the swizzled palette
	for (x = 0; x < 65; x++)
		for (y = 0; y < 256; y++)
			Col2RGB8[x][y] = (((palette[y].r*x)>>4)<<20) |
							  ((palette[y].g*x)>>4) |
							 (((palette[y].b*x)>>4)<<10);

	// create the swizzled palette with the lsb of red and blue forced to 0
	// (for green, a 1 is okay since it never gets added into)
	for (x = 1; x < 64; x++)
	{
		Col2RGB8_LessPrecision[x] = Col2RGB8_2[x-1];
		for (y = 0; y < 256; y++)
		{
			Col2RGB8_2[x-1][y] = Col2RGB8[x][y] & 0x3feffbff;
		}
	}
	Col2RGB8_LessPrecision[0] = Col2RGB8[0];
	Col2RGB8_LessPrecision[64] = Col2RGB8[64];
}

void DCanvas::Blit (int srcx, int srcy, int srcwidth, int srcheight,
			 DCanvas *dest, int destx, int desty, int destwidth, int destheight)
{
	fixed_t fracxstep, fracystep;
	fixed_t fracx, fracy;
	int x, y;
	bool lockthis, lockdest;

	if ( (lockthis = (LockCount == 0)) )
	{
		if (Lock ())
		{ // Surface was lost, so nothing to blit
			Unlock ();
			return;
		}
	}

	if ( (lockdest = (dest->LockCount == 0)) )
	{
		dest->Lock ();
	}

	fracy = srcy << FRACBITS;
	fracystep = (srcheight << FRACBITS) / destheight;
	fracxstep = (srcwidth << FRACBITS) / destwidth;

	byte *destline, *srcline;
	byte *destbuffer = dest->Buffer;
	byte *srcbuffer = Buffer;

	if (fracxstep == FRACUNIT)
	{
		for (y = desty; y < desty + destheight; y++, fracy += fracystep)
		{
			memcpy (destbuffer + y * dest->Pitch + destx,
					srcbuffer + (fracy >> FRACBITS) * Pitch + srcx,
					destwidth);
		}
	}
	else
	{
		for (y = desty; y < desty + destheight; y++, fracy += fracystep)
		{
			srcline = srcbuffer + (fracy >> FRACBITS) * Pitch + srcx;
			destline = destbuffer + y * dest->Pitch + destx;
			for (x = fracx = 0; x < destwidth; x++, fracx += fracxstep)
			{
				destline[x] = srcline[fracx >> FRACBITS];
			}
		}
	}

	if (lockthis)
	{
		Unlock ();
	}
	if (lockdest)
	{
		Unlock ();
	}
}

void DCanvas::CalcGamma (float gamma, BYTE gammalookup[256])
{
	// I found this formula on the web at
	// <http://panda.mostang.com/sane/sane-gamma.html>,
	// but that page no longer exits.

	double invgamma = 1.f / gamma;
	int i;

	for (i = 0; i < 256; i++)
	{
		gammalookup[i] = (BYTE)(255.0 * pow (i / 255.0, invgamma));
	}
}

DSimpleCanvas::DSimpleCanvas (int width, int height)
	: DCanvas (width, height)
{
	// Making the pitch a power of 2 is very bad for performance
	// Try to maximize the number of cache lines that can be filled
	// for each column drawing operation by making the pitch slightly
	// longer than the width. The values used here are all based on
	// empirical evidence.

	if (width <= 640)
	{
		// For low resolutions, just keep the pitch the same as the width.
		// Some speedup can be seen using the technique below, but the speedup
		// is so marginal that I don't consider it worthwhile.
		Pitch = width;
	}
	else
	{
		// The Athlon and P3 have very different caches, apparently.
		// I am going to generalize the Athlon's performance to all AMD
		// processors and the P3's to all non-AMD processors. I don't know
		// how smart that is, but I don't have a vast plethora of
		// processors to test with.
		if (CPU.bIsAMD)
		{
			Pitch = width + CPU.DataL1LineSize;
		}
		else
		{
			Pitch = width + CPU.DataL1LineSize - 8;
		}
	}
	MemBuffer = new BYTE[Pitch * height];
}

DSimpleCanvas::~DSimpleCanvas ()
{
	if (MemBuffer != NULL)
	{
		delete[] MemBuffer;
		MemBuffer = NULL;
	}
}

bool DSimpleCanvas::IsValid ()
{
	return (MemBuffer != NULL);
}

bool DSimpleCanvas::Lock ()
{
	if (LockCount == 0)
	{
		LockCount++;
		Buffer = MemBuffer;
	}
	return false;		// System surfaces are never lost
}

void DSimpleCanvas::Unlock ()
{
	if (LockCount <= 1)
	{
		LockCount = 0;
		Buffer = NULL;	// Enforce buffer access only between Lock/Unlock
	}
}

DFrameBuffer::DFrameBuffer (int width, int height)
	: DSimpleCanvas (width, height)
{
	LastMS = LastSec = FrameCount = LastCount = LastTic = 0;
}

void DFrameBuffer::DrawRateStuff ()
{
	// Draws frame time and cumulative fps
	if (vid_fps)
	{
		QWORD ms = I_MSTime ();
		DWORD howlong = DWORD(ms - LastMS);
		if (howlong > 0)
		{
			char fpsbuff[40];
			int chars;

			chars = sprintf (fpsbuff, "%2lu ms (%3lu fps)", howlong, LastCount);
			Clear (0, screen->GetHeight() - 8, chars * 8, screen->GetHeight(), 0);
			SetFont (ConFont);
			DrawText (CR_WHITE, 0, screen->GetHeight() - 8, (char *)&fpsbuff[0], TAG_DONE);
			SetFont (SmallFont);

			DWORD thisSec = ms/1000;
			if (LastSec < thisSec)
			{
				LastCount = FrameCount / (thisSec - LastSec);
				LastSec = thisSec;
				FrameCount = 0;
			}
			FrameCount++;
		}
		LastMS = ms;
	}

	// draws little dots on the bottom of the screen
	if (ticker)
	{
		int i = I_GetTime(false);
		int tics = i - LastTic;
		BYTE *buffer = GetBuffer () + (GetHeight()-1)*GetPitch();

		LastTic = i;
		if (tics > 20) tics = 20;
		
		for (i = 0; i < tics*2; i += 2)		buffer[i] = 0xff;
		for ( ; i < 20*2; i += 2)			buffer[i] = 0x00;
	}

	// draws the palette for debugging
	if (vid_showpalette)
	{
		int i, j, k, l;

		BYTE *buffer = GetBuffer();
		for (i = k = 0; i < 16; ++i)
		{
			for (j = 0; j < 8; ++j)
			{
				for (l = 0; l < 16; ++l)
				{
					memset (buffer, k, 8);
					buffer += 8;
					k++;
				}
				k -= 16;
				buffer += GetPitch() - 16*8;
			}
			k += 16;
		}
	}
}

void DFrameBuffer::CopyFromBuff (BYTE *src, int srcPitch, int width, int height, BYTE *dest)
{
	if (Pitch == width && Pitch == Width && srcPitch == width)
	{
		memcpy (dest, src, Width * Height);
	}
	else
	{
		for (int y = 0; y < height; y++)
		{
			memcpy (dest, src, width);
			dest += Pitch;
			src += srcPitch;
		}
	}
}

//
// V_SetResolution
//
bool V_DoModeSetup (int width, int height, int bits)
{
	DFrameBuffer *buff = I_SetMode (width, height, screen);

	if (buff == NULL)
	{
		return false;
	}

	screen = buff;
	screen->SetFont (SmallFont);
	screen->SetGamma (Gamma);

	CleanXfac = width / 320;
	CleanYfac = height / 200;

	if (CleanXfac > 1 && CleanYfac > 1 && CleanXfac != CleanYfac)
	{
		if (CleanXfac < CleanYfac)
			CleanYfac = CleanXfac;
		else
			CleanXfac = CleanYfac;
	}

	CleanWidth = width / CleanXfac;
	CleanHeight = height / CleanYfac;

	DisplayWidth = width;
	DisplayHeight = height;
	DisplayBits = bits;

	R_InitColumnDrawers ();
	R_MultiresInit ();

	RenderTarget = screen;
	screen->Lock (true);
	R_SetupBuffer (false);
	screen->Unlock ();

	M_RefreshModesList ();

	return true;
}

bool V_SetResolution (int width, int height, int bits)
{
	int oldwidth, oldheight;
	int oldbits;

	if (screen)
	{
		oldwidth = SCREENWIDTH;
		oldheight = SCREENHEIGHT;
		oldbits = DisplayBits;
	}
	else
	{ // Harmless if screen wasn't allocated
		oldwidth = width;
		oldheight = height;
		oldbits = bits;
	}

	I_ClosestResolution (&width, &height, bits);
	if (!I_CheckResolution (width, height, bits))
	{ // Try specified resolution
		if (!I_CheckResolution (oldwidth, oldheight, oldbits))
		{ // Try previous resolution (if any)
	   		return false;
		}
		else
		{
			width = oldwidth;
			height = oldheight;
			bits = oldbits;
		}
	}
	return V_DoModeSetup (width, height, bits);
}

CCMD (vid_setmode)
{
	BOOL	goodmode = false;
	int		width = 0, height = SCREENHEIGHT;
	int		bits = DisplayBits;

	if (argv.argc() > 1)
	{
		width = atoi (argv[1]);
		if (argv.argc() > 2)
		{
			height = atoi (argv[2]);
			if (argv.argc() > 3)
			{
				bits = atoi (argv[3]);
			}
		}
	}

	if (width && I_CheckResolution (width, height, bits))
	{
		goodmode = true;
	}

	if (goodmode)
	{
		// The actual change of resolution will take place
		// near the beginning of D_Display().
		if (gamestate != GS_STARTUP)
		{
			setmodeneeded = true;
			NewWidth = width;
			NewHeight = height;
			NewBits = bits;
		}
	}
	else if (width)
	{
		Printf ("Unknown resolution %d x %d x %d\n", width, height, bits);
	}
	else
	{
		Printf ("Usage: vid_setmode <width> <height> <mode>\n");
	}
}

//
// V_Init
//

void V_Init (void) 
{ 
	char *i;
	int width, height, bits;

	// [RH] Initialize palette management
	InitPalette ();

	// load the heads-up font
	if (Wads.CheckNumForName ("FONTA_S") >= 0)
	{
		SmallFont = new FFont ("SmallFont", "FONTA%02u", HU_FONTSTART, HU_FONTSIZE, 1);
	}
	else
	{
		SmallFont = new FFont ("SmallFont", "STCFN%.3d", HU_FONTSTART, HU_FONTSIZE, HU_FONTSTART);
	}
	if (Wads.CheckNumForName ("STBFN033") >= 0)
	{
		SmallFont2 = new FFont ("SmallFont2", "STBFN%.3d", HU_FONTSTART, HU_FONTSIZE, HU_FONTSTART);
	}
	else
	{
		SmallFont2 = SmallFont;
	}
	if (gameinfo.gametype == GAME_Doom)
	{
		BigFont = new FSingleLumpFont ("BigFont", Wads.GetNumForName ("DBIGFONT"));
	}
	else if (gameinfo.gametype == GAME_Strife)
	{
		BigFont = new FSingleLumpFont ("BigFont", Wads.GetNumForName ("SBIGFONT"));
	}
	else
	{
		BigFont = new FFont ("BigFont", "FONTB%02u", HU_FONTSTART, HU_FONTSIZE, 1);
	}

	width = height = bits = 0;

	if ( (i = Args.CheckValue ("-width")) )
		width = atoi (i);

	if ( (i = Args.CheckValue ("-height")) )
		height = atoi (i);

	if ( (i = Args.CheckValue ("-bits")) )
		bits = atoi (i);

	if (width == 0)
	{
		if (height == 0)
		{
			width = vid_defwidth;
			height = vid_defheight;
		}
		else
		{
			width = (height * 8) / 6;
		}
	}
	else if (height == 0)
	{
		height = (width * 6) / 8;
	}

	if (bits == 0)
	{
		bits = vid_defbits;
	}

	atterm (FreeCanvasChain);

	I_ClosestResolution (&width, &height, bits);

	if (!V_SetResolution (width, height, bits))
		I_FatalError ("Could not set resolution to %d x %d x %d", width, height, bits);
	else
		Printf ("Resolution: %d x %d\n", SCREENWIDTH, SCREENHEIGHT);

	FBaseCVar::ResetColors ();
	ConFont = new FSingleLumpFont ("ConsoleFont", Wads.GetNumForName ("CONFONT"));

	BuildTransTable (GPalette.BaseColors);
}

void STACK_ARGS FreeCanvasChain ()
{
	while (DCanvas::CanvasChain != NULL)
	{
		delete DCanvas::CanvasChain;
	}
	screen = NULL;
}
