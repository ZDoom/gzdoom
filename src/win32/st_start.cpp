/*
** st_start.cpp
** Handles the startup screen.
**
**---------------------------------------------------------------------------
** Copyright 2006 Randy Heit
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
#define _WIN32_WINNT 0x0501	// required to get the MARQUEE defines
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>

#define USE_WINDOWS_DWORD
#include "st_start.h"
#include "resource.h"
#include "templates.h"
#include "i_system.h"

extern HINSTANCE g_hInst;
extern HWND Window, ConWindow, ProgressBar, NetStartPane;
extern void LayoutMainWindow (HWND hWnd, HWND pane);

INT_PTR CALLBACK NetStartPaneProc (HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam);

int MaxPos, CurPos;
int NetMaxPos, NetCurPos;
LRESULT NetMarqueeMode;

void ST_Init(int maxProgress)
{
	ProgressBar = CreateWindowEx(0, PROGRESS_CLASS,
		NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
		0, 0, 0, 0,
		Window, 0, g_hInst, NULL);
	SendMessage (ProgressBar, PBM_SETRANGE, 0, MAKELPARAM(0,maxProgress));
	LayoutMainWindow (Window, NULL);
	MaxPos = maxProgress;
	CurPos = 0;
}

void ST_Done()
{
	if (ProgressBar != NULL)
	{
		DestroyWindow (ProgressBar);
		ProgressBar = NULL;
		LayoutMainWindow (Window, NULL);
	}
}

void ST_Progress()
{
	if (CurPos < MaxPos)
	{
		CurPos++;
		SendMessage (ProgressBar, PBM_SETPOS, CurPos, 0);
	}
}

//===========================================================================
//
// ST_NetInit
//
// Shows the network startup pane if it isn't visible. Sets the message in
// the pane to the one provided. If numplayers is 0, then the progress bar
// is a scrolling marquee style. If numplayers is 1, then the progress bar
// is just a full bar. If numplayers is >= 2, then the progress bar is a
// normal one, and a progress count is also shown in the pane.
//
//===========================================================================

void ST_NetInit(const char *message, int numplayers)
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
		LayoutMainWindow (Window, NULL);
		// Make sure the last line of output is visible in the log window.
		SendMessage (ConWindow, EM_LINESCROLL, 0, SendMessage (ConWindow, EM_GETLINECOUNT, 0, 0) -
			SendMessage (ConWindow, EM_GETFIRSTVISIBLELINE, 0, 0));
	}
	if (NetStartPane != NULL)
	{
		HWND ctl;

		SetDlgItemText (NetStartPane, IDC_NETSTARTMESSAGE, message);
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
			SetDlgItemText (NetStartPane, IDC_NETSTARTCOUNT, "");
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
				SetDlgItemText (NetStartPane, IDC_NETSTARTCOUNT, "");
			}
		}
	}
	NetMaxPos = numplayers;
	NetCurPos = 0;
	ST_NetProgress(1);	// You always know about yourself
}

void ST_NetDone()
{
	if (NetStartPane != NULL)
	{
		DestroyWindow (NetStartPane);
		NetStartPane = NULL;
		LayoutMainWindow (Window, NULL);
	}
}

void ST_NetProgress(int count)
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

		sprintf (buf, "%d/%d", NetCurPos, NetMaxPos);
		SetDlgItemText (NetStartPane, IDC_NETSTARTCOUNT, buf);
		SendDlgItemMessage (NetStartPane, IDC_NETSTARTPROGRESS, PBM_SETPOS, MIN(NetCurPos, NetMaxPos), 0);
	}
}

//===========================================================================
//
// ST_NetLoop
//
// The timer_callback function is called approximately two times per second
// and passed the userdata value. It should return true to stop the loop and
// return control to the caller or false to continue the loop.
//
// ST_NetLoop will return true if the loop was halted by the callback and
// false if the loop was halted because the user wants to abort the
// network synchronization.
//
//===========================================================================

bool ST_NetLoop(bool (*timer_callback)(void *), void *userdata)
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

INT_PTR CALLBACK NetStartPaneProc (HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (msg == WM_COMMAND && HIWORD(wParam) == BN_CLICKED)
	{
		PostQuitMessage (0);
		return TRUE;
	}
	return FALSE;
}
