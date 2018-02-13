/*
** win32video.cpp
** Code to let ZDoom draw to the screen
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

#ifdef _DEBUG
#define D3D_DEBUG_INFO
#endif
#define DIRECTDRAW_VERSION 0x0300
#define DIRECT3D_VERSION 0x0900

#define _WIN32_WINNT 0x0501
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <ddraw.h>
#include <d3d9.h>

// HEADER FILES ------------------------------------------------------------

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <ddraw.h>
#include <d3d9.h>
#include <stdio.h>
#include <ctype.h>

#include "doomtype.h"

#include "c_dispatch.h"
#include "templates.h"
#include "i_system.h"
#include "i_video.h"
#include "v_video.h"
#include "v_pfx.h"
#include "stats.h"
#include "doomerrors.h"
#include "m_argv.h"
#include "r_defs.h"
#include "v_text.h"
#include "swrenderer/r_swrenderer.h"
#include "version.h"

#include "win32iface.h"
#include "win32swiface.h"

#include "optwin32.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

typedef IDirect3D9 *(WINAPI *DIRECT3DCREATE9FUNC)(UINT SDKVersion);
typedef HRESULT (WINAPI *DIRECTDRAWCREATEFUNC)(GUID FAR *lpGUID, LPDIRECTDRAW FAR *lplpDD, IUnknown FAR *pUnkOuter);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void DoBlending (const PalEntry *from, PalEntry *to, int count, int r, int g, int b, int a);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void StopFPSLimit();

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern HWND Window;
extern IVideo *Video;
extern BOOL AppActive;
extern int SessionState;
extern bool FullscreenReset;
extern bool VidResizing;

EXTERN_CVAR (Bool, fullscreen)
EXTERN_CVAR (Float, Gamma)
EXTERN_CVAR (Bool, cl_capfps)

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static HMODULE D3D9_dll;
static HMODULE DDraw_dll;
static UINT FPSLimitTimer;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

IDirectDraw2 *DDraw;
IDirect3D9 *D3D;
IDirect3DDevice9 *D3Device;
HANDLE FPSLimitEvent;

CVAR (Bool, vid_forceddraw, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR (Int, vid_adapter, 1, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CUSTOM_CVAR (Int, vid_maxfps, 200, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (vid_maxfps < TICRATE && vid_maxfps != 0)
	{
		vid_maxfps = TICRATE;
	}
	else if (vid_maxfps > 1000)
	{
		vid_maxfps = 1000;
	}
	else if (cl_capfps == 0)
	{
		I_SetFPSLimit(vid_maxfps);
	}
}

#if VID_FILE_DEBUG
FILE *dbg;
#endif

// CODE --------------------------------------------------------------------

Win32Video::Win32Video (int parm)
: m_Modes (NULL),
  m_IsFullscreen (false),
  m_Adapter (D3DADAPTER_DEFAULT)
{
	I_SetWndProc();
	if (!InitD3D9())
	{
		InitDDraw();
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
	if (D3D != NULL)
	{
		D3D->Release();
		D3D = NULL;
	}

	STOPLOG;
}

bool Win32Video::InitD3D9 ()
{
	DIRECT3DCREATE9FUNC direct3d_create_9;

	if (vid_forceddraw)
	{
		return false;
	}

	// Load the Direct3D 9 library.
	if ((D3D9_dll = LoadLibraryA ("d3d9.dll")) == NULL)
	{
		Printf("Unable to load d3d9.dll! Falling back to DirectDraw...\n");
		return false;
	}

	// Obtain an IDirect3D interface.
	if ((direct3d_create_9 = (DIRECT3DCREATE9FUNC)GetProcAddress (D3D9_dll, "Direct3DCreate9")) == NULL)
	{
		goto closelib;
	}
	if ((D3D = direct3d_create_9 (D3D_SDK_VERSION)) == NULL)
	{
		goto closelib;
	}

	// Select adapter.
	m_Adapter = (vid_adapter < 1 || (UINT)vid_adapter > D3D->GetAdapterCount())
		? D3DADAPTER_DEFAULT : (UINT)vid_adapter - 1u;

	// Check that we have at least PS 1.4 available.
	D3DCAPS9 devcaps;
	if (FAILED(D3D->GetDeviceCaps (m_Adapter, D3DDEVTYPE_HAL, &devcaps)))
	{
		goto d3drelease;
	}
	if ((devcaps.PixelShaderVersion & 0xFFFF) < 0x104)
	{
		goto d3drelease;
	}
	if (!(devcaps.Caps2 & D3DCAPS2_DYNAMICTEXTURES))
	{
		goto d3drelease;
	}

	// Enumerate available display modes.
	FreeModes ();
	AddD3DModes (m_Adapter);
	AddD3DModes (m_Adapter);
	if (Args->CheckParm ("-2"))
	{ // Force all modes to be pixel-doubled.
		ScaleModes (1);
	}
	else if (Args->CheckParm ("-4"))
	{ // Force all modes to be pixel-quadrupled.
		ScaleModes (2);
	}
	else
	{
		AddLowResModes ();
	}
	AddLetterboxModes ();
	if (m_Modes == NULL)
	{ // Too bad. We didn't find any modes for D3D9. We probably won't find any
	  // for DDraw either...
		goto d3drelease;
	}
	return true;

d3drelease:
	D3D->Release();
	D3D = NULL;
closelib:
	FreeLibrary (D3D9_dll);
	Printf("Direct3D acceleration failed! Falling back to DirectDraw...\n");
	return false;
}

static HRESULT WINAPI EnumDDModesCB(LPDDSURFACEDESC desc, void *data)
{
	((Win32Video *)data)->AddMode(desc->dwWidth, desc->dwHeight, 8, desc->dwHeight, 0);
	return DDENUMRET_OK;
}

void Win32Video::InitDDraw ()
{
	DIRECTDRAWCREATEFUNC directdraw_create;
	LPDIRECTDRAW ddraw1;
	STARTLOG;

	HRESULT dderr;

	// Load the DirectDraw library.
	if ((DDraw_dll = LoadLibraryA ("ddraw.dll")) == NULL)
	{
		I_FatalError ("Could not load ddraw.dll");
	}

	// Obtain an IDirectDraw interface.
	if ((directdraw_create = (DIRECTDRAWCREATEFUNC)GetProcAddress (DDraw_dll, "DirectDrawCreate")) == NULL)
	{
		I_FatalError ("The system file ddraw.dll is missing the DirectDrawCreate export");
	}

	dderr = directdraw_create (NULL, &ddraw1, NULL);

	if (FAILED(dderr))
		I_FatalError ("Could not create DirectDraw object: %08lx", dderr);

	static const GUID IDIRECTDRAW2_GUID = { 0xB3A6F3E0, 0x2B43, 0x11CF, 0xA2, 0xDE, 0x00, 0xAA, 0x00, 0xB9, 0x33, 0x56 };

	dderr = ddraw1->QueryInterface (IDIRECTDRAW2_GUID, (LPVOID*)&DDraw);
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
					"If you started " GAMENAME " from a fullscreen DOS box, run it from "
					"a DOS window instead. If that does not work, you may need to reboot.");
	}
	if (Args->CheckParm ("-2"))
	{ // Force all modes to be pixel-doubled.
		ScaleModes(1);
	}
	else if (Args->CheckParm ("-4"))
	{ // Force all modes to be pixel-quadrupled.
		ScaleModes(2);
	}
	else
	{
		AddLowResModes ();
	}
	AddLetterboxModes ();
}

// Returns true if fullscreen, false otherwise
bool Win32Video::GoFullscreen (bool yes)
{
	static const char *const yestypes[2] = { "windowed", "fullscreen" };
	HRESULT hr[2];
	int count;

	// FIXME: Do this right for D3D.
	if (D3D != NULL)
	{
		return yes;
	}

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

// Flips to the GDI surface and clears it
void Win32Video::BlankForGDI ()
{
	static_cast<BaseWinFB *> (screen)->Blank ();
}

//==========================================================================
//
// Win32Video :: DumpAdapters
//
// Dumps the list of display adapters to the console. Only meaningful for
// Direct3D.
//
//==========================================================================

void Win32Video::DumpAdapters()
{
	using OptWin32::GetMonitorInfoA;

	if (D3D == NULL)
	{
		Printf("Multi-monitor support requires Direct3D.\n");
		return;
	}

	UINT num_adapters = D3D->GetAdapterCount();

	for (UINT i = 0; i < num_adapters; ++i)
	{
		D3DADAPTER_IDENTIFIER9 ai;
		char moreinfo[64] = "";

		if (FAILED(D3D->GetAdapterIdentifier(i, 0, &ai)))
		{
			continue;
		}
		// Strip trailing whitespace from adapter description.
		for (char *p = ai.Description + strlen(ai.Description) - 1;
			 p >= ai.Description && isspace(*p);
			 --p)
		{
			*p = '\0';
		}
		HMONITOR hm = D3D->GetAdapterMonitor(i);
		MONITORINFOEX mi;
		mi.cbSize = sizeof(mi);

		assert(GetMonitorInfo); // Missing in NT4, but so is D3D
		if (GetMonitorInfo(hm, &mi))
		{
			mysnprintf(moreinfo, countof(moreinfo), " [%ldx%ld @ (%ld,%ld)]%s",
				mi.rcMonitor.right - mi.rcMonitor.left,
				mi.rcMonitor.bottom - mi.rcMonitor.top,
				mi.rcMonitor.left, mi.rcMonitor.top,
				mi.dwFlags & MONITORINFOF_PRIMARY ? " (Primary)" : "");
		}
		Printf("%s%u. %s%s\n",
			i == m_Adapter ? TEXTCOLOR_BOLD : "",
			i + 1, ai.Description, moreinfo);
	}
}

// Mode enumeration --------------------------------------------------------

void Win32Video::AddD3DModes (unsigned adapter)
{
	for (D3DFORMAT format : { D3DFMT_X8R8G8B8, D3DFMT_R5G6B5})
	{
		UINT modecount, i;
		D3DDISPLAYMODE mode;

		modecount = D3D->GetAdapterModeCount(adapter, format);
		for (i = 0; i < modecount; ++i)
		{
			if (D3D_OK == D3D->EnumAdapterModes(adapter, format, i, &mode))
			{
				AddMode(mode.Width, mode.Height, 8, mode.Height, 0);
			}
		}
	}
}

//==========================================================================
//
// Win32Video :: AddLowResModes
//
// Recent NVidia drivers no longer support resolutions below 640x480, even
// if you try to add them as a custom resolution. With D3DFB, pixel doubling
// is quite easy to do and hardware-accelerated. If you have 1280x800, then
// you can have 320x200, but don't be surprised if it shows up as widescreen
// on a widescreen monitor, since that's what it is.
//
//==========================================================================

void Win32Video::AddLowResModes()
{
	ModeInfo *mode, *nextmode;

	for (mode = m_Modes; mode != NULL; mode = nextmode)
	{
		nextmode = mode->next;
		if (mode->realheight == mode->height &&
			mode->doubling == 0 &&
			mode->height >= 200*2 &&
			mode->height <= 480*2 &&
			mode->width >= 320*2 &&
			mode->width <= 640*2)
		{
			AddMode (mode->width / 2, mode->height / 2, mode->bits, mode->height / 2, 1);
		}
	}
	for (mode = m_Modes; mode != NULL; mode = nextmode)
	{
		nextmode = mode->next;
		if (mode->realheight == mode->height &&
			mode->doubling == 0 &&
			mode->height >= 200*4 &&
			mode->height <= 480*4 &&
			mode->width >= 320*4 &&
			mode->width <= 640*4)
		{
			AddMode (mode->width / 4, mode->height / 4, mode->bits, mode->height / 4, 2);
		}
	}
}

// Add 16:9 and 16:10 resolutions you can use in a window or letterboxed
void Win32Video::AddLetterboxModes ()
{
	ModeInfo *mode, *nextmode;

	for (mode = m_Modes; mode != NULL; mode = nextmode)
	{
		nextmode = mode->next;
		if (mode->realheight == mode->height && mode->height * 4/3 == mode->width)
		{
			if (mode->width >= 360)
			{
				AddMode (mode->width, mode->width * 9/16, mode->bits, mode->height, mode->doubling);
			}
			if (mode->width > 640)
			{
				AddMode (mode->width, mode->width * 10/16, mode->bits, mode->height, mode->doubling);
			}
		}
	}
}

void Win32Video::AddMode (int x, int y, int bits, int y2, int doubling)
{
	// Reject modes that do not meet certain criteria.
	if ((x & 1) != 0 ||
		y > MAXHEIGHT ||
		x > MAXWIDTH ||
		y < 200 ||
		x < 320)
	{
		return;
	}

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

	*probep = new ModeInfo (x, y, bits, y2, doubling);
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

// For every mode, set its scaling factor. Modes that end up with too
// small a display area are discarded.

void Win32Video::ScaleModes (int doubling)
{
	ModeInfo *mode, **prev;

	prev = &m_Modes;
	mode = m_Modes;

	while (mode != NULL)
	{
		assert(mode->doubling == 0);
		mode->width >>= doubling;
		mode->height >>= doubling;
		mode->realheight >>= doubling;
		mode->doubling = doubling;
		if ((mode->width & 7) != 0 || mode->width < 320 || mode->height < 200)
		{ // Mode became too small. Delete it.
			*prev = mode->next;
			delete mode;
		}
		else
		{
			prev = &mode->next;
		}
		mode = *prev;
	}
}

void Win32Video::StartModeIterator (int bits, bool fs)
{
	m_IteratorMode = m_Modes;
	m_IteratorBits = bits;
	m_IteratorFS = fs;
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

DFrameBuffer *Win32Video::CreateFrameBuffer (int width, int height, bool bgra, bool fullscreen, DFrameBuffer *old)
{
	static int retry = 0;
	static int owidth, oheight;

	BaseWinFB *fb;
	PalEntry flashColor;
	int flashAmount;

	if (fullscreen)
	{
		I_ClosestResolution(&width, &height, D3D ? 32 : 8);
	}

	LOG4 ("CreateFB %d %d %d %p\n", width, height, fullscreen, old);

	if (old != NULL)
	{ // Reuse the old framebuffer if its attributes are the same
		BaseWinFB *fb = static_cast<BaseWinFB *> (old);
		if (fb->Width == width &&
			fb->Height == height &&
			fb->Windowed == !fullscreen &&
			fb->Bgra == bgra)
		{
			return old;
		}
		old->GetFlash (flashColor, flashAmount);
		if (old == screen) screen = nullptr;
		delete old;
	}
	else
	{
		flashColor = 0;
		flashAmount = 0;
	}

	if (D3D != NULL)
	{
		fb = new D3DFB (m_Adapter, width, height, bgra, fullscreen);
	}
	else
	{
		fb = new DDrawFB (width, height, fullscreen);
	}

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
		fb = static_cast<DDrawFB *>(CreateFrameBuffer (width, height, bgra, fullscreen, NULL));
	}
	retry = 0;

	fb->SetFlash (flashColor, flashAmount);
	return fb;
}

void Win32Video::SetWindowedScale (float scale)
{
	// FIXME
}

//==========================================================================
//
// BaseWinFB :: ScaleCoordsFromWindow
//
// Given coordinates in window space, return coordinates in what the game
// thinks screen space is.
//
//==========================================================================

void BaseWinFB::ScaleCoordsFromWindow(int16_t &x, int16_t &y)
{
	RECT rect;

	int TrueHeight = GetTrueHeight();
	if (GetClientRect(Window, &rect))
	{
		x = int16_t(x * Width / (rect.right - rect.left));
		y = int16_t(y * TrueHeight / (rect.bottom - rect.top));
	}
	// Subtract letterboxing borders
	y -= (TrueHeight - Height) / 2;
}

//==========================================================================
//
// SetFPSLimit
//
// Initializes an event timer to fire at a rate of <limit>/sec. The video
// update will wait for this timer to trigger before updating.
//
// Pass 0 as the limit for unlimited.
// Pass a negative value for the limit to use the value of vid_maxfps.
//
//==========================================================================

void I_SetFPSLimit(int limit)
{
	if (limit < 0)
	{
		limit = vid_maxfps;
	}
	// Kill any leftover timer.
	if (FPSLimitTimer != 0)
	{
		timeKillEvent(FPSLimitTimer);
		FPSLimitTimer = 0;
	}
	if (limit == 0)
	{ // no limit
		if (FPSLimitEvent != NULL)
		{
			CloseHandle(FPSLimitEvent);
			FPSLimitEvent = NULL;
		}
		DPrintf(DMSG_NOTIFY, "FPS timer disabled\n");
	}
	else
	{
		if (FPSLimitEvent == NULL)
		{
			FPSLimitEvent = CreateEvent(NULL, FALSE, TRUE, NULL);
			if (FPSLimitEvent == NULL)
			{ // Could not create event, so cannot use timer.
				Printf(DMSG_WARNING, "Failed to create FPS limitter event\n");
				return;
			}
		}
		atterm(StopFPSLimit);
		// Set timer event as close as we can to limit/sec, in milliseconds.
		UINT period = 1000 / limit;
		FPSLimitTimer = timeSetEvent(period, 0, (LPTIMECALLBACK)FPSLimitEvent, 0, TIME_PERIODIC | TIME_CALLBACK_EVENT_SET);
		if (FPSLimitTimer == 0)
		{
			CloseHandle(FPSLimitEvent);
			FPSLimitEvent = NULL;
			Printf("Failed to create FPS limiter timer\n");
			return;
		}
		DPrintf(DMSG_NOTIFY, "FPS timer set to %u ms\n", period);
	}
}

//==========================================================================
//
// StopFPSLimit
//
// Used for cleanup during application shutdown.
//
//==========================================================================

static void StopFPSLimit()
{
	I_SetFPSLimit(0);
}

void I_FPSLimit()
{
	if (FPSLimitEvent != NULL)
	{
		WaitForSingleObject(FPSLimitEvent, 1000);
	}
}