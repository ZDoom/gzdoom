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
#include "m_argv.h"
#include "r_defs.h"
#include "v_text.h"
#include "version.h"

#include "win32iface.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

IMPLEMENT_ABSTRACT_CLASS(BaseWinFB)

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

//==========================================================================
//
// BaseWinFB :: ScaleCoordsFromWindow
//
// Given coordinates in window space, return coordinates in what the game
// thinks screen space is.
//
//==========================================================================

void BaseWinFB::ScaleCoordsFromWindow(SWORD &x, SWORD &y)
{
	RECT rect;

	int TrueHeight = GetTrueHeight();
	if (GetClientRect(Window, &rect))
	{
		x = SWORD(x * Width / (rect.right - rect.left));
		y = SWORD(y * TrueHeight / (rect.bottom - rect.top));
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
		DPrintf("FPS timer disabled\n");
	}
	else
	{
		if (FPSLimitEvent == NULL)
		{
			FPSLimitEvent = CreateEvent(NULL, FALSE, TRUE, NULL);
			if (FPSLimitEvent == NULL)
			{ // Could not create event, so cannot use timer.
				Printf("Failed to create FPS limitter event\n");
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
			Printf("Failed to create FPS limitter timer\n");
			return;
		}
		DPrintf("FPS timer set to %u ms\n", period);
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
