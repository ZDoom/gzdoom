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

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "hardware.h"
#include "c_dispatch.h"
#include "v_text.h"
#include "doomstat.h"
#include "m_argv.h"
#include "version.h"
#include "swrenderer/r_swrenderer.h"

EXTERN_CVAR (Bool, fullscreen)

CVAR(Int, win_x, -1, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Int, win_y, -1, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Bool, win_maximized, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)

extern HWND Window;

IVideo *Video;

// do not include GL headers here, only declare the necessary functions.
IVideo *gl_CreateVideo();

void I_RestartRenderer();
int currentcanvas = -1;
int currentgpuswitch = -1;
bool changerenderer;

// Optimus/Hybrid switcher
CUSTOM_CVAR(Int, vid_gpuswitch, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)
{
	if (self != currentgpuswitch)
	{
		switch (self)
		{
		case 0:
			Printf("Selecting default GPU...\n");
			break;
		case 1:
			Printf("Selecting high-performance dedicated GPU...\n");
			break;
		case 2:
			Printf("Selecting power-saving integrated GPU...\n");
			break;
		default:
			Printf("Unknown option (%d) - falling back to 'default'\n", *vid_gpuswitch);
			self = 0;
			break;
		}
		Printf("You must restart " GAMENAME " for this change to take effect.\n");
	}
}


CCMD (vid_restart)
{
}


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
}

void I_InitGraphics ()
{
	// todo: implement ATI version of this. this only works for nvidia notebooks, for now.
	currentgpuswitch = vid_gpuswitch;
	if (currentgpuswitch == 1)
		putenv("SHIM_MCCOMPAT=0x800000001"); // discrete
	else if (currentgpuswitch == 2)
		putenv("SHIM_MCCOMPAT=0x800000000"); // integrated

	// If the focus window is destroyed, it doesn't go back to the active window.
	// (e.g. because the net pane was up, and a button on it had focus)
	if (GetFocus() == NULL && GetActiveWindow() == Window)
	{
		// Make sure it's in the foreground and focused. (It probably is
		// already foregrounded but may not be focused.)
		SetForegroundWindow(Window);
		SetFocus(Window);
		// Note that when I start a 2-player game on the same machine, the
		// window for the game that isn't focused, active, or foregrounded
		// still receives a WM_ACTIVATEAPP message telling it that it's the
		// active window. The window that is really the active window does
		// not receive a WM_ACTIVATEAPP message, so both games think they
		// are the active app. Huh?
	}

	Video = gl_CreateVideo();

	if (Video == NULL)
		I_FatalError ("Failed to initialize display");
	
	atterm (I_ShutdownGraphics);
}

/** Remaining code is common to Win32 and Linux **/

// VIDEO WRAPPERS ---------------------------------------------------------


static void GetCenteredPos (int &winx, int &winy, int &winw, int &winh, int &scrwidth, int &scrheight)
{
	DEVMODE displaysettings;
	RECT rect;
	int cx, cy;

	memset (&displaysettings, 0, sizeof(displaysettings));
	displaysettings.dmSize = sizeof(displaysettings);
	EnumDisplaySettings (NULL, ENUM_CURRENT_SETTINGS, &displaysettings);
	scrwidth = (int)displaysettings.dmPelsWidth;
	scrheight = (int)displaysettings.dmPelsHeight;
	GetWindowRect (Window, &rect);
	cx = scrwidth / 2;
	cy = scrheight / 2;
	winx = cx - (winw = rect.right - rect.left) / 2;
	winy = cy - (winh = rect.bottom - rect.top) / 2;
}

static void KeepWindowOnScreen (int &winx, int &winy, int winw, int winh, int scrwidth, int scrheight)
{
	// If the window is too large to fit entirely on the screen, at least
	// keep its upperleft corner visible.
	if (winx + winw > scrwidth)
	{
		winx = scrwidth - winw;
	}
	if (winx < 0)
	{
		winx = 0;
	}
	if (winy + winh > scrheight)
	{
		winy = scrheight - winh;
	}
	if (winy < 0)
	{
		winy = 0;
	}
}

void I_SaveWindowedPos ()
{
	// Don't save if we were run with the -0 option.
	if (Args->CheckParm ("-0"))
	{
		return;
	}
	// Make sure we only save the window position if it's not fullscreen.
	static const int WINDOW_STYLE = WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX;
	if ((GetWindowLong (Window, GWL_STYLE) & WINDOW_STYLE) == WINDOW_STYLE)
	{
		RECT wrect;

		if (GetWindowRect (Window, &wrect))
		{
			// If (win_x,win_y) specify to center the window, don't change them
			// if the window is still centered.
			if (win_x < 0 || win_y < 0)
			{
				int winx, winy, winw, winh, scrwidth, scrheight;

				GetCenteredPos (winx, winy, winw, winh, scrwidth, scrheight);
				KeepWindowOnScreen (winx, winy, winw, winh, scrwidth, scrheight);
				if (win_x < 0 && winx == wrect.left)
				{
					wrect.left = win_x;
				}
				if (win_y < 0 && winy == wrect.top)
				{
					wrect.top = win_y;
				}
			}
			win_x = wrect.left;
			win_y = wrect.top;
		}

		win_maximized = IsZoomed(Window) == TRUE;
	}
}

void I_RestoreWindowedPos ()
{
	int winx, winy, winw, winh, scrwidth, scrheight;

	GetCenteredPos (winx, winy, winw, winh, scrwidth, scrheight);

	// Just move to (0,0) if we were run with the -0 option.
	if (Args->CheckParm ("-0"))
	{
		winx = winy = 0;
	}
	else
	{
		if (win_x >= 0)
		{
			winx = win_x;
		}
		if (win_y >= 0)
		{
			winy = win_y;
		}
		KeepWindowOnScreen (winx, winy, winw, winh, scrwidth, scrheight);
	}
	MoveWindow (Window, winx, winy, winw, winh, TRUE);

	if (win_maximized && !Args->CheckParm("-0"))
		ShowWindow(Window, SW_MAXIMIZE);
}
