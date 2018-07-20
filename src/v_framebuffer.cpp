/*
** The base framebuffer class
**
**---------------------------------------------------------------------------
** Copyright 1999-2016 Randy Heit
** Copyright 2005-2018 Christoph Oelckers
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

#include <stdio.h>

#include "i_system.h"
#include "x86.h"
#include "actor.h"

#include "v_video.h"

#include "c_dispatch.h"
#include "sbar.h"
#include "hardware.h"
#include "r_utility.h"
#include "r_renderer.h"
#include "vm.h"
#include "r_videoscale.h"
#include "i_time.h"


CVAR(Bool, gl_scale_viewport, true, CVAR_ARCHIVE);
CVAR(Bool, vid_fps, false, 0)
CVAR(Int, vid_showpalette, 0, 0)

EXTERN_CVAR(Bool, ticker)
EXTERN_CVAR(Float, vid_brightness)
EXTERN_CVAR(Float, vid_contrast)
EXTERN_CVAR(Int, screenblocks)

//==========================================================================
//
// DCanvas :: CalcGamma
//
//==========================================================================

void DFrameBuffer::CalcGamma (float gamma, uint8_t gammalookup[256])
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
	PixelBuffer = nullptr;
	Resize(width, height);
}

void DSimpleCanvas::Resize(int width, int height)
{
	Width = width;
	Height = height;

	if (PixelBuffer != NULL)
	{
		delete[] PixelBuffer;
		PixelBuffer = NULL;
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
	PixelBuffer = new uint8_t[Pitch * height * bytes_per_pixel];
	memset (PixelBuffer, 0, Pitch * height * bytes_per_pixel);
}

//==========================================================================
//
// DSimpleCanvas Destructor
//
//==========================================================================

DSimpleCanvas::~DSimpleCanvas ()
{
	if (PixelBuffer != NULL)
	{
		delete[] PixelBuffer;
		PixelBuffer = NULL;
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

DFrameBuffer::DFrameBuffer (int width, int height)
{
	SetSize(width, height);
}

void DFrameBuffer::SetSize(int width, int height)
{
	Width = ViewportScaledWidth(width, height);
	Height = ViewportScaledHeight(width, height);
}

//==========================================================================
//
// 
//
//==========================================================================

void V_DrawPaletteTester(int paletteno)
{
	int blocksize = screen->GetHeight() / 50;

	int t = paletteno;
	int k = 0;
	for (int i = 0; i < 16; ++i)
	{
		for (int j = 0; j < 16; ++j)
		{
			int palindex = (t > 1) ? translationtables[TRANSLATION_Standard][t - 2]->Remap[k] : k;
			PalEntry pe = GPalette.BaseColors[palindex];
			k++;
			screen->Dim(pe, 1.f, j*blocksize, i*blocksize, blocksize, blocksize);
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

			chars = mysnprintf (fpsbuff, countof(fpsbuff), "%2llu ms (%3llu fps)", howlong, LastCount);
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
		int64_t t = I_GetTime();
		int64_t tics = t - LastTic;

		LastTic = t;
		if (tics > 20) tics = 20;

		int i;
		for (i = 0; i < tics*2; i += 2)		Clear(i, Height-1, i+1, Height, 255, 0);
		for ( ; i < 20*2; i += 2)			Clear(i, Height-1, i+1, Height, 0, 0);
	}

	// draws the palette for debugging
	if (vid_showpalette)
	{
		V_DrawPaletteTester(vid_showpalette);
	}
}

//==========================================================================
//
// Palette stuff.
//
//==========================================================================

void DFrameBuffer::GetFlashedPalette(PalEntry pal[256])
{
	DoBlending(SourcePalette, pal, 256, Flash.r, Flash.g, Flash.b, Flash.a);
}

PalEntry *DFrameBuffer::GetPalette()
{
	return SourcePalette;
}

bool DFrameBuffer::SetFlash(PalEntry rgb, int amount)
{
	Flash = PalEntry(amount, rgb.r, rgb.g, rgb.b);
	return true;
}

void DFrameBuffer::GetFlash(PalEntry &rgb, int &amount)
{
	rgb = Flash;
	rgb.a = 0;
	amount = Flash.a;
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
// DFrameBuffer :: WipeStartScreen
//
// Grabs a copy of the screen currently displayed to serve as the initial
// frame of a screen wipe. Also determines which screenwipe will be
// performed.
//
//==========================================================================

bool DFrameBuffer::WipeStartScreen(int type)
{
	return false;
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
	return false;
}

//==========================================================================
//
// DFrameBuffer :: WipeCleanup
//
//==========================================================================

void DFrameBuffer::WipeCleanup()
{
}

//==========================================================================
//
// DFrameBuffer :: InitPalette
//
//==========================================================================

void DFrameBuffer::InitPalette()
{
	memcpy(SourcePalette, GPalette.BaseColors, sizeof(PalEntry) * 256);
	UpdatePalette();
}

//==========================================================================
//
//
//
//==========================================================================

void DFrameBuffer::BuildGammaTable(uint16_t *gammaTable)
{
	float gamma = clamp<float>(Gamma, 0.1f, 4.f);
	float contrast = clamp<float>(vid_contrast, 0.1f, 3.f);
	float bright = clamp<float>(vid_brightness, -0.8f, 0.8f);

	double invgamma = 1 / gamma;
	double norm = pow(255., invgamma - 1);

	for (int i = 0; i < 256; i++)
	{
		double val = i * contrast - (contrast - 1) * 127;
		val += bright * 128;
		if (gamma != 1) val = pow(val, invgamma) / norm;

		gammaTable[i] = gammaTable[i + 256] = gammaTable[i + 512] = (uint16_t)clamp<double>(val * 256, 0, 0xffff);
	}
}

//==========================================================================
//
// DFrameBuffer :: GetCaps
//
//==========================================================================

EXTERN_CVAR(Bool, r_drawvoxels)

uint32_t DFrameBuffer::GetCaps()
{
	ActorRenderFeatureFlags FlagSet = 0;

	if (V_IsPolyRenderer())
		FlagSet |= RFF_POLYGONAL | RFF_TILTPITCH | RFF_SLOPE3DFLOORS;
	else
	{
		FlagSet |= RFF_UNCLIPPEDTEX;
		if (r_drawvoxels)
			FlagSet |= RFF_VOXELS;
	}

	if (V_IsTrueColor())
		FlagSet |= RFF_TRUECOLOR;
	else
		FlagSet |= RFF_COLORMAP;

	return (uint32_t)FlagSet;
}

void DFrameBuffer::RenderTextureView(FCanvasTexture *tex, AActor *Viewpoint, double FOV)
{
	SWRenderer->RenderTextureView(tex, Viewpoint, FOV);
}

void DFrameBuffer::WriteSavePic(player_t *player, FileWriter *file, int width, int height)
{
	SWRenderer->WriteSavePic(player, file, width, height);
}


//==========================================================================
//
// Calculates the viewport values needed for 2D and 3D operations
//
//==========================================================================

void DFrameBuffer::SetViewportRects(IntRect *bounds)
{
	if (bounds)
	{
		mSceneViewport = *bounds;
		mScreenViewport = *bounds;
		mOutputLetterbox = *bounds;
		return;
	}

	// Special handling so the view with a visible status bar displays properly
	int height, width;
	if (screenblocks >= 10)
	{
		height = GetHeight();
		width = GetWidth();
	}
	else
	{
		height = (screenblocks*GetHeight() / 10) & ~7;
		width = (screenblocks*GetWidth() / 10);
	}

	// Back buffer letterbox for the final output
	int clientWidth = GetClientWidth();
	int clientHeight = GetClientHeight();
	if (clientWidth == 0 || clientHeight == 0)
	{
		// When window is minimized there may not be any client area.
		// Pretend to the rest of the render code that we just have a very small window.
		clientWidth = 160;
		clientHeight = 120;
	}
	int screenWidth = GetWidth();
	int screenHeight = GetHeight();
	float scaleX, scaleY;
	if (ViewportIsScaled43())
	{
		scaleX = MIN(clientWidth / (float)screenWidth, clientHeight / (screenHeight * 1.2f));
		scaleY = scaleX * 1.2f;
	}
	else
	{
		scaleX = MIN(clientWidth / (float)screenWidth, clientHeight / (float)screenHeight);
		scaleY = scaleX;
	}
	mOutputLetterbox.width = (int)round(screenWidth * scaleX);
	mOutputLetterbox.height = (int)round(screenHeight * scaleY);
	mOutputLetterbox.left = (clientWidth - mOutputLetterbox.width) / 2;
	mOutputLetterbox.top = (clientHeight - mOutputLetterbox.height) / 2;

	// The entire renderable area, including the 2D HUD
	mScreenViewport.left = 0;
	mScreenViewport.top = 0;
	mScreenViewport.width = screenWidth;
	mScreenViewport.height = screenHeight;

	// Viewport for the 3D scene
	mSceneViewport.left = viewwindowx;
	mSceneViewport.top = screenHeight - (height + viewwindowy - ((height - viewheight) / 2));
	mSceneViewport.width = viewwidth;
	mSceneViewport.height = height;

	// Scale viewports to fit letterbox
	bool notScaled = ((mScreenViewport.width == ViewportScaledWidth(mScreenViewport.width, mScreenViewport.height)) &&
		(mScreenViewport.width == ViewportScaledHeight(mScreenViewport.width, mScreenViewport.height)) &&
		!ViewportIsScaled43());
	if (gl_scale_viewport && !IsFullscreen() && notScaled)
	{
		mScreenViewport.width = mOutputLetterbox.width;
		mScreenViewport.height = mOutputLetterbox.height;
		mSceneViewport.left = (int)round(mSceneViewport.left * scaleX);
		mSceneViewport.top = (int)round(mSceneViewport.top * scaleY);
		mSceneViewport.width = (int)round(mSceneViewport.width * scaleX);
		mSceneViewport.height = (int)round(mSceneViewport.height * scaleY);
	}
}

//===========================================================================
// 
// Calculates the OpenGL window coordinates for a zdoom screen position
//
//===========================================================================

int DFrameBuffer::ScreenToWindowX(int x)
{
	return mScreenViewport.left + (int)round(x * mScreenViewport.width / (float)GetWidth());
}

int DFrameBuffer::ScreenToWindowY(int y)
{
	return mScreenViewport.top + mScreenViewport.height - (int)round(y * mScreenViewport.height / (float)GetHeight());
}

void DFrameBuffer::ScaleCoordsFromWindow(int16_t &x, int16_t &y)
{
	int letterboxX = mOutputLetterbox.left;
	int letterboxY = mOutputLetterbox.top;
	int letterboxWidth = mOutputLetterbox.width;
	int letterboxHeight = mOutputLetterbox.height;

	x = int16_t((x - letterboxX) * Width / letterboxWidth);
	y = int16_t((y - letterboxY) * Height / letterboxHeight);
}

//===========================================================================
// 
// 
//
//===========================================================================

#define DBGBREAK assert(0)

class DDummyFrameBuffer : public DFrameBuffer
{
	typedef DFrameBuffer Super;
public:
	DDummyFrameBuffer(int width, int height)
		: DFrameBuffer(0, 0)
	{
		SetVirtualSize(width, height);
	}
	// These methods should never be called.
	void Update() { DBGBREAK; }
	bool IsFullscreen() { DBGBREAK; return 0; }
	int GetClientWidth() { DBGBREAK; return 0; }
	int GetClientHeight() { DBGBREAK; return 0; }

	float Gamma;
};

