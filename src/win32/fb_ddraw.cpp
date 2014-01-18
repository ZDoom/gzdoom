/*
** fb_ddraw.cpp
** Code to let ZDoom use DirectDraw 3
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

#define DIRECTDRAW_VERSION 0x0300
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <ddraw.h>
#include <stdio.h>

#define USE_WINDOWS_DWORD
#include "doomtype.h"

#include "c_dispatch.h"
#include "templates.h"
#include "i_system.h"
#include "i_video.h"
#include "v_video.h"
#include "v_pfx.h"
#include "stats.h"
#include "doomerrors.h"

#include "win32iface.h"
#include "v_palette.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

IMPLEMENT_CLASS(DDrawFB)

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void DoBlending (const PalEntry *from, PalEntry *to, int count, int r, int g, int b, int a);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern HWND Window;
extern IVideo *Video;
extern BOOL AppActive;
extern int SessionState;
extern bool VidResizing;

EXTERN_CVAR (Bool, fullscreen)
EXTERN_CVAR (Float, Gamma)
EXTERN_CVAR (Int, vid_refreshrate)

extern IDirectDraw2 *DDraw;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

CVAR (Bool, vid_palettehack, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Bool, vid_attachedsurfaces, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Bool, vid_noblitter, true, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Int, vid_displaybits, 8, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
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

cycle_t BlitCycles;

// CODE --------------------------------------------------------------------

DDrawFB::DDrawFB (int width, int height, bool fullscreen)
	: BaseWinFB (width, height)
{
	int i;

	LastHR = 0;

	Palette = NULL;
	PrimarySurf = NULL;
	BackSurf = NULL;
	BackSurf2 = NULL;
	BlitSurf = NULL;
	Clipper = NULL;
	GDIPalette = NULL;
	ClipSize = 0;
	BufferCount = 1;
	Gamma = 1.0;
	BufferPitch = Pitch;
	FlipFlags = vid_vsync ? DDFLIP_WAIT : DDFLIP_WAIT|DDFLIP_NOVSYNC;
	PixelDoubling = 0;

	NeedGammaUpdate = false;
	NeedPalUpdate = false;
	NeedResRecreate = false;
	PaletteChangeExpected = false;
	MustBuffer = false;
	BufferingNow = false;
	WasBuffering = false;
	Write8bit = false;
	UpdatePending = false;
	UseBlitter = false;

	FlashAmount = 0;

	if (MemBuffer == NULL)
	{
		return;
	}

	for (i = 0; i < 256; i++)
	{
		PalEntries[i].peRed = GPalette.BaseColors[i].r;
		PalEntries[i].peGreen = GPalette.BaseColors[i].g;
		PalEntries[i].peBlue = GPalette.BaseColors[i].b;
		GammaTable[0][i] = GammaTable[1][i] = GammaTable[2][i] = (BYTE)i;
	}
	memcpy (SourcePalette, GPalette.BaseColors, sizeof(PalEntry)*256);

	MustBuffer = false;

	Windowed = !(static_cast<Win32Video *>(Video)->GoFullscreen (fullscreen));

	if (vid_noblitter)
	{
		LOG ("Blitter forced off\n");
	}
	else
	{
		DDCAPS hwcaps = { sizeof(DDCAPS) };
		HRESULT hr = DDraw->GetCaps (&hwcaps, NULL);
		if (SUCCEEDED(hr))
		{
			LOG2 ("dwCaps = %08lx, dwSVBCaps = %08lx\n", hwcaps.dwCaps, hwcaps.dwSVBCaps);
			if (hwcaps.dwCaps & DDCAPS_BLT)
			{
				LOG ("Driver supports blits\n");
				if (hwcaps.dwSVBCaps & DDCAPS_CANBLTSYSMEM)
				{
					LOG ("Driver can blit from system memory\n");
					if (hwcaps.dwCaps & DDCAPS_BLTQUEUE)
					{
						LOG ("Driver supports asynchronous blits\n");
						UseBlitter = true;
					}
					else
					{
						LOG ("Driver does not support asynchronous blits\n");
					}
				}
				else
				{
					LOG ("Driver cannot blit from system memory\n");
				}
			}
			else
			{
				LOG ("Driver does not support blits\n");
			}
		}
	}

	if (!CreateResources ())
	{
		SAFE_RELEASE( PrimarySurf );
	}
}

DDrawFB::~DDrawFB ()
{
	I_SaveWindowedPos ();
	ReleaseResources ();
}

bool DDrawFB::CreateResources ()
{
	DDSURFACEDESC ddsd = { sizeof(ddsd), };
	HRESULT hr;
	int bits;

	BufferCount = 1;

	if (!Windowed)
	{
		// Remove the window border in fullscreen mode
		SetWindowLong (Window, GWL_STYLE, WS_POPUP|WS_VISIBLE|WS_SYSMENU);

		TrueHeight = Height;
		for (Win32Video::ModeInfo *mode = static_cast<Win32Video *>(Video)->m_Modes; mode != NULL; mode = mode->next)
		{
			if (mode->width == Width && mode->height == Height)
			{
				TrueHeight = mode->realheight;
				PixelDoubling = mode->doubling;
				break;
			}
		}
		hr = DDraw->SetDisplayMode (Width << PixelDoubling, TrueHeight << PixelDoubling, bits = vid_displaybits, vid_refreshrate, 0);
		if (FAILED(hr))
		{
			hr = DDraw->SetDisplayMode (Width << PixelDoubling, TrueHeight << PixelDoubling, bits = vid_displaybits, 0, 0);
			bits = 32;
			while (FAILED(hr) && bits >= 8)
			{
				hr = DDraw->SetDisplayMode (Width << PixelDoubling, Height << PixelDoubling, bits, vid_refreshrate, 0);
				if (FAILED(hr))
				{
					hr = DDraw->SetDisplayMode (Width << PixelDoubling, Height << PixelDoubling, bits, 0, 0);
				}
				bits -= 8;
			}
			if (FAILED(hr))
			{
				LastHR = hr;
				return false;
			}
		}
		LOG3 ("Mode set to %d x %d x %d\n", Width, Height, bits);

		if (vid_attachedsurfaces && OSPlatform == os_WinNT4)
		{
			if (!CreateSurfacesAttached ())
				return false;
		}
		else
		{
			if (!CreateSurfacesComplex ())
				return false;
		}

		if (UseBlitter)
		{
			UseBlitter = CreateBlitterSource ();
		}
	}
	else
	{
		MustBuffer = true;

		LOG ("Running in a window\n");
		TrueHeight = Height;

		// Create the primary surface
		ddsd.dwFlags = DDSD_CAPS;
		ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
//		PixelDoubling = 1;
		do
		{
			hr = DDraw->CreateSurface (&ddsd, &PrimarySurf, NULL);
			LOG1 ("Create primary: %08lx\n", hr);
		} while (0);
		if (FAILED(hr))
		{
			LastHR = hr;
			return false;
		}

		MaybeCreatePalette ();

		// Resize the window to match desired dimensions
		int sizew = (Width << PixelDoubling) + GetSystemMetrics (SM_CXSIZEFRAME)*2;
		int sizeh = (Height << PixelDoubling) + GetSystemMetrics (SM_CYSIZEFRAME) * 2 +
					 GetSystemMetrics (SM_CYCAPTION);
		LOG2 ("Resize window to %dx%d\n", sizew, sizeh);
		VidResizing = true;
		// Make sure the window has a border in windowed mode
		SetWindowLong (Window, GWL_STYLE, WS_VISIBLE|WS_OVERLAPPEDWINDOW);
		if (!SetWindowPos (Window, NULL, 0, 0, sizew, sizeh,
			SWP_DRAWFRAME | SWP_NOCOPYBITS | SWP_NOMOVE | SWP_NOZORDER))
		{
			LOG1 ("SetWindowPos failed because %08lx\n", GetLastError());
		}
		I_RestoreWindowedPos ();
		VidResizing = false;

		// Create the clipper
		hr = DDraw->CreateClipper (0, &Clipper, NULL);
		LOG1 ("Create clipper: %08lx\n", hr);
		if (FAILED(hr))
		{
			LastHR = hr;
			return false;
		}
		// Associate the clipper with the window
		Clipper->SetHWnd (0, Window);
		PrimarySurf->SetClipper (Clipper);
		LOG1 ("Clipper @ %p set\n", Clipper);

		// Create the backbuffer
		ddsd.dwFlags        = DDSD_WIDTH | DDSD_HEIGHT | DDSD_CAPS;
		ddsd.dwWidth        = Width << PixelDoubling;
		ddsd.dwHeight       = Height << PixelDoubling;
		ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | (UseBlitter ? DDSCAPS_SYSTEMMEMORY : 0);

		hr = DDraw->CreateSurface (&ddsd, &BackSurf, NULL);
		LOG1 ("Create backbuffer: %08lx\n", hr);
		if (FAILED(hr))
		{
			LastHR = hr;
			return false;
		}
		LockingSurf = BackSurf;
		LOG1 ("LockingSurf and BackSurf @ %p\n", BackSurf);
		LOG ("Created backbuf\n");
	}
	SetGamma (Gamma);
	SetFlash (Flash, FlashAmount);
	return true;
}

bool DDrawFB::CreateSurfacesAttached ()
{
	DDSURFACEDESC ddsd = { sizeof(ddsd), };
	HRESULT hr;

	LOG ("creating surfaces using AddAttachedSurface\n");

	ddsd.dwFlags = DDSD_CAPS;
	ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE | DDSCAPS_VIDEOMEMORY;
	hr = DDraw->CreateSurface (&ddsd, &PrimarySurf, NULL);
	if (FAILED(hr))
	{
		LastHR = hr;
		return false;
	}

	LOG1 ("Primary surface @ %p\n", PrimarySurf);

	// Under NT 4 and with bad DDraw drivers under 9x (and maybe Win2k?)
	// if the palette is not attached to the primary surface before any
	// back buffers are added to it, colors 0 and 255 will remain black
	// and white respectively.
	MaybeCreatePalette ();

	// Try for triple buffering. Unbuffered output is only allowed if
	// we manage to get triple buffering. Even with double buffering,
	// framerate can slow significantly compared to triple buffering,
	// so we force buffering in that case, which effectively emulates
	// triple buffering (after a fashion).
	if (!AddBackBuf (&BackSurf, 1) || !AddBackBuf (&BackSurf2, 2))
	{
//		MustBuffer = true;
	}
	if (BackSurf != NULL)
	{
		DDSCAPS caps = { DDSCAPS_BACKBUFFER, };
		hr = PrimarySurf->GetAttachedSurface (&caps, &LockingSurf);
		if (FAILED (hr))
		{
			LOG1 ("Could not get attached surface: %08lx\n", hr);
			if (BackSurf2 != NULL)
			{
				PrimarySurf->DeleteAttachedSurface (0, BackSurf2);
				BackSurf2->Release ();
				BackSurf2 = NULL;
			}
			PrimarySurf->DeleteAttachedSurface (0, BackSurf);
			BackSurf->Release ();
			BackSurf = NULL;
//			MustBuffer = true;
			LockingSurf = PrimarySurf;
		}
		else
		{
			BufferCount = (BackSurf2 != NULL) ? 3 : 2;
			LOG ("Got attached surface\n");
		}
	}
	else
	{
		LOG ("No flip chain\n");
		LockingSurf = PrimarySurf;
	}
	return true;
}

bool DDrawFB::AddBackBuf (LPDIRECTDRAWSURFACE *surface, int num)
{
	DDSURFACEDESC ddsd = { sizeof(ddsd), };
	HRESULT hr;

	ddsd.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT;
	ddsd.dwWidth = Width;
	ddsd.dwHeight = Height;
	ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_VIDEOMEMORY;
	hr = DDraw->CreateSurface (&ddsd, surface, NULL);
	if (FAILED(hr))
	{
		LOG2 ("could not create back buf %d: %08lx\n", num, hr);
		return false;
	}
	else
	{
		LOG2 ("BackBuf %d created @ %p\n", num, *surface);
		hr = PrimarySurf->AddAttachedSurface (*surface);
		if (FAILED(hr))
		{
			LOG2 ("could not add back buf %d: %08lx\n", num, hr);
			(*surface)->Release ();
			*surface = NULL;
			return false;
		}
		else
		{
			LOG1 ("Attachment of back buf %d succeeded\n", num);
		}
	}
	return true;
}

bool DDrawFB::CreateSurfacesComplex ()
{
	DDSURFACEDESC ddsd = { sizeof(ddsd), };
	HRESULT hr;
	int tries = 2;

	LOG ("creating surfaces using a complex primary\n");

	// Try for triple buffering first.
	// If that fails, try for double buffering.
	// If that fails, settle for single buffering.
	// If that fails, then give up.
	//
	// However, if using the blitter, then do not triple buffer the
	// primary surface, because that is effectively like quadruple
	// buffering and player response starts feeling too sluggish.
	ddsd.dwFlags = DDSD_CAPS | DDSD_BACKBUFFERCOUNT;
	ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE | DDSCAPS_VIDEOMEMORY
		| DDSCAPS_FLIP | DDSCAPS_COMPLEX;
	do
	{
		LOG1 ("Try #%d\n", tries);
		ddsd.dwBackBufferCount = UseBlitter ? 1 : 2;
		hr = DDraw->CreateSurface (&ddsd, &PrimarySurf, NULL);
		if (FAILED(hr))
		{
			if (hr == DDERR_NOEXCLUSIVEMODE)
			{
				LOG ("Exclusive mode was lost, so restoring it now.\n");
				hr = DDraw->SetCooperativeLevel (Window, DDSCL_ALLOWMODEX | DDSCL_ALLOWREBOOT | DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN);
				LOG1 ("SetCooperativeLevel result: %08lx\n", hr);
				hr = DDraw->SetDisplayMode (Width, Height, 8, 0, 0);
				//hr = DDraw->RestoreDisplayMode ();
				LOG1 ("SetDisplayMode result: %08lx\n", hr);
				++tries;
				hr = E_FAIL;
				continue;
			}

			LOG1 ("Could not create with 2 backbuffers: %lx\n", hr);
			ddsd.dwBackBufferCount = 1;
			hr = DDraw->CreateSurface (&ddsd, &PrimarySurf, NULL);
			if (FAILED(hr))
			{
				LOG1 ("Could not create with 1 backbuffer: %lx\n", hr);
				ddsd.ddsCaps.dwCaps &= ~DDSCAPS_FLIP | DDSCAPS_COMPLEX;
				ddsd.dwBackBufferCount = 0;
				hr = DDraw->CreateSurface (&ddsd, &PrimarySurf, NULL);
				if (FAILED (hr))
				{
					LOG1 ("Could not create with 0 backbuffers: %lx\n", hr);
					if (tries == 2)
					{
						LOG ("Retrying without DDSCAPS_VIDEOMEMORY\n");
						ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE
							| DDSCAPS_FLIP | DDSCAPS_COMPLEX;
					}
				}
			}
		}
	} while (FAILED(hr) && --tries);

	if (FAILED(hr))
	{
		LastHR = hr;
		return false;
	}

	LOG1 ("Complex surface chain @ %p\n", PrimarySurf);
	if (PrimarySurf == NULL)
	{
		LOG ("It's NULL but it didn't fail?!?\n");
		LastHR = E_FAIL;
		return false;
	}

	if (ddsd.dwBackBufferCount == 0)
	{
		LOG ("No flip chain\n");
//		MustBuffer = true;
		LockingSurf = PrimarySurf;
	}
	else
	{
		DDSCAPS caps = { DDSCAPS_BACKBUFFER, };
		hr = PrimarySurf->GetAttachedSurface (&caps, &LockingSurf);
		if (FAILED (hr))
		{
			LOG1 ("Could not get attached surface: %08lx\n", hr);
//			MustBuffer = true;
			LockingSurf = PrimarySurf;
		}
		else
		{
			BufferCount = ddsd.dwBackBufferCount + 1;
			LOG1 ("Got attached surface. %d buffers\n", BufferCount);
		}
	}

	MaybeCreatePalette ();
	return true;
}

bool DDrawFB::CreateBlitterSource ()
{
	DDSURFACEDESC ddsd = { sizeof(ddsd), };
	HRESULT hr;

	LOG ("Creating surface for blitter source\n");
	ddsd.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT | DDSD_BACKBUFFERCOUNT;
	ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY
		| DDSCAPS_FLIP | DDSCAPS_COMPLEX;
	ddsd.dwBackBufferCount = 2;
	ddsd.dwWidth = (Width==1024?1024+16:Width);
	ddsd.dwHeight = Height;
	hr = DDraw->CreateSurface (&ddsd, &BlitSurf, NULL);
	if (FAILED(hr))
	{
		LOG1 ("Trying to create blitter source with only one surface (%08lx)\n", hr);
		ddsd.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT;
		ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY;
		hr = DDraw->CreateSurface (&ddsd, &BlitSurf, NULL);
		if (FAILED(hr))
		{
			LOG1 ("Could not create blitter source: %08lx\n", hr);
			MustBuffer = true;
			return false;
		}
		BufferCount = MAX (BufferCount, 1);
	}
	else
	{
		BufferCount = MAX (BufferCount, 2);
	}
	LOG1 ("Blitter source created successfully @ %p\n", BlitSurf);
	return true;
}

void DDrawFB::MaybeCreatePalette ()
{
	DDPIXELFORMAT fmt = { sizeof(fmt), };
	HRESULT hr;
	int i;

	UsePfx = false;

	// If the surface needs a palette, try to create one. If the palette
	// cannot be created, the result is ugly but non-fatal.
	hr = PrimarySurf->GetPixelFormat (&fmt);
	if (SUCCEEDED (hr) && (fmt.dwFlags & DDPF_PALETTEINDEXED8))
	{
		LOG ("Surface is paletted\n");
		GPfx.SetFormat (fmt.dwRGBBitCount,
			fmt.dwRBitMask, fmt.dwGBitMask, fmt.dwBBitMask);

		if (Windowed)
		{
			struct { LOGPALETTE head; PALETTEENTRY filler[255]; } pal;

			LOG ("Writing in a window\n");
			Write8bit = true;
			pal.head.palVersion = 0x300;
			pal.head.palNumEntries = 256;
			memcpy (pal.head.palPalEntry, PalEntries, 256*sizeof(PalEntries[0]));
			for (i = 0; i < 256; i++)
			{
				pal.head.palPalEntry[i].peFlags = 0;
			}
			GDIPalette = ::CreatePalette (&pal.head);
			LOG ("Created GDI palette\n");
			if (GDIPalette != NULL)
			{
				HDC dc = GetDC (Window);
				SelectPalette (dc, GDIPalette, FALSE);
				RealizePalette (dc);
				ReleaseDC (Window, dc);
				RebuildColorTable ();
			}
		}
		else
		{
			hr = DDraw->CreatePalette (DDPCAPS_8BIT|DDPCAPS_ALLOW256, PalEntries, &Palette, NULL);
			if (FAILED(hr))
			{
				LOG ("Could not create palette\n");
				Palette = NULL;		// NULL it just to be safe
			}
			else
			{
				hr = PrimarySurf->SetPalette (Palette);
				if (FAILED(hr))
				{
					LOG ("Could not attach palette to surface\n");
					Palette->Release ();
					Palette = NULL;
				}
				else
				{
					// The palette was supposed to have been initialized with
					// the correct colors, but some drivers don't do that.
					// (On the other hand, the docs for the SetPalette method
					// don't state that the surface will be set to the
					// palette's colors when it gets set, so this might be
					// legal behavior. Wish I knew...)
					NeedPalUpdate = true;
				}
			}
			if (PixelDoubling)
			{
				UsePfx = true;
				GPfx.SetFormat (-8, 0, 0, 0);
			}
		}
	}
	else
	{
		LOG ("Surface is direct color\n");
		UsePfx = true;
		GPfx.SetFormat (fmt.dwRGBBitCount,
			fmt.dwRBitMask, fmt.dwGBitMask, fmt.dwBBitMask);
		GPfx.SetPalette (GPalette.BaseColors);
	}
}

void DDrawFB::ReleaseResources ()
{
	if (LockCount)
	{
		LockCount = 1;
		Unlock ();
	}

	SAFE_RELEASE( Clipper );
	SAFE_RELEASE( PrimarySurf );
	SAFE_RELEASE( BackSurf );
	SAFE_RELEASE( BackSurf2 );
	SAFE_RELEASE( BlitSurf );
	SAFE_RELEASE( Palette );
	if (GDIPalette != NULL)
	{
		HDC dc = GetDC (Window);
		SelectPalette (dc, (HPALETTE)GetStockObject (DEFAULT_PALETTE), TRUE);
		DeleteObject (GDIPalette);
		ReleaseDC (Window, dc);
		GDIPalette = NULL;
	}
	LockingSurf = NULL;
}

int DDrawFB::GetPageCount ()
{
	return MustBuffer ? 1 : BufferCount+1;
}

void DDrawFB::PaletteChanged ()
{
	// Somebody else changed the palette. If we are running fullscreen,
	// they are obviously jerks, and we need to restore our own palette.
	if (!Windowed)
	{
		if (!PaletteChangeExpected && Palette != NULL)
		{
			// It is not enough to set NeedPalUpdate to true. Some palette
			// entries might now be reserved for system usage, and nothing
			// we do will change them. The only way I have found to fix this
			// is to recreate all our surfaces and the palette from scratch.

			// IMPORTANT: Do not recreate the resources here. The screen might
			// be locked for a drawing operation. Do it later the next time
			// somebody tries to lock it.
			NeedResRecreate = true;
		}
		PaletteChangeExpected = false;
	}
	else
	{
		QueryNewPalette ();
	}
}

int DDrawFB::QueryNewPalette ()
{
	LOG ("QueryNewPalette\n");
	if (GDIPalette == NULL && Windowed)
	{
		if (Write8bit)
		{
			RebuildColorTable ();
		}
		return 0;
	}

	HDC dc = GetDC (Window);
	HPALETTE oldPal = SelectPalette (dc, GDIPalette, FALSE);
	int i = RealizePalette (dc);
	SelectPalette (dc, oldPal, TRUE);
	RealizePalette (dc);
	ReleaseDC (Window, dc);
	if (i != 0)
	{
		RebuildColorTable ();
	}
	return i;
}

void DDrawFB::RebuildColorTable ()
{
	int i;

	if (Write8bit)
	{
		PALETTEENTRY syspal[256];
		HDC dc = GetDC (Window);

		GetSystemPaletteEntries (dc, 0, 256, syspal);

		for (i = 0; i < 256; i++)
		{
			swapvalues (syspal[i].peRed, syspal[i].peBlue);
		}
		for (i = 0; i < 256; i++)
		{
			GPfxPal.Pal8[i] = (BYTE)BestColor ((uint32 *)syspal, PalEntries[i].peRed,
				PalEntries[i].peGreen, PalEntries[i].peBlue);
		}
	}
}

bool DDrawFB::Is8BitMode()
{
	if (Windowed)
	{
		return Write8bit;
	}
	DDPIXELFORMAT fmt = { sizeof(fmt), };
	HRESULT hr;

	hr = PrimarySurf->GetPixelFormat(&fmt);
	if (SUCCEEDED(hr))
	{
		return !!(fmt.dwFlags & DDPF_PALETTEINDEXED8);
	}
	// Can't get the primary surface's pixel format, so assume
	// vid_displaybits is accurate.
	return vid_displaybits == 8;
}

bool DDrawFB::IsValid ()
{
	return PrimarySurf != NULL;
}

HRESULT DDrawFB::GetHR ()
{
	return LastHR;
}

bool DDrawFB::Lock (bool useSimpleCanvas)
{
	static int lock_num;
	bool wasLost;

//	LOG2 ("  Lock (%d) <%d>\n", buffered, LockCount);

	LOG3("Lock %5x <%d> %d\n", (AppActive << 16) | (SessionState << 12) | (MustBuffer << 8) |
		(useSimpleCanvas << 4) | (int)UseBlitter, LockCount, lock_num++);

	if (LockCount++ > 0)
	{
		return false;
	}

	wasLost = false;

	if (NeedResRecreate && LockCount == 1)
	{
		LOG("Recreating resources\n");
		NeedResRecreate = false;
		ReleaseResources ();
		CreateResources ();
		// ReleaseResources sets LockCount to 0.
		LockCount = 1;
	}

	if (!AppActive || SessionState || MustBuffer || useSimpleCanvas || !UseBlitter)
	{
		Buffer = MemBuffer;
		Pitch = BufferPitch;
		BufferingNow = true;
	}
	else
	{
		HRESULT hr GCCNOWARN = BlitSurf->Flip (NULL, DDFLIP_WAIT);
		hr;
		LOG1 ("Blit flip = %08lx\n", hr);
		LockSurfRes res = LockSurf (NULL, BlitSurf);
		
		if (res == NoGood)
		{ // We must have a surface locked before returning,
		  // but we could not lock the hardware surface, so buffer
		  // for this frame.
			Buffer = MemBuffer;
			Pitch = BufferPitch;
			BufferingNow = true;
		}
		else
		{
			wasLost = (res == GoodWasLost);
		}
	}

	wasLost = wasLost || (BufferingNow != WasBuffering);
	WasBuffering = BufferingNow;
	return wasLost;
}

void DDrawFB::Unlock ()
{
	LOG1 ("Unlock     <%d>\n", LockCount);

	if (LockCount == 0)
	{
		LOG("Unlock called when already unlocked\n");
		return;
	}

	if (UpdatePending && LockCount == 1)
	{
		Update ();
	}
	else if (--LockCount == 0)
	{
		if (!BufferingNow)
		{
			if (BlitSurf == NULL)
			{
				LockingSurf->Unlock (NULL);
			}
			else
			{
				BlitSurf->Unlock (NULL);
			}
		}
		Buffer = NULL;
	}
}

DDrawFB::LockSurfRes DDrawFB::LockSurf (LPRECT lockrect, LPDIRECTDRAWSURFACE toLock)
{
	HRESULT hr;
	DDSURFACEDESC desc = { sizeof(desc), };
	bool wasLost = false;
	bool lockingLocker = false;

	if (toLock == NULL)
	{
		lockingLocker = true;
		if (LockingSurf == NULL)
		{
			LOG("LockingSurf lost\n");
			if (!CreateResources ())
			{
				if (LastHR != DDERR_UNSUPPORTEDMODE)
				{
					I_FatalError ("Could not rebuild framebuffer: %08lx", LastHR);
				}
				else
				{
					LOG ("Display is in unsupported mode right now.\n");
					return NoGood;
				}
			}
		}
		toLock = LockingSurf;
	}

	hr = toLock->Lock (lockrect, &desc, DDLOCK_SURFACEMEMORYPTR | DDLOCK_WAIT | DDLOCK_WRITEONLY, NULL);
	LOG3 ("LockSurf %p (%d): %08lx\n", toLock, lockingLocker, hr);

	if (hr == DDERR_SURFACELOST)
	{
		wasLost = true;
		if (FAILED (AttemptRestore ()))
		{
			return NoGood;
		}
		if (BlitSurf && FAILED(BlitSurf->IsLost ()))
		{
			LOG ("Restore blitter surface\n");
			hr = BlitSurf->Restore ();
			if (FAILED (hr))
			{
				LOG1 ("Could not restore blitter surface: %08lx", hr);
				BlitSurf->Release ();
				if (BlitSurf == toLock)
				{
					BlitSurf = NULL;
					return NoGood;
				}
				BlitSurf = NULL;
			}
		}
		if (lockingLocker)
		{
			toLock = LockingSurf;
		}
		LOG ("Trying to lock again\n");
		hr = toLock->Lock (lockrect, &desc, DDLOCK_SURFACEMEMORYPTR | DDLOCK_WAIT | DDLOCK_WRITEONLY, NULL);
		if (hr == DDERR_SURFACELOST && Windowed)
		{ // If this is NT, the user probably opened the Windows NT Security dialog.
		  // If this is not NT, trying to recreate everything from scratch won't hurt.
			ReleaseResources ();
			if (!CreateResources ())
			{
				if (LastHR != DDERR_UNSUPPORTEDMODE)
				{
					I_FatalError ("Could not rebuild framebuffer: %08lx", LastHR);
				}
				else
				{
					LOG ("Display is in unsupported mode right now.\n");
					return NoGood;
				}
			}
			if (lockingLocker)
			{
				toLock = LockingSurf;
			}
			hr = toLock->Lock (lockrect, &desc, DDLOCK_SURFACEMEMORYPTR | DDLOCK_WAIT | DDLOCK_WRITEONLY, NULL);
		}
	}
	if (FAILED (hr))
	{ // Still could not restore the surface, so don't draw anything
		//I_FatalError ("Could not lock framebuffer: %08lx", hr);
		LOG1 ("Final result after restoration attempts: %08lx\n", hr);
		return NoGood;
	}
	Buffer = (BYTE *)desc.lpSurface;
	Pitch = desc.lPitch;
	BufferingNow = false;
	return wasLost ? GoodWasLost : Good;
}

HRESULT DDrawFB::AttemptRestore ()
{
	LOG ("Restore primary\n");
	HRESULT hr = PrimarySurf->Restore ();
	if (hr == DDERR_WRONGMODE && Windowed)
	{ // The user changed the screen mode
		LOG ("DDERR_WRONGMODE and windowed, so recreating all resources\n");
		ReleaseResources ();
		if (!CreateResources ())
		{
			LOG1 ("Could not recreate framebuffer: %08lx", LastHR);
			return LastHR;
		}
	}
	else if (FAILED (hr))
	{
		LOG1 ("Could not restore primary surface: %08lx", hr);
		return hr;
	}
	if (BackSurf && FAILED(BackSurf->IsLost ()))
	{
		LOG ("Restore backbuffer\n");
		hr = BackSurf->Restore ();
		if (FAILED (hr))
		{
			I_FatalError ("Could not restore backbuffer: %08lx", hr);
		}
	}
	if (BackSurf2 && FAILED(BackSurf2->IsLost ()))
	{
		LOG ("Restore backbuffer 2\n");
		hr = BackSurf2->Restore ();
		if (FAILED (hr))
		{
			I_FatalError ("Could not restore backbuffer 2: %08lx", hr);
		}
	}
	return 0;
}

void DDrawFB::Update ()
{
	bool pchanged = false;
	int i;

	LOG3 ("Update     <%d,%c:%d>\n", LockCount, AppActive?'Y':'N', SessionState);

	if (LockCount != 1)
	{
		//I_FatalError ("Framebuffer must have exactly 1 lock to be updated");
		if (LockCount > 0)
		{
			UpdatePending = true;
			--LockCount;
		}
		return;
	}

	DrawRateStuff ();

	if (NeedGammaUpdate)
	{
		NeedGammaUpdate = false;
		CalcGamma (Windowed || rgamma == 0.f ? Gamma : Gamma * rgamma, GammaTable[0]);
		CalcGamma (Windowed || ggamma == 0.f ? Gamma : Gamma * ggamma, GammaTable[1]);
		CalcGamma (Windowed || bgamma == 0.f ? Gamma : Gamma * bgamma, GammaTable[2]);
		NeedPalUpdate = true;
	}
	
	if (NeedPalUpdate || vid_palettehack)
	{
		NeedPalUpdate = false;
		if (Palette != NULL || GDIPalette != NULL)
		{
			for (i = 0; i < 256; i++)
			{
				PalEntries[i].peRed = GammaTable[0][SourcePalette[i].r];
				PalEntries[i].peGreen = GammaTable[1][SourcePalette[i].g];
				PalEntries[i].peBlue = GammaTable[2][SourcePalette[i].b];
			}
			if (FlashAmount)
			{
				DoBlending ((PalEntry *)PalEntries, (PalEntry *)PalEntries,
					256, GammaTable[2][Flash.b], GammaTable[1][Flash.g], GammaTable[0][Flash.r],
					FlashAmount);
			}
			if (Palette != NULL)
			{
				pchanged = true;
			}
			else
			{
				/* Argh! Too slow!
				SetPaletteEntries (GDIPalette, 0, 256, PalEntries);
				HDC dc = GetDC (Window);
				SelectPalette (dc, GDIPalette, FALSE);
				RealizePalette (dc);
				ReleaseDC (Window, dc);
				*/
				RebuildColorTable ();
			}
		}
		else
		{
			for (i = 0; i < 256; i++)
			{
				((PalEntry *)PalEntries)[i].r = GammaTable[0][SourcePalette[i].r];
				((PalEntry *)PalEntries)[i].g = GammaTable[1][SourcePalette[i].g];
				((PalEntry *)PalEntries)[i].b = GammaTable[2][SourcePalette[i].b];
			}
			if (FlashAmount)
			{
				DoBlending ((PalEntry *)PalEntries, (PalEntry *)PalEntries,
					256, GammaTable[0][Flash.r], GammaTable[1][Flash.g], GammaTable[2][Flash.b],
					FlashAmount);
			}
			GPfx.SetPalette ((PalEntry *)PalEntries);
		}
	}

	BlitCycles.Reset();
	BlitCycles.Clock();

	if (BufferingNow)
	{
		LockCount = 0;
		if ((Windowed || AppActive) && !SessionState && !PaintToWindow())
		{
			if (LockSurf (NULL, NULL) != NoGood)
			{
				BYTE *writept = Buffer + (TrueHeight - Height)/2*Pitch;
				LOG3 ("Copy %dx%d (%d)\n", Width, Height, BufferPitch);
				if (UsePfx)
				{
					GPfx.Convert (MemBuffer, BufferPitch,
						writept, Pitch, Width << PixelDoubling, Height << PixelDoubling,
						FRACUNIT >> PixelDoubling, FRACUNIT >> PixelDoubling, 0, 0);
				}
				else
				{
					CopyFromBuff (MemBuffer, BufferPitch, Width, Height, writept);
				}
				if (TrueHeight != Height)
				{
					// Letterbox time! Draw black top and bottom borders.
					int topborder = (TrueHeight - Height) / 2;
					int botborder = TrueHeight - topborder - Height;
					memset (Buffer, 0, Pitch*topborder);
					memset (writept + Height*Pitch, 0, Pitch*botborder);
				}
				LockingSurf->Unlock (NULL);
			}
		}
	}
	else
	{
		if (BlitSurf != NULL)
		{
			HRESULT hr;
			BlitSurf->Unlock (NULL);
			RECT srcRect = { 0, 0, Width, Height };
			hr = LockingSurf->BltFast (0, 0, BlitSurf, &srcRect, DDBLTFAST_NOCOLORKEY|DDBLTFAST_WAIT);
			if (FAILED (hr))
			{
				LOG1 ("Could not blit: %08lx\n", hr);
				if (hr == DDERR_SURFACELOST)
				{
					if (SUCCEEDED (AttemptRestore ()))
					{
						hr = LockingSurf->BltFast (0, 0, BlitSurf, &srcRect, DDBLTFAST_NOCOLORKEY|DDBLTFAST_WAIT);
						if (FAILED (hr))
						{
							LOG1 ("Blit retry also failed: %08lx\n", hr);
						}
					}
				}
			}
			else
			{
				LOG ("Blit ok\n");
			}
		}
		else
		{
			LockingSurf->Unlock (NULL);
		}
	}

	BlitCycles.Unclock();
	LOG1 ("cycles = %.1f ms\n", BlitCycles.TimeMS());

	Buffer = NULL;
	LockCount = 0;
	UpdatePending = false;

	if (FPSLimitEvent != NULL)
	{
		WaitForSingleObject(FPSLimitEvent, 1000);
	}
	if (!Windowed && AppActive && !SessionState /*&& !UseBlitter && !MustBuffer*/)
	{
		HRESULT hr = PrimarySurf->Flip (NULL, FlipFlags);
		LOG1 ("Flip = %08lx\n", hr);
		if (hr == DDERR_INVALIDPARAMS)
		{
			if (FlipFlags & DDFLIP_NOVSYNC)
			{
				FlipFlags &= ~DDFLIP_NOVSYNC;
				Printf ("Can't disable vsync\n");
				PrimarySurf->Flip (NULL, FlipFlags);
			}
		}
	}

	if (pchanged && AppActive && !SessionState)
	{
		PaletteChangeExpected = true;
		Palette->SetEntries (0, 0, 256, PalEntries);
	}
}

bool DDrawFB::PaintToWindow ()
{
	if (Windowed && LockCount == 0)
	{
		HRESULT hr;
		RECT rect;
		GetClientRect (Window, &rect);
		if (rect.right != 0 && rect.bottom != 0)
		{
			// Use blit to copy/stretch to window's client rect
			ClientToScreen (Window, (POINT*)&rect.left);
			ClientToScreen (Window, (POINT*)&rect.right);
			LOG ("Paint to window\n");
			if (LockSurf (NULL, NULL) != NoGood)
			{
				GPfx.Convert (MemBuffer, BufferPitch,
					Buffer, Pitch, Width << PixelDoubling, Height << PixelDoubling,
					FRACUNIT >> PixelDoubling, FRACUNIT >> PixelDoubling, 0, 0);
				LockingSurf->Unlock (NULL);
				if (FAILED (hr = PrimarySurf->Blt (&rect, BackSurf, NULL, DDBLT_WAIT|DDBLT_ASYNC, NULL)))
				{
					if (hr == DDERR_SURFACELOST)
					{
						PrimarySurf->Restore ();
					}
					PrimarySurf->Blt (&rect, BackSurf, NULL, DDBLT_WAIT, NULL);
				}
			}
			Buffer = NULL;
			LOG ("Did paint to window\n");
		}
		return true;
	}
	return false;
}

PalEntry *DDrawFB::GetPalette ()
{
	return SourcePalette;
}

void DDrawFB::UpdatePalette ()
{
	NeedPalUpdate = true;
}

bool DDrawFB::SetGamma (float gamma)
{
	LOG1 ("SetGamma %g\n", gamma);
	Gamma = gamma;
	NeedGammaUpdate = true;
	return true;
}

bool DDrawFB::SetFlash (PalEntry rgb, int amount)
{
	Flash = rgb;
	FlashAmount = amount;
	NeedPalUpdate = true;
	return true;
}

void DDrawFB::GetFlash (PalEntry &rgb, int &amount)
{
	rgb = Flash;
	amount = FlashAmount;
}

// Q: Should I gamma adjust the returned palette?
// A: No. PNG screenshots save the gamma value, so there is no need.
void DDrawFB::GetFlashedPalette (PalEntry pal[256])
{
	memcpy (pal, SourcePalette, 256*sizeof(PalEntry));
	if (FlashAmount)
	{
		DoBlending (pal, pal, 256, Flash.r, Flash.g, Flash.b, FlashAmount);
	}
}

void DDrawFB::SetVSync (bool vsync)
{
	LOG1 ("vid_vsync set to %d\n", vsync);
	FlipFlags = vsync ? DDFLIP_WAIT : DDFLIP_WAIT|DDFLIP_NOVSYNC;
}

void DDrawFB::NewRefreshRate()
{
	if (!Windowed)
	{
		NeedResRecreate = true;
	}
}

void DDrawFB::Blank ()
{
	if (IsFullscreen ())
	{
		DDBLTFX blitFX = { sizeof(blitFX) };

		blitFX.dwFillColor = 0;
		DDraw->FlipToGDISurface ();
		PrimarySurf->Blt (NULL, NULL, NULL, DDBLT_COLORFILL, &blitFX);
	}
}

ADD_STAT (blit)
{
	FString out;
	out.Format ("blit=%04.1f ms", BlitCycles.TimeMS());
	return out;
}
