/*
** hardware.cpp
** Somewhat OS-independant interface to the screen, mouse, keyboard, and stick
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

#include <SDL.h>
#include <signal.h>

#include "hardware.h"
#include "c_dispatch.h"
#include "v_text.h"
#include "doomstat.h"
#include "m_argv.h"
#include "swrenderer/r_swrenderer.h"

IVideo *Video;

void I_RestartRenderer();


void I_ShutdownGraphics ()
{
	if (screen)
	{
		DFrameBuffer *s = screen;
		screen = NULL;
		delete s;
	}
	if (Video)
		delete Video, Video = NULL;

	SDL_QuitSubSystem (SDL_INIT_VIDEO);
}

void I_InitGraphics ()
{
#ifdef __APPLE__
	SDL_SetHint(SDL_HINT_VIDEO_MAC_FULLSCREEN_SPACES, "0");
#endif // __APPLE__

	if (SDL_InitSubSystem (SDL_INIT_VIDEO) < 0)
	{
		I_FatalError ("Could not initialize SDL video:\n%s\n", SDL_GetError());
		return;
	}

	Printf("Using video driver %s\n", SDL_GetCurrentVideoDriver());

	extern IVideo *gl_CreateVideo();
	Video = gl_CreateVideo();
	
	if (Video == NULL)
		I_FatalError ("Failed to initialize display");

	atterm (I_ShutdownGraphics);
}

/** Remaining code is common to Win32 and Linux **/

// VIDEO WRAPPERS ---------------------------------------------------------

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

EXTERN_CVAR(Int, vid_maxfps);
EXTERN_CVAR(Bool, cl_capfps);

#if !defined(__APPLE__) && !defined(__OpenBSD__)
Semaphore FPSLimitSemaphore;

static void FPSLimitNotify(sigval val)
{
	SEMAPHORE_SIGNAL(FPSLimitSemaphore)
}

void I_SetFPSLimit(int limit)
{
	static sigevent FPSLimitEvent;
	static timer_t FPSLimitTimer;
	static bool FPSLimitTimerEnabled = false;
	static bool EventSetup = false;
	if(!EventSetup)
	{
		EventSetup = true;
		FPSLimitEvent.sigev_notify = SIGEV_THREAD;
		FPSLimitEvent.sigev_signo = 0;
		FPSLimitEvent.sigev_value.sival_int = 0;
		FPSLimitEvent.sigev_notify_function = FPSLimitNotify;
		FPSLimitEvent.sigev_notify_attributes = NULL;

		SEMAPHORE_INIT(FPSLimitSemaphore, 0, 0)
	}

	if (limit < 0)
	{
		limit = vid_maxfps;
	}
	// Kill any leftover timer.
	if (FPSLimitTimerEnabled)
	{
		timer_delete(FPSLimitTimer);
		FPSLimitTimerEnabled = false;
	}
	if (limit == 0)
	{ // no limit
		DPrintf(DMSG_NOTIFY, "FPS timer disabled\n");
	}
	else
	{
		FPSLimitTimerEnabled = true;
		if(timer_create(CLOCK_REALTIME, &FPSLimitEvent, &FPSLimitTimer) == -1)
			Printf(DMSG_WARNING, "Failed to create FPS limitter event\n");
		itimerspec period = { {0, 0}, {0, 0} };
		period.it_value.tv_nsec = period.it_interval.tv_nsec = 1000000000 / limit;
		if(timer_settime(FPSLimitTimer, 0, &period, NULL) == -1)
			Printf(DMSG_WARNING, "Failed to set FPS limitter timer\n");
		DPrintf(DMSG_NOTIFY, "FPS timer set to %u ms\n", (unsigned int) period.it_interval.tv_nsec / 1000000);
	}
}
#else
// So Apple doesn't support POSIX timers and I can't find a good substitute short of
// having Objective-C Cocoa events or something like that.
void I_SetFPSLimit(int limit)
{
}
#endif
