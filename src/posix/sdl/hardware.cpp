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
#include <time.h>

#include "version.h"
#include "hardware.h"
#include "i_video.h"
#include "i_system.h"
#include "c_console.h"
#include "c_cvars.h"
#include "c_dispatch.h"
#include "sdlvideo.h"
#include "v_text.h"
#include "doomstat.h"
#include "m_argv.h"
#include "sdlglvideo.h"
#include "r_renderer.h"
#include "r_swrenderer.h"

EXTERN_CVAR (Bool, ticker)
EXTERN_CVAR (Bool, fullscreen)
EXTERN_CVAR (Float, vid_winscale)

IVideo *Video;

extern int NewWidth, NewHeight, NewBits, DisplayBits;
bool V_DoModeSetup (int width, int height, int bits);
void I_RestartRenderer();

int currentrenderer;

// [ZDoomGL]
CUSTOM_CVAR (Int, vid_renderer, 1, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)
{
	// 0: Software renderer
	// 1: OpenGL renderer

	if (self != currentrenderer)
	{
		switch (self)
		{
		case 0:
			Printf("Switching to software renderer...\n");
			break;
		case 1:
			Printf("Switching to OpenGL renderer...\n");
			break;
		default:
			Printf("Unknown renderer (%d).  Falling back to software renderer...\n", (int) vid_renderer);
			self = 0; // make sure to actually switch to the software renderer
			break;
		}
		Printf("You must restart " GAMENAME " to switch the renderer\n");
	}
}

void I_ShutdownGraphics ()
{
	if (screen)
	{
		DFrameBuffer *s = screen;
		screen = NULL;
		s->ObjectFlags |= OF_YesReallyDelete;
		delete s;
	}
	if (Video)
		delete Video, Video = NULL;

	SDL_QuitSubSystem (SDL_INIT_VIDEO);
}

void I_InitGraphics ()
{
	if (SDL_InitSubSystem (SDL_INIT_VIDEO) < 0)
	{
		I_FatalError ("Could not initialize SDL video:\n%s\n", SDL_GetError());
		return;
	}

	Printf("Using video driver %s\n", SDL_GetCurrentVideoDriver());

	UCVarValue val;

	val.Bool = !!Args->CheckParm ("-devparm");
	ticker.SetGenericRepDefault (val, CVAR_Bool);
	
	//currentrenderer = vid_renderer;
	if (currentrenderer==1) Video = new SDLGLVideo(0);
	else Video = new SDLVideo (0);
	
	if (Video == NULL)
		I_FatalError ("Failed to initialize display");

	atterm (I_ShutdownGraphics);

	Video->SetWindowedScale (vid_winscale);
}

static void I_DeleteRenderer()
{
	if (Renderer != NULL) delete Renderer;
}

void I_CreateRenderer()
{
	currentrenderer = vid_renderer;
	if (Renderer == NULL)
	{
		if (currentrenderer==1) Renderer = gl_CreateInterface();
		else Renderer = new FSoftwareRenderer;
		atterm(I_DeleteRenderer);
	}
}


/** Remaining code is common to Win32 and Linux **/

// VIDEO WRAPPERS ---------------------------------------------------------

DFrameBuffer *I_SetMode (int &width, int &height, DFrameBuffer *old)
{
	bool fs = false;
	switch (Video->GetDisplayType ())
	{
	case DISPLAY_WindowOnly:
		fs = false;
		break;
	case DISPLAY_FullscreenOnly:
		fs = true;
		break;
	case DISPLAY_Both:
		fs = fullscreen;
		break;
	}
	DFrameBuffer *res = Video->CreateFrameBuffer (width, height, fs, old);

	/* Right now, CreateFrameBuffer cannot return NULL
	if (res == NULL)
	{
		I_FatalError ("Mode %dx%d is unavailable\n", width, height);
	}
	*/
	return res;
}

bool I_CheckResolution (int width, int height, int bits)
{
	int twidth, theight;

	Video->StartModeIterator (bits, screen ? screen->IsFullscreen() : fullscreen);
	while (Video->NextMode (&twidth, &theight, NULL))
	{
		if (width == twidth && height == theight)
			return true;
	}
	return false;
}

void I_ClosestResolution (int *width, int *height, int bits)
{
	int twidth, theight;
	int cwidth = 0, cheight = 0;
	int iteration;
	DWORD closest = 4294967295u;

	for (iteration = 0; iteration < 2; iteration++)
	{
		Video->StartModeIterator (bits, screen ? screen->IsFullscreen() : fullscreen);
		while (Video->NextMode (&twidth, &theight, NULL))
		{
			if (twidth == *width && theight == *height)
				return;

			if (iteration == 0 && (twidth < *width || theight < *height))
				continue;

			DWORD dist = (twidth - *width) * (twidth - *width)
				+ (theight - *height) * (theight - *height);

			if (dist < closest)
			{
				closest = dist;
				cwidth = twidth;
				cheight = theight;
			}
		}
		if (closest != 4294967295u)
		{
			*width = cwidth;
			*height = cheight;
			return;
		}
	}
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

EXTERN_CVAR(Int, vid_maxfps);
EXTERN_CVAR(Bool, cl_capfps);

#ifndef __APPLE__
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
		DPrintf("FPS timer disabled\n");
	}
	else
	{
		FPSLimitTimerEnabled = true;
		if(timer_create(CLOCK_REALTIME, &FPSLimitEvent, &FPSLimitTimer) == -1)
			Printf("Failed to create FPS limitter event\n");
		itimerspec period = { {0, 0}, {0, 0} };
		period.it_value.tv_nsec = period.it_interval.tv_nsec = 1000000000 / limit;
		if(timer_settime(FPSLimitTimer, 0, &period, NULL) == -1)
			Printf("Failed to set FPS limitter timer\n");
		DPrintf("FPS timer set to %u ms\n", (unsigned int) period.it_interval.tv_nsec / 1000000);
	}
}
#else
// So Apple doesn't support POSIX timers and I can't find a good substitute short of
// having Objective-C Cocoa events or something like that.
void I_SetFPSLimit(int limit)
{
}
#endif

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

extern int NewWidth, NewHeight, NewBits, DisplayBits;

CUSTOM_CVAR (Bool, fullscreen, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	NewWidth = screen->GetWidth();
	NewHeight = screen->GetHeight();
	NewBits = DisplayBits;
	setmodeneeded = true;
}

CUSTOM_CVAR (Float, vid_winscale, 1.f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (self < 1.f)
	{
		self = 1.f;
	}
	else if (Video)
	{
		Video->SetWindowedScale (self);
		NewWidth = screen->GetWidth();
		NewHeight = screen->GetHeight();
		NewBits = DisplayBits;
		setmodeneeded = true;
	}
}

CCMD (vid_listmodes)
{
	static const char *ratios[7] = { "", " - 16:9", " - 16:10", "", " - 5:4", "", " - 21:9" };
	int width, height, bits;
	bool letterbox;

	if (Video == NULL)
	{
		return;
	}
	for (bits = 1; bits <= 32; bits++)
	{
		Video->StartModeIterator (bits, screen->IsFullscreen());
		while (Video->NextMode (&width, &height, &letterbox))
		{
			bool thisMode = (width == DisplayWidth && height == DisplayHeight && bits == DisplayBits);
			int ratio = CheckRatio (width, height);
			Printf (thisMode ? PRINT_BOLD : PRINT_HIGH,
				"%s%4d x%5d x%3d%s%s\n",
				thisMode || !IsRatioWidescreen(ratio) ? "" : TEXTCOLOR_GOLD,
				width, height, bits,
				ratios[ratio],
				thisMode || !letterbox ? "" : TEXTCOLOR_BROWN " LB"
				);
		}
	}
}

CCMD (vid_currentmode)
{
	Printf ("%dx%dx%d\n", DisplayWidth, DisplayHeight, DisplayBits);
}
