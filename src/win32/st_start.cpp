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
#define _WIN32_WINNT 0x0501	// required to get the MARQUEE defines
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include "resource.h"

#define USE_WINDOWS_DWORD
#include "st_start.h"
#include "resource.h"
#include "templates.h"
#include "i_system.h"
#include "i_input.h"
#include "gi.h"
#include "w_wad.h"
#include "s_sound.h"

// MACROS ------------------------------------------------------------------

#define ST_MAX_NOTCHES		32
#define ST_NOTCH_WIDTH		16
#define ST_NOTCH_HEIGHT		23
#define ST_PROGRESS_X		64			// Start of notches x screen pos.
#define ST_PROGRESS_Y		441			// Start of notches y screen pos.

#define ST_NETPROGRESS_X		288
#define ST_NETPROGRESS_Y		32
#define ST_NETNOTCH_WIDTH		4
#define ST_NETNOTCH_HEIGHT		16
#define ST_MAX_NETNOTCHES		8

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

extern void ST_Util_PlanarToChunky4 (BYTE *dest, const BYTE *src, int width, int height);
extern void ST_Util_DrawBlock (BYTE *dest, const BYTE *src, int bytewidth, int height, int destpitch);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static INT_PTR CALLBACK NetStartPaneProc (HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam);

static void ST_Basic_Init ();
static void ST_Basic_Done ();
static void ST_Basic_Progress ();
static void ST_Basic_NetInit (const char *message, int numplayers);
static void ST_Basic_NetProgress (int count);
static void ST_Basic_NetMessage (const char *format, ...);
static void ST_Basic_NetDone ();
static bool ST_Basic_NetLoop (bool (*timer_callback)(void *), void *userdata);

static void ST_Hexen_Init ();
static void ST_Hexen_Done ();
static void ST_Hexen_Progress ();

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern HINSTANCE g_hInst;
extern HWND Window, ConWindow, ProgressBar, NetStartPane, StartupScreen;
extern void LayoutMainWindow (HWND hWnd, HWND pane);

// PUBLIC DATA DEFINITIONS -------------------------------------------------

void (*ST_Done)();
void (*ST_Progress)();
void (*ST_NetInit)(const char *message, int numplayers);
void (*ST_NetProgress)(int count);
void (*ST_NetMessage)(const char *format, ...);
void (*ST_NetDone)();
bool (*ST_NetLoop)(bool (*timer_callback)(void *), void *userdata);

struct
{
	BITMAPINFOHEADER Header;
	RGBQUAD			 Colors[16];
} StartupBitmapInfo;
BYTE *StartupBitmapBits;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int MaxPos, CurPos, NotchPos;
static int NetMaxPos, NetCurPos;
static LRESULT NetMarqueeMode;
static RGBQUAD StartupPalette[16];

// Hexen's notch graphics, converted to chunky pixels.

static const BYTE NotchBits[] =
{
	0x00, 0x00, 0x00, 0x00, 0x70, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x07, 0x70, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x0c, 0xc0, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x08, 0x80, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x68, 0x86, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x78, 0x87, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0xd8, 0x8d, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0xd8, 0x8d, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0xd8, 0x8d, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0xd8, 0x8d, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0xd8, 0x87, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0xd7, 0x7d, 0x60, 0x00, 0x00,
	0x00, 0x66, 0x99, 0x99, 0x96, 0x69, 0x66, 0x00,
	0x00, 0x69, 0x96, 0x99, 0x69, 0x96, 0x96, 0x00,
	0x06, 0x9d, 0x99, 0x69, 0x96, 0xd9, 0x79, 0x60,
	0x06, 0x7d, 0xdd, 0xdd, 0xdd, 0xdd, 0x77, 0x60,
	0x06, 0x78, 0x88, 0x88, 0x88, 0x88, 0xd6, 0x60,
	0x06, 0x7a, 0xaa, 0xaa, 0xaa, 0xaa, 0xd6, 0x60,
	0x06, 0x7a, 0x77, 0x77, 0x77, 0xa7, 0x96, 0x60,
	0x06, 0x77, 0xa7, 0x77, 0x77, 0xa7, 0x96, 0x60,
	0x06, 0x97, 0xa7, 0x79, 0x77, 0x77, 0x96, 0x60,
	0x00, 0x67, 0x79, 0x99, 0x99, 0xd7, 0x96, 0x60,
	0x00, 0x69, 0x99, 0x66, 0x69, 0x69, 0x66, 0x00
};

static const BYTE NetNotchBits[] =
{
	0x52, 0x20,
	0x23, 0x25,
	0x33, 0x25,
	0x31, 0x35,
	0x31, 0x35,
	0x31, 0x35,
	0x33, 0x35,
	0x31, 0x35,
	0x31, 0x35,
	0x31, 0x25,
	0x33, 0x35,
	0x31, 0x20,
	0x21, 0x35,
	0x23, 0x25,
	0x52, 0x20,
	0x05, 0x50
};

// CODE --------------------------------------------------------------------

//==========================================================================
//
// ST_Init
//
// Initializes the startup screen for the detected game.
// Sets the size of the progress bar and displays the startup screen.
//
//==========================================================================

void ST_Init(int maxProgress)
{
	MaxPos = maxProgress;
	CurPos = 0;
	NotchPos = 0;

	if (gameinfo.gametype == GAME_Hexen)
	{
		ST_Hexen_Init ();
	}
	else
	{
		ST_Basic_Init ();
	}
}

//==========================================================================
//
// ST_Basic_Init
//
// Shows a progress bar at the bottom of the window.
//
//==========================================================================

static void ST_Basic_Init ()
{
	ProgressBar = CreateWindowEx(0, PROGRESS_CLASS,
		NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
		0, 0, 0, 0,
		Window, 0, g_hInst, NULL);
	SendMessage (ProgressBar, PBM_SETRANGE, 0, MAKELPARAM(0,MaxPos));
	LayoutMainWindow (Window, NULL);

	ST_Done = ST_Basic_Done;
	ST_Progress = ST_Basic_Progress;
	ST_NetInit = ST_Basic_NetInit;
	ST_NetProgress = ST_Basic_NetProgress;
	ST_NetMessage = ST_Basic_NetMessage;
	ST_NetDone = ST_Basic_NetDone;
	ST_NetLoop = ST_Basic_NetLoop;
}

//==========================================================================
//
// ST_Basic_Done
//
// Called just before entering graphics mode to deconstruct the startup
// screen.
//
//==========================================================================

static void ST_Basic_Done()
{
	if (ProgressBar != NULL)
	{
		DestroyWindow (ProgressBar);
		ProgressBar = NULL;
		LayoutMainWindow (Window, NULL);
	}
}

//==========================================================================
//
// ST_Basic_Progress
//
// Bumps the progress meter one notch.
//
//==========================================================================

static void ST_Basic_Progress()
{
	if (CurPos < MaxPos)
	{
		CurPos++;
		SendMessage (ProgressBar, PBM_SETPOS, CurPos, 0);
	}
}

//==========================================================================
//
// ST_Basic_NetInit
//
// Shows the network startup pane if it isn't visible. Sets the message in
// the pane to the one provided. If numplayers is 0, then the progress bar
// is a scrolling marquee style. If numplayers is 1, then the progress bar
// is just a full bar. If numplayers is >= 2, then the progress bar is a
// normal one, and a progress count is also shown in the pane.
//
//==========================================================================

static void ST_Basic_NetInit(const char *message, int numplayers)
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

//==========================================================================
//
// ST_Basic_NetDone
//
// Removes the network startup pane.
//
//==========================================================================

static void ST_Basic_NetDone()
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
// ST_Basic_NetMessage
//
// Call this between ST_NetInit() and ST_NetDone() instead of Printf() to
// display messages, in case the progress meter is mixed in the same output
// stream as normal messages.
//
//==========================================================================

static void ST_Basic_NetMessage(const char *format, ...)
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
// ST_Basic_NetProgress
//
// Sets the network progress meter. If count is 0, it gets bumped by 1.
// Otherwise, it is set to count.
//
//==========================================================================

static void ST_Basic_NetProgress(int count)
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

//==========================================================================
//
// ST_Basic_NetLoop
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

static bool ST_Basic_NetLoop(bool (*timer_callback)(void *), void *userdata)
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
	if (msg == WM_COMMAND && HIWORD(wParam) == BN_CLICKED)
	{
		PostQuitMessage (0);
		return TRUE;
	}
	return FALSE;
}

//==========================================================================
//
// ST_Hexen_Init
//
// Shows the Hexen startup screen. If the screen doesn't appear to be
// valid, it falls back to ST_Basic_Init.
//
// The startup graphic is a planar, 4-bit 640x480 graphic preceded by a
// 16 entry (48 byte) VGA palette.
//
//==========================================================================

static void ST_Hexen_Init ()
{
	int startup_lump = Wads.CheckNumForName ("STARTUP");

	if (startup_lump < 0 || Wads.LumpLength (startup_lump) != 153648)
	{
		ST_Basic_Init ();
		return;
	}

	StartupScreen = CreateWindowEx (WS_EX_NOPARENTNOTIFY, "STATIC", NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | SS_OWNERDRAW, 0, 0, 0, 0, Window, NULL, g_hInst, NULL);
	if (StartupScreen == NULL)
	{
		ST_Basic_Init ();
		return;
	}
	SetWindowLong (StartupScreen, GWL_ID, IDC_STATIC_STARTUP);

	BYTE startup_screen[153648];
	union
	{
		RGBQUAD color;
		DWORD	quad;
	};

	Wads.ReadLump (startup_lump, startup_screen);

	color.rgbReserved = 0;

	// Initialize the bitmap info header.
	StartupBitmapInfo.Header.biSize = sizeof(BITMAPINFOHEADER);
	StartupBitmapInfo.Header.biWidth = 640;
	StartupBitmapInfo.Header.biHeight = 480;
	StartupBitmapInfo.Header.biPlanes = 1;
	StartupBitmapInfo.Header.biBitCount = 4;
	StartupBitmapInfo.Header.biCompression = 0;
	StartupBitmapInfo.Header.biSizeImage = 153600;
	StartupBitmapInfo.Header.biXPelsPerMeter = 0;
	StartupBitmapInfo.Header.biYPelsPerMeter = 0;
	StartupBitmapInfo.Header.biClrUsed = 16;
	StartupBitmapInfo.Header.biClrImportant = 16;

	// Initialize the bitmap header.
	for (int i = 0; i < 16; ++i)
	{
		color.rgbRed = startup_screen[i*3+0];
		color.rgbGreen = startup_screen[i*3+1];
		color.rgbBlue = startup_screen[i*3+2];
		// Convert from 6-bit per component to 8-bit per component.
		quad = (quad << 2) | ((quad >> 4) & 0x03030303);
		StartupBitmapInfo.Colors[i] = color;
	}

	// Fill in the bitmap data. Convert to chunky, because I can't figure out
	// if Windows actually supports planar images or not, despite the presence
	// of biPlanes in the BITMAPINFOHEADER.
	StartupBitmapBits = new BYTE[640*480/2];
	ST_Util_PlanarToChunky4 (StartupBitmapBits, startup_screen + 48, 640, 480);

	LayoutMainWindow (Window, NULL);
	InvalidateRect (StartupScreen, NULL, TRUE);

	ST_Done = ST_Hexen_Done;
	ST_Progress = ST_Hexen_Progress;
	ST_NetInit = ST_Basic_NetInit;
	ST_NetProgress = ST_Basic_NetProgress;
	ST_NetMessage = ST_Basic_NetMessage;
	ST_NetDone = ST_Basic_NetDone;
	ST_NetLoop = ST_Basic_NetLoop;

	// Not that this screen will be around long enough for anyone to
	// really hear the music, but start it anyway.
	S_ChangeMusic ("orb", true, true);
}

//==========================================================================
//
// ST_Hexen_Done
//
// Called just before entering graphics mode to deconstruct the startup
// screen.
//
//==========================================================================

static void ST_Hexen_Done()
{
	if (StartupScreen != NULL)
	{
		DestroyWindow (StartupScreen);
		StartupScreen = NULL;
	}
}

//==========================================================================
//
// ST_Hexen_Progress
//
// Bumps the progress meter one notch.
//
//==========================================================================

static void ST_Hexen_Progress()
{
	int notch_pos, x, y;
	RECT rect;

	if (CurPos < MaxPos)
	{
		CurPos++;
		notch_pos = (CurPos * ST_MAX_NOTCHES) / MaxPos;
		if (notch_pos != NotchPos)
		{ // Time to draw another notch.
			for (; NotchPos < notch_pos; NotchPos++)
			{
				x = ST_PROGRESS_X + ST_NOTCH_WIDTH * NotchPos;
				y = ST_PROGRESS_Y;
				ST_Util_DrawBlock (StartupBitmapBits + x / 2 + y * 320, NotchBits, ST_NOTCH_WIDTH / 2, ST_NOTCH_HEIGHT, 320);
				GetClientRect (StartupScreen, &rect);
				rect.left = x * rect.right / 640 - 1;
				rect.top = y * rect.bottom / 480 - 1;
				rect.right = (x + ST_NOTCH_WIDTH) * rect.right / 640 + 1;
				rect.bottom = (y + ST_NOTCH_HEIGHT) * rect.bottom / 480 + 1;
				InvalidateRect (StartupScreen, &rect, FALSE);
			}
			S_Sound (CHAN_BODY, "StartupTick", 1, ATTN_NONE);
		}
	}
	I_GetEvent ();
}

//==========================================================================
//
// ST_Util_PlanarToChunky4
//
// Convert a 4-bpp planar image to chunky pixels.
//
//==========================================================================

void ST_Util_PlanarToChunky4 (BYTE *dest, const BYTE *src, int width, int height)
{
	int y, x;
	const BYTE *src1, *src2, *src3, *src4;
	size_t plane_size = width / 8 * height;

	src1 = src;
	src2 = src1 + plane_size;
	src3 = src2 + plane_size;
	src4 = src3 + plane_size;

	for (y = height; y > 0; --y)
	{
		for (x = width; x > 0; x -= 8)
		{
			// Pixels 0 and 1
			dest[0] = (*src4 & 0x80) | ((*src3 & 0x80) >> 1) | ((*src2 & 0x80) >> 2) | ((*src1 & 0x80) >> 3) |
					  ((*src4 & 0x40) >> 3) | ((*src3 & 0x40) >> 4) | ((*src2 & 0x40) >> 5) | ((*src1 & 0x40) >> 6);
			// Pixels 2 and 3
			dest[1] = ((*src4 & 0x20) << 2) | ((*src3 & 0x20) << 1) | ((*src2 & 0x20)) | ((*src1 & 0x20) >> 1) |
					  ((*src4 & 0x10) >> 1) | ((*src3 & 0x10) >> 2) | ((*src2 & 0x10) >> 3) | ((*src1 & 0x10) >> 4);
			// Pixels 4 and 5
			dest[2] = ((*src4 & 0x08) << 4) | ((*src3 & 0x08) << 3) | ((*src2 & 0x08) << 2) | ((*src1 & 0x08) << 1) |
					  ((*src4 & 0x04) << 1) | ((*src3 & 0x04)) | ((*src2 & 0x04) >> 1) | ((*src1 & 0x04) >> 2);
			// Pixels 6 and 7
			dest[3] = ((*src4 & 0x02) << 6) | ((*src3 & 0x02) << 5) | ((*src2 & 0x02) << 4) | ((*src1 & 0x02) << 3) |
					  ((*src4 & 0x01) << 3) | ((*src3 & 0x01) << 2) | ((*src2 & 0x01) << 1) | ((*src1 & 0x01));
			dest += 4;
			src1 += 1;
			src2 += 1;
			src3 += 1;
			src4 += 1;
		}
	}
}

//==========================================================================
//
// ST_Util_DrawBlock
//
//==========================================================================

void ST_Util_DrawBlock (BYTE *dest, const BYTE *src, int bytewidth, int height, int destpitch)
{
	if (bytewidth == 8)
	{ // progress notches
		for (; height > 0; --height)
		{
			((DWORD *)dest)[0] = ((const DWORD *)src)[0];
			((DWORD *)dest)[1] = ((const DWORD *)src)[1];
			dest += destpitch;
			src += 8;
		}
	}
	else if (bytewidth == 2)
	{ // net progress notches
		for (; height > 0; --height)
		{
			*((WORD *)dest) = *((const WORD *)src);
			dest += destpitch;
			src += 2;
		}
	}
}
