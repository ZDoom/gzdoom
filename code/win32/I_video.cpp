//**************************************************************************
//**
//** i_video.cpp: Low level DOOM graphics routines.
//**			  This version uses PTC. (Prometheus True Color)
//** NEW (1.17a): Support for DirectDraw is back!
//**
//** There are two reasons why I switched from DirectDraw to PTC:
//**	  1. PTC makes windowed modes easy without any special work.
//**	  2. PTC makes it easy to support *all* hi- and true-color modes.
//**
//**************************************************************************

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

// MACROS ------------------------------------------------------------------

#define true TRUE
#define false FALSE

// TYPES -------------------------------------------------------------------

struct screenchain {
	DCanvas *scr;
	screenchain *next;
} *chain;

typedef struct modelist_s {
	modelist_s *next;
	int width, height, bpp;
} modelist_t;

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void I_StartModeIterator (int id);
BOOL I_NextMode (int *width, int *height);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void AddMode (int x, int y, int bpp, modelist_t **lastmode);
static void FreeModes (void);
static void MakeModesList (void);
static void refreshDisplay (void);
static void fullChanged (cvar_t *var);

static HRESULT WINAPI EnumDDModesCB (LPDDSURFACEDESC desc, void *modes);
static void InitDDraw (void);
static void NewDDMode (int width, int height);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern HWND Window;
extern BOOL vidactive;	// Checked to avoid annoying flashes on console
						// during startup if developer is true
extern BOOL setmodeneeded;
extern int	NewWidth, NewHeight, NewBits;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

CVAR (vid_fps, "0", 0)
CVAR (ticker, "0", 0)
CVAR (vid_palettehack, "0", CVAR_ARCHIVE)
CVAR (vid_noptc, "0", CVAR_ARCHIVE)

BOOL Fullscreen;
static modelist_t *IteratorMode;
static int IteratorBits;

int DisplayBits, RealDisplayBits;
int DisplayWidth, DisplayHeight;	// Display size


// PRIVATE DATA DEFINITIONS ------------------------------------------------

static modelist_t *Modes = NULL;
static int nummodes = 0;

static Console *Con;
static Palette *DisPal;		// the display palette

static IDirectDraw			*DDObj;
static LPDIRECTDRAWPALETTE	DDPalette;
static LPDIRECTDRAWSURFACE	DDPrimary;
static LPDIRECTDRAWSURFACE	DDBack;
static int NeedPalChange;
static bool CalledCoInitialize;

static union {
	PALETTEENTRY pe[256];
	int32		 ui[256];
} PalEntries;

static BOOL Have320x200x8, Have320x240x8;

// CODE --------------------------------------------------------------------

BEGIN_CUSTOM_CVAR (fullscreen, "1", CVAR_ARCHIVE)
{
	if (!DDObj && Fullscreen != var.value)
		refreshDisplay ();
}
END_CUSTOM_CVAR (fullscreen)

//==========================================================================
//
// AddMode
//
//==========================================================================

static void AddMode (int x, int y, int bpp, modelist_t **lastmode)
{
	modelist_t *newmode = new modelist_t;

	nummodes++;
	newmode->next = NULL;
	newmode->width = x;
	newmode->height = y;
	newmode->bpp = bpp;

	if (x == 320 && bpp == 8)
	{
		if (y == 200)
			Have320x200x8 = true;
		else if (y == 240)
			Have320x240x8 = true;
	}
	(*lastmode)->next = newmode;
	*lastmode = newmode;
}

//==========================================================================
//
// FreeModes
//
//==========================================================================

static void FreeModes (void)
{
	modelist_t *mode = Modes, *tempmode;

	while (mode)
	{
		tempmode = mode;
		mode = mode->next;
		delete tempmode;
	}
	Modes = NULL;
	nummodes = 0;
}

//==========================================================================
//
// MakeModesList
//
//==========================================================================

static void MakeModesList (void)
{
	modelist_t *lastmode = (modelist_t *)&Modes;

	// get ptc modelist
	const Mode *modes = Con->modes();
	if (!modes[0].valid())
	{
		I_FatalError ("Could not get list of available resolutions.\n"
					  "If you ran ZDoom from a fullscreen DOS box, try\n",
					  "running it from a window instead. Alternatively,\n",
					  "try running with the -noptc switch. If that doesn't\n"
					  "work, try rebooting and running ZDoom again.");
	}

	int index=0;
	while (modes[index].valid())
	{
		// Filter out modes taller than 1024 pixels because some
		// of the assembly routines will choke on them.
		int width = modes[index].width();
		int height = modes[index].height();
		int bits = modes[index].format().bits();

		if (height <= 1024 && bits == 8)
		{
			nummodes++;
			AddMode (width, height, bits, &lastmode);
		}
		index++;
	}
}

//==========================================================================
//
// I_ShutdownGraphics
//
//==========================================================================

void STACK_ARGS I_ShutdownGraphics (void)
{
	FreeModes ();

	while (chain)
		I_FreeScreen (chain->scr);

	if (DDObj)
	{
		if (DDPalette)
		{
			DDPalette->Release();
			DDPalette = NULL;
		}
		if (DDPrimary)
		{
			DDPrimary->Release();
			DDPrimary = NULL;
		}
		DDObj->Release();
		DDObj = NULL;
	}
	else
	{
		if (DisPal)
		{
			delete DisPal;
			DisPal = NULL;
		}

		if (Con)
		{
			delete Con;
			Con = NULL;
		}
	}

	if (CalledCoInitialize)
	{
		CoUninitialize ();
		CalledCoInitialize = false;
	}
	ShowWindow (Window, SW_HIDE);
}

//==========================================================================
//
// [RH] I_BeginUpdate
//
//==========================================================================

void I_BeginUpdate (void)
{
	screen->Lock ();
}

//==========================================================================
//
// I_FinishUpdateNoBlit
//
//==========================================================================

void I_FinishUpdateNoBlit (void)
{
	screen->Unlock ();
}

//==========================================================================
//
// I_FinishUpdate
//
//==========================================================================

void I_FinishUpdate (void)
{
	static int lasttic, lastms = 0, lastsec = 0;
	static int framecount = 0, lastcount = 0;
	int		tics, ms;
	int		i;
	int		howlong;

	ms = timeGetTime ();
	if ((howlong = ms - lastms) && vid_fps.value)
	{
		char fpsbuff[40];
		int chars;

		sprintf (fpsbuff, "%d ms (%d fps)",
				 howlong,
				 lastcount);
		chars = strlen (fpsbuff);
		screen->Clear (0, screen->height - 8, chars * 8, screen->height, 0);
		screen->PrintStr (0, screen->height - 8, (char *)&fpsbuff[0], chars);
		if (lastsec != ms / 1000)
		{
			lastcount = framecount;
			framecount = 0;
			lastsec = ms / 1000;
		}
		framecount++;
	}
	lastms = ms;

	if (vid_palettehack.value)
	{
		if (NeedPalChange <= 0)
			NeedPalChange = 1;
		else
			NeedPalChange++;
	}

	// draws little dots on the bottom of the screen
	if (ticker.value)
	{
		i = I_GetTime();
		tics = i - lasttic;
		lasttic = i;
		if (tics > 20) tics = 20;

		for (i=0 ; i<tics*2 ; i+=2)
			screen->buffer[(screen->height-1)*screen->pitch + i] = 0xff;
		for ( ; i<20*2 ; i+=2)
			screen->buffer[(screen->height-1)*screen->pitch + i] = 0x0;
	}

	if (DDObj)
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
			dderr = DDBack->Lock (NULL, &ddsd, DDLOCK_WRITEONLY |
								  DDLOCK_SURFACEMEMORYPTR, NULL);
		} while ((dderr == DDERR_WASSTILLDRAWING) || (dderr == DDERR_SURFACEBUSY));

		if (dderr == DDERR_SURFACELOST)
		{
			dderr = DDPrimary->Restore ();
			do
			{
				dderr = DDBack->Lock (NULL, &ddsd, DDLOCK_WRITEONLY |
									  DDLOCK_SURFACEMEMORYPTR, NULL);
			} while ((dderr == DDERR_WASSTILLDRAWING) || (dderr == DDERR_SURFACEBUSY));
		}

		if (dderr == DD_OK)
		{
			byte *to = (byte *)ddsd.lpSurface;
			byte *from = screen->buffer;
			int y;

			if (ddsd.lPitch == screen->width && screen->width == screen->pitch)
			{
				memcpy (to, from, screen->width * screen->height);
			}
			else
			{
				for (y = 0; y < screen->height; y++)
				{
					memcpy (to, from, screen->width);
					to += ddsd.lPitch;
					from += screen->pitch;
				}
			}
			do
			{
				dderr = DDBack->Unlock (ddsd.lpSurface);
			} while ((dderr == DDERR_WASSTILLDRAWING) || (dderr == DDERR_SURFACEBUSY));
		}

		screen->Unlock ();

		while (1)
		{
			dderr = DDPrimary->Flip (NULL, DDFLIP_WAIT);
			if (dderr == DDERR_SURFACELOST)
			{
				dderr = DDPrimary->Restore ();
				if (dderr != DD_OK)
					break;
			}
			if (dderr != DDERR_WASSTILLDRAWING)
				break;
		}

		if (NeedPalChange > 0)
		{
			NeedPalChange--;
			DDPalette->SetEntries (0, 0, 256, PalEntries.pe);
		}
	}
	else
	{
		try
		{
			screen->Unlock ();
			((Surface *)(screen->m_Private))->copy (*Con);
			Con->update ();

			if (DisPal && NeedPalChange > 0)
			{
				int32 *palentries;

				palentries = DisPal->lock ();
				NeedPalChange--;
				memcpy (palentries, PalEntries.ui, 256*sizeof(int32));
				DisPal->unlock ();
				Con->palette (*DisPal);
				if (screen->is8bit)
					((Surface *)(screen->m_Private))->palette (*DisPal);
			}
		}
		catch (Error &error)
		{
			I_FatalError (error.message());
		}
	}
}

//==========================================================================
//
// I_ReadScreen
//
//==========================================================================

void I_ReadScreen (byte *scr)
{
	int y;
	byte *source;

	screen->Lock ();
	source = screen->buffer;

	for (y = 0; y < screen->height; y++)
	{
		memcpy (scr, source, screen->width);
		scr += screen->width;
		source += screen->pitch;
	}

	screen->Unlock ();
}


//==========================================================================
//
// I_SetPalette
//
//==========================================================================

void I_SetPalette (unsigned int *pal)
{
	if (!pal)
		return;

	if (DDPalette)
	{
		int i;

		for (i = 0; i < 256; i++)
		{
			PalEntries.pe[i].peRed = RPART(pal[i]);
			PalEntries.pe[i].peGreen = GPART(pal[i]);
			PalEntries.pe[i].peBlue = BPART(pal[i]);
		}
	}
	else
	{
		memcpy (PalEntries.ui, pal, 256*sizeof(int32));
	}
	NeedPalChange++;
}

//==========================================================================
//
// I_SetMode
//
//==========================================================================

void I_SetMode (int width, int height, int bits)
{
	DisplayWidth = width;
	DisplayHeight = height;
	DisplayBits = bits;

	if (DDObj)
	{
		NewDDMode (width, height);
	}
	else
	{
		try
		{
			Con->close ();
			I_PauseMouse ();
			Con->option ("DirectX");
			if (Fullscreen)
			{
				if (!fullscreen.value)
				{
					Fullscreen = FALSE;
				}
			}
			else
			{
				if (fullscreen.value)
				{
					Fullscreen = TRUE;
				}
			}

			if (Fullscreen)
			{
				Con->option ("fullscreen output");
			}
			else
			{
				Con->option ("windowed output");
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
			Con->open (Window, width, height, Format(8));
			Con->option ("disable key buffering");
			if (Fullscreen || (!menuactive && gamestate != GS_FULLCONSOLE))
				I_ResumeMouse ();	// Make sure mouse pointer is grabbed.
		}
		catch (Error &error)
		{
			I_FatalError (error.message());
		}
	}

	// cheapo hack to make sure current display depth is stored in screen
	if (screen)
		screen->bits = DisplayBits;
}

//==========================================================================
//
// refreshDisplay
//
//==========================================================================

static void refreshDisplay (void)
{
	if (gamestate != GS_STARTUP)
	{
		NewWidth = DisplayWidth;
		NewHeight = DisplayHeight;
		NewBits = DisplayBits;
		setmodeneeded = true;
	}
}

//==========================================================================
//
// I_InitGraphics
//
//==========================================================================

void I_InitGraphics (void)
{
	char num[8];
	modelist_t *mode = (modelist_t *)&Modes;

	I_DetectOS ();

	num[0] = '1' - !Args.CheckParm ("-devparm");
	num[1] = 0;
	ticker.SetDefault (num);

	Fullscreen = (BOOL)fullscreen.value;

	if (vid_noptc.value || Args.CheckParm ("-noptc"))
	{
		Printf_Bold ("Bypassing PTC\n");
		InitDDraw ();
	}
	else
	{
		try
		{
			Con = new Console;
			if (!Con)
				throw Error ("Could not create console");
//			Con->option ("enable logging");
			DisPal = new Palette;
			MakeModesList ();
			sprintf (num, "%d", nummodes);
			new cvar_t ("vid_nummodes", num, CVAR_NOSET);
		}
		catch (Error &err)
		{
			FreeModes ();
			Printf_Bold ("Failed to initialize PTC. You won't be\n"
						 "able to play in a window. Reason:\n", err.message ());
			InitDDraw ();
		}
	}

	atterm (I_ShutdownGraphics);
}

//==========================================================================
//
// InitDDraw
//
//==========================================================================

static void InitDDraw (void)
{
	modelist_t *lastmode = (modelist_t *)&Modes;
	HRESULT dderr;
	char num[8];

	CoInitialize (NULL);
	CalledCoInitialize = true;

	dderr = CoCreateInstance (CLSID_DirectDraw, 0, CLSCTX_INPROC_SERVER,
		IID_IDirectDraw, (void **)&DDObj);

	if (FAILED(dderr))
		I_FatalError ("Could not create DirectDraw object: %x", dderr);

	dderr = DDObj->Initialize (0);
	if (FAILED(dderr))
	{
		DDObj->Release ();
		I_FatalError ("Could not initialize DirectDraw object");
	}

	dderr = DDObj->SetCooperativeLevel (Window, DDSCL_EXCLUSIVE |
		DDSCL_FULLSCREEN | DDSCL_ALLOWREBOOT | DDSCL_ALLOWMODEX);
	if (FAILED(dderr))
	{
		DDObj->Release ();
		I_FatalError ("Could not set cooperative level: %x", dderr);
	}

	FreeModes ();
	dderr = DDObj->EnumDisplayModes (0, NULL, &lastmode, EnumDDModesCB);
	if (FAILED(dderr))
	{
		DDObj->Release ();
		I_FatalError ("Could not enumerate display modes: %x", dderr);
	}
	if (nummodes == 0)
	{
		DDObj->Release ();
		I_FatalError ("DirectDraw returned no display modes. Try rebooting.");
	}
	if (OSPlatform == os_Win95)
	{
		// Windows 95 will let us use Mode X. If we didn't find any linear
		// modes in the loop above, add the Mode X modes here.

		if (!Have320x200x8)
			nummodes++, AddMode (320, 200, 8, &lastmode);
		if (!Have320x240x8)
			nummodes++, AddMode (320, 240, 8, &lastmode);
	}
	sprintf (num, "%d", nummodes);
	cvar_forceset ("vid_nummodes", num);
}

//==========================================================================
//
// EnumDDModesCallback
//
//==========================================================================

static HRESULT WINAPI EnumDDModesCB (LPDDSURFACEDESC desc, void *modes)
{
	if (desc->ddpfPixelFormat.dwRGBBitCount == 8 &&
		desc->dwHeight <= 1024)
	{
		nummodes++;
		AddMode (desc->dwWidth, desc->dwHeight, 8, (modelist_t **)modes);
	}

	return DDENUMRET_OK;
}

//==========================================================================
//
// NewDDMode
//
//==========================================================================

static void NewDDMode (int width, int height)
{
	DDSURFACEDESC ddsd;
	DDSCAPS ddscaps;
	HRESULT err;

	err = DDObj->SetDisplayMode (width, height, 8);
	if (err != DD_OK)
		I_FatalError ("Could not set display mode: %x", err);

	if (DDPrimary)	DDPrimary->Release(),	DDPrimary = NULL;
	if (DDPalette)	DDPalette->Release(),	DDPalette = NULL;

	memset (&ddsd, 0, sizeof(ddsd));
	ddsd.dwSize = sizeof(ddsd);
	ddsd.dwFlags = DDSD_CAPS | DDSD_BACKBUFFERCOUNT;
	ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE |
						  DDSCAPS_FLIP |
						  DDSCAPS_COMPLEX |
						  DDSCAPS_VIDEOMEMORY;
	ddsd.dwBackBufferCount = 2;

	// try to get a triple buffered video memory surface.
	err = DDObj->CreateSurface (&ddsd, &DDPrimary, NULL);

	if (err != DD_OK)
	{
		// try to get a double buffered video memory surface.
		ddsd.dwBackBufferCount = 1;
		err = DDObj->CreateSurface (&ddsd, &DDPrimary, NULL);
	}

	if (err != DD_OK)
	{
		// settle for a main memory surface.
		ddsd.ddsCaps.dwCaps &= ~DDSCAPS_VIDEOMEMORY;
		err = DDObj->CreateSurface (&ddsd, &DDPrimary, NULL);
	}

	Printf_Bold ("%s-buffering\n", ddsd.dwBackBufferCount == 2 ? "triple" : "double");

	ddscaps.dwCaps = DDSCAPS_BACKBUFFER;
	err = DDPrimary->GetAttachedSurface (&ddscaps, &DDBack);
	if (err != DD_OK)
		I_FatalError ("Could not get back buffer: %x", err);

	err = DDObj->CreatePalette (DDPCAPS_8BIT|DDPCAPS_ALLOW256, PalEntries.pe,
		&DDPalette, NULL);
	if (err != DD_OK)
		I_FatalError ("Could not create palette: %x", err);
	DDPrimary->SetPalette (DDPalette);
}

//==========================================================================
//
// I_CheckResolution
//
//==========================================================================

BOOL I_CheckResolution (int width, int height, int bits)
{
	modelist_t *mode = Modes;

	while (mode)
	{
		if (mode->width == width && mode->height == height && mode->bpp == bits)
			return true;

		mode = mode->next;
	}
	return false;
}

//==========================================================================
//
// I_StartModeIteratior
//
//==========================================================================

void I_StartModeIterator (int bits)
{
	IteratorMode = Modes;
	IteratorBits = bits;
}

//==========================================================================
//
// I_NextMode
//
//==========================================================================

BOOL I_NextMode (int *width, int *height)
{
	if (IteratorMode)
	{
		while (IteratorMode && IteratorMode->bpp != IteratorBits)
			IteratorMode = IteratorMode->next;

		if (IteratorMode)
		{
			*width = IteratorMode->width;
			*height = IteratorMode->height;
			IteratorMode = IteratorMode->next;
			return true;
		}
	}
	return false;
}

//==========================================================================
//
// Cmd_Vid_DescribeModes
//
//==========================================================================

BEGIN_COMMAND (vid_listmodes)
{
	modelist_t *mode = Modes;

	while (mode)
	{
		// print accumulated mode info
		Printf (PRINT_HIGH, "%4dx%-4d  %2dbit\n",
				mode->width,
				mode->height,
				mode->bpp);

		// go to next mode
		mode = mode->next;
	}
}
END_COMMAND (vid_listmodes)

//==========================================================================
//
// Cmd_Vid_DescribeCurrentMode
//
//==========================================================================

BEGIN_COMMAND (vid_currentmode)
{
	Printf (PRINT_HIGH, "%dx%dx%d\n", screen->width, screen->height,
			DisplayBits);
}
END_COMMAND (vid_currentmode)

//==========================================================================
//
// I_AllocateScreen
//
//==========================================================================

bool I_AllocateScreen (DCanvas *scrn, int width, int height, int bits)
{
	if (scrn->m_Private)
		I_FreeScreen (scrn);

	scrn->width = width;
	scrn->height = height;
	scrn->is8bit = (bits == 8) ? true : false;
	scrn->bits = DisplayBits;
	scrn->m_LockCount = 0;
	scrn->m_Palette = NULL;

	if (DDObj)
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

		dderr = DDObj->CreateSurface (&ddsd, &surface, NULL);
		if (SUCCEEDED(dderr))
		{
			screenchain *tracer = new screenchain;

			scrn->m_Private = surface;
			tracer->scr = scrn;
			tracer->next = chain;
			chain = tracer;

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

			screenchain *tracer = new screenchain;

			scrn->m_Private = surface;
			tracer->scr = scrn;
			tracer->next = chain;
			chain = tracer;

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

//==========================================================================
//
// I_FreeScreen
//
//==========================================================================

void I_FreeScreen (DCanvas *scrn)
{
	struct screenchain *thisone, *prev;

	if (scrn->m_Private)
	{
		if (DDObj)
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

	thisone = chain;
	prev = NULL;
	while (thisone)
	{
		if (thisone->scr == scrn)
		{
			if (prev)
				prev->next = thisone->next;
			else
				chain = thisone->next;
			delete thisone;
			break;
		}
		else
		{
			prev = thisone;
			thisone = thisone->next;
		}
	}
}

//==========================================================================
//
// I_LockScreen
//
//==========================================================================

void I_LockScreen (DCanvas *scrn)
{
	if (scrn->m_Private)
	{
		if (DDObj)
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

//==========================================================================
//
// I_UnlockScreen
//
//==========================================================================

void I_UnlockScreen (DCanvas *scrn)
{
	if (scrn->m_Private)
	{
		if (DDObj)
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

//==========================================================================
//
// I_Blit
//
//==========================================================================

void I_Blit (DCanvas *src, int srcx, int srcy, int srcwidth, int srcheight,
			 DCanvas *dest, int destx, int desty, int destwidth, int destheight)
{
	if (!src->m_Private || !dest->m_Private)
		return;
/*
	Funny problem with calling BitBlt() here. The more I do, the slower it gets.
	It also doesn't give me quite the right dimensions. (?) For now, I just do
	the conversion and copying myself.
*/
#if 0
	RECTANGLE srcrect (srcx, srcy, srcx + srcwidth, srcy + srcheight);
	RECTANGLE destrect (destx, desty, destx + destwidth, desty + destheight);

	((Surface *)(src->m_Private))->BitBlt (*(Surface *)(dest->m_Private),
										 srcrect, destrect);
#else
	fixed_t fracxstep, fracystep;
	fixed_t fracx, fracy;
	int x, y;

	if (!src->m_LockCount)
		src->Lock ();
	if (!dest->m_LockCount)
		dest->Lock ();

	fracy = srcy << FRACBITS;
	fracystep = (srcheight << FRACBITS) / destheight;
	fracxstep = (srcwidth << FRACBITS) / destwidth;

	if (src->is8bit == dest->is8bit)
	{
		// INDEX8 -> INDEX8 or ARGB8888 -> ARGB8888

		byte *destline, *srcline;

		if (!dest->is8bit)
		{
			destwidth <<= 2;
			srcwidth <<= 2;
			srcx <<= 2;
			destx <<= 2;
		}

		if (fracxstep == FRACUNIT)
		{
			for (y = desty; y < desty + destheight; y++, fracy += fracystep)
			{
				memcpy (dest->buffer + y * dest->pitch + destx,
						src->buffer + (fracy >> FRACBITS) * src->pitch + srcx,
						destwidth);
			}
		}
		else
		{
			for (y = desty; y < desty + destheight; y++, fracy += fracystep)
			{
				srcline = src->buffer + (fracy >> FRACBITS) * src->pitch + srcx;
				destline = dest->buffer + y * dest->pitch + destx;
				for (x = fracx = 0; x < destwidth; x++, fracx += fracxstep)
				{
					destline[x] = srcline[fracx >> FRACBITS];
				}
			}
		}
	}
	else if (!src->is8bit && dest->is8bit)
	{
		// ARGB8888 -> INDEX8
		I_FatalError ("Can't I_Blit() an ARGB8888 source to\nan INDEX8 destination");
	}
	else
	{
		// INDEX8 -> ARGB8888 (Palette set in V_Palette)
		int32 *destline;
		byte *srcline;

		if (fracxstep == FRACUNIT)
		{
			// No X-scaling
			for (y = desty; y < desty + destheight; y++, fracy += fracystep)
			{
				srcline = src->buffer + (fracy >> FRACBITS) * src->pitch + srcx;
				destline = (int32 *)(dest->buffer + y * dest->pitch) + destx;
				for (x = 0; x < destwidth; x++)
				{
					destline[x] = V_Palette[srcline[x]];
				}
			}
		}
		else
		{
			// With X-scaling
			for (y = desty; y < desty + destheight; y++, fracy += fracystep)
			{
				srcline = src->buffer + (fracy >> FRACBITS) * src->pitch + srcx;
				destline = (int32 *)(dest->buffer + y * dest->pitch) + destx;
				for (x = fracx = 0; x < destwidth; x++, fracx += fracxstep)
				{
					destline[x] = V_Palette[srcline[fracx >> FRACBITS]];
				}
			}
		}
	}

	if (!src->m_LockCount)
		I_UnlockScreen (src);
	if (!dest->m_LockCount)
		I_UnlockScreen (dest);
#endif
}
