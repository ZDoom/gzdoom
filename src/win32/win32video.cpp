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

#define _WIN32_WINNT 0x0501
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>

// HEADER FILES ------------------------------------------------------------

#include "c_cvars.h"
#include "i_system.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void StopFPSLimit();

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

EXTERN_CVAR(Int, vid_maxfps)

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static UINT FPSLimitTimer;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

HANDLE FPSLimitEvent;

CVAR (Int, vid_adapter, 1, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)


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

