
// HEADER FILES ------------------------------------------------------------

#ifndef INITGUID
#define INITGUID
#endif

#include <objbase.h>
#include <initguid.h>
#include <ddraw.h>
#include <mmsystem.h>
#include <stdio.h>
#include <math.h>

#include "ptc.h"

#define __BYTEBOOL__
#include "doomtype.h"
#include "c_dispatch.h"
#include "c_console.h"

#include "doomstat.h"
#include "cmdlib.h"
#include "i_system.h"
#include "i_input.h"
#include "i_video.h"
#include "v_video.h"
#include "v_text.h"
#include "m_argv.h"
#include "d_main.h"
#include "m_alloc.h"
#include "c_console.h"
#include "c_cvars.h"
#include "c_dispatch.h"
#include "st_stuff.h"
#include "doomdef.h"
#include "z_zone.h"

#include "win32iface.h"

// MACROS ------------------------------------------------------------------

#define true TRUE
#define false FALSE

// TYPES -------------------------------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern HWND Window;
extern BOOL vidactive;	// Checked to avoid annoying flashes on console
						// during startup if developer is true
extern BOOL setmodeneeded;
extern int	NewWidth, NewHeight, NewBits;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

CVAR (vid_palettehack, "0", CVAR_ARCHIVE)
CVAR (vid_noptc, "0", CVAR_ARCHIVE)

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

Win32Video::Win32Video (int parm)
{
	m_CalledCoInitialize = false;
	m_Modes = NULL;
	m_Chain = NULL;
	m_Con = NULL;
	m_DisPal = NULL;
	m_Have320x200x8 = false;
	m_Have320x240x8 = false;
	m_DDObj = NULL;
	m_DDPalette = NULL;
	m_DDPrimary = NULL;
	m_DDBack = NULL;
	m_LastUpdatedCanvas = NULL;
	m_NeedPalChange = 0;

	I_DetectOS ();
	if (vid_noptc.value || Args.CheckParm ("-noptc"))
	{
		Printf_Bold ("Bypassing PTC\n");
		InitDDraw ();
	}
	else
	{
		try
		{
			m_Con = new Console;
			if (!m_Con)
				throw Error ("Could not create console");
//			Con->option ("enable logging");
			m_DisPal = new Palette;
			MakeModesList ();
		}
		catch (Error &err)
		{
			FreeModes ();
			Printf_Bold ("Failed to initialize PTC. You won't be\n"
						 "able to play in a window. Reason:\n", err.message ());
			InitDDraw ();
		}
	}
}

Win32Video::~Win32Video ()
{
	FreeModes ();

	while (m_Chain)
		ReleaseSurface (m_Chain->canvas);

	if (m_DDObj)
	{
		if (m_DDPalette)
		{
			m_DDPalette->Release();
			m_DDPalette = NULL;
		}
		if (m_DDPrimary)
		{
			m_DDPrimary->Release();
			m_DDPrimary = NULL;
		}
		m_DDObj->Release();
		m_DDObj = NULL;
	}
	else
	{
		if (m_DisPal)
		{
			delete m_DisPal;
			m_DisPal = NULL;
		}

		if (m_Con)
		{
			delete m_Con;
			m_Con = NULL;
		}
	}

	if (m_CalledCoInitialize)
	{
		CoUninitialize ();
		m_CalledCoInitialize = false;
	}

	ShowWindow (Window, SW_HIDE);
}

void Win32Video::InitDDraw ()
{
	CBData cbdata;
	HRESULT dderr;

	CoInitialize (NULL);
	m_CalledCoInitialize = true;

	dderr = CoCreateInstance (CLSID_DirectDraw, 0, CLSCTX_INPROC_SERVER,
		IID_IDirectDraw, (void **)&m_DDObj);

	if (FAILED(dderr))
		I_FatalError ("Could not create DirectDraw object: %x", dderr);

	dderr = m_DDObj->Initialize (0);
	if (FAILED(dderr))
	{
		m_DDObj->Release ();
		I_FatalError ("Could not initialize DirectDraw object");
	}

	dderr = m_DDObj->SetCooperativeLevel (Window, DDSCL_EXCLUSIVE |
		DDSCL_FULLSCREEN | DDSCL_ALLOWREBOOT | DDSCL_ALLOWMODEX);
	if (FAILED(dderr))
	{
		m_DDObj->Release ();
		I_FatalError ("Could not set cooperative level: %x", dderr);
	}

	FreeModes ();
	cbdata.self = this;
	cbdata.modes = (ModeInfo *)&m_Modes;
	dderr = m_DDObj->EnumDisplayModes (0, NULL, &cbdata, EnumDDModesCB);
	if (FAILED(dderr))
	{
		m_DDObj->Release ();
		I_FatalError ("Could not enumerate display modes: %x", dderr);
	}
	if (m_Modes == NULL)
	{
		m_DDObj->Release ();
		I_FatalError ("DirectDraw returned no display modes. Try rebooting.");
	}
	if (OSPlatform == os_Win95)
	{
		// Windows 95 will let us use Mode X. If we didn't find any linear
		// modes in the loop above, add the Mode X modes here.

		if (!m_Have320x200x8)
			AddMode (320, 200, 8, &cbdata.modes);
		if (!m_Have320x240x8)
			AddMode (320, 240, 8, &cbdata.modes);
	}
}

HRESULT WINAPI Win32Video::EnumDDModesCB (LPDDSURFACEDESC desc, void *data)
{
	if (desc->ddpfPixelFormat.dwRGBBitCount == 8 &&
		desc->dwHeight <= 1024)
	{
		((CBData *)data)->self->AddMode
			(desc->dwWidth, desc->dwHeight, 8,
			&((CBData *)data)->modes);
	}

	return DDENUMRET_OK;
}

void Win32Video::NewDDMode (int width, int height)
{
	DDSURFACEDESC ddsd;
	DDSCAPS ddscaps;
	HRESULT err;

	err = m_DDObj->SetDisplayMode (width, height, 8);
	if (err != DD_OK)
		I_FatalError ("Could not set display mode: %x", err);

	if (m_DDPrimary)
		m_DDPrimary->Release(),	m_DDPrimary = NULL;
	if (m_DDPalette)
		m_DDPalette->Release(),	m_DDPalette = NULL;

	memset (&ddsd, 0, sizeof(ddsd));
	ddsd.dwSize = sizeof(ddsd);
	ddsd.dwFlags = DDSD_CAPS | DDSD_BACKBUFFERCOUNT;
	ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE |
						  DDSCAPS_FLIP |
						  DDSCAPS_COMPLEX |
						  DDSCAPS_VIDEOMEMORY;
	ddsd.dwBackBufferCount = 2;

	// try to get a triple buffered video memory surface.
	err = m_DDObj->CreateSurface (&ddsd, &m_DDPrimary, NULL);

	if (err != DD_OK)
	{
		// try to get a double buffered video memory surface.
		ddsd.dwBackBufferCount = 1;
		err = m_DDObj->CreateSurface (&ddsd, &m_DDPrimary, NULL);
	}

	if (err != DD_OK)
	{
		// settle for a main memory surface.
		ddsd.ddsCaps.dwCaps &= ~DDSCAPS_VIDEOMEMORY;
		err = m_DDObj->CreateSurface (&ddsd, &m_DDPrimary, NULL);
	}

	Printf_Bold ("%s-buffering\n", ddsd.dwBackBufferCount == 2 ? "triple" : "double");

	ddscaps.dwCaps = DDSCAPS_BACKBUFFER;
	err = m_DDPrimary->GetAttachedSurface (&ddscaps, &m_DDBack);
	if (err != DD_OK)
		I_FatalError ("Could not get back buffer: %x", err);

	err = m_DDObj->CreatePalette (DDPCAPS_8BIT|DDPCAPS_ALLOW256, m_PalEntries.pe,
		&m_DDPalette, NULL);
	if (err != DD_OK)
		I_FatalError ("Could not create palette: %x", err);
	m_DDPrimary->SetPalette (m_DDPalette);
}

void Win32Video::AddMode (int x, int y, int bits, ModeInfo **lastmode)
{
	ModeInfo *newmode = new ModeInfo (x, y, bits);

	if (x == 320 && bits == 8)
	{
		if (y == 200)
			m_Have320x200x8 = true;
		else if (y == 240)
			m_Have320x240x8 = true;
	}
	(*lastmode)->next = newmode;
	*lastmode = newmode;
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

void Win32Video::MakeModesList ()
{
	ModeInfo *lastmode = (ModeInfo *)&m_Modes;

	// get ptc modelist
	const Mode *modes = m_Con->modes();
	if (!modes[0].valid())
	{
		I_FatalError ("Could not get list of available resolutions.\n"
					  "If you ran ZDoom from a fullscreen DOS box, try\n",
					  "running it from a window instead. Alternatively,\n",
					  "try running with the -noptc switch. If that doesn't\n"
					  "work, try rebooting and running ZDoom again.");
	}

	int index = 0;
	while (modes[index].valid())
	{
		// Filter out modes taller than 1024 pixels because some
		// of the assembly routines will choke on them.
		int width = modes[index].width();
		int height = modes[index].height();
		int bits = modes[index].format().bits();

		if (height <= 1024 && bits == 8)
		{
			AddMode (width, height, bits, &lastmode);
		}
		index++;
	}
}

bool Win32Video::SetMode (int width, int height, int bits, bool fullscreen)
{
	if (m_DDObj)
	{
		NewDDMode (width, height);
	}
	else
	{
		try
		{
			m_Con->close ();
			I_PauseMouse ();
			m_Con->option ("DirectX");

			if (fullscreen)
			{
				m_DisplayWidth = width;
				m_DisplayHeight = height;
				m_DisplayBits = bits;

				m_Con->option ("fullscreen output");
			}
			else
			{
				m_DisplayWidth = GetSystemMetrics (SM_CXSCREEN);
				m_DisplayHeight = GetSystemMetrics (SM_CYSCREEN);
				m_DisplayBits = bits;

				m_Con->option ("windowed output");
				SetWindowPos (Window, NULL, 0, 0,
					width + GetSystemMetrics (SM_CXSIZEFRAME) * 2,
					height + GetSystemMetrics (SM_CYSIZEFRAME) * 2 +
						     GetSystemMetrics (SM_CYCAPTION)
					, SWP_DRAWFRAME|
					  SWP_NOACTIVATE|
					  SWP_NOCOPYBITS|
					  SWP_NOMOVE|
					  SWP_NOZORDER);
			}
			ShowWindow (Window, SW_SHOW);
			m_Con->open (Window, width, height, Format(8));
			m_Con->option ("disable key buffering");
		}
		catch (Error &error)
		{
			I_FatalError (error.message());
		}
	}
	return true;
}

void Win32Video::SetWindowedScale (float scale)
{
	// FIXME
}

void Win32Video::SetPalette (DWORD *pal)
{
	if (!pal)
		return;

	if (m_DDPalette)
	{
		int i;

		for (i = 0; i < 256; i++)
		{
			m_PalEntries.pe[i].peRed = RPART(pal[i]);
			m_PalEntries.pe[i].peGreen = GPART(pal[i]);
			m_PalEntries.pe[i].peBlue = BPART(pal[i]);
		}
	}
	else
	{
		memcpy (m_PalEntries.ui, pal, 256*sizeof(int32));
	}
	m_NeedPalChange++;
}

void Win32Video::UpdateScreen (DCanvas *canvas)
{
	m_LastUpdatedCanvas = canvas;

	if (vid_palettehack.value)
	{
		if (m_NeedPalChange <= 0)
			m_NeedPalChange = 1;
		else
			m_NeedPalChange++;
	}

	if (m_DDObj)
	{
		// Using DirectDrawSurface's Blt method is horrendously slow.
		// Using it to copy an 800x600 framebuffer to the back buffer is
		// slower than using the following routine for a 960x720 surface.

		HRESULT dderr;
		DDSURFACEDESC ddsd;

		memset (&ddsd, 0, sizeof(ddsd));
		ddsd.dwSize = sizeof(DDSURFACEDESC);

		do
		{
			dderr = m_DDBack->Lock (NULL, &ddsd, DDLOCK_WRITEONLY |
									DDLOCK_SURFACEMEMORYPTR, NULL);
		} while ((dderr == DDERR_WASSTILLDRAWING) || (dderr == DDERR_SURFACEBUSY));

		if (dderr == DDERR_SURFACELOST)
		{
			dderr = m_DDPrimary->Restore ();
			do
			{
				dderr = m_DDBack->Lock (NULL, &ddsd, DDLOCK_WRITEONLY |
										DDLOCK_SURFACEMEMORYPTR, NULL);
			} while ((dderr == DDERR_WASSTILLDRAWING) || (dderr == DDERR_SURFACEBUSY));
		}

		if (dderr == DD_OK)
		{
			byte *to = (byte *)ddsd.lpSurface;
			byte *from = canvas->buffer;
			int y;

			if (ddsd.lPitch == canvas->width && canvas->width == canvas->pitch)
			{
				memcpy (to, from, canvas->width * canvas->height);
			}
			else
			{
				for (y = 0; y < canvas->height; y++)
				{
					memcpy (to, from, canvas->width);
					to += ddsd.lPitch;
					from += canvas->pitch;
				}
			}
			do
			{
				dderr = m_DDBack->Unlock (ddsd.lpSurface);
			} while ((dderr == DDERR_WASSTILLDRAWING) || (dderr == DDERR_SURFACEBUSY));
		}

		canvas->Unlock ();

		while (1)
		{
			dderr = m_DDPrimary->Flip (NULL, DDFLIP_WAIT);
			if (dderr == DDERR_SURFACELOST)
			{
				dderr = m_DDPrimary->Restore ();
				if (dderr != DD_OK)
					break;
			}
			if (dderr != DDERR_WASSTILLDRAWING)
				break;
		}

		if (m_NeedPalChange > 0)
		{
			m_NeedPalChange--;
			m_DDPalette->SetEntries (0, 0, 256, m_PalEntries.pe);
		}
	}
	else
	{
		try
		{
			canvas->Unlock ();
			((Surface *)(canvas->m_Private))->copy (*m_Con);
			m_Con->update ();

			if (m_DisPal && m_NeedPalChange > 0)
			{
				int32 *palentries;

				palentries = m_DisPal->lock ();
				m_NeedPalChange--;
				memcpy (palentries, m_PalEntries.ui, 256*sizeof(int32));
				m_DisPal->unlock ();
				m_Con->palette (*m_DisPal);
				if (canvas->is8bit)
					((Surface *)(canvas->m_Private))->palette (*m_DisPal);
			}
		}
		catch (Error &error)
		{
			I_FatalError (error.message());
		}
	}
}

void Win32Video::ReadScreen (byte *block)
{
	int y;
	byte *source;

	m_LastUpdatedCanvas->Lock ();
	source = m_LastUpdatedCanvas->buffer;

	for (y = 0; y < m_LastUpdatedCanvas->height; y++)
	{
		memcpy (block, source, m_LastUpdatedCanvas->width);
		block += m_LastUpdatedCanvas->width;
		source += m_LastUpdatedCanvas->pitch;
	}

	m_LastUpdatedCanvas->Unlock ();
}

// This only changes how the iterator lists modes
bool Win32Video::FullscreenChanged (bool fs)
{
	if (m_IteratorFS != fs)
	{
		m_IteratorFS = fs;
		return true;
	}
	return false;
}

int Win32Video::GetModeCount ()
{
	int count = 0;
	ModeInfo *mode = m_Modes;

	while (mode)
	{
		count++;
		mode = mode->next;
	}
	return count;
}

void Win32Video::StartModeIterator (int bits)
{
	m_IteratorMode = m_Modes;
	m_IteratorBits = bits;
}

bool Win32Video::NextMode (int *width, int *height)
{
	if (m_IteratorMode)
	{
		while (m_IteratorMode && m_IteratorMode->bits != m_IteratorBits)
			m_IteratorMode = m_IteratorMode->next;

		if (m_IteratorMode)
		{
			*width = m_IteratorMode->width;
			*height = m_IteratorMode->height;
			m_IteratorMode = m_IteratorMode->next;
			return true;
		}
	}
	return false;
}

bool Win32Video::AllocateSurface (DCanvas *scrn, int width, int height, int bits, bool primary)
{
	if (scrn->m_Private)
		ReleaseSurface (scrn);

	scrn->width = width;
	scrn->height = height;
	scrn->is8bit = (bits == 8) ? true : false;
	scrn->bits = m_DisplayBits;
	scrn->m_LockCount = 0;
	scrn->m_Palette = NULL;

	if (m_DDObj)
	{
		DDSURFACEDESC ddsd;
		HRESULT dderr;
		LPDIRECTDRAWSURFACE surface;

		if (!scrn->is8bit)
			I_FatalError ("Only 8-bit screens are supported");

		ddsd.dwSize = sizeof(ddsd);
		ddsd.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH;
		ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY;
		ddsd.dwWidth = width;
		ddsd.dwHeight = height;

		dderr = m_DDObj->CreateSurface (&ddsd, &surface, NULL);
		if (SUCCEEDED(dderr))
		{
			Chain *tracer = new Chain (scrn);

			scrn->m_Private = surface;
			tracer->next = m_Chain;
			m_Chain = tracer;

			return true;
		}
	}
	else
	{
		try
		{
			Surface *surface;

			surface = new Surface (width, height,
				(bits == 8) ? Format(8) : Format(32,0xff0000,0x00ff00,0x0000ff));

			Chain *tracer = new Chain (scrn);

			scrn->m_Private = surface;
			tracer->next = m_Chain;
			m_Chain = tracer;

			if (developer.value && vidactive)
			{
				Printf_Bold ("Allocated new surface. (%dx%dx%d)/%d\n",
						surface->width(),
						surface->height(),
						surface->format().bits(),
						surface->pitch());
			}
			return true;
		}
		catch (Error &error)
		{
			I_FatalError (error.message());
		}
	}
	return false;
}

void Win32Video::ReleaseSurface (DCanvas *scrn)
{
	Chain *thisone, **prev;

	if (scrn->m_Private)
	{
		if (m_DDObj)
		{
			// Make sure the surface is unlocked
			if (scrn->m_LockCount)
				((LPDIRECTDRAWSURFACE)(scrn->m_Private))->Unlock (NULL);

			((LPDIRECTDRAWSURFACE)(scrn->m_Private))->Release ();
			scrn->m_Private = NULL;
		}
		else
		{
			try
			{
				// Make sure the surface is unlocked
				if (scrn->m_LockCount)
					((Surface *)(scrn->m_Private))->unlock ();

				delete (Surface *)(scrn->m_Private);
				scrn->m_Private = NULL;
			}
			catch (Error &error)
			{
				I_FatalError (error.message());
			}
		}
	}

	scrn->DetachPalette ();

	thisone = m_Chain;
	prev = &m_Chain;
	while (thisone && thisone->canvas != scrn)
	{
		prev = &thisone->next;
		thisone = thisone->next;
	}
	if (thisone)
	{
		*prev = thisone->next;
		delete thisone;
	}
}

void Win32Video::LockSurface (DCanvas *scrn)
{
	if (scrn->m_Private)
	{
		if (m_DDObj)
		{
			DDSURFACEDESC ddsd;
			HRESULT dderr;

			memset (&ddsd, 0, sizeof(ddsd));
			ddsd.dwSize = sizeof(ddsd);
			do
			{
				dderr = ((LPDIRECTDRAWSURFACE)(scrn->m_Private))->Lock (NULL,
					&ddsd, DDLOCK_SURFACEMEMORYPTR | DDLOCK_WAIT, NULL);
			} while (dderr == DDERR_SURFACEBUSY || dderr == DDERR_WASSTILLDRAWING);

			// the surface is always in system memory, so it should never be lost
			if (FAILED(dderr))
				I_FatalError ("Could not lock surface: %x", dderr);

			scrn->buffer = (byte *)ddsd.lpSurface;
			scrn->pitch = ddsd.lPitch;
		}
		else
		{
			try
			{
				scrn->buffer = (byte *)((Surface *)(scrn->m_Private))->lock ();
				scrn->pitch = ((Surface *)(scrn->m_Private))->pitch ();
			}
			catch (Error &error)
			{
				I_FatalError (error.message());
			}
		}
	}
	else
	{
		scrn->m_LockCount--;
	}
}

void Win32Video::UnlockSurface (DCanvas *scrn)
{
	if (scrn->m_Private)
	{
		if (m_DDObj)
		{
			((LPDIRECTDRAWSURFACE)(scrn->m_Private))->Unlock (scrn->buffer);
		}
		else
		{
			try
			{
				((Surface *)(scrn->m_Private))->unlock ();
			}
			catch (Error &error)
			{
				I_FatalError (error.message());
			}
		}
		scrn->buffer = NULL;
	}
}

bool Win32Video::Blit (DCanvas *src, int sx, int sy, int sw, int sh,
					   DCanvas *dst, int dx, int dy, int dw, int dh)
{
	return false;
}
