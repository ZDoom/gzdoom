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

#include "ptc.h"

extern "C" {

#define __BYTEBOOL__
#include "doomtype.h"
#include "c_dispch.h"
#include "c_consol.h"

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
#include "c_consol.h"
#include "c_cvars.h"
#include "c_dispch.h"
#include "st_stuff.h"
#include "doomdef.h"
#include "z_zone.h"

// MACROS ------------------------------------------------------------------

#define true TRUE
#define false FALSE

// TYPES -------------------------------------------------------------------

struct screenchain {
	screen_t *scr;
	struct screenchain *next;
} *chain;

typedef struct modelist_s {
	modelist_s *next;
	int width, height, bpp;
	int id;
} modelist_t;

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void I_StartModeIterator (int id);
BOOLI_NextMode (int *width, int *height);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

struct player_s;
static void Cmd_Vid_DescribeModes (struct player_s*,int,char**);
static void Cmd_Vid_DescribeCurrentMode (struct player_s*,int,char**);

static void AddMode (int x, int y, int bpp, int id, modelist_t **lastmode);
static void FreeModes (void);
static void MakeModesList (void);
static char *GetFormatName (const int id);
static void ReinitPTC (char *iface);
static void refreshDisplay (void);
static void stretchChanged (cvar_t *var);
static void fullChanged (cvar_t *var);

static HRESULT WINAPI EnumDDModesCB (LPDDSURFACEDESC desc, void *modes);
static void InitDDraw (void);
static void NewDDMode (int width, int height);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern HWND Window;
extern BOOL vidactive;	// Checked to avoid annoying flashes on console
						// during startup if developer is true
extern BOOL setmodeneeded;
extern int	NewWidth, NewHeight, NewID;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

cvar_t *I_ShowFPS;
cvar_t *ticker;
cvar_t *win_stretchx;
cvar_t *win_stretchy;
cvar_t *fullscreen;
cvar_t *vid_palettehack;
BOOL Fullscreen;
static modelist_t *IteratorMode;
static int IteratorID;

char *IdStrings[22] = {
	"ARGB8888",
	"ABGR8888",
	"BGRA8888",
	"RGBA8888",
	"RGB888",
	"BGR888",
	"RGB565",
	"BGR565",
	"ARGB1555",
	"ABGR1555",
	"INDEX8",
	"FAKEMODE1A",
	"FAKEMODE1B",
	"FAKEMODE1C",
	"FAKEMODE2A",
	"FAKEMODE2B",
	"FAKEMODE2C",
	"FAKEMODE3A",
	"FAKEMODE3B",
	"FAKEMODE3C",
	"GREY8",
	"RGB332"
};

byte IdTobpp[22] = {
	32, 32, 32, 32, 24, 24, 16, 16, 15, 15, 8,
	18, 18, 18, 14, 14, 14, 12, 12, 12, 8, 8
};

int DisplayID;
int DisplayBPP;	// Number of bits-per-pixel of output device
int DisplayWidth, DisplayHeight;	// Display size


// PRIVATE DATA DEFINITIONS ------------------------------------------------

static modelist_t *Modes = NULL;
static char WindowedIFace[8];
static char FullscreenIFace[8];
static int nummodes = 0;

static struct CmdDispatcher VidCommands[] = {
	{ "vid_currentmode",	Cmd_Vid_DescribeCurrentMode },
	{ "vid_listmodes",		Cmd_Vid_DescribeModes },
	{ NULL, }
};

}	// extern "C"

static PTC ptc;				// the PTC interface object
static Palette *DisPal;		// the display palette

static IDirectDraw			*DDObj;
static LPDIRECTDRAWPALETTE	DDPalette;
static LPDIRECTDRAWSURFACE	DDPrimary;
static LPDIRECTDRAWSURFACE	DDBack;
static int NeedPalChange;
static BOOL CalledCoInitialize;

static union {
	PALETTEENTRY pe[256];
	uint		 ui[256];
} PalEntries;

extern "C" {

// CODE --------------------------------------------------------------------

//==========================================================================
//
// AddMode
//
//==========================================================================

static void AddMode (int x, int y, int bpp, int id, modelist_t **lastmode)
{
	modelist_t *newmode = (modelist_t *)Z_Malloc (sizeof (modelist_t), PU_STATIC, 0);

	nummodes++;
	newmode->next = NULL;
	newmode->width = x;
	newmode->height = y;
	newmode->bpp = bpp;
	newmode->id = id;

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
		Z_Free (tempmode);
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
	BOOL have320x200x8 = false, have320x240x8 = false;

	// get ptc modelist
	List<MODE> list;
	ptc.GetModeList(list);

	// modelist iterator
	List<MODE>::Iterator iterator = list.first();
	MODE *ptcmode = iterator.current();

	while (ptcmode) {
		if (ptcmode->output == WINDOWED) {
			if (!WindowedIFace[0])
				strncpy (WindowedIFace, ptcmode->i, 8);
		} else {
			if (!FullscreenIFace[0])
				strncpy (FullscreenIFace, ptcmode->i, 8);

			// Filter out modes taller than 1024 pixels since the
			// assembly routines will choke on them. (Besides, they
			// are really slow so why use them?)
			if (ptcmode->y <= 1024 && ptcmode->format.id == INDEX8) {
				nummodes++;

				AddMode (ptcmode->x, ptcmode->y, ptcmode->format.bits,
					ptcmode->format.id, &lastmode);
				if (ptcmode->x == 320 && ptcmode->format.bits == 8) {
					if (ptcmode->y == 200)
						have320x200x8 = true;
					else if (ptcmode->y == 240)
						have320x240x8 = true;
				}
			}
		}
		ptcmode = iterator.next ();
	}

	if (OSPlatform == os_Win95) {
		// Windows 95 will let us use Mode X (assuming my PTC patch has been
		// applied). If we didn't find any linear modes in the loop above,
		// add the Mode X modes here.

		if (!have320x200x8)
			AddMode (320, 200, 8, INDEX8, &lastmode);
		if (!have320x240x8)
			AddMode (320, 240, 8, INDEX8, &lastmode);
	}
}

//==========================================================================
//
// GetFormatName
//
//==========================================================================

static char *GetFormatName (const int id)
{
	return IdStrings[id-1000];
}

//==========================================================================
//
// I_ShutdownGraphics
//
//==========================================================================

void STACK_ARGS I_ShutdownGraphics (void)
{
	FreeModes ();

	while (chain) {
		I_FreeScreen (chain->scr);
	}

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

		ptc.Close ();
	}

	if (CalledCoInitialize)
	{
		CoUninitialize ();
		CalledCoInitialize = false;
	}
}

//==========================================================================
//
// [RH] I_BeginUpdate
//
//==========================================================================

void I_BeginUpdate (void)
{
	V_LockScreen (&screen);
}

//==========================================================================
//
// I_FinishUpdateNoBlit
//
//==========================================================================

void I_FinishUpdateNoBlit (void)
{
	V_UnlockScreen (&screen);
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
	if ((howlong = ms - lastms) && I_ShowFPS->value) {
		char fpsbuff[40];
		int chars;

		sprintf (fpsbuff, "%d ms (%d fps)",
				 howlong,
				 lastcount);
		chars = strlen (fpsbuff);
		V_Clear (0, screen.height - 8, chars * 8, screen.height, &screen, 0);
		V_PrintStr (0, screen.height - 8, (byte *)&fpsbuff[0], chars);
		if (lastsec != ms / 1000) {
			lastcount = framecount;
			framecount = 0;
			lastsec = ms / 1000;
		}
		framecount++;
	}
	lastms = ms;

	if (vid_palettehack->value)
	{
		if (NeedPalChange <= 0)
			NeedPalChange = 1;
		else
			NeedPalChange++;
	}

	// draws little dots on the bottom of the screen
	if (ticker->value) {
		i = I_GetTime();
		tics = i - lasttic;
		lasttic = i;
		if (tics > 20) tics = 20;

		for (i=0 ; i<tics*2 ; i+=2)
			screen.buffer[(screen.height-1)*screen.pitch + i] = 0xff;
		for ( ; i<20*2 ; i+=2)
			screen.buffer[(screen.height-1)*screen.pitch + i] = 0x0;
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

		if (dderr == DDERR_SURFACELOST) {
			dderr = DDPrimary->Restore ();
			do
			{
				dderr = DDBack->Lock (NULL, &ddsd, DDLOCK_WRITEONLY |
									  DDLOCK_SURFACEMEMORYPTR, NULL);
			} while ((dderr == DDERR_WASSTILLDRAWING) || (dderr == DDERR_SURFACEBUSY));
		}

		if (dderr == DD_OK) {
			byte *to = (byte *)ddsd.lpSurface;
			byte *from = screen.buffer;
			int y;

			if (ddsd.lPitch == screen.width && screen.width == screen.pitch) {
				memcpy (to, from, screen.width * screen.height);
			} else {
				for (y = 0; y < screen.height; y++) {
					memcpy (to, from, screen.width);
					to += ddsd.lPitch;
					from += screen.pitch;
				}
			}
			do
			{
				dderr = DDBack->Unlock (ddsd.lpSurface);
			} while ((dderr == DDERR_WASSTILLDRAWING) || (dderr == DDERR_SURFACEBUSY));
		}

		V_UnlockScreen (&screen);

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
		V_UnlockScreen (&screen);
		((Surface *)(screen.impdata))->Update ();

		if (DisPal && NeedPalChange > 0)
		{
			uint *palentries;

			if (palentries = (uint *)DisPal->Lock ())
			{
				NeedPalChange--;
				memcpy (palentries, PalEntries.ui, 256*sizeof(uint));
				DisPal->Unlock ();

				if (screen.is8bit)
					((Surface *)(screen.impdata))->SetPalette (*DisPal);

				// Only set the display palette if it is indexed color
				FORMAT format = ptc.GetFormat ();

				if (format.type == INDEXED && format.model != GREYSCALE)
					ptc.SetPalette (*DisPal);
			}
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

	V_LockScreen (&screen);
	source = screen.buffer;

	for (y = 0; y < screen.height; y++)
	{
		memcpy (scr, source, screen.width);
		scr += screen.width;
		source += screen.pitch;
	}

	V_UnlockScreen (&screen);
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
		memcpy (PalEntries.ui, pal, 256*sizeof(uint));
	}
	NeedPalChange++;
}

//==========================================================================
//
// ReinitPTC
//
//==========================================================================

static void ReinitPTC (char *iface)
{
	if (!ptc.Init (iface, (WINDOW)Window))
		I_FatalError ("Could not reinitialize PTC");
#ifdef _DEBUG
	// We don't want our priority class bumped up!
	SetPriorityClass (GetCurrentProcess(), NORMAL_PRIORITY_CLASS);
#endif
	if (developer->value && vidactive)
		Printf (PRINT_HIGH, "PTC reinitialized to use %s\n\n", iface);
}

//==========================================================================
//
// I_SetMode
//
//==========================================================================

void I_SetMode (int width, int height, int id)
{
	DisplayWidth = width;
	DisplayHeight = height;
	DisplayID = id;

	ShowWindow (Window, SW_SHOW);

	if (DDObj)
	{
		NewDDMode (width, height);
	}
	else
	{
		if (Fullscreen)
		{
			if (!fullscreen->value)
			{
				Fullscreen = FALSE;
				// Get out of fullscreen mode
				ptc.Close ();
				//ReinitPTC (WindowedIFace);
			}
		}
		else
		{
			if (fullscreen->value)
			{
				Fullscreen = TRUE;
				ReinitPTC (FullscreenIFace);
				I_ResumeMouse ();	// Make sure mouse pointer is grabbed.
			}
		}

		if (Fullscreen)
		{
			ptc.SetMode (width, height, id);
		}
		else
		{
			width *= win_stretchx->value;
			height *= win_stretchy->value;

			height += GetSystemMetrics (SM_CYFIXEDFRAME) * 2 +
					  GetSystemMetrics (SM_CYCAPTION);
			width  += GetSystemMetrics (SM_CXFIXEDFRAME) * 2;

			SetWindowPos (Window, NULL, 0, 0, width, height, SWP_DRAWFRAME|
															 SWP_NOACTIVATE|
															 SWP_NOCOPYBITS|
															 SWP_NOMOVE|
															 SWP_NOZORDER);

			// Let PTC know about the new window size
			ReinitPTC (WindowedIFace);

			// If the menu is active, make sure the mouse is released.
			if (menuactive || gamestate == GS_FULLCONSOLE)
				I_PauseMouse ();

			MODE mode = ptc.GetMode();

			DisplayBPP = mode.format.bits;
			if (developer->value && vidactive)
				Printf (PRINT_HIGH, "Mode set: %dx%d (%s)\n", mode.x, mode.y,
					IdStrings[DisplayID-1000]);
		}
	}

	// cheapo hack to make sure current display BPP is stored in screen
	screen.Bpp = IdTobpp[DisplayID-1000] >> 3;
}

//==========================================================================
//
// refreshDisplay
//
//==========================================================================

static void refreshDisplay (void)
{
	NewWidth = DisplayWidth;
	NewHeight = DisplayHeight;
	NewID = DisplayID;
	setmodeneeded = true;
}

//==========================================================================
//
// stretchChanged
//
//==========================================================================

static void stretchChanged (cvar_t *var)
{
	if (!Fullscreen && !DDObj)
		refreshDisplay ();
}

//==========================================================================
//
// fullChanged
//
//==========================================================================

static void fullChanged (cvar_t *var)
{
	if (!DDObj && Fullscreen != var->value)
		refreshDisplay ();
}

//==========================================================================
//
// I_InitGraphics
//
//==========================================================================

void I_InitGraphics (void)
{
	char num[8], *iface;
	modelist_t *mode = (modelist_t *)&Modes;
	cvar_t *vid_noptc;

	I_DetectOS ();

	I_ShowFPS = cvar ("vid_fps", "0", 0);
	num[0] = '0' + !!M_CheckParm ("-devparm");
	num[1] = 0;
	ticker = cvar ("ticker", num, 0);
	win_stretchx = cvar ("win_stretchx", "1.0", CVAR_ARCHIVE|CVAR_CALLBACK);
	win_stretchy = cvar ("win_stretchy", "1.0", CVAR_ARCHIVE|CVAR_CALLBACK);
	fullscreen = cvar ("fullscreen", "1", CVAR_ARCHIVE|CVAR_CALLBACK);
	vid_noptc = cvar ("vid_noptc", "0", CVAR_ARCHIVE);
	vid_palettehack = cvar ("vid_palettehack", "0", CVAR_ARCHIVE);

	Fullscreen = (BOOL)fullscreen->value;

	win_stretchx->u.callback =
	win_stretchy->u.callback = stretchChanged;
	fullscreen->u.callback = fullChanged;

	C_RegisterCommands (VidCommands);

	if (vid_noptc->value || M_CheckParm ("-noptc"))
	{
		Printf_Bold ("Bypassing PTC\n");
		InitDDraw ();
	}
	else
	{
		MakeModesList ();
		sprintf (num, "%d", nummodes);
		cvar ("vid_nummodes", num, CVAR_NOSET);

		if (Fullscreen)
			iface = FullscreenIFace;
		else
			iface = WindowedIFace;

		if (!ptc.Init (iface, Window))
		{
			Printf_Bold ("Failed to initialize PTC. You won't be\n");
			Printf_Bold ("able to play in a window. (Terrible, huh?)\n");
			InitDDraw ();
		}
		else
		{
#ifdef _DEBUG
			// We don't want our priority class bumped up!
			SetPriorityClass (GetCurrentProcess(), NORMAL_PRIORITY_CLASS);
#endif
			if (!(DisPal = new Palette))
			{
				FreeModes ();
				I_FatalError ("Could not create palette");
			}
		}
	}

	atexit (I_ShutdownGraphics);
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
		AddMode (desc->dwWidth, desc->dwHeight, 8, INDEX8, (modelist_t **)modes);
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

BOOL I_CheckResolution (int width, int height, int id)
{
	modelist_t *mode = Modes;

	while (mode) {
		if (mode->width == width && mode->height == height && mode->id == id)
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

void I_StartModeIterator (int id)
{
	IteratorMode = Modes;
	IteratorID = id;
}

//==========================================================================
//
// I_NextMode
//
//==========================================================================

BOOL I_NextMode (int *width, int *height)
{
	if (IteratorMode) {
		while (IteratorMode && IteratorMode->id != IteratorID)
			IteratorMode = IteratorMode->next;

		if (IteratorMode) {
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

static void Cmd_Vid_DescribeModes (struct player_s *p, int c, char **v)
{
	modelist_t *mode = Modes;

	while (mode)
	{
		// print accumulated mode info
		Printf (PRINT_HIGH, "%4dx%-4d  %2dbit %10s\n",
				mode->width,
				mode->height,
				mode->bpp,
				IdStrings[mode->id-1000]);

		// go to next mode
		mode = mode->next;
	}
}

//==========================================================================
//
// Cmd_Vid_DescribeCurrentMode
//
//==========================================================================

void Cmd_Vid_DescribeCurrentMode (struct player_s *p, int c, char **v)
{
	Printf (PRINT_HIGH, "%dx%d (%s)\n", screen.width, screen.height,
			IdStrings[DisplayID-1000]);
}

//==========================================================================
//
// I_AllocateScreen
//
//==========================================================================

BOOL I_AllocateScreen (screen_t *scrn, int width, int height, int Bpp)
{
	if (scrn->impdata)
		I_FreeScreen (scrn);

	scrn->width = width;
	scrn->height = height;
	scrn->is8bit = (Bpp == 8) ? true : false;
	scrn->lockcount = 0;
	scrn->Bpp = IdTobpp[DisplayID-1000] >> 3;
	scrn->palette = NULL;

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
			screenchain *tracer = (screenchain *)Malloc (sizeof(*tracer));

			scrn->impdata = surface;
			tracer->scr = scrn;
			tracer->next = chain;
			chain = tracer;

			return true;
		}
	}
	else
	{
		Surface *surface;

		if (scrn == &screen)
			surface = new Surface (ptc, width, height, (Bpp == 8) ? INDEX8 : ARGB8888);
		else
			surface = new Surface (width, height, (Bpp == 8) ? INDEX8 : ARGB8888);

		if (surface)
		{
			screenchain *tracer = (screenchain *)Malloc (sizeof(*tracer));

			scrn->impdata = surface;
			tracer->scr = scrn;
			tracer->next = chain;
			chain = tracer;

			if (developer->value && vidactive)
			{
				Printf_Bold ("Allocated new surface. (%dx%dx%d)\n",
						surface->GetWidth (),
						surface->GetHeight (),
						surface->GetBitsPerPixel ());
				Printf (PRINT_HIGH, "Orientation: %d\n", surface->GetOrientation ());
				Printf (PRINT_HIGH, "Layout: %d\n", surface->GetLayout ());
				Printf (PRINT_HIGH, "Advance: %d\n", surface->GetAdvance ());
				Printf (PRINT_HIGH, "Pitch: %d\n", surface->GetPitch ());
				Printf (PRINT_HIGH, "Format: %s\n\n", GetFormatName ((surface->GetFormat ()).id));
			}
			return true;
		}
	}
	return false;
}

//==========================================================================
//
// I_FreeScreen
//
//==========================================================================

void I_FreeScreen (screen_t *scrn)
{
	struct screenchain *thisone, *prev;

	if (scrn->impdata)
	{
		if (DDObj)
		{
			// Make sure the surface is unlocked
			if (scrn->lockcount)
				((LPDIRECTDRAWSURFACE)(scrn->impdata))->Unlock (NULL);

			((LPDIRECTDRAWSURFACE)(scrn->impdata))->Release ();
			scrn->impdata = NULL;
		}
		else
		{
			// Make sure the surface is unlocked
			if (scrn->lockcount)
				((Surface *)(scrn->impdata))->Unlock ();

			delete (Surface *)(scrn->impdata);
			scrn->impdata = NULL;
		}
	}

	V_DetachPalette (scrn);

	thisone = chain;
	prev = NULL;
	while (thisone) {
		if (thisone->scr == scrn) {
			if (prev) {
				prev->next = thisone->next;
			} else {
				chain = thisone->next;
			}
			free (thisone);
			break;
		} else {
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

void I_LockScreen (screen_t *scrn)
{
	if (scrn->impdata)
	{
		if (DDObj)
		{
			DDSURFACEDESC ddsd;
			HRESULT dderr;

			memset (&ddsd, 0, sizeof(ddsd));
			ddsd.dwSize = sizeof(ddsd);
			do
			{
				dderr = ((LPDIRECTDRAWSURFACE)(scrn->impdata))->Lock (NULL,
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
			scrn->buffer = (byte *)((Surface *)(scrn->impdata))->Lock ();

			if (scrn->buffer == NULL)
				I_FatalError ("Could not lock surface");

			scrn->pitch = ((Surface *)(scrn->impdata))->GetPitch ();
		}
	} else {
		scrn->lockcount--;
	}
}

//==========================================================================
//
// I_UnlockScreen
//
//==========================================================================

void I_UnlockScreen (screen_t *scrn)
{
	if (scrn->impdata)
	{
		if (DDObj)
		{
			((LPDIRECTDRAWSURFACE)(scrn->impdata))->Unlock (scrn->buffer);
		}
		else
		{
			((Surface *)(scrn->impdata))->Unlock ();
		}
		scrn->buffer = NULL;
	}
}

//==========================================================================
//
// I_Blit
//
//==========================================================================

void I_Blit (screen_t *src, int srcx, int srcy, int srcwidth, int srcheight,
			 screen_t *dest, int destx, int desty, int destwidth, int destheight)
{
	if (!src->impdata || !dest->impdata)
		return;
/*
	Funny problem with calling BitBlt() here. The more I do, the slower it gets.
	It also doesn't give me quite the right dimensions. (?) For now, I just do
	the conversion and copying myself.
*/
#if 0
	RECTANGLE srcrect (srcx, srcy, srcx + srcwidth, srcy + srcheight);
	RECTANGLE destrect (destx, desty, destx + destwidth, desty + destheight);

	((Surface *)(src->impdata))->BitBlt (*(Surface *)(dest->impdata),
										 srcrect, destrect);
#else
	fixed_t fracxstep, fracystep;
	fixed_t fracx, fracy;
	int x, y;

	if (!src->lockcount)
		I_LockScreen (src);
	if (!dest->lockcount)
		I_LockScreen (dest);

	fracy = srcy << FRACBITS;
	fracystep = (srcheight << FRACBITS) / destheight;
	fracxstep = (srcwidth << FRACBITS) / destwidth;

	if (src->is8bit == dest->is8bit) {
		// INDEX8 -> INDEX8 or ARGB8888 -> ARGB8888

		byte *destline, *srcline;

		if (!dest->is8bit) {
			destwidth <<= 2;
			srcwidth <<= 2;
			srcx <<= 2;
			destx <<= 2;
		}

		if (fracxstep == FRACUNIT) {
			for (y = desty; y < desty + destheight; y++, fracy += fracystep) {
				memcpy (dest->buffer + y * dest->pitch + destx,
						src->buffer + (fracy >> FRACBITS) * src->pitch + srcx,
						destwidth);
			}
		} else {
			for (y = desty; y < desty + destheight; y++, fracy += fracystep) {
				srcline = src->buffer + (fracy >> FRACBITS) * src->pitch + srcx;
				destline = dest->buffer + y * dest->pitch + destx;
				for (x = fracx = 0; x < destwidth; x++, fracx += fracxstep) {
					destline[x] = srcline[fracx >> FRACBITS];
				}
			}
		}
	} else if (!src->is8bit && dest->is8bit) {
		// ARGB8888 -> INDEX8
		I_FatalError ("Can't I_Blit() an ARGB8888 source to\nan INDEX8 destination");
	} else {
		// INDEX8 -> ARGB8888 (Palette set in V_Palette)
		uint *destline;
		byte *srcline;

		if (fracxstep == FRACUNIT) {
			// No X-scaling
			for (y = desty; y < desty + destheight; y++, fracy += fracystep) {
				srcline = src->buffer + (fracy >> FRACBITS) * src->pitch + srcx;
				destline = (uint *)(dest->buffer + y * dest->pitch) + destx;
				for (x = 0; x < destwidth; x++) {
					destline[x] = V_Palette[srcline[x]];
				}
			}
		} else {
			// With X-scaling
			for (y = desty; y < desty + destheight; y++, fracy += fracystep) {
				srcline = src->buffer + (fracy >> FRACBITS) * src->pitch + srcx;
				destline = (uint *)(dest->buffer + y * dest->pitch) + destx;
				for (x = fracx = 0; x < destwidth; x++, fracx += fracxstep) {
					destline[x] = V_Palette[srcline[fracx >> FRACBITS]];
				}
			}
		}
	}

	if (!src->lockcount)
		I_UnlockScreen (src);
	if (!dest->lockcount)
		I_UnlockScreen (dest);
#endif
}

// NON-PTC DIRECTDRAW STUFF FOLLOWS ----------------------------------------


}	// extern "C"