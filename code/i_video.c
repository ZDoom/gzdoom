// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// $Log:$
//
// DESCRIPTION:
//	DOOM graphics stuff for DirectDraw.
//
//-----------------------------------------------------------------------------

static const char
rcsid[] = "$Id: i_x.c,v 1.6 1997/02/03 22:45:10 b1 Exp $";

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ddraw.h>
#include <mmsystem.h>
#define false 0
#define true 1

#define __BYTEBOOL__

#include "doomstat.h"
#include "cmdlib.h"
#include "i_system.h"
#include "i_input.h"
#include "i_video.h"
#include "v_video.h"
#include "m_argv.h"
#include "d_main.h"

#include "c_cvars.h"
#include "c_dispatch.h"
#include "st_stuff.h"

#include "doomdef.h"

typedef struct drivers_s {
	struct drivers_s *next;
	char *description;
	char *name;
	GUID guid;
} driver_t;

static driver_t *Drivers = NULL;
static modelist_t *Modes = NULL;

static driver_t *DefDriver, *CurrDriver;

extern HWND		Window;

LPDIRECTDRAW		 DDObj;
LPDIRECTDRAWSURFACE  DDPrimary;
LPDIRECTDRAWSURFACE  DDBack;
LPDIRECTDRAWPALETTE  DDPalette;
PALETTEENTRY		 Palette[256];

cvar_t *I_ShowFPS;
cvar_t *vid_defdriver;

static int nummodes = 0;

void I_ShutdownGraphics(void)
{
	{
		driver_t *driver = Drivers, *tempdriver;

		while (driver) {
			tempdriver = driver;
			driver = driver->next;
			free (tempdriver->description);
			free (tempdriver->name);
			free (tempdriver);
		}
		Drivers = NULL;
	}
	{
		modelist_t *mode = Modes, *tempmode;

		while (mode) {
			tempmode = mode;
			mode = mode->next;
			free (tempmode);
		}
		Modes = NULL;
	}

	if (DDObj) {
		if (DDPalette) {
			if (DDPrimary) {
				IDirectDrawSurface_Release (DDPrimary);
				DDPrimary = NULL;
			}
			IDirectDrawPalette_Release (DDPalette);
			DDPalette = NULL;
		}
		IDirectDraw_Release (DDObj);
		DDObj = NULL;
	}
}

void I_WaitVBL(int count)
{
	static DWORD lastms = 0;
	DWORD ms;

	ms = timeGetTime();
	if (!lastms) {
		lastms = ms;
	} else if (lastms - ms > 14 /* approx. 70 fps */) {
		count--;
	}

	if (DDObj) {
		while (count--) {
			IDirectDraw2_WaitForVerticalBlank (DDObj, DDWAITVB_BLOCKBEGIN, NULL);
		}
	}
}

//
// I_StartFrame
//
void I_StartFrame (void)
{
	// er?

}


//
// I_UpdateNoBlit
//
void I_UpdateNoBlit (void)
{
	// what is this?
}

//
// I_FinishUpdate
//
void I_FinishUpdate (void)
{
	static int	lasttic, lastms = 0;
	int		tics, ms;
	int		i;

	DDSURFACEDESC ddsd;
	int x, y;
	HRESULT dderr;
	long *to, *from;

	// draws little dots on the bottom of the screen
	if (devparm) {
		i = I_GetTime();
		tics = i - lasttic;
		lasttic = i;
		if (tics > 20) tics = 20;

		for (i=0 ; i<tics*2 ; i+=2)
			screens[0][ (SCREENHEIGHT-1)*SCREENPITCH + i] = 0xff;
		for ( ; i<20*2 ; i+=2)
			screens[0][ (SCREENHEIGHT-1)*SCREENPITCH + i] = 0x0;
	}

	ms = timeGetTime ();
	if (lastms) {
		if (I_ShowFPS->value) {
			char fpsbuff[40];
			int chars, x, y;
			byte *zap;

			sprintf (fpsbuff, "%d ms (%.1f fps)",
					 ms - lastms,
					 (float)1000.0 / (ms - lastms));
			chars = strlen (fpsbuff);
			for (y = SCREENHEIGHT - 8; y < SCREENHEIGHT; y++) {
				zap = screens[0] + SCREENPITCH * y;
				for (x = 0; x < chars * 8; x++) {
					*zap++ = 0;
				}
			}
			V_PrintStr (0, SCREENHEIGHT - 8, fpsbuff, chars, 0);
		}
	}
	lastms = ms;

	ddsd.dwSize = sizeof(DDSURFACEDESC);
	ddsd.dwFlags = 0;
	dderr = IDirectDrawSurface2_Lock (DDBack, NULL, &ddsd,
				DDLOCK_WRITEONLY|DDLOCK_WAIT|DDLOCK_SURFACEMEMORYPTR, NULL);
	if (dderr == DDERR_SURFACELOST) {
		IDirectDrawSurface2_Restore (DDPrimary);
		dderr = IDirectDrawSurface2_Lock (DDBack, NULL, &ddsd,
					DDLOCK_WRITEONLY|DDLOCK_WAIT|DDLOCK_SURFACEMEMORYPTR, NULL);
	}

	if (dderr == DD_OK) {
		to = (long *)ddsd.lpSurface;
		from = (long *)screens[0];

		if (ddsd.lPitch == SCREENWIDTH && SCREENWIDTH == SCREENPITCH) {
			memcpy (to, from, SCREENWIDTH*SCREENHEIGHT);
		} else {
			for (y = 0; y < SCREENHEIGHT; y++) {
				for (x = 0; x < (signed)(SCREENWIDTH / sizeof(long)); x++) {
					*to++ = *from++;
				}
				to += (ddsd.lPitch - SCREENWIDTH) / sizeof(long);
			}
		}
		IDirectDrawSurface2_Unlock (DDBack, ddsd.lpSurface);


		while (1) {
			dderr = IDirectDrawSurface2_Flip (DDPrimary, NULL, DDFLIP_WAIT);
			if (dderr == DDERR_SURFACELOST) {
				dderr = IDirectDrawSurface2_Restore (DDPrimary);
				if (dderr != DD_OK)
					break;
			} else {
				break;
			}
		}
	}
}


//
// I_ReadScreen
//
void I_ReadScreen (byte* scr)
{
	memcpy (scr, screens[0], SCREENWIDTH*SCREENHEIGHT);
}


//
// I_SetPalette
//
void I_SetPalette (byte* pal)
{
	static byte lastpal[256*3] = { 0, };
	int i;

	if (!DDPalette)
		return;

	if (!pal) {
		pal = lastpal;
	} else {
		memcpy (lastpal, pal, 256 * 3);
	}

	for (i = 0; i < 256; i++) {
		Palette[i].peRed = newgamma[*pal++];
		Palette[i].peGreen = newgamma[*pal++];
		Palette[i].peBlue = newgamma[*pal++];
		Palette[i].peFlags = PC_NOCOLLAPSE;
	}

	IDirectDrawPalette_SetEntries (DDPalette, 0, 0, 256, Palette);
}


void I_InitGraphics(void)
{
	HRESULT dderr;
	DDSURFACEDESC ddsd;
	DDSCAPS ddscaps;

	dderr = IDirectDraw_SetCooperativeLevel (DDObj, Window, DDSCL_EXCLUSIVE|DDSCL_FULLSCREEN|DDSCL_ALLOWREBOOT);
	if (dderr != DD_OK)
		I_FatalError ("Could not set cooperative level");

	dderr = IDirectDraw_SetDisplayMode (DDObj, SCREENWIDTH, SCREENHEIGHT, 8);
	if (dderr != DD_OK)
		I_FatalError ("Could not set display mode");

	dderr = IDirectDraw2_CreatePalette (DDObj, DDPCAPS_8BIT|DDPCAPS_ALLOW256, Palette, &DDPalette, NULL);
	if (dderr != DD_OK)
		I_FatalError ("Could not create palette");

	ddsd.dwSize = sizeof(ddsd);
	ddsd.dwFlags = DDSD_CAPS|DDSD_BACKBUFFERCOUNT;
	ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE|DDSCAPS_FLIP|DDSCAPS_COMPLEX;
	ddsd.dwBackBufferCount = 1;

	dderr = IDirectDraw_CreateSurface (DDObj, &ddsd, &DDPrimary, NULL);
	if (dderr != DD_OK)
		I_FatalError ("Could not create DirectDraw surfaces");

	ddscaps.dwCaps = DDSCAPS_BACKBUFFER;
	dderr = IDirectDrawSurface2_GetAttachedSurface (DDPrimary, &ddscaps, &DDBack);
	if (dderr != DD_OK)
		I_FatalError ("Could not get back buffer");

	IDirectDrawSurface2_SetPalette (DDPrimary, DDPalette);
}

static driver_t *FindDriver (char *name)
{
	driver_t *driver = Drivers;
	GUID *guid;


	if (!name)
		return NULL;

	if ((guid = StrToGUID (name)) == NULL) {
		// Look for the driver by name

		while (driver) {
			if (stricmp (driver->name, name) == 0)
				return driver;
			driver = driver->next;
		}
	} else {
		// Look for the driver by GUID

		while (driver) {
			if (memcmp (&driver->guid, guid, sizeof(GUID)) == 0)
				return driver;
			driver = driver->next;
		}
	}
	return NULL;
}

static BOOL WINAPI DriverEnumCallback (GUID FAR *lpGUID,
								 LPSTR lpDriverDescription,
								 LPSTR lpDriverName,
								 driver_t **drivers)
{
	driver_t *driver = malloc (sizeof(driver_t));

	driver->next = NULL;
	driver->description = copystring (lpDriverDescription);
	driver->name = copystring (lpDriverName);
	if (lpGUID) {
		driver->guid = *lpGUID;
	} else {
		DefDriver = driver;
		memset (&driver->guid, 0, sizeof(GUID));
	}

	(*drivers)->next = driver;
	*drivers = driver;

	return DDENUMRET_OK;
}

static HRESULT WINAPI EnumModesCallback (LPDDSURFACEDESC desc, modelist_t **modes)
{
	modelist_t *mode;

	if (desc->ddpfPixelFormat.dwRGBBitCount == 8) {
		nummodes++;
		mode = malloc (sizeof (modelist_t));

		mode->next = NULL;
		mode->width = desc->dwWidth;
		mode->height = desc->dwHeight;
		(*modes)->next = mode;
		*modes = mode;
	}

	return DDENUMRET_OK;
}

void I_StartGraphics (void)
{
	HRESULT dderr;
	GUID FAR *guid;
	driver_t *driver = (driver_t *)&Drivers;
	char num[8];
	modelist_t *mode = (modelist_t *)&Modes;

	dderr = DirectDrawEnumerate (DriverEnumCallback, &driver);

	if (driver = FindDriver (vid_defdriver->string)) {
		CurrDriver = driver;
		guid = &driver->guid;
	} else {
		CurrDriver = DefDriver;
		guid = NULL;
	}

	dderr = DirectDrawCreate (guid, &DDObj, NULL);
	if (dderr != DD_OK)
		I_FatalError ("Could not create DirectDraw object");

	dderr = IDirectDraw2_EnumDisplayModes (DDObj, 0, NULL, &mode, EnumModesCallback);

	sprintf (num, "%d", nummodes);
	cvar ("vid_nummodes", num, CVAR_NOSET);
}

void *StrToGUID (char *str)
{
	static GUID guid;
	char temp[4];
	int count, boop;

	// First make sure str actually describes a GUID
	if (str[0]  != '{' ||
		str[9]  != '-' ||
		str[14] != '-' ||
		str[19] != '-' ||
		str[24] != '-' ||
		str[37] != '}' ||
		str[38] != 0)
		return NULL;

	str[9] = str[14] = str[19] = 0;
	guid.Data1 = ParseHex (&str[1]);
	guid.Data2 = ParseHex (&str[10]);
	guid.Data3 = ParseHex (&str[15]);
	str[9] = str[14] = str[19] = '-';

	temp[2] = 0;
	for (boop = 20, count = 0; count < 8; count++) {
		temp[0] = str[boop];
		temp[1] = str[boop + 1];
		guid.Data4[count] = ParseHex (temp);
		boop += 2;
		if (count = 1)
			boop++;
	}

	return &guid;
}

char *GUIDToStr (void *guid)
{
	static char str[40];

	sprintf (str,
		"{%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
		((GUID *)guid)->Data1,
		((GUID *)guid)->Data2,
		((GUID *)guid)->Data3,
		((GUID *)guid)->Data4[0],
		((GUID *)guid)->Data4[1],
		((GUID *)guid)->Data4[2],
		((GUID *)guid)->Data4[3],
		((GUID *)guid)->Data4[4],
		((GUID *)guid)->Data4[5],
		((GUID *)guid)->Data4[6],
		((GUID *)guid)->Data4[7]
	);
	return str;
}

boolean I_CheckResolution (int width, int height)
{
	modelist_t *mode;

	if (Modes == NULL)
		return true;

	mode = Modes;
	while (mode) {
		if (mode->width == width && mode->height == height)
			return true;
		mode = mode->next;
	}
	return false;
}

static void describedriver (driver_t *driver)
{
	Printf_Bold ("%s:\n", driver->name);
	Printf ("%s\n%s\n", GUIDToStr (&driver->guid), driver->description);
}

void Cmd_Vid_DescribeDrivers (void *player, int argc, char **argv)
{
	driver_t *driver;

	driver = Drivers;
	while (driver) {
		describedriver (driver);
		driver = driver->next;
		if (driver)
			Printf ("\n");
	}
}

void Cmd_Vid_DescribeCurrentDriver (void *player, int argc, char **argv)
{
	describedriver (CurrDriver);
}

static void describemodes (int width, int height)
{
	modelist_t *mode;
	int count = 0;

	mode = Modes;
	while (mode) {
		if (width == 0 || (width == mode->width && height == mode->height))
			Printf ("%2d: %d x %d\n", count, mode->width, mode->height);
		mode = mode->next;
		count++;
	}
}

void Cmd_Vid_DescribeModes (void *player, int argc, char **argv)
{
	describemodes (0, 0);
}

void Cmd_Vid_DescribeCurrentMode (void *player, int argc, char **argv)
{
	describemodes (SCREENWIDTH, SCREENHEIGHT);
}