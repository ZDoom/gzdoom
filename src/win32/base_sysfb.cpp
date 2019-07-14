/*
**
**
**---------------------------------------------------------------------------
** Copyright 2003-2005 Tim Stump
** Copyright 2005-2016 Christoph Oelckers
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
#include <GL/gl.h>
#include "wglext.h"

#include "gl_sysfb.h"
#include "hardware.h"
#include "x86.h"
#include "templates.h"
#include "version.h"
#include "c_console.h"
#include "v_video.h"
#include "i_input.h"
#include "i_system.h"
#include "doomstat.h"
#include "v_text.h"
#include "m_argv.h"
#include "doomerrors.h"
#include "base_sysfb.h"
#include "win32basevideo.h"
#include "c_dispatch.h"


extern HWND			Window;

extern "C" {
    __declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;	
}

EXTERN_CVAR(Int, vid_defwidth)
EXTERN_CVAR(Int, vid_defheight)

//==========================================================================
//
// Windows framebuffer
//
//==========================================================================

//==========================================================================
//
// 
//
//==========================================================================

 void SystemBaseFrameBuffer::GetCenteredPos(int in_w, int in_h, int &winx, int &winy, int &winw, int &winh, int &scrwidth, int &scrheight)
{
	DEVMODE displaysettings;
	RECT rect;
	int cx, cy;

	memset(&displaysettings, 0, sizeof(displaysettings));
	displaysettings.dmSize = sizeof(displaysettings);
	EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &displaysettings);
	scrwidth = (int)displaysettings.dmPelsWidth;
	scrheight = (int)displaysettings.dmPelsHeight;
	GetWindowRect(Window, &rect);
	cx = scrwidth / 2;
	cy = scrheight / 2;
	if (in_w > 0) winw = in_w;
	else winw = rect.right - rect.left;
	if (in_h > 0) winh = in_h;
	else winh = rect.bottom - rect.top;
	winx = cx - winw / 2;
	winy = cy - winh / 2;
}

//==========================================================================
//
// 
//
//==========================================================================

void SystemBaseFrameBuffer::KeepWindowOnScreen(int &winx, int &winy, int winw, int winh, int scrwidth, int scrheight)
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

//==========================================================================
//
// 
//
//==========================================================================

void SystemBaseFrameBuffer::SaveWindowedPos()
{
	// Don't save if we were run with the -0 option.
	if (Args->CheckParm("-0"))
	{
		return;
	}
	// Make sure we only save the window position if it's not fullscreen.
	static const int WINDOW_STYLE = WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX;
	if ((GetWindowLong(Window, GWL_STYLE) & WINDOW_STYLE) == WINDOW_STYLE)
	{
		RECT wrect;

		if (GetWindowRect(Window, &wrect))
		{
			// If (win_x,win_y) specify to center the window, don't change them
			// if the window is still centered.
			if (win_x < 0 || win_y < 0 || win_w < 0 || win_h < 0)
			{
				int winx, winy, winw, winh, scrwidth, scrheight;

				GetCenteredPos(win_w, win_h, winx, winy, winw, winh, scrwidth, scrheight);
				KeepWindowOnScreen(winx, winy, winw, winh, scrwidth, scrheight);
				if (win_x < 0 && winx == wrect.left)
				{
					wrect.left = win_x;
				}
				if (win_y < 0 && winy == wrect.top)
				{
					wrect.top = win_y;
				}
				wrect.right = winw + wrect.left;
				wrect.bottom = winh + wrect.top;
			}

			win_x = wrect.left;
			win_y = wrect.top;
			win_w = wrect.right - wrect.left;
			win_h = wrect.bottom - wrect.top;
		}

		win_maximized = IsZoomed(Window) == TRUE;
	}
}

//==========================================================================
//
// 
//
//==========================================================================

void SystemBaseFrameBuffer::RestoreWindowedPos()
{
	int winx, winy, winw, winh, scrwidth, scrheight;

	GetCenteredPos(win_w, win_h, winx, winy, winw, winh, scrwidth, scrheight);

	// Just move to (0,0) if we were run with the -0 option.
	if (Args->CheckParm("-0"))
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
		KeepWindowOnScreen(winx, winy, winw, winh, scrwidth, scrheight);
	}
	SetWindowPos(Window, nullptr, winx, winy, winw, winh, SWP_NOZORDER | SWP_FRAMECHANGED);

	if (win_maximized && !Args->CheckParm("-0"))
		ShowWindow(Window, SW_MAXIMIZE);
}

//==========================================================================
//
// 
//
//==========================================================================

void SystemBaseFrameBuffer::SetWindowSize(int w, int h)
{
	if (w < 0 || h < 0)
	{
		RestoreWindowedPos();
	}
	else
	{
		LONG style = WS_VISIBLE | WS_CLIPSIBLINGS | WS_OVERLAPPEDWINDOW;
		LONG exStyle = WS_EX_WINDOWEDGE;
		SetWindowLong(Window, GWL_STYLE, style);
		SetWindowLong(Window, GWL_EXSTYLE, exStyle);

		int winx, winy, winw, winh, scrwidth, scrheight;

		RECT r = { 0, 0, w, h };
		AdjustWindowRectEx(&r, style, false, exStyle);
		w = int(r.right - r.left);
		h = int(r.bottom - r.top);

		GetCenteredPos(w, h, winx, winy, winw, winh, scrwidth, scrheight);

		// Just move to (0,0) if we were run with the -0 option.
		if (Args->CheckParm("-0"))
		{
			winx = winy = 0;
		}
		else
		{
			KeepWindowOnScreen(winx, winy, winw, winh, scrwidth, scrheight);
		}

		if (!fullscreen)
		{
			ShowWindow(Window, SW_SHOWNORMAL);
			SetWindowPos(Window, nullptr, winx, winy, winw, winh, SWP_NOZORDER | SWP_FRAMECHANGED);
			win_maximized = false;
			SetSize(GetClientWidth(), GetClientHeight());
			SaveWindowedPos();
		}
		else
		{
			win_x = winx;
			win_y = winy;
			win_w = winw;
			win_h = winh;
			win_maximized = false;
			fullscreen = false;
		}
	}
}

//==========================================================================
//
// 
//
//==========================================================================

void SystemBaseFrameBuffer::PositionWindow(bool fullscreen, bool initialcall)
{
	RECT r;
	LONG style, exStyle;

	RECT monRect;

	if (!m_Fullscreen && fullscreen && !initialcall) SaveWindowedPos();
	if (m_Monitor)
	{
		MONITORINFOEXA mi;
		mi.cbSize = sizeof mi;

		if (GetMonitorInfoA(HMONITOR(m_Monitor), &mi))
		{
			strcpy(m_displayDeviceNameBuffer, mi.szDevice);
			m_displayDeviceName = m_displayDeviceNameBuffer;
			monRect = mi.rcMonitor;

			// Set the default windowed size if not specified yet.
			if (win_w < 0 || win_h < 0)
			{
				win_w = int(monRect.right - monRect.left) * 8 / 10;
				win_h = int(monRect.bottom - monRect.top) * 8 / 10;
			}
		}
	}

	ShowWindow(Window, SW_SHOW);

	GetWindowRect(Window, &r);
	style = WS_VISIBLE | WS_CLIPSIBLINGS;
	exStyle = 0;

	if (fullscreen)
		style |= WS_POPUP;
	else
	{
		style |= WS_OVERLAPPEDWINDOW;
		exStyle |= WS_EX_WINDOWEDGE;
	}

	SetWindowLong(Window, GWL_STYLE, style);
	SetWindowLong(Window, GWL_EXSTYLE, exStyle);

	if (fullscreen)
	{
		SetWindowPos(Window, 0, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
		MoveWindow(Window, monRect.left, monRect.top, monRect.right-monRect.left, monRect.bottom-monRect.top, FALSE);

		// And now, seriously, it IS in the right place. Promise.
	}
	else
	{
		RestoreWindowedPos();
		// This doesn't restore the window size properly so we must force a set size the next tic.
		if (m_Fullscreen)
		{
			::fullscreen = false;
		}

	}
	m_Fullscreen = fullscreen;
	SetSize(GetClientWidth(), GetClientHeight());
}

//==========================================================================
//
// 
//
//==========================================================================

SystemBaseFrameBuffer::SystemBaseFrameBuffer(void *hMonitor, bool fullscreen) : DFrameBuffer(vid_defwidth, vid_defheight)
{
	m_Monitor = hMonitor;
	m_displayDeviceName = 0;
	PositionWindow(fullscreen, true);

	HDC hDC = GetDC(Window);

	ReleaseDC(Window, hDC);
}

//==========================================================================
//
// 
//
//==========================================================================

SystemBaseFrameBuffer::~SystemBaseFrameBuffer()
{
	if (!m_Fullscreen) SaveWindowedPos();

	ShowWindow (Window, SW_SHOW);
	SetWindowLong(Window, GWL_STYLE, WS_VISIBLE | WS_CLIPSIBLINGS | WS_OVERLAPPEDWINDOW);
	SetWindowLong(Window, GWL_EXSTYLE, WS_EX_WINDOWEDGE);
	SetWindowPos(Window, 0, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
	I_GetEvent();

	static_cast<Win32BaseVideo *>(Video)->Shutdown();
}


//==========================================================================
//
// 
//
//==========================================================================

bool SystemBaseFrameBuffer::IsFullscreen()
{
	return m_Fullscreen;
}

//==========================================================================
//
// 
//
//==========================================================================

void SystemBaseFrameBuffer::ToggleFullscreen(bool yes)
{
	PositionWindow(yes);
}

//==========================================================================
//
// 
//
//==========================================================================

int SystemBaseFrameBuffer::GetClientWidth()
{
	RECT rect = { 0 };
	GetClientRect(Window, &rect);
	return rect.right - rect.left;
}

int SystemBaseFrameBuffer::GetClientHeight()
{
	RECT rect = { 0 };
	GetClientRect(Window, &rect);
	return rect.bottom - rect.top;
}
