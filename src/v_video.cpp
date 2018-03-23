/*
**
**
**---------------------------------------------------------------------------
** Copyright 1999-2016 Randy Heit
** Copyright 2005-2016 Christoph Oelckers
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//		Functions to draw patches (by post) directly to screen->
//		Functions to blit a block to the screen->
//
//-----------------------------------------------------------------------------


#include <stdio.h>

#include "i_system.h"
#include "x86.h"
#include "i_video.h"
#include "r_state.h"

#include "doomdef.h"
#include "doomdata.h"
#include "doomstat.h"

#include "c_console.h"
#include "hu_stuff.h"

#include "m_argv.h"
#include "m_bbox.h"
#include "m_swap.h"

#include "i_video.h"
#include "v_video.h"
#include "v_text.h"
#include "sc_man.h"

#include "w_wad.h"

#include "c_cvars.h"
#include "c_dispatch.h"
#include "cmdlib.h"
#include "gi.h"
#include "templates.h"
#include "sbar.h"
#include "hardware.h"
#include "r_data/r_translate.h"
#include "f_wipe.h"
#include "m_png.h"
#include "colormatcher.h"
#include "v_palette.h"
#include "r_sky.h"
#include "r_utility.h"
#include "r_renderer.h"
#include "menu/menu.h"
#include "r_data/voxels.h"
#include "vm.h"
#include "r_videoscale.h"
#include "i_time.h"

EXTERN_CVAR(Bool, r_blendmethod)

int active_con_scale();

FRenderer *Renderer;

EXTERN_CVAR (Bool, swtruecolor)
EXTERN_CVAR (Bool, fullscreen)

#if defined(_DEBUG) && defined(_M_IX86) && !defined(__MINGW32__)
#define DBGBREAK	{ __asm int 3 }
#else
#define DBGBREAK
#endif

class DDummyFrameBuffer : public DFrameBuffer
{
	typedef DFrameBuffer Super;
public:
	DDummyFrameBuffer (int width, int height)
		: DFrameBuffer (0, 0, false)
	{
		Width = width;
		Height = height;
	}
	bool Lock(bool buffered) { DBGBREAK; return false; }
	void Update() { DBGBREAK; }
	PalEntry *GetPalette() { DBGBREAK; return NULL; }
	void GetFlashedPalette(PalEntry palette[256]) { DBGBREAK; }
	void UpdatePalette() { DBGBREAK; }
	bool SetGamma(float gamma) { Gamma = gamma; return true; }
	bool SetFlash(PalEntry rgb, int amount) { DBGBREAK; return false; }
	void GetFlash(PalEntry &rgb, int &amount) { DBGBREAK; }
	int GetPageCount() { DBGBREAK; return 0; }
	bool IsFullscreen() { DBGBREAK; return 0; }
#ifdef _WIN32
	void PaletteChanged() {}
	int QueryNewPalette() { return 0; }
	bool Is8BitMode() { return false; }
#endif

	float Gamma;
};

class FPaletteTester : public FTexture
{
public:
	FPaletteTester ();

	const uint8_t *GetColumn(FRenderStyle, unsigned int column, const Span **spans_out) override;
	const uint8_t *GetPixels(FRenderStyle);
	bool CheckModified(FRenderStyle);
	void SetTranslation(int num);

protected:
	uint8_t Pixels[16*16];
	int CurTranslation;
	int WantTranslation;
	static const Span DummySpan[2];

	void MakeTexture();
};

const FTexture::Span FPaletteTester::DummySpan[2] = { { 0, 16 }, { 0, 0 } };

int DisplayWidth, DisplayHeight, DisplayBits;

FFont *SmallFont, *SmallFont2, *BigFont, *ConFont, *IntermissionFont;

uint32_t Col2RGB8[65][256];
uint32_t *Col2RGB8_LessPrecision[65];
uint32_t Col2RGB8_Inverse[65][256];
ColorTable32k RGB32k;
ColorTable256k RGB256k;


static uint32_t Col2RGB8_2[63][256];

// [RH] The framebuffer is no longer a mere byte array.
// There's also only one, not four.
DFrameBuffer *screen;

CVAR (Int, vid_defwidth, 640, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Int, vid_defheight, 480, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Int, vid_defbits, 8, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Bool, vid_fps, false, 0)
CVAR (Bool, ticker, false, 0)
CVAR (Int, vid_showpalette, 0, 0)

CUSTOM_CVAR (Bool, vid_vsync, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (screen != NULL)
	{
		screen->SetVSync (*self);
	}
}

CUSTOM_CVAR (Int, vid_refreshrate, 0, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (screen != NULL)
	{
		screen->NewRefreshRate();
	}
}

CUSTOM_CVAR (Float, dimamount, -1.f, CVAR_ARCHIVE)
{
	if (self < 0.f && self != -1.f)
	{
		self = -1.f;
	}
	else if (self > 1.f)
	{
		self = 1.f;
	}
}
CVAR (Color, dimcolor, 0xffd700, CVAR_ARCHIVE)

// [RH] Set true when vid_setmode command has been executed
bool	setmodeneeded = false;
// [RH] Resolution to change to when setmodeneeded is true
int		NewWidth, NewHeight, NewBits;


//==========================================================================
//
// DCanvas Constructor
//
//==========================================================================

DCanvas::DCanvas (int _width, int _height, bool _bgra)
{
	// Init member vars
	Buffer = NULL;
	LockCount = 0;
	Width = _width;
	Height = _height;
	Bgra = _bgra;
}

//==========================================================================
//
// DCanvas Destructor
//
//==========================================================================

DCanvas::~DCanvas ()
{
}

//==========================================================================
//
// DCanvas :: IsValid
//
//==========================================================================

bool DCanvas::IsValid ()
{
	// A nun-subclassed DCanvas is never valid
	return false;
}

//==========================================================================
//
// DCanvas :: FlatFill
//
// Fill an area with a texture. If local_origin is false, then the origin
// used for the wrapping is (0,0). Otherwise, (left,right) is used.
//
//==========================================================================

void DCanvas::FlatFill (int left, int top, int right, int bottom, FTexture *src, bool local_origin)
{
	int w = src->GetWidth();
	int h = src->GetHeight();

	// Repeatedly draw the texture, left-to-right, top-to-bottom.
	for (int y = local_origin ? top : (top / h * h); y < bottom; y += h)
	{
		for (int x = local_origin ? left : (left / w * w); x < right; x += w)
		{
			DrawTexture (src, x, y,
				DTA_ClipLeft, left,
				DTA_ClipRight, right,
				DTA_ClipTop, top,
				DTA_ClipBottom, bottom,
				DTA_TopOffset, 0,
				DTA_LeftOffset, 0,
				TAG_DONE);
		}
	}
}

//==========================================================================
//
// DCanvas :: Dim
//
// Applies a colored overlay to the entire screen, with the opacity
// determined by the dimamount cvar.
//
//==========================================================================

void DCanvas::Dim (PalEntry color)
{
	PalEntry dimmer;
	float amount;

	if (dimamount >= 0)
	{
		dimmer = PalEntry(dimcolor);
		amount = dimamount;
	}
	else
	{
		dimmer = gameinfo.dimcolor;
		amount = gameinfo.dimamount;
	}

	if (gameinfo.gametype == GAME_Hexen && gamestate == GS_DEMOSCREEN)
	{ // On the Hexen title screen, the default dimming is not
		// enough to make the menus readable.
		amount = MIN<float> (1.f, amount*2.f);
	}
	// Add the cvar's dimming on top of the color passed to the function
	if (color.a != 0)
	{
		float dim[4] = { color.r/255.f, color.g/255.f, color.b/255.f, color.a/255.f };
		V_AddBlend (dimmer.r/255.f, dimmer.g/255.f, dimmer.b/255.f, amount, dim);
		dimmer = PalEntry (uint8_t(dim[0]*255), uint8_t(dim[1]*255), uint8_t(dim[2]*255));
		amount = dim[3];
	}
	Dim (dimmer, amount, 0, 0, Width, Height);
}

//==========================================================================
//
// DCanvas :: GetScreenshotBuffer
//
// Returns a buffer containing the most recently displayed frame. The
// width and height of this buffer are the same as the canvas.
//
//==========================================================================

void DCanvas::GetScreenshotBuffer(const uint8_t *&buffer, int &pitch, ESSType &color_type, float &gamma)
{
	Lock(true);
	buffer = GetBuffer();
	pitch = IsBgra() ? GetPitch() * 4 : GetPitch();
	color_type = IsBgra() ? SS_BGRA : SS_PAL;
	gamma = Gamma;
}

//==========================================================================
//
// DCanvas :: ReleaseScreenshotBuffer
//
// Releases the buffer obtained through GetScreenshotBuffer. These calls
// must not be nested.
//
//==========================================================================

void DCanvas::ReleaseScreenshotBuffer()
{
	Unlock();
}

//==========================================================================
//
// V_GetColorFromString
//
// Passed a string of the form "#RGB", "#RRGGBB", "R G B", or "RR GG BB",
// returns a number representing that color. If palette is non-NULL, the
// index of the best match in the palette is returned, otherwise the
// RRGGBB value is returned directly.
//
//==========================================================================

int V_GetColorFromString (const uint32_t *palette, const char *cstr, FScriptPosition *sc)
{
	int c[3], i, p;
	char val[3];

	val[2] = '\0';

	// Check for HTML-style #RRGGBB or #RGB color string
	if (cstr[0] == '#')
	{
		size_t len = strlen (cstr);

		if (len == 7)
		{
			// Extract each eight-bit component into c[].
			for (i = 0; i < 3; ++i)
			{
				val[0] = cstr[1 + i*2];
				val[1] = cstr[2 + i*2];
				c[i] = ParseHex (val, sc);
			}
		}
		else if (len == 4)
		{
			// Extract each four-bit component into c[], expanding to eight bits.
			for (i = 0; i < 3; ++i)
			{
				val[1] = val[0] = cstr[1 + i];
				c[i] = ParseHex (val, sc);
			}
		}
		else
		{
			// Bad HTML-style; pretend it's black.
			c[2] = c[1] = c[0] = 0;
		}
	}
	else
	{
		if (strlen(cstr) == 6)
		{
			char *p;
			int color = strtol(cstr, &p, 16);
			if (*p == 0)
			{
				// RRGGBB string
				c[0] = (color & 0xff0000) >> 16;
				c[1] = (color & 0xff00) >> 8;
				c[2] = (color & 0xff);
			}
			else goto normal;
		}
		else
		{
normal:
			// Treat it as a space-delimited hexadecimal string
			for (i = 0; i < 3; ++i)
			{
				// Skip leading whitespace
				while (*cstr <= ' ' && *cstr != '\0')
				{
					cstr++;
				}
				// Extract a component and convert it to eight-bit
				for (p = 0; *cstr > ' '; ++p, ++cstr)
				{
					if (p < 2)
					{
						val[p] = *cstr;
					}
				}
				if (p == 0)
				{
					c[i] = 0;
				}
				else
				{
					if (p == 1)
					{
						val[1] = val[0];
					}
					c[i] = ParseHex (val, sc);
				}
			}
		}
	}
	if (palette)
		return ColorMatcher.Pick (c[0], c[1], c[2]);
	else
		return MAKERGB(c[0], c[1], c[2]);
}

//==========================================================================
//
// V_GetColorStringByName
//
// Searches for the given color name in x11r6rgb.txt and returns an
// HTML-ish "#RRGGBB" string for it if found or the empty string if not.
//
//==========================================================================

FString V_GetColorStringByName (const char *name, FScriptPosition *sc)
{
	FMemLump rgbNames;
	char *rgbEnd;
	char *rgb, *endp;
	int rgblump;
	int c[3], step;
	size_t namelen;

	if (Wads.GetNumLumps()==0) return FString();

	rgblump = Wads.CheckNumForName ("X11R6RGB");
	if (rgblump == -1)
	{
		if (!sc) Printf ("X11R6RGB lump not found\n");
		else sc->Message(MSG_WARNING, "X11R6RGB lump not found");
		return FString();
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
				FString descr;
				descr.Format ("#%02x%02x%02x", c[0], c[1], c[2]);
				return descr;
			}
			rgb = endp;
			step = 0;
		}
	}
	if (rgb < rgbEnd)
	{
		if (!sc) Printf ("X11R6RGB lump is corrupt\n");
		else sc->Message(MSG_WARNING, "X11R6RGB lump is corrupt");
	}
	return FString();
}

//==========================================================================
//
// V_GetColor
//
// Works like V_GetColorFromString(), but also understands X11 color names.
//
//==========================================================================

int V_GetColor (const uint32_t *palette, const char *str, FScriptPosition *sc)
{
	FString string = V_GetColorStringByName (str, sc);
	int res;

	if (!string.IsEmpty())
	{
		res = V_GetColorFromString (palette, string, sc);
	}
	else
	{
		res = V_GetColorFromString (palette, str, sc);
	}
	return res;
}

int V_GetColor(const uint32_t *palette, FScanner &sc)
{
	FScriptPosition scc = sc;
	return V_GetColor(palette, sc.String, &scc);
}

//==========================================================================
//
// BuildTransTable
//
// Build the tables necessary for blending
//
//==========================================================================

static void BuildTransTable (const PalEntry *palette)
{
	int r, g, b;

	// create the RGB555 lookup table
	for (r = 0; r < 32; r++)
		for (g = 0; g < 32; g++)
			for (b = 0; b < 32; b++)
				RGB32k.RGB[r][g][b] = ColorMatcher.Pick ((r<<3)|(r>>2), (g<<3)|(g>>2), (b<<3)|(b>>2));
	// create the RGB666 lookup table
	for (r = 0; r < 64; r++)
		for (g = 0; g < 64; g++)
			for (b = 0; b < 64; b++)
				RGB256k.RGB[r][g][b] = ColorMatcher.Pick ((r<<2)|(r>>4), (g<<2)|(g>>4), (b<<2)|(b>>4));

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

	// create the inverse swizzled palette
	for (x = 0; x < 65; x++)
		for (y = 0; y < 256; y++)
		{
			Col2RGB8_Inverse[x][y] = (((((255-palette[y].r)*x)>>4)<<20) |
									  (((255-palette[y].g)*x)>>4) |
									  ((((255-palette[y].b)*x)>>4)<<10)) & 0x3feffbff;
		}
}

//==========================================================================
//
// DCanvas :: CalcGamma
//
//==========================================================================

void DCanvas::CalcGamma (float gamma, uint8_t gammalookup[256])
{
	// I found this formula on the web at
	// <http://panda.mostang.com/sane/sane-gamma.html>,
	// but that page no longer exits.
	double invgamma = 1.f / gamma;
	int i;

	for (i = 0; i < 256; i++)
	{
		gammalookup[i] = (uint8_t)(255.0 * pow (i / 255.0, invgamma) + 0.5);
	}
}

//==========================================================================
//
// DSimpleCanvas Constructor
//
// A simple canvas just holds a buffer in main memory.
//
//==========================================================================

DSimpleCanvas::DSimpleCanvas (int width, int height, bool bgra)
	: DCanvas (width, height, bgra)
{
	MemBuffer = nullptr;
	Resize(width, height);
}

void DSimpleCanvas::Resize(int width, int height)
{
	Width = width;
	Height = height;

	if (MemBuffer != NULL)
	{
		delete[] MemBuffer;
		MemBuffer = NULL;
	}

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
		// If we couldn't figure out the CPU's L1 cache line size, assume
		// it's 32 bytes wide.
		if (CPU.DataL1LineSize == 0)
		{
			CPU.DataL1LineSize = 32;
		}
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
			Pitch = width + MAX(0, CPU.DataL1LineSize - 8);
		}
	}
	int bytes_per_pixel = Bgra ? 4 : 1;
	MemBuffer = new uint8_t[Pitch * height * bytes_per_pixel];
	memset (MemBuffer, 0, Pitch * height * bytes_per_pixel);
}

//==========================================================================
//
// DSimpleCanvas Destructor
//
//==========================================================================

DSimpleCanvas::~DSimpleCanvas ()
{
	if (MemBuffer != NULL)
	{
		delete[] MemBuffer;
		MemBuffer = NULL;
	}
}

//==========================================================================
//
// DSimpleCanvas :: IsValid
//
//==========================================================================

bool DSimpleCanvas::IsValid ()
{
	return (MemBuffer != NULL);
}

//==========================================================================
//
// DSimpleCanvas :: Lock
//
//==========================================================================

bool DSimpleCanvas::Lock (bool)
{
	if (LockCount == 0)
	{
		Buffer = MemBuffer;
	}
	LockCount++;
	return false;		// System surfaces are never lost
}

//==========================================================================
//
// DSimpleCanvas :: Unlock
//
//==========================================================================

void DSimpleCanvas::Unlock ()
{
	if (--LockCount <= 0)
	{
		LockCount = 0;
		Buffer = NULL;	// Enforce buffer access only between Lock/Unlock
	}
}

//==========================================================================
//
// DFrameBuffer Constructor
//
// A frame buffer canvas is the most common and represents the image that
// gets drawn to the screen.
//
//==========================================================================

DFrameBuffer::DFrameBuffer (int width, int height, bool bgra)
	: DSimpleCanvas (ViewportScaledWidth(width, height), ViewportScaledHeight(width, height), bgra)
{
	LastMS = LastSec = FrameCount = LastCount = LastTic = 0;
	Accel2D = false;

	VideoWidth = width;
	VideoHeight = height;
}

//==========================================================================
//
// DFrameBuffer :: PostprocessBgra
//
// Copies data to destination buffer while performing gamma and flash.
// This is only needed if a target cannot do this with shaders.
//
//==========================================================================

void DFrameBuffer::CopyWithGammaBgra(void *output, int pitch, const uint8_t *gammared, const uint8_t *gammagreen, const uint8_t *gammablue, PalEntry flash, int flash_amount)
{
	const uint8_t *gammatables[3] = { gammared, gammagreen, gammablue };

	if (flash_amount > 0)
	{
		uint16_t inv_flash_amount = 256 - flash_amount;
		uint16_t flash_red = flash.r * flash_amount;
		uint16_t flash_green = flash.g * flash_amount;
		uint16_t flash_blue = flash.b * flash_amount;
		
		for (int y = 0; y < Height; y++)
		{
			uint8_t *dest = (uint8_t*)output + y * pitch;
			uint8_t *src = MemBuffer + y * Pitch * 4;
			for (int x = 0;  x < Width; x++)
			{
				uint16_t fg_red = src[2];
				uint16_t fg_green = src[1];
				uint16_t fg_blue = src[0];
				uint16_t red = (fg_red * inv_flash_amount + flash_red) >> 8;
				uint16_t green = (fg_green * inv_flash_amount + flash_green) >> 8;
				uint16_t blue = (fg_blue * inv_flash_amount + flash_blue) >> 8;

				dest[0] = gammatables[2][blue];
				dest[1] = gammatables[1][green];
				dest[2] = gammatables[0][red];
				dest[3] = 0xff;

				dest += 4;
				src += 4;
			}
		}
	}
	else
	{
		for (int y = 0; y < Height; y++)
		{
			uint8_t *dest = (uint8_t*)output + y * pitch;
			uint8_t *src = MemBuffer + y * Pitch * 4;
			for (int x = 0;  x < Width; x++)
			{
				dest[0] = gammatables[2][src[0]];
				dest[1] = gammatables[1][src[1]];
				dest[2] = gammatables[0][src[2]];
				dest[3] = 0xff;

				dest += 4;
				src += 4;
			}
		}
	}
}


//==========================================================================
//
// DFrameBuffer :: DrawRateStuff
//
// Draws the fps counter, dot ticker, and palette debug.
//
//==========================================================================

void DFrameBuffer::DrawRateStuff ()
{
	// Draws frame time and cumulative fps
	if (vid_fps)
	{
		uint64_t ms = screen->FrameTime;
		uint64_t howlong = ms - LastMS;
		if ((signed)howlong >= 0)
		{
			char fpsbuff[40];
			int chars;
			int rate_x;

			int textScale = active_con_scale();

			chars = mysnprintf (fpsbuff, countof(fpsbuff), "%2" PRIu64 " ms (%3" PRIu64 " fps)", howlong, LastCount);
			rate_x = Width / textScale - ConFont->StringWidth(&fpsbuff[0]);
			Clear (rate_x * textScale, 0, Width, ConFont->GetHeight() * textScale, GPalette.BlackIndex, 0);
			DrawText (ConFont, CR_WHITE, rate_x, 0, (char *)&fpsbuff[0],
				DTA_VirtualWidth, screen->GetWidth() / textScale,
				DTA_VirtualHeight, screen->GetHeight() / textScale,
				DTA_KeepRatio, true, TAG_DONE);

			uint32_t thisSec = (uint32_t)(ms/1000);
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
		int64_t i = I_GetTime();
		int64_t tics = i - LastTic;
		uint8_t *buffer = GetBuffer();

		LastTic = i;
		if (tics > 20) tics = 20;

		// Buffer can be NULL if we're doing hardware accelerated 2D
		if (buffer != NULL)
		{
			if (IsBgra())
			{
				uint32_t *buffer32 = (uint32_t*)buffer;
				buffer32 += (GetHeight() - 1) * GetPitch();

				for (i = 0; i < tics * 2; i += 2)	buffer32[i] = 0xffffffff;
				for (; i < 20 * 2; i += 2)			buffer32[i] = 0xff000000;
			}
			else
			{
				buffer += (GetHeight() - 1) * GetPitch();

				for (i = 0; i < tics * 2; i += 2)	buffer[i] = 0xff;
				for (; i < 20 * 2; i += 2)			buffer[i] = 0x00;
			}
		}
		else
		{
			int i;
			for (i = 0; i < tics*2; i += 2)		Clear(i, Height-1, i+1, Height, 255, 0);
			for ( ; i < 20*2; i += 2)			Clear(i, Height-1, i+1, Height, 0, 0);
		}
	}

	// draws the palette for debugging
	if (vid_showpalette)
	{
		// This used to just write the palette to the display buffer.
		// With hardware-accelerated 2D, that doesn't work anymore.
		// Drawing it as a texture does and continues to show how
		// well the PalTex shader is working.
		static FPaletteTester palette;
		int size = screen->GetHeight() < 800 ? 16 * 7 : 16 * 7 * 2;

		palette.SetTranslation(vid_showpalette);
		DrawTexture(&palette, 0, 0,
			DTA_DestWidth, size,
			DTA_DestHeight, size,
			DTA_Masked, false,
			TAG_DONE);
	}
}

//==========================================================================
//
// FPaleteTester Constructor
//
// This is just a 16x16 image with every possible color value.
//
//==========================================================================

FPaletteTester::FPaletteTester()
{
	Width = 16;
	Height = 16;
	WidthBits = 4;
	HeightBits = 4;
	WidthMask = 15;
	CurTranslation = 0;
	WantTranslation = 1;
	MakeTexture();
}

//==========================================================================
//
// FPaletteTester :: CheckModified
//
//==========================================================================

bool FPaletteTester::CheckModified(FRenderStyle)
{
	return CurTranslation != WantTranslation;
}

//==========================================================================
//
// FPaletteTester :: SetTranslation
//
//==========================================================================

void FPaletteTester::SetTranslation(int num)
{
	if (num >= 1 && num <= 9)
	{
		WantTranslation = num;
	}
}

//==========================================================================
//
// FPaletteTester :: GetColumn
//
//==========================================================================

const uint8_t *FPaletteTester::GetColumn(FRenderStyle, unsigned int column, const Span **spans_out)
{
	if (CurTranslation != WantTranslation)
	{
		MakeTexture();
	}
	column &= 15;
	if (spans_out != NULL)
	{
		*spans_out = DummySpan;
	}
	return Pixels + column*16;
}

//==========================================================================
//
// FPaletteTester :: GetPixels
//
//==========================================================================

const uint8_t *FPaletteTester::GetPixels (FRenderStyle)
{
	if (CurTranslation != WantTranslation)
	{
		MakeTexture();
	}
	return Pixels;
}

//==========================================================================
//
// FPaletteTester :: MakeTexture
//
//==========================================================================

void FPaletteTester::MakeTexture()
{
	int i, j, k, t;
	uint8_t *p;

	t = WantTranslation;
	p = Pixels;
	k = 0;
	for (i = 0; i < 16; ++i)
	{
		for (j = 0; j < 16; ++j)
		{
			*p++ = (t > 1) ? translationtables[TRANSLATION_Standard][t - 2]->Remap[k] : k;
			k += 16;
		}
		k -= 255;
	}
	CurTranslation = t;
}

//==========================================================================
//
// DFrameBuffer :: CopyFromBuff
//
// Copies pixels from main memory to video memory. This is only used by
// DDrawFB.
//
//==========================================================================

void DFrameBuffer::CopyFromBuff (uint8_t *src, int srcPitch, int width, int height, uint8_t *dest)
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

//==========================================================================
//
// DFrameBuffer :: SetVSync
//
// Turns vertical sync on and off, if supported.
//
//==========================================================================

void DFrameBuffer::SetVSync (bool vsync)
{
}

//==========================================================================
//
// DFrameBuffer :: NewRefreshRate
//
// Sets the fullscreen display to the new refresh rate in vid_refreshrate,
// if possible.
//
//==========================================================================

void DFrameBuffer::NewRefreshRate ()
{
}

//==========================================================================
//
// DFrameBuffer :: SetBlendingRect
//
// Defines the area of the screen containing the 3D view.
//
//==========================================================================

void DFrameBuffer::SetBlendingRect (int x1, int y1, int x2, int y2)
{
}

//==========================================================================
//
// DFrameBuffer :: Begin2D
//
// Signal that 3D rendering is complete, and the rest of the operations on
// the canvas until Unlock() will be 2D ones.
//
//==========================================================================

bool DFrameBuffer::Begin2D (bool copy3d)
{
	isIn2D = true;
	ClearClipRect();
	return false;
}

//==========================================================================
//
// DFrameBuffer :: DrawBlendingRect
//
// In hardware 2D modes, the blending rect needs to be drawn separately
// from transferring the 3D scene to video memory, because the weapon
// sprite is drawn on top of that.
//
//==========================================================================

void DFrameBuffer::DrawBlendingRect()
{
}

//==========================================================================
//
// DFrameBuffer :: CreateTexture
//
// Creates a native texture for a game texture, if supported.
// The hardware renderer does not use this interface because it is 
// far too limited
//
//==========================================================================

FNativeTexture *DFrameBuffer::CreateTexture(FTexture *gametex, FTextureFormat fmt, bool wrapping)
{
	return NULL;
}

//==========================================================================
//
// DFrameBuffer :: CreatePalette
//
// Creates a native palette from a remap table, if supported.
//
//==========================================================================

FNativePalette *DFrameBuffer::CreatePalette(FRemapTable *remap)
{
	return NULL;
}

//==========================================================================
//
// DFrameBuffer :: WipeStartScreen
//
// Grabs a copy of the screen currently displayed to serve as the initial
// frame of a screen wipe. Also determines which screenwipe will be
// performed.
//
//==========================================================================

bool DFrameBuffer::WipeStartScreen(int type)
{
	return wipe_StartScreen(type);
}

//==========================================================================
//
// DFrameBuffer :: WipeEndScreen
//
// Grabs a copy of the most-recently drawn, but not yet displayed, screen
// to serve as the final frame of a screen wipe.
//
//==========================================================================

void DFrameBuffer::WipeEndScreen()
{
	wipe_EndScreen();
	Unlock();
}

//==========================================================================
//
// DFrameBuffer :: WipeDo
//
// Draws one frame of a screenwipe. Should be called no more than 35
// times per second. If called less than that, ticks indicates how many
// ticks have passed since the last call.
//
//==========================================================================

bool DFrameBuffer::WipeDo(int ticks)
{
	Lock(true);
	return wipe_ScreenWipe(ticks);
}

//==========================================================================
//
// DFrameBuffer :: WipeCleanup
//
//==========================================================================

void DFrameBuffer::WipeCleanup()
{
	wipe_Cleanup();
}

//==========================================================================
//
// DFrameBuffer :: GameRestart
//
//==========================================================================

void DFrameBuffer::GameRestart()
{
}

//===========================================================================
//
// 
//
//===========================================================================

FNativePalette::~FNativePalette()
{
}

FNativeTexture::~FNativeTexture()
{
	// Remove link from the game texture
	if (mGameTex != nullptr)
	{
		mGameTex->Native[mFormat] = nullptr;
	}

}

bool FNativeTexture::CheckWrapping(bool wrapping)
{
	return true;
}

CCMD(clean)
{
	Printf ("CleanXfac: %d\nCleanYfac: %d\n", CleanXfac, CleanYfac);
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
	screen->SetGamma (Gamma);

	// Load fonts now so they can be packed into textures straight away,
	// if D3DFB is being used for the display.
	FFont::StaticPreloadFonts();

	DisplayBits = bits;
	V_UpdateModeSize(screen->GetWidth(), screen->GetHeight());

	M_RefreshModesList ();

	return true;
}

void V_UpdateModeSize (int width, int height)
{
	int cx1, cx2;
	V_CalcCleanFacs(320, 200, width, height, &CleanXfac, &CleanYfac, &cx1, &cx2);

	CleanWidth = width / CleanXfac;
	CleanHeight = height / CleanYfac;
	assert(CleanWidth >= 320);
	assert(CleanHeight >= 200);

	if (width < 800 || width >= 960)
	{
		if (cx1 < cx2)
		{
			// Special case in which we don't need to scale down.
			CleanXfac_1 = 
			CleanYfac_1 = cx1;
		}
		else
		{
			CleanXfac_1 = MAX(CleanXfac - 1, 1);
			CleanYfac_1 = MAX(CleanYfac - 1, 1);
			// On larger screens this is not enough so make sure it's at most 3/4 of the screen's width
			while (CleanXfac_1 * 320 > screen->GetWidth()*3/4 && CleanXfac_1 > 2)
			{
				CleanXfac_1--;
				CleanYfac_1--;
			}
		}
		CleanWidth_1 = width / CleanXfac_1;
		CleanHeight_1 = height / CleanYfac_1;
	}
	else // if the width is between 800 and 960 the ratio between the screensize and CleanXFac-1 becomes too large.
	{
		CleanXfac_1 = CleanXfac;
		CleanYfac_1 = CleanYfac;
		CleanWidth_1 = CleanWidth;
		CleanHeight_1 = CleanHeight;
	}


	DisplayWidth = width;
	DisplayHeight = height;

	R_OldBlend = ~0;
	Renderer->OnModeSet();
}

void V_OutputResized (int width, int height)
{
	V_UpdateModeSize(width, height);
	setsizeneeded = true;
	if (StatusBar != NULL)
	{
		StatusBar->CallScreenSizeChanged();
	}
	C_NewModeAdjust();
}

void V_CalcCleanFacs (int designwidth, int designheight, int realwidth, int realheight, int *cleanx, int *cleany, int *_cx1, int *_cx2)
{
	float ratio;
	int cwidth;
	int cheight;
	int cx1, cy1, cx2, cy2;

	// For larger screems always use at least a 16:9 ratio for clean factor calculation, even if the actual ratio is narrower.
	if (realwidth > 1280 && (double)realwidth / realheight < 16./9)
	{
		realheight = realwidth * 9 / 16;
	}

	ratio = ActiveRatio(realwidth, realheight);
	if (AspectTallerThanWide(ratio))
	{
		cwidth = realwidth;
		cheight = realheight * AspectMultiplier(ratio) / 48;
	}
	else
	{
		cwidth = realwidth * AspectMultiplier(ratio) / 48;
		cheight = realheight;
	}
	// Use whichever pair of cwidth/cheight or width/height that produces less difference
	// between CleanXfac and CleanYfac.
	cx1 = MAX(cwidth / designwidth, 1);
	cy1 = MAX(cheight / designheight, 1);
	cx2 = MAX(realwidth / designwidth, 1);
	cy2 = MAX(realheight / designheight, 1);
	if (abs(cx1 - cy1) <= abs(cx2 - cy2) || MAX(cx1, cx2) >= 4)
	{ // e.g. 640x360 looks better with this.
		*cleanx = cx1;
		*cleany = cy1;
	}
	else
	{ // e.g. 720x480 looks better with this.
		*cleanx = cx2;
		*cleany = cy2;
	}

	if (*cleanx < *cleany)
		*cleany = *cleanx;
	else
		*cleanx = *cleany;

	if (_cx1 != NULL)	*_cx1 = cx1;
	if (_cx2 != NULL)	*_cx2 = cx2;
}

bool IVideo::SetResolution (int width, int height, int bits)
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
	bool	goodmode = false;
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

	if (!fullscreen)
		goodmode = true;

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

void V_Init (bool restart) 
{ 
	const char *i;
	int width, height, bits;

	atterm (V_Shutdown);

	// [RH] Initialize palette management
	InitPalette ();

	if (!restart)
	{
		width = height = bits = 0;

		if ( (i = Args->CheckValue ("-width")) )
			width = atoi (i);

		if ( (i = Args->CheckValue ("-height")) )
			height = atoi (i);

		if ( (i = Args->CheckValue ("-bits")) )
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
		screen = new DDummyFrameBuffer (width, height);
	}
	// Update screen palette when restarting
	else
	{
		PalEntry *palette = screen->GetPalette ();
		for (int i = 0; i < 256; ++i)
			*palette++ = GPalette.BaseColors[i];
		screen->UpdatePalette();
	}

	BuildTransTable (GPalette.BaseColors);
}

void V_Init2()
{
	int width = screen->GetWidth();
	int height = screen->GetHeight();
	float gamma = static_cast<DDummyFrameBuffer *>(screen)->Gamma;

	{
		DFrameBuffer *s = screen;
		screen = NULL;
		delete s;
	}

	I_InitGraphics();
	I_ClosestResolution (&width, &height, 8);

	if (!Video->SetResolution (width, height, 8))
		I_FatalError ("Could not set resolution to %d x %d x %d", width, height, 8);
	else
		Printf ("Resolution: %d x %d\n", SCREENWIDTH, SCREENHEIGHT);

	screen->SetGamma (gamma);
	Renderer->RemapVoxels();
	FBaseCVar::ResetColors ();
	C_NewModeAdjust();
	M_InitVideoModesMenu();
	V_SetBorderNeedRefresh();
	setsizeneeded = true;
}

void V_Shutdown()
{
	if (screen)
	{
		DFrameBuffer *s = screen;
		screen = NULL;
		delete s;
	}
	V_ClearFonts();
}

CUSTOM_CVAR (Int, vid_aspect, 0, CVAR_GLOBALCONFIG|CVAR_ARCHIVE)
{
	setsizeneeded = true;
	if (StatusBar != NULL)
	{
		StatusBar->CallScreenSizeChanged();
	}
}

// Helper for ActiveRatio and CheckRatio. Returns the forced ratio type, or -1 if none.
int ActiveFakeRatio(int width, int height)
{
	int fakeratio = -1;
	if ((vid_aspect >= 1) && (vid_aspect <= 6))
	{
		// [SP] User wants to force aspect ratio; let them.
		fakeratio = int(vid_aspect);
		if (fakeratio == 3)
		{
			fakeratio = 0;
		}
		else if (fakeratio == 5)
		{
			fakeratio = 3;
		}
	}
	else if (vid_aspect == 0 && ViewportIsScaled43())
	{
		fakeratio = 0;
	}
	return fakeratio;
}

// Active screen ratio based on cvars and size
float ActiveRatio(int width, int height, float *trueratio)
{
	static float forcedRatioTypes[] =
	{
		4 / 3.0f,
		16 / 9.0f,
		16 / 10.0f,
		17 / 10.0f,
		5 / 4.0f,
		17 / 10.0f,
		21 / 9.0f
	};

	float ratio = width / (float)height;
	int fakeratio = ActiveFakeRatio(width, height);

	if (trueratio)
		*trueratio = ratio;
	return (fakeratio != -1) ? forcedRatioTypes[fakeratio] : ratio;
}

DEFINE_ACTION_FUNCTION(_Screen, GetAspectRatio)
{
	ACTION_RETURN_FLOAT(ActiveRatio(screen->GetWidth(), screen->GetHeight(), nullptr));
}

// Tries to guess the physical dimensions of the screen based on the
// screen's pixel dimensions. Can return:
// 0: 4:3
// 1: 16:9
// 2: 16:10
// 3: 17:10
// 4: 5:4
// 5: 17:10 (redundant, never returned)
// 6: 21:9
int CheckRatio (int width, int height, int *trueratio)
{
	float aspect = width / (float)height;

	static std::pair<float, int> ratioTypes[] =
	{
		{ 21 / 9.0f , 6 },
		{ 16 / 9.0f , 1 },
		{ 17 / 10.0f , 3 },
		{ 16 / 10.0f , 2 },
		{ 4 / 3.0f , 0 },
		{ 5 / 4.0f , 4 },
		{ 0.0f, 0 }
	};

	int ratio = ratioTypes[0].second;
	float distance = fabs(ratioTypes[0].first - aspect);
	for (int i = 1; ratioTypes[i].first != 0.0f; i++)
	{
		float d = fabs(ratioTypes[i].first - aspect);
		if (d < distance)
		{
			ratio = ratioTypes[i].second;
			distance = d;
		}
	}

	int fakeratio = ActiveFakeRatio(width, height);
	if (fakeratio == -1)
		fakeratio = ratio;

	if (trueratio)
		*trueratio = ratio;
	return fakeratio;
}

int AspectBaseWidth(float aspect)
{
	return (int)round(240.0f * aspect * 3.0f);
}

int AspectBaseHeight(float aspect)
{
	if (!AspectTallerThanWide(aspect))
		return (int)round(200.0f * (320.0f / (AspectBaseWidth(aspect) / 3.0f)) * 3.0f);
	else
		return (int)round((200.0f * (4.0f / 3.0f)) / aspect * 3.0f);
}

double AspectPspriteOffset(float aspect)
{
	if (!AspectTallerThanWide(aspect))
		return 0.0;
	else
		return ((4.0 / 3.0) / aspect - 1.0) * 97.5;
}

int AspectMultiplier(float aspect)
{
	if (!AspectTallerThanWide(aspect))
		return (int)round(320.0f / (AspectBaseWidth(aspect) / 3.0f) * 48.0f);
	else
		return (int)round(200.0f / (AspectBaseHeight(aspect) / 3.0f) * 48.0f);
}

bool AspectTallerThanWide(float aspect)
{
	return aspect < 1.333f;
}

void ScaleWithAspect (int &w, int &h, int Width, int Height)
{
	int resRatio = CheckRatio (Width, Height);
	int screenRatio;
	CheckRatio (w, h, &screenRatio);
	if (resRatio == screenRatio)
		return;

	double yratio;
	switch(resRatio)
	{
		case 0: yratio = 4./3.; break;
		case 1: yratio = 16./9.; break;
		case 2: yratio = 16./10.; break;
		case 3: yratio = 17./10.; break;
		case 4: yratio = 5./4.; break;
		case 6: yratio = 21./9.; break;
		default: return;
	}
	double y = w/yratio;
	if (y > h)
		w = static_cast<int>(h * yratio);
	else
		h = static_cast<int>(y);
}

void IVideo::DumpAdapters ()
{
	Printf("Multi-monitor support unavailable.\n");
}

CCMD(vid_listadapters)
{
	if (Video != NULL)
		Video->DumpAdapters();
}

DEFINE_GLOBAL(SmallFont)
DEFINE_GLOBAL(SmallFont2)
DEFINE_GLOBAL(BigFont)
DEFINE_GLOBAL(ConFont)
DEFINE_GLOBAL(IntermissionFont)
DEFINE_GLOBAL(CleanXfac)
DEFINE_GLOBAL(CleanYfac)
DEFINE_GLOBAL(CleanWidth)
DEFINE_GLOBAL(CleanHeight)
DEFINE_GLOBAL(CleanXfac_1)
DEFINE_GLOBAL(CleanYfac_1)
DEFINE_GLOBAL(CleanWidth_1)
DEFINE_GLOBAL(CleanHeight_1)
