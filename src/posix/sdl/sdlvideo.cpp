/*
** sdlvideo.cpp
**
**---------------------------------------------------------------------------
** Copyright 2005-2016 Randy Heit
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

// HEADER FILES ------------------------------------------------------------

#include "doomtype.h"

#include "templates.h"
#include "i_system.h"
#include "i_video.h"
#include "v_video.h"
#include "v_pfx.h"
#include "stats.h"
#include "v_palette.h"
#include "sdlvideo.h"
#include "swrenderer/r_swrenderer.h"
#include "version.h"

#include <SDL.h>

#ifdef __APPLE__
#include <OpenGL/OpenGL.h>
#endif // __APPLE__

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

struct MiniModeInfo
{
	uint16_t Width, Height;
};

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern IVideo *Video;
extern bool GUICapture;

EXTERN_CVAR (Float, Gamma)
EXTERN_CVAR (Int, vid_maxfps)
EXTERN_CVAR (Bool, cl_capfps)
EXTERN_CVAR (Bool, vid_vsync)

// PUBLIC DATA DEFINITIONS -------------------------------------------------

CVAR (Int, vid_adapter, 0, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)

CVAR (Int, vid_displaybits, 32, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)

CVAR (Bool, vid_forcesurface, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)

CUSTOM_CVAR (Float, rgamma, 1.f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (screen != NULL)
	{
		screen->SetGamma (Gamma);
	}
}
CUSTOM_CVAR (Float, ggamma, 1.f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (screen != NULL)
	{
		screen->SetGamma (Gamma);
	}
}
CUSTOM_CVAR (Float, bgamma, 1.f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (screen != NULL)
	{
		screen->SetGamma (Gamma);
	}
}

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static cycle_t BlitCycles;
static cycle_t SDLFlipCycles;

// CODE --------------------------------------------------------------------

// FrameBuffer implementation -----------------------------------------------

SDLFB::SDLFB (int width, int height, bool bgra, bool fullscreen, SDL_Window *oldwin)
	: SDLBaseFB (width, height, bgra)
{
	int i;
	
	NeedPalUpdate = false;
	NeedGammaUpdate = false;
	UpdatePending = false;
	NotPaletted = false;
	FlashAmount = 0;

	if (oldwin)
	{
		// In some cases (Mac OS X fullscreen) SDL2 doesn't like having multiple windows which
		// appears to inevitably happen while compositor animations are running. So lets try
		// to reuse the existing window.
		Screen = oldwin;
		SDL_SetWindowSize (Screen, width, height);
		SetFullscreen (fullscreen);
	}
	else
	{
		FString caption;
		caption.Format(GAMESIG " %s (%s)", GetVersionString(), GetGitTime());

		Screen = SDL_CreateWindow (caption,
			SDL_WINDOWPOS_UNDEFINED_DISPLAY(vid_adapter), SDL_WINDOWPOS_UNDEFINED_DISPLAY(vid_adapter),
			width, height, (fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0)|SDL_WINDOW_RESIZABLE);

		if (Screen == NULL)
			return;
	}

	Renderer = NULL;
	Texture = NULL;
	ResetSDLRenderer ();

	for (i = 0; i < 256; i++)
	{
		GammaTable[0][i] = GammaTable[1][i] = GammaTable[2][i] = i;
	}

	memcpy (SourcePalette, GPalette.BaseColors, sizeof(PalEntry)*256);
	UpdateColors ();

#ifdef __APPLE__
	SetVSync (vid_vsync);
#endif
}


SDLFB::~SDLFB ()
{
	if (Renderer)
	{
		if (Texture)
			SDL_DestroyTexture (Texture);
		SDL_DestroyRenderer (Renderer);
	}

	if(Screen)
	{
		SDL_DestroyWindow (Screen);
	}
}

bool SDLFB::IsValid ()
{
	return DFrameBuffer::IsValid() && Screen != NULL;
}

int SDLFB::GetPageCount ()
{
	return 1;
}

bool SDLFB::Lock (bool buffered)
{
	return DSimpleCanvas::Lock ();
}

bool SDLFB::Relock ()
{
	return DSimpleCanvas::Lock ();
}

void SDLFB::Unlock ()
{
	if (UpdatePending && LockCount == 1)
	{
		Update ();
	}
	else if (--LockCount <= 0)
	{
		Buffer = NULL;
		LockCount = 0;
	}
}

void SDLFB::Update ()
{
	if (LockCount != 1)
	{
		if (LockCount > 0)
		{
			UpdatePending = true;
			--LockCount;
		}
		return;
	}

	DrawRateStuff ();

#if !defined(__APPLE__) && !defined(__OpenBSD__)
	if(vid_maxfps && !cl_capfps)
	{
		SEMAPHORE_WAIT(FPSLimitSemaphore)
	}
#endif

	Buffer = NULL;
	LockCount = 0;
	UpdatePending = false;

	BlitCycles.Reset();
	SDLFlipCycles.Reset();
	BlitCycles.Clock();

	void *pixels;
	int pitch;
	if (UsingRenderer)
	{
		if (SDL_LockTexture (Texture, NULL, &pixels, &pitch))
			return;
	}
	else
	{
		if (SDL_LockSurface (Surface))
			return;

		pixels = Surface->pixels;
		pitch = Surface->pitch;
	}

	if (Bgra)
	{
		CopyWithGammaBgra(pixels, pitch, GammaTable[0], GammaTable[1], GammaTable[2], Flash, FlashAmount);
	}
	else if (NotPaletted)
	{
		GPfx.Convert (MemBuffer, Pitch,
			pixels, pitch, Width, Height,
			FRACUNIT, FRACUNIT, 0, 0);
	}
	else
	{
		if (pitch == Pitch)
		{
			memcpy (pixels, MemBuffer, Width*Height);
		}
		else
		{
			for (int y = 0; y < Height; ++y)
			{
				memcpy ((uint8_t *)pixels+y*pitch, MemBuffer+y*Pitch, Width);
			}
		}
	}

	if (UsingRenderer)
	{
		SDL_UnlockTexture (Texture);

		SDLFlipCycles.Clock();
		SDL_RenderClear(Renderer);
		SDL_RenderCopy(Renderer, Texture, NULL, NULL);
		SDL_RenderPresent(Renderer);
		SDLFlipCycles.Unclock();
	}
	else
	{
		SDL_UnlockSurface (Surface);

		SDLFlipCycles.Clock();
		SDL_UpdateWindowSurface (Screen);
		SDLFlipCycles.Unclock();
	}

	BlitCycles.Unclock();

	if (NeedGammaUpdate)
	{
		bool Windowed = false;
		NeedGammaUpdate = false;
		CalcGamma ((Windowed || rgamma == 0.f) ? Gamma : (Gamma * rgamma), GammaTable[0]);
		CalcGamma ((Windowed || ggamma == 0.f) ? Gamma : (Gamma * ggamma), GammaTable[1]);
		CalcGamma ((Windowed || bgamma == 0.f) ? Gamma : (Gamma * bgamma), GammaTable[2]);
		NeedPalUpdate = true;
	}
	
	if (NeedPalUpdate)
	{
		NeedPalUpdate = false;
		UpdateColors ();
	}
}

void SDLFB::UpdateColors ()
{
	if (NotPaletted)
	{
		PalEntry palette[256];
		
		for (int i = 0; i < 256; ++i)
		{
			palette[i].r = GammaTable[0][SourcePalette[i].r];
			palette[i].g = GammaTable[1][SourcePalette[i].g];
			palette[i].b = GammaTable[2][SourcePalette[i].b];
		}
		if (FlashAmount)
		{
			DoBlending (palette, palette,
				256, GammaTable[0][Flash.r], GammaTable[1][Flash.g], GammaTable[2][Flash.b],
				FlashAmount);
		}
		GPfx.SetPalette (palette);
	}
	else
	{
		SDL_Color colors[256];
		
		for (int i = 0; i < 256; ++i)
		{
			colors[i].r = GammaTable[0][SourcePalette[i].r];
			colors[i].g = GammaTable[1][SourcePalette[i].g];
			colors[i].b = GammaTable[2][SourcePalette[i].b];
		}
		if (FlashAmount)
		{
			DoBlending ((PalEntry *)colors, (PalEntry *)colors,
				256, GammaTable[2][Flash.b], GammaTable[1][Flash.g], GammaTable[0][Flash.r],
				FlashAmount);
		}
		SDL_SetPaletteColors (Surface->format->palette, colors, 0, 256);
	}
}

PalEntry *SDLFB::GetPalette ()
{
	return SourcePalette;
}

void SDLFB::UpdatePalette ()
{
	NeedPalUpdate = true;
}

bool SDLFB::SetGamma (float gamma)
{
	Gamma = gamma;
	NeedGammaUpdate = true;
	return true;
}

bool SDLFB::SetFlash (PalEntry rgb, int amount)
{
	Flash = rgb;
	FlashAmount = amount;
	NeedPalUpdate = true;
	return true;
}

void SDLFB::GetFlash (PalEntry &rgb, int &amount)
{
	rgb = Flash;
	amount = FlashAmount;
}

// Q: Should I gamma adjust the returned palette?
void SDLFB::GetFlashedPalette (PalEntry pal[256])
{
	memcpy (pal, SourcePalette, 256*sizeof(PalEntry));
	if (FlashAmount)
	{
		DoBlending (pal, pal, 256, Flash.r, Flash.g, Flash.b, FlashAmount);
	}
}

void SDLFB::SetFullscreen (bool fullscreen)
{
	if (IsFullscreen() == fullscreen)
		return;

	SDL_SetWindowFullscreen (Screen, fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
	if (!fullscreen)
	{
		// Restore proper window size
		SDL_SetWindowSize (Screen, Width, Height);
	}

	ResetSDLRenderer ();
}

bool SDLFB::IsFullscreen ()
{
	return (SDL_GetWindowFlags (Screen) & SDL_WINDOW_FULLSCREEN_DESKTOP) != 0;
}

void SDLFB::ResetSDLRenderer ()
{
	if (Renderer)
	{
		if (Texture)
			SDL_DestroyTexture (Texture);
		SDL_DestroyRenderer (Renderer);
	}

	UsingRenderer = !vid_forcesurface;
	if (UsingRenderer)
	{
		Renderer = SDL_CreateRenderer (Screen, -1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_TARGETTEXTURE|
										(vid_vsync ? SDL_RENDERER_PRESENTVSYNC : 0));
		if (!Renderer)
			return;

		SDL_SetRenderDrawColor(Renderer, 0, 0, 0, 255);

		Uint32 fmt;
		if (Bgra)
		{
			fmt = SDL_PIXELFORMAT_ARGB8888;
		}
		else
		{
			switch (vid_displaybits)
			{
				default: fmt = SDL_PIXELFORMAT_ARGB8888; break;
				case 30: fmt = SDL_PIXELFORMAT_ARGB2101010; break;
				case 24: fmt = SDL_PIXELFORMAT_RGB888; break;
				case 16: fmt = SDL_PIXELFORMAT_RGB565; break;
				case 15: fmt = SDL_PIXELFORMAT_ARGB1555; break;
			}
		}
		Texture = SDL_CreateTexture (Renderer, fmt, SDL_TEXTUREACCESS_STREAMING, Width, Height);

		{
			NotPaletted = true;

			Uint32 format;
			SDL_QueryTexture(Texture, &format, NULL, NULL, NULL);

			Uint32 Rmask, Gmask, Bmask, Amask;
			int bpp;
			SDL_PixelFormatEnumToMasks(format, &bpp, &Rmask, &Gmask, &Bmask, &Amask);
			GPfx.SetFormat (bpp, Rmask, Gmask, Bmask);
		}
	}
	else
	{
		Surface = SDL_GetWindowSurface (Screen);

		if (Surface->format->palette == NULL)
		{
			NotPaletted = true;
			GPfx.SetFormat (Surface->format->BitsPerPixel, Surface->format->Rmask, Surface->format->Gmask, Surface->format->Bmask);
		}
		else
			NotPaletted = false;
	}

	// In fullscreen, set logical size according to animorphic ratio.
	// Windowed modes are rendered to fill the window (usually 1:1)
	if (IsFullscreen ())
	{
		int w, h;
		SDL_GetWindowSize (Screen, &w, &h);
		ScaleWithAspect (w, h, Width, Height);
		SDL_RenderSetLogicalSize (Renderer, w, h);
	}
}

void SDLFB::SetVSync (bool vsync)
{
#ifdef __APPLE__
	if (CGLContextObj context = CGLGetCurrentContext())
	{
		// Apply vsync for native backend only (where OpenGL context is set)

#if MAC_OS_X_VERSION_MAX_ALLOWED < 1050
		// Inconsistency between 10.4 and 10.5 SDKs:
		// third argument of CGLSetParameter() is const long* on 10.4 and const GLint* on 10.5
		// So, GLint typedef'ed to long instead of int to workaround this issue
		typedef long GLint;
#endif // prior to 10.5

		const GLint value = vsync ? 1 : 0;
		CGLSetParameter(context, kCGLCPSwapInterval, &value);
	}
#else
	ResetSDLRenderer ();
#endif // __APPLE__
}

void SDLFB::ScaleCoordsFromWindow(int16_t &x, int16_t &y)
{
	int w, h;
	SDL_GetWindowSize (Screen, &w, &h);

	// Detect if we're doing scaling in the Window and adjust the mouse
	// coordinates accordingly. This could be more efficent, but I
	// don't think performance is an issue in the menus.
	if(IsFullscreen())
	{
		int realw = w, realh = h;
		ScaleWithAspect (realw, realh, SCREENWIDTH, SCREENHEIGHT);
		if (realw != SCREENWIDTH || realh != SCREENHEIGHT)
		{
			double xratio = (double)SCREENWIDTH/realw;
			double yratio = (double)SCREENHEIGHT/realh;
			if (realw < w)
			{
				x = (x - (w - realw)/2)*xratio;
				y *= yratio;
			}
			else
			{
				y = (y - (h - realh)/2)*yratio;
				x *= xratio;
			}
		}
	}
	else
	{
		x = (int16_t)(x*Width/w);
		y = (int16_t)(y*Height/h);
	}
}

ADD_STAT (blit)
{
	FString out;
	out.Format ("blit=%04.1f ms  flip=%04.1f ms",
		BlitCycles.TimeMS(), SDLFlipCycles.TimeMS());
	return out;
}
