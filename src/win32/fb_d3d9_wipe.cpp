/*
** fb_d3d9_wipe.cpp
** Implements the different screen wipes using Direct3D calls.
**
**---------------------------------------------------------------------------
** Copyright 1998-2008 Randy Heit
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

#ifdef _DEBUG
#define D3D_DEBUG_INFO
#endif
#define DIRECT3D_VERSION 0x0900
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <d3d9.h>
#include <stdio.h>

#define USE_WINDOWS_DWORD
#include "doomtype.h"
#include "f_wipe.h"
#include "win32iface.h"
#include "templates.h"
#include "m_random.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

class D3DFB::Wiper_Crossfade : public D3DFB::Wiper
{
public:
	Wiper_Crossfade();
	bool Run(int ticks, D3DFB *fb);

private:
	int Clock;
};

class D3DFB::Wiper_Melt : public D3DFB::Wiper
{
public:
	Wiper_Melt();
	bool Run(int ticks, D3DFB *fb);

private:
	// Match the strip sizes that oldschool Doom used.
	static const int WIDTH = 160, HEIGHT = 200;
	int y[WIDTH];
};

class D3DFB::Wiper_Burn : public D3DFB::Wiper
{
public:
	Wiper_Burn(D3DFB *fb);
	~Wiper_Burn();
	bool Run(int ticks, D3DFB *fb);

private:
	static const int WIDTH = 64, HEIGHT = 64;
	BYTE BurnArray[WIDTH * (HEIGHT + 5)];
	IDirect3DTexture9 *BurnTexture;
	int Density;
	int BurnTime;

	struct BURNVERTEX
	{
		FLOAT x, y, z, rhw;
		FLOAT tu0, tv0;
		FLOAT tu1, tv1;
	};
#define D3DFVF_BURNVERTEX (D3DFVF_XYZRHW|D3DFVF_TEX2)
};

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

EXTERN_CVAR(Bool, test2d)

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// CODE --------------------------------------------------------------------

//==========================================================================
//
// D3DFB :: WipeStartScreen
//
// Called before the current screen has started rendering. This needs to
// save what was drawn the previous frame so that it can be animated into
// what gets drawn this frame.
//
// In fullscreen mode, we use GetFrontBufferData() to grab the data that
// is visible on screen right now.
//
// In windowed mode, we can't do that because we'll get the whole desktop.
// Instead, we can conveniently use the TempRenderTexture, which is normally
// used for gamma-correcting copying the image to the back buffer.
//
//==========================================================================

bool D3DFB::WipeStartScreen(int type)
{
	IDirect3DSurface9 *tsurf;
	D3DSURFACE_DESC desc;

	if (!test2d)
	{
		return Super::WipeStartScreen(type);
	}

	switch (type)
	{
	case wipe_Melt:
		ScreenWipe = new Wiper_Melt;
		break;

	case wipe_Burn:
		ScreenWipe = new Wiper_Burn(this);
		break;

	case wipe_Fade:
		ScreenWipe = new Wiper_Crossfade;
		break;

	default:
		return false;
	}

	InitialWipeScreen = GetCurrentScreen();

	if (!Windowed)
	{
		FinalWipeScreen = TempRenderTexture;
	}
	else
	{
		// Create another texture to copy the final wipe screen to so
		// we can still gamma correct the wipe. Since this is just for
		// gamma correction, it's okay to fail (though not desirable.)
		if (GammaFixerShader != NULL && Gamma != 1)
		{
			if (SUCCEEDED(TempRenderTexture->GetSurfaceLevel(0, &tsurf)))
			{
				if (FAILED(tsurf->GetDesc(&desc)) ||
					FAILED(D3DDevice->CreateTexture(desc.Width, desc.Height,
						1, D3DUSAGE_RENDERTARGET, desc.Format, D3DPOOL_DEFAULT,
						&FinalWipeScreen, NULL)))
				{
					FinalWipeScreen = TempRenderTexture;
				}
				tsurf->Release();
			}
		}
		else
		{
			FinalWipeScreen = TempRenderTexture;
		}
	}

	// Make even fullscreen model render to the TempRenderTexture, so
	// we can have a copy of the new screen readily available.
	GatheringWipeScreen = true;
	return true;
}

//==========================================================================
//
// D3DFB :: WipeEndScreen
//
// The screen we want to animate to has just been drawn. This function is
// called in place of Update(), so it has not been Presented yet.
//
//==========================================================================

void D3DFB::WipeEndScreen()
{
	if (!test2d)
	{
		Super::WipeEndScreen();
		return;
	}

	// Don't do anything if there is no starting point.
	if (InitialWipeScreen == NULL)
	{
		return;
	}

	// If the whole screen was drawn without 2D accel, get it in to
	// video memory now.
	if (!In2D)
	{
		Begin2D(true);
	}

	EndBatch();			// Make sure all batched primitives have been drawn.

	// Don't do anything if there is no ending point.
	if (OldRenderTarget == NULL)
	{
		return;
	}

	// If these are different, reverse their roles so we don't need to
	// waste time copying from TempRenderTexture to FinalWipeScreen.
	swap(FinalWipeScreen, TempRenderTexture);

	// At this point, InitialWipeScreen holds the screen we are wiping from.
	// FinalWipeScreen holds the screen we are wiping to, which may be the
	// same texture as TempRenderTexture.
}

//==========================================================================
//
// D3DFB :: WipeDo
//
// Perform the actual wipe animation. The number of tics since the last
// time this function was called is passed in. Returns true when the wipe
// is over. The first time this function has been called, the screen is
// still locked from before and EndScene() still has not been called.
// Successive times need to call BeginScene().
//
//==========================================================================

bool D3DFB::WipeDo(int ticks)
{
	if (!test2d)
	{
		return Super::WipeDo(ticks);
	}

	// Sanity checks.
	if (InitialWipeScreen == NULL || FinalWipeScreen == NULL)
	{
		return true;
	}
	if (GatheringWipeScreen)
	{ // This is the first time we've been called for this wipe.
		GatheringWipeScreen = false;

		if (OldRenderTarget == NULL)
		{
			return true;
		}
		D3DDevice->SetRenderTarget(0, OldRenderTarget);
	}
	else
	{ // This is the second or later time we've been called for this wipe.
		D3DDevice->BeginScene();
		InScene = true;
	}
	SAFE_RELEASE( OldRenderTarget );
	if (TempRenderTexture != NULL && TempRenderTexture != FinalWipeScreen &&
		(Windowed && GammaFixerShader))
	{
		IDirect3DSurface9 *targetsurf;
		if (SUCCEEDED(TempRenderTexture->GetSurfaceLevel(0, &targetsurf)))
		{
			if (SUCCEEDED(D3DDevice->GetRenderTarget(0, &OldRenderTarget)))
			{
				if (FAILED(D3DDevice->SetRenderTarget(0, targetsurf)))
				{
					// Setting the render target failed.
				}
			}
			targetsurf->Release();
		}
	}
	In2D = 3;

	bool done = ScreenWipe->Run(ticks, this);
	DrawLetterbox();
	return done;
}

//==========================================================================
//
// D3DFB :: WipeCleanup
//
// Release any resources that were specifically created for the wipe.
//
//==========================================================================

void D3DFB::WipeCleanup()
{
	if (ScreenWipe != NULL)
	{
		delete ScreenWipe;
		ScreenWipe = NULL;
	}
	SAFE_RELEASE( InitialWipeScreen );
	if (FinalWipeScreen != NULL && FinalWipeScreen != TempRenderTexture)
	{
		FinalWipeScreen->Release();
	}
	FinalWipeScreen = NULL;
	GatheringWipeScreen = false;
	if (!test2d)
	{
		Super::WipeCleanup();
		return;
	}
}

//==========================================================================
//
// D3DFB :: Wiper Constructor
//
//==========================================================================

D3DFB::Wiper::~Wiper()
{
}

// WIPE: CROSSFADE ---------------------------------------------------------

//==========================================================================
//
// D3DFB :: Wiper_Crossfade Constructor
//
//==========================================================================

D3DFB::Wiper_Crossfade::Wiper_Crossfade()
: Clock(0)
{
}

//==========================================================================
//
// D3DFB :: Wiper_Crossfade :: Run
//
// Fades the old screen into the new one over 32 ticks.
//
//==========================================================================

bool D3DFB::Wiper_Crossfade::Run(int ticks, D3DFB *fb)
{
	Clock += ticks;

	// Put the initial screen back to the buffer, presumably with DMA.
	IDirect3DSurface9 *source = NULL, *target = NULL;

	if (SUCCEEDED(fb->InitialWipeScreen->GetSurfaceLevel(0, &source)) &&
		SUCCEEDED(fb->D3DDevice->GetRenderTarget(0, &target)))
	{
		fb->D3DDevice->UpdateSurface(source, NULL, target, NULL);
		target->Release();
	}
	if (source != NULL)
	{
		source->Release();
	}

	// Draw the new screen on top of it.
	FBVERTEX verts[4];

	fb->CalcFullscreenCoords(verts, false, D3DCOLOR_COLORVALUE(0,0,0,Clock / 32.f), D3DCOLOR_RGBA(255,255,255,0));
	fb->D3DDevice->SetFVF(D3DFVF_FBVERTEX);
	fb->SetTexture(0, fb->FinalWipeScreen);
	fb->SetAlphaBlend(TRUE, D3DBLEND_SRCALPHA, D3DBLEND_INVSRCALPHA);
	fb->SetPixelShader(fb->PlainShader);
	fb->D3DDevice->DrawPrimitiveUP(D3DPT_TRIANGLEFAN, 2, verts, sizeof(FBVERTEX));

	return Clock >= 32;
}

// WIPE: MELT --------------------------------------------------------------

//==========================================================================
//
// D3DFB :: Wiper_Melt Constructor
//
//==========================================================================

D3DFB::Wiper_Melt::Wiper_Melt()
{
	int i, r;

	// setup initial column positions
	// (y<0 => not ready to scroll yet)
	y[0] = -(M_Random() & 15);
	for (i = 1; i < WIDTH; ++i)
	{
		r = (M_Random()%3) - 1;
		y[i] = clamp(y[i-1] + r, -15, 0);
	}
}

//==========================================================================
//
// D3DFB :: Wiper_Melt :: Run
//
// Fades the old screen into the new one over 32 ticks.
//
//==========================================================================

bool D3DFB::Wiper_Melt::Run(int ticks, D3DFB *fb)
{
	IDirect3DSurface9 *source = NULL, *target;

	if (FAILED(fb->InitialWipeScreen->GetSurfaceLevel(0, &source)) ||
		FAILED(fb->D3DDevice->GetRenderTarget(0, &target)))
	{
		// A fat lot of good we can do if we can't get these two surfaces.
		if (source != NULL)
		{
			source->Release();
		}
		return true;
	}

	// Draw the new screen on the bottom.
	FBVERTEX verts[4];

	fb->CalcFullscreenCoords(verts, false, 0, 0xFFFFFFFF);
	fb->D3DDevice->SetFVF(D3DFVF_FBVERTEX);
	fb->SetTexture(0, fb->FinalWipeScreen);
	fb->SetAlphaBlend(FALSE);
	fb->SetPixelShader(fb->PlainShader);
	fb->D3DDevice->DrawPrimitiveUP(D3DPT_TRIANGLEFAN, 2, verts, sizeof(FBVERTEX));

	int i, dy;
	bool done = true;

	// Copy the old screen in vertical strips on top of the new one.
	while (ticks--)
	{
		done = true;
		for (i = 0; i < WIDTH; i++)
		{
			if (y[i] < 0)
			{
				y[i]++;
				done = false;
			} 
			else if (y[i] < HEIGHT)
			{
				dy = (y[i] < 16) ? y[i]+1 : 8;
				y[i] = MIN(y[i] + dy, HEIGHT);
				done = false;
			}
			if (ticks == 0)
			{ // Only draw for the final tick.
				RECT rect;
				POINT dpt;

				dpt.x = i * fb->Width / WIDTH;
				dpt.y = MAX(0, y[i] * fb->Height / HEIGHT);
				rect.left = dpt.x;
				rect.top = fb->LBOffsetI;
				rect.right = (i + 1) * fb->Width / WIDTH;
				rect.bottom = fb->Height - dpt.y + fb->LBOffsetI;
				dpt.y += fb->LBOffsetI;
				if (rect.bottom > rect.top)
				{
					fb->D3DDevice->UpdateSurface(source, &rect, target, &dpt);
				}
			}
		}
	}
	target->Release();
	source->Release();
	return done;
}

// WIPE: BURN --------------------------------------------------------------

//==========================================================================
//
// D3DFB :: Wiper_Burn Constructor
//
//==========================================================================

D3DFB::Wiper_Burn::Wiper_Burn(D3DFB *fb)
{
	Density = 4;
	BurnTime = 0;
	memset(BurnArray, 0, sizeof(BurnArray));
	if (fb->BurnShader == NULL || FAILED(fb->D3DDevice->CreateTexture(WIDTH, HEIGHT, 1,
		D3DUSAGE_DYNAMIC, D3DFMT_L8, D3DPOOL_DEFAULT, &BurnTexture, NULL)))
	{
		BurnTexture = NULL;
	}
}

//==========================================================================
//
// D3DFB :: Wiper_Burn Destructor
//
//==========================================================================

D3DFB::Wiper_Burn::~Wiper_Burn()
{
	SAFE_RELEASE( BurnTexture );
}

//==========================================================================
//
// D3DFB :: Wiper_Burn :: Run
//
//==========================================================================

bool D3DFB::Wiper_Burn::Run(int ticks, D3DFB *fb)
{
	bool done;

	BurnTime += ticks;
	ticks *= 2;

	// Make the fire burn
	done = false;
	while (!done && ticks--)
	{
		Density = wipe_CalcBurn(BurnArray, WIDTH, HEIGHT, Density);
		done = (Density < 0);
	}

	// Update the burn texture with the new burn data
	D3DLOCKED_RECT lrect;
	if (SUCCEEDED(BurnTexture->LockRect(0, &lrect, NULL, D3DLOCK_DISCARD)))
	{
		const BYTE *src = BurnArray;
		BYTE *dest = (BYTE *)lrect.pBits;
		for (int y = HEIGHT; y != 0; --y)
		{
			for (int x = WIDTH; x != 0; --x)
			{
				*dest++ = *src++;
			}
			dest += lrect.Pitch - WIDTH;
		}
		BurnTexture->UnlockRect(0);
	}

	// Put the initial screen back to the buffer.
	IDirect3DSurface9 *source = NULL, *target;

	if (SUCCEEDED(fb->InitialWipeScreen->GetSurfaceLevel(0, &source)) &&
		SUCCEEDED(fb->D3DDevice->GetRenderTarget(0, &target)))
	{
		fb->D3DDevice->UpdateSurface(source, NULL, target, NULL);
		target->Release();
	}
	if (source != NULL)
	{
		source->Release();
	}

	// Burn the new screen on top of it.
	float top = fb->LBOffset - 0.5f;
	float right = float(fb->Width) - 0.5f;
	float bot = float(fb->Height) + top;
	float texright = float(fb->Width) / float(fb->FBWidth);
	float texbot = float(fb->Height) / float(fb->FBHeight);

	BURNVERTEX verts[4] =
	{
		{ -0.5f, top, 0.5f, 1.f,      0.f,    0.f, 0, 0 },
		{ right, top, 0.5f, 1.f, texright,    0.f, 1, 0 },
		{ right, bot, 0.5f, 1.f, texright, texbot, 1, 1 },
		{ -0.5f, bot, 0.5f, 1.f,      0.f, texbot, 0, 1 }
	};

	fb->D3DDevice->SetFVF(D3DFVF_BURNVERTEX);
	fb->SetTexture(0, fb->FinalWipeScreen);
	fb->SetTexture(1, BurnTexture);
	fb->SetAlphaBlend(TRUE, D3DBLEND_SRCALPHA, D3DBLEND_INVSRCALPHA);
	fb->SetPixelShader(fb->BurnShader);
	fb->D3DDevice->SetSamplerState(1, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
	fb->D3DDevice->DrawPrimitiveUP(D3DPT_TRIANGLEFAN, 2, verts, sizeof(BURNVERTEX));
	fb->D3DDevice->SetSamplerState(1, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
	fb->D3DDevice->SetFVF(D3DFVF_FBVERTEX);

	// The fire may not always stabilize, so the wipe is forced to end
	// after an arbitrary maximum time.
	return done || (BurnTime > 40);
}
