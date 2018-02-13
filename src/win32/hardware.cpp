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
#include "win32iface.h"
#include "i_video.h"
#include "i_system.h"
#include "c_console.h"
#include "c_cvars.h"
#include "c_dispatch.h"
#include "v_text.h"
#include "doomstat.h"
#include "m_argv.h"
#include "version.h"
#include "swrenderer/r_swrenderer.h"

EXTERN_CVAR (Bool, ticker)
EXTERN_CVAR (Bool, fullscreen)
EXTERN_CVAR (Bool, swtruecolor)
EXTERN_CVAR (Float, vid_winscale)
EXTERN_CVAR (Bool, vid_forceddraw)
EXTERN_CVAR (Bool, win_borderless)

CVAR(Int, win_x, -1, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Int, win_y, -1, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Bool, win_maximized, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)

extern HWND Window;

bool ForceWindowed;

IVideo *Video;

// do not include GL headers here, only declare the necessary functions.
IVideo *gl_CreateVideo();
FRenderer *gl_CreateInterface();

void I_RestartRenderer();
int currentrenderer = -1;
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

// Software OpenGL canvas
CUSTOM_CVAR(Bool, vid_glswfb, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)
{
	if ((self ? 1 : 0) != currentcanvas)
		Printf("You must restart " GAMENAME " for this change to take effect.\n");
}

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
			Printf("Unknown renderer (%d).  Falling back to software renderer...\n", *vid_renderer);
			self = 0; // make sure to actually switch to the software renderer
			break;
		}
		//changerenderer = true;
		Printf("You must restart " GAMENAME " to switch the renderer\n");
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
	UCVarValue val;

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
	val.Bool = !!Args->CheckParm ("-devparm");
	ticker.SetGenericRepDefault (val, CVAR_Bool);

	if (currentcanvas == 0) // Software Canvas: 0 = D3D or DirectDraw, 1 = OpenGL
		if (currentrenderer == 1)
			Video = gl_CreateVideo();
		else
			Video = new Win32Video(0);
	else
		Video = gl_CreateVideo();

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
	currentcanvas = vid_glswfb;
	if (currentrenderer == 1)
		Printf("Renderer: OpenGL\n");
	else if (currentcanvas == 1)
		Printf("Renderer: Software on OpenGL\n");
	else if (currentcanvas == 0 && vid_forceddraw == false)
		Printf("Renderer: Software on Direct3D\n");
	else if (currentcanvas == 0)
		Printf("Renderer: Software on DirectDraw\n");
	else
		Printf("Renderer: Unknown\n");
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
		if (ForceWindowed)
		{
			fs = false;
		}
		else
		{
			fs = fullscreen;
		}
		break;
	}
	DFrameBuffer *res = Video->CreateFrameBuffer (width, height, swtruecolor, fs, old);

	//* Right now, CreateFrameBuffer cannot return NULL
	if (res == NULL)
	{
		I_FatalError ("Mode %dx%d is unavailable\n", width, height);
	}
	//*/
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

	if (win_borderless && !Args->CheckParm("-0"))
	{
		LONG lStyle = GetWindowLong(Window, GWL_STYLE);
		lStyle &= ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZE | WS_MAXIMIZE | WS_SYSMENU);
		SetWindowLong(Window, GWL_STYLE, lStyle);
		SetWindowPos(Window, HWND_TOP, 0, 0, scrwidth, scrheight, 0);
	}
	if (win_maximized && !Args->CheckParm("-0"))
		ShowWindow(Window, SW_MAXIMIZE);
}

extern int NewWidth, NewHeight, NewBits, DisplayBits;

CUSTOM_CVAR(Bool, swtruecolor, true, CVAR_ARCHIVE|CVAR_GLOBALCONFIG|CVAR_NOINITCALL)
{
	// Strictly speaking this doesn't require a mode switch, but it is the easiest
	// way to force a CreateFramebuffer call without a lot of refactoring.
	if (currentrenderer == 0)
	{
		NewWidth = screen->VideoWidth;
		NewHeight = screen->VideoHeight;
		NewBits = DisplayBits;
		setmodeneeded = true;
	}
}

CUSTOM_CVAR(Bool, win_borderless, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)
{
	// Just reinit the window. Saves a lot of trouble.
	if (!fullscreen)
	{
		NewWidth = screen->VideoWidth;
		NewHeight = screen->VideoHeight;
		NewBits = DisplayBits;
		setmodeneeded = true;
	}
}

CUSTOM_CVAR (Bool, fullscreen, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG|CVAR_NOINITCALL)
{
	NewWidth = screen->VideoWidth;
	NewHeight = screen->VideoHeight;
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
		NewWidth = screen->VideoWidth;
		NewHeight = screen->VideoHeight;
		NewBits = DisplayBits;
		//setmodeneeded = true;	// This CVAR doesn't do anything and only causes problems!
	}
}

CCMD (vid_listmodes)
{
	static const char *ratios[7] = { "", " - 16:9", " - 16:10", " - 17:10", " - 5:4", " - 17:10", " - 21:9" };
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

