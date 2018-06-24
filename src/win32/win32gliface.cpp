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
#include "win32glvideo.h"

#include "gl/renderer/gl_renderer.h"
#include "gl/system/gl_framebuffer.h"

extern HWND			Window;

extern "C" {
    __declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;	
}

PFNWGLSWAPINTERVALEXTPROC myWglSwapIntervalExtProc;

CVAR(Int, vid_adapter, 1, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)

CVAR(Int, win_x, -1, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Int, win_y, -1, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Int, win_w, -1, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Int, win_h, -1, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Bool, win_maximized, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)

// For broadest GL compatibility, require user to explicitly enable quad-buffered stereo mode.
// Setting vr_enable_quadbuffered_stereo does not automatically invoke quad-buffered stereo,
// but makes it possible for subsequent "vr_mode 7" to invoke quad-buffered stereo
CUSTOM_CVAR(Bool, vr_enable_quadbuffered, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)
{
	Printf("You must restart " GAMENAME " to switch quad stereo mode\n");
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

static void GetCenteredPos(int in_w, int in_h, int &winx, int &winy, int &winw, int &winh, int &scrwidth, int &scrheight)
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

static void KeepWindowOnScreen(int &winx, int &winy, int winw, int winh, int scrwidth, int scrheight)
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

void SystemFrameBuffer::SaveWindowedPos()
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

void SystemFrameBuffer::RestoreWindowedPos()
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

void SystemFrameBuffer::PositionWindow(bool fullscreen)
{
	RECT r;
	LONG style, exStyle;

	RECT monRect;

	if (!m_Fullscreen) SaveWindowedPos();
	if (m_Monitor)
	{
		MONITORINFOEX mi;
		mi.cbSize = sizeof mi;

		if (GetMonitorInfo(HMONITOR(m_Monitor), &mi))
		{
			strcpy(m_displayDeviceNameBuffer, mi.szDevice);
			m_displayDeviceName = m_displayDeviceNameBuffer;
			monRect = mi.rcMonitor;
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

	m_Fullscreen = fullscreen;
	if (fullscreen)
	{
		SetWindowPos(Window, 0, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
		MoveWindow(Window, monRect.left, monRect.top, monRect.right-monRect.left, monRect.bottom-monRect.top, FALSE);

		// And now, seriously, it IS in the right place. Promise.
	}
	else
	{
		RestoreWindowedPos();
	}
	SetSize(GetClientWidth(), GetClientHeight());
}

//==========================================================================
//
// 
//
//==========================================================================

SystemFrameBuffer::SystemFrameBuffer(void *hMonitor, bool fullscreen) : DFrameBuffer(vid_defwidth, vid_defheight)
{
	m_Monitor = hMonitor;
	m_displayDeviceName = 0;
	PositionWindow(fullscreen);

	if (!static_cast<Win32GLVideo *>(Video)->InitHardware(Window, 0))
	{
		I_FatalError("Unable to initialize OpenGL");
		return;
	}
	HDC hDC = GetDC(Window);
	const char *wglext = nullptr;

	myWglSwapIntervalExtProc = (PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress("wglSwapIntervalEXT");
	auto myWglGetExtensionsStringARB = (PFNWGLGETEXTENSIONSSTRINGARBPROC)wglGetProcAddress("wglGetExtensionsStringARB");
	if (myWglGetExtensionsStringARB)
	{
		wglext = myWglGetExtensionsStringARB(hDC);
	}
	else
	{
		auto myWglGetExtensionsStringEXT = (PFNWGLGETEXTENSIONSSTRINGEXTPROC)wglGetProcAddress("wglGetExtensionsStringEXT");
		if (myWglGetExtensionsStringEXT)
		{
			wglext = myWglGetExtensionsStringEXT();
		}
	}
	SwapInterval = 1; 
	if (wglext != nullptr)
	{
		if (strstr(wglext, "WGL_EXT_swap_control_tear"))
		{
			SwapInterval = -1;
		}
	}

	m_supportsGamma = !!GetDeviceGammaRamp(hDC, (void *)m_origGamma);
	ReleaseDC(Window, hDC);
    enable_quadbuffered = vr_enable_quadbuffered;
}

//==========================================================================
//
// 
//
//==========================================================================

SystemFrameBuffer::~SystemFrameBuffer()
{
	ResetGammaTable();
	SaveWindowedPos();

	ShowWindow (Window, SW_SHOW);
	SetWindowLong(Window, GWL_STYLE, WS_VISIBLE | WS_CLIPSIBLINGS | WS_OVERLAPPEDWINDOW);
	SetWindowLong(Window, GWL_EXSTYLE, WS_EX_WINDOWEDGE);
	SetWindowPos(Window, 0, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
	I_GetEvent();

	static_cast<Win32GLVideo *>(Video)->Shutdown();
}


//==========================================================================
//
// 
//
//==========================================================================

void SystemFrameBuffer::InitializeState()
{
}

//==========================================================================
//
// 
//
//==========================================================================

void SystemFrameBuffer::ResetGammaTable()
{
	if (m_supportsGamma)
	{
		HDC hDC = GetDC(Window);
		SetDeviceGammaRamp(hDC, (void *)m_origGamma);
		ReleaseDC(Window, hDC);
	}
}

void SystemFrameBuffer::SetGammaTable(uint16_t *tbl)
{
	if (m_supportsGamma)
	{
		HDC hDC = GetDC(Window);
		SetDeviceGammaRamp(hDC, (void *)tbl);
		ReleaseDC(Window, hDC);
	}
}

//==========================================================================
//
// 
//
//==========================================================================

bool SystemFrameBuffer::IsFullscreen()
{
	return m_Fullscreen;
}

//==========================================================================
//
// 
//
//==========================================================================

void SystemFrameBuffer::ToggleFullscreen(bool yes)
{
	PositionWindow(yes);
}

//==========================================================================
//
// 
//
//==========================================================================
EXTERN_CVAR(Bool, vid_vsync);
CUSTOM_CVAR(Bool, gl_control_tear, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	vid_vsync.Callback();
}

void SystemFrameBuffer::SetVSync (bool vsync)
{
	if (myWglSwapIntervalExtProc != NULL) myWglSwapIntervalExtProc(vsync ? (gl_control_tear? SwapInterval : 1) : 0);
}

void SystemFrameBuffer::SwapBuffers()
{
	// Limiting the frame rate is as simple as waiting for the timer to signal this event.
	I_FPSLimit();
	::SwapBuffers(static_cast<Win32GLVideo *>(Video)->m_hDC);
}

//==========================================================================
//
// 
//
//==========================================================================

int SystemFrameBuffer::GetClientWidth()
{
	RECT rect = { 0 };
	GetClientRect(Window, &rect);
	return rect.right - rect.left;
}

int SystemFrameBuffer::GetClientHeight()
{
	RECT rect = { 0 };
	GetClientRect(Window, &rect);
	return rect.bottom - rect.top;
}

IVideo *gl_CreateVideo()
{
	return new Win32GLVideo(0);
}
