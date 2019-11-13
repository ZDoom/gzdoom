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
#include "templates.h"
#include "i_system.h"
#include "i_input.h"
#include "hardware.h"
#include "gi.h"
#include "w_wad.h"
#include "s_sound.h"
#include "m_argv.h"
#include "d_main.h"
#include "doomerrors.h"
#include "s_music.h"

// MACROS ------------------------------------------------------------------


// How many ms elapse between blinking text flips. On a standard VGA
// adapter, the characters are on for 16 frames and then off for another 16.
// The number here therefore corresponds roughly to the blink rate on a
// 60 Hz display.
#define BLINK_PERIOD			267
#define TEXT_FONT_NAME			"vga-rom-font.16"


// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

void RestoreConView();
void LayoutMainWindow (HWND hWnd, HWND pane);
int LayoutNetStartPane (HWND pane, int w);

bool ST_Util_CreateStartupWindow ();
void ST_Util_SizeWindowForBitmap(int scale);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static INT_PTR CALLBACK NetStartPaneProc (HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern HINSTANCE g_hInst;
extern HWND Window, ConWindow, ProgressBar, NetStartPane, GameTitleWindow;

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
// FBasicStartupScreen Constructor
//
// Shows a progress bar at the bottom of the window.
//
//==========================================================================

FBasicStartupScreen::FBasicStartupScreen(int max_progress, bool show_bar)
: FStartupScreen(max_progress)
{
	/*
	if (show_bar)
	{
		ProgressBar = CreateWindowEx(0, PROGRESS_CLASS,
			NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
			0, 0, 0, 0,
			Window, 0, g_hInst, NULL);
		SendMessage (ProgressBar, PBM_SETRANGE, 0, MAKELPARAM(0,MaxPos));
		LayoutMainWindow (Window, NULL);
	}
	*/
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
	/*
	if (ProgressBar != NULL)
	{
		DestroyWindow (ProgressBar);
		ProgressBar = NULL;
		LayoutMainWindow (Window, NULL);
	}
	*/
	KillTimer(Window, 1337);
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
	if (NetStartPane == NULL)
	{
		NetStartPane = CreateDialogParam (g_hInst, MAKEINTRESOURCE(IDD_NETSTARTPANE), Window, NetStartPaneProc, 0);
		// We don't need two progress bars.
		if (ProgressBar != NULL)
		{
			DestroyWindow (ProgressBar);
			ProgressBar = NULL;
		}
		RECT winrect;
		GetWindowRect (Window, &winrect);
		SetWindowPos (Window, NULL, 0, 0,
			winrect.right - winrect.left, winrect.bottom - winrect.top + LayoutNetStartPane (NetStartPane, 0),
			SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOZORDER);
		LayoutMainWindow (Window, NULL);
		SetFocus (NetStartPane);
	}
	if (NetStartPane != NULL)
	{
		HWND ctl;

		std::wstring wmessage = WideString(message);
		SetDlgItemTextW (NetStartPane, IDC_NETSTARTMESSAGE, wmessage.c_str());
		ctl = GetDlgItem (NetStartPane, IDC_NETSTARTPROGRESS);

		if (numplayers == 0)
		{
			// PBM_SETMARQUEE is only available under XP and above, so this might fail.
			NetMarqueeMode = SendMessage (ctl, PBM_SETMARQUEE, TRUE, 100);
			if (NetMarqueeMode == FALSE)
			{
				SendMessage (ctl, PBM_SETRANGE, 0, MAKELPARAM(0,16));
			}
			else
			{
				// If we don't set the PBS_MARQUEE style, then the marquee will never show up.
				SetWindowLong (ctl, GWL_STYLE, GetWindowLong (ctl, GWL_STYLE) | PBS_MARQUEE);
			}
			SetDlgItemTextW (NetStartPane, IDC_NETSTARTCOUNT, L"");
		}
		else
		{
			NetMarqueeMode = FALSE;
			SendMessage (ctl, PBM_SETMARQUEE, FALSE, 0);
			// Make sure the marquee really is turned off.
			SetWindowLong (ctl, GWL_STYLE, GetWindowLong (ctl, GWL_STYLE) & (~PBS_MARQUEE));

			SendMessage (ctl, PBM_SETRANGE, 0, MAKELPARAM(0,numplayers));
			if (numplayers == 1)
			{
				SendMessage (ctl, PBM_SETPOS, 1, 0);
				SetDlgItemTextW (NetStartPane, IDC_NETSTARTCOUNT, L"");
			}
		}
	}
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
	if (NetStartPane != NULL)
	{
		DestroyWindow (NetStartPane);
		NetStartPane = NULL;
		LayoutMainWindow (Window, NULL);
	}
}

//==========================================================================
//
// FBasicStartupScreen :: NetMessage
//
// Call this between NetInit() and NetDone() instead of Printf() to
// display messages, in case the progress meter is mixed in the same output
// stream as normal messages.
//
//==========================================================================

void FBasicStartupScreen::NetMessage(const char *format, ...)
{
	FString str;
	va_list argptr;
	
	va_start (argptr, format);
	str.VFormat (format, argptr);
	va_end (argptr);
	Printf ("%s\n", str.GetChars());
}

//==========================================================================
//
// FBasicStartupScreen :: NetProgress
//
// Sets the network progress meter. If count is 0, it gets bumped by 1.
// Otherwise, it is set to count.
//
//==========================================================================

void FBasicStartupScreen :: NetProgress(int count)
{
	if (count == 0)
	{
		NetCurPos++;
	}
	else
	{
		NetCurPos = count;
	}
	if (NetStartPane == NULL)
	{
		return;
	}
	if (NetMaxPos == 0 && !NetMarqueeMode)
	{
		// PBM_SETMARQUEE didn't work, so just increment the progress bar endlessly.
		SendDlgItemMessage (NetStartPane, IDC_NETSTARTPROGRESS, PBM_SETPOS, NetCurPos & 15, 0);
	}
	else if (NetMaxPos > 1)
	{
		char buf[16];

		mysnprintf (buf, countof(buf), "%d/%d", NetCurPos, NetMaxPos);
		SetDlgItemTextA (NetStartPane, IDC_NETSTARTCOUNT, buf);
		SendDlgItemMessage (NetStartPane, IDC_NETSTARTPROGRESS, PBM_SETPOS, MIN(NetCurPos, NetMaxPos), 0);
	}
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
	BOOL bRet;
	MSG msg;

	if (SetTimer (Window, 1337, 500, NULL) == 0)
	{
		I_FatalError ("Could not set network synchronization timer.");
	}

	while ((bRet = GetMessage(&msg, NULL, 0, 0)) != 0)
	{
		if (bRet == -1)
		{
			KillTimer (Window, 1337);
			return false;
		}
		else
		{
			if (msg.message == WM_TIMER && msg.hwnd == Window && msg.wParam == 1337)
			{
				if (timer_callback (userdata))
				{
					KillTimer (NetStartPane, 1);
					return true;
				}
			}
			if (!IsDialogMessage (NetStartPane, &msg))
			{
				TranslateMessage (&msg);
				DispatchMessage (&msg);
			}
		}
	}
	KillTimer (Window, 1337);
	return false;
}

//==========================================================================
//
// NetStartPaneProc
//
// DialogProc for the network startup pane. It just waits for somebody to
// click a button, and the only button available is the abort one.
//
//==========================================================================

static INT_PTR CALLBACK NetStartPaneProc (HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (msg == WM_COMMAND && HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == IDCANCEL)
	{
		PostQuitMessage (0);
		return TRUE;
	}
	return FALSE;
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
}

//==========================================================================
//
// FGraphicalStartupScreen Destructor
//
//==========================================================================

FGraphicalStartupScreen::~FGraphicalStartupScreen()
{
	if (StartupBitmap != NULL)
	{
		ST_Util_FreeBitmap (StartupBitmap);
		StartupBitmap = NULL;
	}
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
	if (showendoom == 0 || gameinfo.Endoom.Len() == 0) 
	{
		return 0;
	}

	int endoom_lump = Wads.CheckNumForFullName (gameinfo.Endoom, true);

	uint8_t endoom_screen[4000];
	MSG mess;
	BOOL bRet;
	bool blinking = false, blinkstate = false;
	int i;

	if (endoom_lump < 0 || Wads.LumpLength (endoom_lump) != 4000)
	{
		return 0;
	}

	if (Wads.GetLumpFile(endoom_lump) == Wads.GetMaxIwadNum() && showendoom == 2)
	{
		// showendoom==2 means to show only lumps from PWADs.
		return 0;
	}

	/*
	if (!ST_Util_CreateStartupWindow())
	{
		return 0;
	}
	*/

	I_ShutdownGraphics ();
	RestoreConView ();
	S_StopMusic(true);

	Wads.ReadLump (endoom_lump, endoom_screen);

	// Draw the loading screen to a bitmap.
	StartupBitmap = ST_Util_AllocTextBitmap ();
	ST_Util_DrawTextScreen (StartupBitmap, endoom_screen);

	// Make the title banner go away.
	if (GameTitleWindow != NULL)
	{
		DestroyWindow (GameTitleWindow);
		GameTitleWindow = NULL;
	}

	ST_Util_SizeWindowForBitmap (1);
	LayoutMainWindow (Window, NULL);
	//InvalidateRect (StartupScreen, NULL, TRUE);

	// Does this screen need blinking?
	for (i = 0; i < 80*25; ++i)
	{
		if (endoom_screen[1+i*2] & 0x80)
		{
			blinking = true;
			break;
		}
	}
	if (blinking && SetTimer (Window, 0x5A15A, BLINK_PERIOD, NULL) == 0)
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
				KillTimer (Window, 0x5A15A);
			}
			ST_Util_FreeBitmap (StartupBitmap);
			return int(bRet == 0 ? mess.wParam : 0);
		}
		else if (blinking && mess.message == WM_TIMER && mess.hwnd == Window && mess.wParam == 0x5A15A)
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
	int code = RunEndoom();
	throw CExitEvent(code);

}

