/*
** st_start.cpp
** Handles the startup screen.
**
**---------------------------------------------------------------------------
** Copyright 2006-2007 Randy Heit
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

// HEADER FILES ------------------------------------------------------------

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include "resource.h"

#include "st_start.h"
#include "cmdlib.h"

#include "i_system.h"
#include "i_input.h"
#include "hardware.h"
#include "filesystem.h"
#include "m_argv.h"
#include "engineerrors.h"
#include "s_music.h"
#include "printf.h"
#include "startupinfo.h"
#include "i_interface.h"
#include "texturemanager.h"
#include "i_mainwindow.h"

// MACROS ------------------------------------------------------------------

// How many ms elapse between blinking text flips. On a standard VGA
// adapter, the characters are on for 16 frames and then off for another 16.
// The number here therefore corresponds roughly to the blink rate on a
// 60 Hz display.
#define BLINK_PERIOD			267


// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

void ST_Util_SizeWindowForBitmap (int scale);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern HINSTANCE g_hInst;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

FStartupScreen *StartScreen;

CUSTOM_CVAR(Int, showendoom, 0, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (self < 0) self = 0;
	else if (self > 2) self=2;
}

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

//==========================================================================
//
// FStartupScreen :: CreateInstance
//
// Initializes the startup screen for the detected game.
// Sets the size of the progress bar and displays the startup screen.
//
//==========================================================================

FStartupScreen *FStartupScreen::CreateInstance(int max_progress)
{
	FStartupScreen *scr = NULL;
	HRESULT hr = E_FAIL;

	if (!Args->CheckParm("-nostartup"))
	{
		if (GameStartupInfo.Type == FStartupInfo::HexenStartup)
		{
			scr = new FHexenStartupScreen(max_progress, hr);
		}
		else if (GameStartupInfo.Type == FStartupInfo::HereticStartup)
		{
			scr = new FHereticStartupScreen(max_progress, hr);
		}
		else if (GameStartupInfo.Type == FStartupInfo::StrifeStartup)
		{
			scr = new FStrifeStartupScreen(max_progress, hr);
		}
		if (scr != NULL && FAILED(hr))
		{
			delete scr;
			scr = NULL;
		}
	}
	if (scr == NULL)
	{
		scr = new FBasicStartupScreen(max_progress, true);
	}
	return scr;
}

//==========================================================================
//
// FBasicStartupScreen Constructor
//
// Shows a progress bar at the bottom of the window.
//
//==========================================================================

FBasicStartupScreen::FBasicStartupScreen(int max_progress, bool show_bar)
: FStartupScreen(max_progress)
{
	if (show_bar)
	{
		mainwindow.ShowProgressBar(MaxPos);
	}
	NetMaxPos = 0;
	NetCurPos = 0;
}

//==========================================================================
//
// FBasicStartupScreen Destructor
//
// Called just before entering graphics mode to deconstruct the startup
// screen.
//
//==========================================================================

FBasicStartupScreen::~FBasicStartupScreen()
{
	mainwindow.HideProgressBar();
	KillTimer(mainwindow.GetHandle(), 1337);
}

//==========================================================================
//
// FBasicStartupScreen :: Progress
//
// Bumps the progress meter one notch.
//
//==========================================================================

void FBasicStartupScreen::Progress()
{
	if (CurPos < MaxPos)
	{
		CurPos++;
		mainwindow.SetProgressPos(CurPos);
	}
}

//==========================================================================
//
// FBasicStartupScreen :: NetInit
//
// Shows the network startup pane if it isn't visible. Sets the message in
// the pane to the one provided. If numplayers is 0, then the progress bar
// is a scrolling marquee style. If numplayers is 1, then the progress bar
// is just a full bar. If numplayers is >= 2, then the progress bar is a
// normal one, and a progress count is also shown in the pane.
//
//==========================================================================

void FBasicStartupScreen::NetInit(const char *message, int numplayers)
{
	NetMaxPos = numplayers;
	mainwindow.ShowNetStartPane(message, numplayers);

	NetMaxPos = numplayers;
	NetCurPos = 0;
	NetProgress(1);	// You always know about yourself
}

//==========================================================================
//
// FBasicStartupScreen :: NetDone
//
// Removes the network startup pane.
//
//==========================================================================

void FBasicStartupScreen::NetDone()
{
	mainwindow.HideNetStartPane();
}

//==========================================================================
//
// FBasicStartupScreen :: NetProgress
//
// Sets the network progress meter. If count is 0, it gets bumped by 1.
// Otherwise, it is set to count.
//
//==========================================================================

void FBasicStartupScreen::NetProgress(int count)
{
	if (count == 0)
	{
		NetCurPos++;
	}
	else
	{
		NetCurPos = count;
	}

	mainwindow.SetNetStartProgress(count);
}

//==========================================================================
//
// FBasicStartupScreen :: NetLoop
//
// The timer_callback function is called at least two times per second
// and passed the userdata value. It should return true to stop the loop and
// return control to the caller or false to continue the loop.
//
// ST_NetLoop will return true if the loop was halted by the callback and
// false if the loop was halted because the user wants to abort the
// network synchronization.
//
//==========================================================================

bool FBasicStartupScreen::NetLoop(bool (*timer_callback)(void *), void *userdata)
{
	return mainwindow.RunMessageLoop(timer_callback, userdata);
}

//==========================================================================
//
// FGraphicalStartupScreen Constructor
//
// This doesn't really do anything. The subclass is responsible for
// creating the resources that will be freed by this class's destructor.
//
//==========================================================================

FGraphicalStartupScreen::FGraphicalStartupScreen(int max_progress)
: FBasicStartupScreen(max_progress, false)
{
	mainwindow.ShowStartupScreen();
}

//==========================================================================
//
// FGraphicalStartupScreen Destructor
//
//==========================================================================

FGraphicalStartupScreen::~FGraphicalStartupScreen()
{
	mainwindow.HideStartupScreen();
	if (StartupBitmap != NULL)
	{
		ST_Util_FreeBitmap (StartupBitmap);
		StartupBitmap = NULL;
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FHexenStartupScreen::SetWindowSize()
{
	ST_Util_SizeWindowForBitmap(1);
	mainwindow.UpdateLayout();
	mainwindow.InvalidateStartupScreen();
}

//==========================================================================
//
//
//
//==========================================================================

void FHereticStartupScreen::SetWindowSize()
{
	ST_Util_SizeWindowForBitmap(1);
	mainwindow.UpdateLayout();
	mainwindow.InvalidateStartupScreen();
}

//==========================================================================
//
//
//
//==========================================================================

void FStrifeStartupScreen::SetWindowSize()
{
	ST_Util_SizeWindowForBitmap(2);
	mainwindow.UpdateLayout();
	mainwindow.InvalidateStartupScreen();
}

//==========================================================================
//
// ST_Endoom
//
// Shows an ENDOOM text screen
//
//==========================================================================

int RunEndoom()
{
	if (showendoom == 0 || endoomName.Len() == 0) 
	{
		return 0;
	}

	int endoom_lump = fileSystem.CheckNumForFullName (endoomName, true);

	uint8_t endoom_screen[4000];
	MSG mess;
	BOOL bRet;
	bool blinking = false, blinkstate = false;
	int i;

	if (endoom_lump < 0 || fileSystem.FileLength (endoom_lump) != 4000)
	{
		return 0;
	}

	if (fileSystem.GetFileContainer(endoom_lump) == fileSystem.GetMaxIwadNum() && showendoom == 2)
	{
		// showendoom==2 means to show only lumps from PWADs.
		return 0;
	}

	mainwindow.ShowStartupScreen();

	I_ShutdownGraphics ();
	mainwindow.RestoreConView ();
	S_StopMusic(true);

	fileSystem.ReadFile (endoom_lump, endoom_screen);

	// Draw the loading screen to a bitmap.
	StartupBitmap = ST_Util_AllocTextBitmap();
	ST_Util_DrawTextScreen (StartupBitmap, endoom_screen);

	// Make the title banner go away.
	mainwindow.HideGameTitleWindow();

	ST_Util_SizeWindowForBitmap (1);
	mainwindow.UpdateLayout();
	mainwindow.InvalidateStartupScreen();

	// Does this screen need blinking?
	for (i = 0; i < 80*25; ++i)
	{
		if (endoom_screen[1+i*2] & 0x80)
		{
			blinking = true;
			break;
		}
	}
	if (blinking && SetTimer (mainwindow.GetHandle(), 0x5A15A, BLINK_PERIOD, NULL) == 0)
	{
		blinking = false;
	}

	// Wait until any key has been pressed or a quit message has been received
	for (;;)
	{
		bRet = GetMessage (&mess, NULL, 0, 0);
		if (bRet == 0 || bRet == -1 ||	// bRet == 0 means we received WM_QUIT
			mess.message == WM_KEYDOWN || mess.message == WM_SYSKEYDOWN || mess.message == WM_LBUTTONDOWN)
		{
			if (blinking)
			{
				KillTimer (mainwindow.GetHandle(), 0x5A15A);
			}
			ST_Util_FreeBitmap (StartupBitmap);
			return int(bRet == 0 ? mess.wParam : 0);
		}
		else if (blinking && mess.message == WM_TIMER && mess.hwnd == mainwindow.GetHandle() && mess.wParam == 0x5A15A)
		{
			ST_Util_UpdateTextBlink (StartupBitmap, endoom_screen, blinkstate);
			blinkstate = !blinkstate;
		}
		TranslateMessage (&mess);
		DispatchMessage (&mess);
	}
}

void ST_Endoom()
{
	TexMan.DeleteAll();
	int code = RunEndoom();
	throw CExitEvent(code);

}

//==========================================================================
//
// ST_Util_SizeWindowForBitmap
//
// Resizes the main window so that the startup bitmap will be drawn
// at the desired scale.
//
//==========================================================================

void ST_Util_SizeWindowForBitmap (int scale)
{
	DEVMODE displaysettings;
	int w, h, cx, cy, x, y;
	RECT rect;

	RECT sizerect = { 0, 0, StartupBitmap->bmiHeader.biWidth * scale, StartupBitmap->bmiHeader.biHeight * scale + mainwindow.GetGameTitleWindowHeight() };
	AdjustWindowRectEx(&sizerect, WS_VISIBLE|WS_OVERLAPPEDWINDOW, FALSE, WS_EX_APPWINDOW);
	w = sizerect.right - sizerect.left;
	h = sizerect.bottom - sizerect.top;

	// Resize the window, but keep its center point the same, unless that
	// puts it partially offscreen.
	memset (&displaysettings, 0, sizeof(displaysettings));
	displaysettings.dmSize = sizeof(displaysettings);
	EnumDisplaySettings (NULL, ENUM_CURRENT_SETTINGS, &displaysettings);
	GetWindowRect (mainwindow.GetHandle(), &rect);
	cx = (rect.left + rect.right) / 2;
	cy = (rect.top + rect.bottom) / 2;
	x = cx - w / 2;
	y = cy - h / 2;
	if (x + w > (int)displaysettings.dmPelsWidth)
	{
		x = displaysettings.dmPelsWidth - w;
	}
	if (x < 0)
	{
		x = 0;
	}
	if (y + h > (int)displaysettings.dmPelsHeight)
	{
		y = displaysettings.dmPelsHeight - h;
	}
	if (y < 0)
	{
		y = 0;
	}
	MoveWindow (mainwindow.GetHandle(), x, y, w, h, TRUE);
}

//==========================================================================
//
// ST_Util_InvalidateRect
//
// Invalidates the portion of the window that the specified rect of the
// bitmap appears in.
//
//==========================================================================

void ST_Util_InvalidateRect(BitmapInfo* bitmap_info, int left, int top, int right, int bottom)
{
	mainwindow.InvalidateStartupScreen(left, top, right, bottom);
}
