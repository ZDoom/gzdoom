/*
** win32video.cpp
** Code to let ZDoom use DirectDraw
**
**---------------------------------------------------------------------------
** Copyright 1998-2006 Randy Heit
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

#define __BYTEBOOL__
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

// MACROS ------------------------------------------------------------------

#define true TRUE
#define false FALSE

#if 0
#define STARTLOG		do { if (!dbg) dbg = fopen ("k:/vid.log", "w"); } while(0)
#define STOPLOG			do { if (dbg) { fclose (dbg); dbg=NULL; } } while(0)
#define LOG(x)			do { if (dbg) { fprintf (dbg, x); fflush (dbg); } } while(0)
#define LOG1(x,y)		do { if (dbg) { fprintf (dbg, x, y); fflush (dbg); } } while(0)
#define LOG2(x,y,z)		do { if (dbg) { fprintf (dbg, x, y, z); fflush (dbg); } } while(0)
#define LOG3(x,y,z,zz)	do { if (dbg) { fprintf (dbg, x, y, z, zz); fflush (dbg); } } while(0)
#define LOG4(x,y,z,a,b)	do { if (dbg) { fprintf (dbg, x, y, z, a, b); fflush (dbg); } } while(0)
#define LOG5(x,y,z,a,b,c) do { if (dbg) { fprintf (dbg, x, y, z, a, b, c); fflush (dbg); } } while(0)
FILE *dbg;
#else
#define STARTLOG
#define STOPLOG
#define LOG(x)
#define LOG1(x,y)
#define LOG2(x,y,z)
#define LOG3(x,y,z,zz)
#define LOG4(x,y,z,a,b)
#define LOG5(x,y,z,a,b,c)
#endif

// TYPES -------------------------------------------------------------------

class CVidError : public CRecoverableError
{
	CVidError() : CRecoverableError() {}
	CVidError(const char *message) : CRecoverableError(message) {}
};

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void DoBlending (const PalEntry *from, PalEntry *to, int count, int r, int g, int b, int a);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern HWND Window;
extern IVideo *Video;
extern BOOL AppActive;
extern int SessionState;
extern bool FullscreenReset;
extern bool VidResizing;

EXTERN_CVAR (Bool, fullscreen)
EXTERN_CVAR (Float, Gamma)

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static IDirectDraw2 *DDraw;
static cycle_t BlitCycles;
static DWORD FlipFlags;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

CVAR (Bool, vid_palettehack, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Bool, vid_attachedsurfaces, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Bool, vid_noblitter, true, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Int, vid_displaybits, 8, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CUSTOM_CVAR (Bool, vid_vsync, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	LOG1 ("vid_vsync set to %d\n", *self);
	FlipFlags = self ? DDFLIP_WAIT : DDFLIP_WAIT|DDFLIP_NOVSYNC;
}
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

// CODE --------------------------------------------------------------------

Win32Video::Win32Video (int parm)
: m_Modes (NULL),
  m_IsFullscreen (false)
{
	LPDIRECTDRAW ddraw1;
	STARTLOG;

	HRESULT dderr;
	ModeInfo *mode, *nextmode;

	dderr = DirectDrawCreate (NULL, &ddraw1, NULL);

	if (FAILED(dderr))
		I_FatalError ("Could not create DirectDraw object: %08lx", dderr);

	dderr = ddraw1->QueryInterface (IID_IDirectDraw2, (LPVOID*)&DDraw);
	if (FAILED(dderr))
	{
		ddraw1->Release ();
		DDraw = NULL;
		I_FatalError ("Could not initialize IDirectDraw2 interface: %08lx", dderr);
	}

	// Okay, we have the IDirectDraw2 interface now, so we can release the
	// really old-fashioned IDirectDraw one.
	ddraw1->Release ();

	DDraw->SetCooperativeLevel (Window, DDSCL_NORMAL);
	FreeModes ();
	dderr = DDraw->EnumDisplayModes (0, NULL, this, EnumDDModesCB);
	if (FAILED(dderr))
	{
		DDraw->Release ();
		DDraw = NULL;
		I_FatalError ("Could not enumerate display modes: %08lx", dderr);
	}
	if (m_Modes == NULL)
	{
		DDraw->Release ();
		DDraw = NULL;
		I_FatalError ("DirectDraw returned no display modes.\n\n"
					"If you started ZDoom from a fullscreen DOS box, run it from "
					"a DOS window instead. If that does not work, you may need to reboot.");
	}
	if (OSPlatform == os_Win95)
	{
		// Windows 95 will let us use Mode X. If we didn't find any linear
		// modes in the loop above, add the Mode X modes here.
		AddMode (320, 200, 8, 200);
		AddMode (320, 240, 8, 240);
	}
	// Now add 16:9 and 16:10 resolutions you can use in a window or letterboxed
	for (mode = m_Modes; mode != NULL; mode = nextmode)
	{
		nextmode = mode->next;
		if (mode->realheight == mode->height && mode->height * 4/3 == mode->width)
		{
			if (mode->width >= 360)
			{
				AddMode (mode->width, mode->width * 9/16, mode->bits, mode->height);
			}
			if (mode->width > 640)
			{
				AddMode (mode->width, mode->width * 10/16, mode->bits, mode->height);
			}
		}
	}
}

Win32Video::~Win32Video ()
{
	FreeModes ();

	if (DDraw != NULL)
	{
		if (m_IsFullscreen)
		{
			DDraw->SetCooperativeLevel (NULL, DDSCL_NORMAL);
		}
		DDraw->Release();
		DDraw = NULL;
	}

	ShowWindow (Window, SW_HIDE);

	STOPLOG;
}

// Returns true if fullscreen, false otherwise
bool Win32Video::GoFullscreen (bool yes)
{
	static const char *const yestypes[2] = { "windowed", "fullscreen" };
	HRESULT hr[2];
	int count;

	if (m_IsFullscreen == yes)
		return yes;

	for (count = 0; count < 2; ++count)
	{
		LOG1 ("fullscreen: %d\n", yes);
		hr[count] = DDraw->SetCooperativeLevel (Window, !yes ? DDSCL_NORMAL :
			DDSCL_ALLOWMODEX | DDSCL_ALLOWREBOOT | DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN);
		if (SUCCEEDED(hr[count]))
		{
			if (count != 0)
			{
// Ack! Cannot print because the screen does not exist right now.
//				Printf ("Setting %s mode failed. Error %08lx\n",
//					yestypes[!yes], hr[0]);
			}
			m_IsFullscreen = yes;
			return yes;
		}
		yes = !yes;
	}

	I_FatalError ("Could not set %s mode: %08lx\n"
				  "Could not set %s mode: %08lx\n",
				  yestypes[yes], hr[0], yestypes[!yes], hr[1]);

	// Appease the compiler, even though we never return if we get here.
	return false;
}

// Flips to the GDI surface and clears it; used by the movie player
void Win32Video::BlankForGDI ()
{
	static_cast<DDrawFB *> (screen)->Blank ();
}

// Mode enumeration --------------------------------------------------------

HRESULT WINAPI Win32Video::EnumDDModesCB (LPDDSURFACEDESC desc, void *data)
{
	if (desc->ddpfPixelFormat.dwRGBBitCount == 8 &&
		(desc->dwWidth & 7) == 0 &&
		desc->dwHeight <= MAXHEIGHT &&
		desc->dwWidth <= MAXWIDTH &&
		desc->dwHeight >= 200 &&
		desc->dwWidth >= 320)
	{
		((Win32Video *)data)->AddMode (desc->dwWidth, desc->dwHeight, 8, desc->dwHeight);
	}

	return DDENUMRET_OK;
}

void Win32Video::AddMode (int x, int y, int bits, int y2)
{
	ModeInfo **probep = &m_Modes;
	ModeInfo *probe = m_Modes;

	// This mode may have been already added to the list because it is
	// enumerated multiple times at different refresh rates. If it's
	// not present, add it to the right spot in the list; otherwise, do nothing.
	// Modes are sorted first by width, then by height, then by depth. In each
	// case the order is ascending.
	for (; probe != 0; probep = &probe->next, probe = probe->next)
	{
		if (probe->width > x)		break;
		if (probe->width < x)		continue;
		// Width is equal
		if (probe->height > y)		break;
		if (probe->height < y)		continue;
		// Height is equal
		if (probe->bits > bits)		break;
		if (probe->bits < bits)		continue;
		// Bits is equal
		return;
	}

	*probep = new ModeInfo (x, y, bits, y2);
	(*probep)->next = probe;
}

void Win32Video::FreeModes ()
{
	ModeInfo *mode = m_Modes;

	while (mode)
	{
		ModeInfo *tempmode = mode;
		mode = mode->next;
		delete tempmode;
	}
	m_Modes = NULL;
}

// This only changes how the iterator lists modes
bool Win32Video::FullscreenChanged (bool fs)
{
	LOG1 ("FS-changed: %d\n", fs);
	m_IteratorFS = fs;
	return true;
}

void Win32Video::StartModeIterator (int bits)
{
	m_IteratorMode = m_Modes;
	m_IteratorBits = bits;
}

bool Win32Video::NextMode (int *width, int *height, bool *letterbox)
{
	if (m_IteratorMode)
	{
		while (m_IteratorMode && m_IteratorMode->bits != m_IteratorBits)
		{
			m_IteratorMode = m_IteratorMode->next;
		}

		if (m_IteratorMode)
		{
			*width = m_IteratorMode->width;
			*height = m_IteratorMode->height;
			if (letterbox != NULL) *letterbox = m_IteratorMode->realheight != m_IteratorMode->height;
			m_IteratorMode = m_IteratorMode->next;
			return true;
		}
	}
	return false;
}

DFrameBuffer *Win32Video::CreateFrameBuffer (int width, int height, bool fullscreen, DFrameBuffer *old)
{
	static int retry = 0;
	static int owidth, oheight;

	PalEntry flashColor;
	int flashAmount;

	LOG4 ("CreateFB %d %d %d %p\n", width, height, fullscreen, old);

	if (old != NULL)
	{ // Reuse the old framebuffer if its attributes are the same
		BaseWinFB *fb = static_cast<BaseWinFB *> (old);
		if (fb->Width == width &&
			fb->Height == height &&
			fb->Windowed == !fullscreen)
		{
			return old;
		}
		old->GetFlash (flashColor, flashAmount);
		delete old;
	}
	else
	{
		flashColor = 0;
		flashAmount = 0;
	}

	DDrawFB *fb = new DDrawFB (width, height, fullscreen);
	LOG1 ("New fb created @ %p\n", fb);

	// If we could not create the framebuffer, try again with slightly
	// different parameters in this order:
	// 1. Try with the closest size
	// 2. Try in the opposite screen mode with the original size
	// 3. Try in the opposite screen mode with the closest size
	// This is a somewhat confusing mass of recursion here.

	while (fb == NULL || !fb->IsValid ())
	{
		static HRESULT hr;

		if (fb != NULL)
		{
			if (retry == 0)
			{
				hr = fb->GetHR ();
			}
			delete fb;

			LOG1 ("fb is bad: %08lx\n", hr);
		}
		else
		{
			LOG ("Could not create fb at all\n");
		}
		screen = NULL;

		LOG1 ("Retry number %d\n", retry);

		switch (retry)
		{
		case 0:
			owidth = width;
			oheight = height;
		case 2:
			// Try a different resolution. Hopefully that will work.
			I_ClosestResolution (&width, &height, 8);
			LOG2 ("Retry with size %d,%d\n", width, height);
			break;

		case 1:
			// Try changing fullscreen mode. Maybe that will work.
			width = owidth;
			height = oheight;
			fullscreen = !fullscreen;
			LOG1 ("Retry with fullscreen %d\n", fullscreen);
			break;

		default:
			// I give up!
			LOG3 ("Could not create new screen (%d x %d): %08lx", owidth, oheight, hr);
			I_FatalError ("Could not create new screen (%d x %d): %08lx", owidth, oheight, hr);
		}

		++retry;
		fb = static_cast<DDrawFB *>(CreateFrameBuffer (width, height, fullscreen, NULL));
	}
	retry = 0;

	if (fb->IsFullscreen() != fullscreen)
	{
		Video->FullscreenChanged (!fullscreen);
	}

	fb->SetFlash (flashColor, flashAmount);

	return fb;
}

void Win32Video::SetWindowedScale (float scale)
{
	// FIXME
}

// FrameBuffer implementation -----------------------------------------------

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
	ClipRegion = NULL;
	ClipSize = 0;
	BufferCount = 1;
	Gamma = 1.0;
	BufferPitch = Pitch;
	FlipFlags = vid_vsync ? DDFLIP_WAIT : DDFLIP_WAIT|DDFLIP_NOVSYNC;

	NeedGammaUpdate = false;
	NeedPalUpdate = false;
	NeedResRecreate = false;
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
		GammaTable[0][i] = GammaTable[1][i] = GammaTable[2][i] = i;
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
		if (PrimarySurf != NULL)
		{
			PrimarySurf->Release ();
			PrimarySurf = NULL;
		}
	}
}

DDrawFB::~DDrawFB ()
{
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
		ShowWindow (Window, SW_SHOW);
		// Remove the window border in fullscreen mode
		SetWindowLongPtr (Window, GWL_STYLE, WS_VISIBLE|WS_SYSMENU);

		TrueHeight = Height;
		for (Win32Video::ModeInfo *mode = static_cast<Win32Video *>(Video)->m_Modes; mode != NULL; mode = mode->next)
		{
			if (mode->width == Width && mode->height == Height)
			{
				TrueHeight = mode->realheight;
				break;
			}
		}
		hr = DDraw->SetDisplayMode (Width, TrueHeight, bits = vid_displaybits, 0, 0);
		if (FAILED(hr))
		{
			bits = 32;
			while (FAILED(hr) && bits >= 8)
			{
				hr = DDraw->SetDisplayMode (Width, Height, bits, 0, 0);
				bits -= 8;
			}
			if (FAILED(hr))
			{
				LastHR = hr;
				return false;
			}
		}
		LOG3 ("Mode set to %d x %d x %d\n", Width, Height, bits);

		if (vid_attachedsurfaces && OSPlatform == os_WinNT)
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
		int sizew = Width + GetSystemMetrics (SM_CXSIZEFRAME)*2;
		int sizeh = Height + GetSystemMetrics (SM_CYSIZEFRAME) * 2 +
					 GetSystemMetrics (SM_CYCAPTION);
		LOG2 ("Resize window to %dx%d\n", sizew, sizeh);
		VidResizing = true;
		// Make sure the window has a border in windowed mode
		SetWindowLongPtr (Window, GWL_STYLE, WS_VISIBLE|WS_OVERLAPPEDWINDOW);
		if (!SetWindowPos (Window, NULL, 0, 0, sizew, sizeh,
			SWP_DRAWFRAME | SWP_NOCOPYBITS | SWP_NOMOVE | SWP_NOZORDER))
		{
			LOG1 ("SetWindowPos failed because %08lx\n", GetLastError());
		}
		VidResizing = false;
		ShowWindow (Window, SW_SHOWNORMAL);

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
		ddsd.dwWidth        = Width;
		ddsd.dwHeight       = Height;
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
			GDIPalette = CreatePalette (&pal.head);
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

	if (ClipRegion != NULL)
	{
		delete[] ClipRegion;
		ClipRegion = NULL;
	}
	if (Clipper != NULL)
	{
		Clipper->Release ();
		Clipper = NULL;
	}
	if (PrimarySurf != NULL)
	{
		//Blank ();
		PrimarySurf->Release ();
		PrimarySurf = NULL;
	}
	if (BackSurf != NULL)
	{
		BackSurf->Release ();
		BackSurf = NULL;
	}
	if (BackSurf2 != NULL)
	{
		BackSurf2->Release ();
		BackSurf2 = NULL;
	}
	if (BlitSurf != NULL)
	{
		BlitSurf->Release ();
		BlitSurf = NULL;
	}
	if (Palette != NULL)
	{
		Palette->Release ();
		Palette = NULL;
	}
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
		if (Palette != NULL)
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
			swap (syspal[i].peRed, syspal[i].peBlue);
		}
		for (i = 0; i < 256; i++)
		{
			GPfxPal.Pal8[i] = BestColor ((DWORD *)syspal, PalEntries[i].peRed,
				PalEntries[i].peGreen, PalEntries[i].peBlue);
		}
	}
}

bool DDrawFB::IsValid ()
{
	return PrimarySurf != NULL;
}

bool DDrawFB::Lock ()
{
	return Lock (false);
}

bool DDrawFB::Lock (bool useSimpleCanvas)
{
	bool wasLost;

//	LOG2 ("  Lock (%d) <%d>\n", buffered, LockCount);

	if (LockCount++ > 0)
	{
		return false;
	}

	wasLost = false;

	if (NeedResRecreate)
	{
		NeedResRecreate = false;
		ReleaseResources ();
		CreateResources ();
	}

	LOG5 ("Lock %d %d %d %d %d\n", AppActive, SessionState, MustBuffer, useSimpleCanvas, UseBlitter);

	if (!AppActive || SessionState || MustBuffer || useSimpleCanvas || !UseBlitter)
	{
		Buffer = MemBuffer;
		Pitch = BufferPitch;
		BufferingNow = true;
	}
	else
	{
		HRESULT hr GCCNOWARN = BlitSurf->Flip (NULL, DDFLIP_WAIT);
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

	BlitCycles = 0;
	clock (BlitCycles);

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
						writept, Pitch, Width, Height,
						FRACUNIT, FRACUNIT, 0, 0);
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

	unclock (BlitCycles);
	LOG1 ("cycles = %d\n", BlitCycles);

	Buffer = NULL;
	LockCount = 0;
	UpdatePending = false;

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
					Buffer, Pitch, Width, Height,
					FRACUNIT, FRACUNIT, 0, 0);
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

void DDrawFB::DoSpeedTest ()
{
	static const int testarea = 2048*1536*60;
	LARGE_INTEGER start, end, diff, speed;
	unsigned long vspeed, mspeed;
	int area, realtestarea, repetitions;

	if (LockSurf (NULL, NULL) == NoGood)
	{
		Printf ("Failed to lock video memory\n");
	}

	area = Width * Height;
	repetitions = testarea / area;
	realtestarea = repetitions * area;

	QueryPerformanceCounter (&start);
	do
	{
		memset (Buffer, repetitions, area);
	} while (--repetitions);
	QueryPerformanceCounter (&end);
	LockingSurf->Unlock (NULL);

	QueryPerformanceFrequency (&speed);
	diff.QuadPart = end.QuadPart - start.QuadPart;
	vspeed = (unsigned long)(realtestarea * speed.QuadPart / diff.QuadPart);

	repetitions = testarea / area;
	QueryPerformanceCounter (&start);
	do
	{
		memset (MemBuffer, repetitions, area);
	} while (--repetitions);
	QueryPerformanceCounter (&end);

	diff.QuadPart = end.QuadPart - start.QuadPart;
	mspeed = (unsigned long)(realtestarea * speed.QuadPart / diff.QuadPart);

	Printf ("Write to vid mem:%9luk/sec\n", vspeed / 1024);
	Printf ("Write to sys mem:%9luk/sec\n", mspeed / 1024);
}

CCMD (vid_speedtest)
{
	if (screen != NULL)
	{
		static_cast<DDrawFB *>(screen)->DoSpeedTest ();
	}
}

ADD_STAT (blit, out)
{
	sprintf (out,
		"blit=%04.1f ms",
		(double)BlitCycles * SecondsPerCycle * 1000
		);
}
