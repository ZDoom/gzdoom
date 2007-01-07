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
#include "hardware.h"
#include "gi.h"
#include "w_wad.h"
#include "s_sound.h"
#include "m_alloc.h"

// MACROS ------------------------------------------------------------------

// Hexen startup screen
#define ST_MAX_NOTCHES			32
#define ST_NOTCH_WIDTH			16
#define ST_NOTCH_HEIGHT			23
#define ST_PROGRESS_X			64			// Start of notches x screen pos.
#define ST_PROGRESS_Y			441			// Start of notches y screen pos.

#define ST_NETPROGRESS_X		288
#define ST_NETPROGRESS_Y		32
#define ST_NETNOTCH_WIDTH		4
#define ST_NETNOTCH_HEIGHT		16
#define ST_MAX_NETNOTCHES		8

// Heretic startup screen
#define HERETIC_MINOR_VERSION	'3'			// Since we're based on Heretic 1.3

#define THERM_X					14
#define THERM_Y					14
#define THERM_LEN				51
#define THERM_COLOR				0xAA		// light green

// Strife startup screen
#define PEASANT_INDEX			0
#define LASER_INDEX				4
#define BOT_INDEX				6

#define ST_LASERSPACE_X			60
#define ST_LASERSPACE_Y			156
#define ST_LASERSPACE_WIDTH		200
#define ST_LASER_WIDTH			16
#define ST_LASER_HEIGHT			16

#define ST_BOT_X				14
#define ST_BOT_Y				138
#define ST_BOT_WIDTH			48
#define ST_BOT_HEIGHT			48

#define ST_PEASANT_X			262
#define ST_PEASANT_Y			136
#define ST_PEASANT_WIDTH		32
#define ST_PEASANT_HEIGHT		64

// Text mode color values
#define LO						 85
#define MD						170
#define HI						255

#define TEXT_FONT_NAME			"vga-rom-font.16"

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

void RestoreConView();

bool ST_Util_CreateStartupWindow ();
void ST_Util_SizeWindowForBitmap (int scale);
BITMAPINFO *ST_Util_CreateBitmap (int width, int height, int color_bits);
BYTE *ST_Util_BitsForBitmap (BITMAPINFO *bitmap_info);
void ST_Util_FreeBitmap (BITMAPINFO *bitmap_info);
void ST_Util_BitmapColorsFromPlaypal (BITMAPINFO *bitmap_info);
void ST_Util_PlanarToChunky4 (BYTE *dest, const BYTE *src, int width, int height);
void ST_Util_DrawBlock (BITMAPINFO *bitmap_info, const BYTE *src, int x, int y, int bytewidth, int height);
void ST_Util_ClearBlock (BITMAPINFO *bitmap_info, BYTE fill, int x, int y, int bytewidth, int height);
void ST_Util_InvalidateRect (HWND hwnd, BITMAPINFO *bitmap_info, int left, int top, int right, int bottom);
BYTE *ST_Util_LoadFont (const char *filename);
void ST_Util_FreeFont (BYTE *font);
BITMAPINFO *ST_Util_AllocTextBitmap (const BYTE *font);
void ST_Util_DrawTextScreen (BITMAPINFO *bitmap_info, const BYTE *text_screen, const BYTE *font);
void ST_Util_DrawChar (BITMAPINFO *screen, const BYTE *font, int x, int y, BYTE charnum, BYTE attrib);

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

static void ST_Heretic_Init ();
static void ST_Heretic_Progress ();

static void ST_Strife_Init ();
static void ST_Strife_Done ();
static void ST_Strife_Progress ();
static void ST_Strife_DrawStuff (int old_laser, int new_laser);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern HINSTANCE g_hInst;
extern HWND Window, ConWindow, ProgressBar, NetStartPane, StartupScreen, GameTitleWindow;
extern void LayoutMainWindow (HWND hWnd, HWND pane);

// PUBLIC DATA DEFINITIONS -------------------------------------------------

void (*ST_Done)();
void (*ST_Progress)();
void (*ST_NetInit)(const char *message, int numplayers);
void (*ST_NetProgress)(int count);
void (*ST_NetMessage)(const char *format, ...);
void (*ST_NetDone)();
bool (*ST_NetLoop)(bool (*timer_callback)(void *), void *userdata);

BITMAPINFO *StartupBitmap;

CVAR(Bool, showendoom, true, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int MaxPos, CurPos, NotchPos;
static int NetMaxPos, NetCurPos;
static LRESULT NetMarqueeMode;
static int ThermX, ThermY, ThermWidth, ThermHeight;
static BYTE *StrifeStartupPics[4+2+1];

static const char *StrifeStartupPicNames[4+2+1] =
{
	"STRTPA1", "STRTPB1", "STRTPC1", "STRTPD1",
	"STRTLZ1", "STRTLZ2",
	"STRTBOT"
};
static const int StrifeStartupPicSizes[4+2+1] =
{
	2048, 2048, 2048, 2048,
	256, 256,
	2304
};

static const RGBQUAD TextModePalette[16] =
{
	{  0,  0,  0, 0 },		// 0 black
	{ MD,  0,  0, 0 },		// 1 blue
	{  0, MD,  0, 0 },		// 2 green
	{ MD, MD,  0, 0 },		// 3 cyan
	{  0,  0, MD, 0 },		// 4 red
	{ MD,  0, MD, 0 },		// 5 magenta
	{  0, MD, MD, 0 },		// 6 brown
	{ MD, MD, MD, 0 },		// 7 light gray

	{ LO, LO, LO, 0 },		// 8 dark gray
	{ HI, LO, LO, 0 },		// 9 light blue
	{ LO, HI, LO, 0 },		// A light green
	{ HI, HI, LO, 0 },		// B light cyan
	{ LO, LO, HI, 0 },		// C light red
	{ HI, LO, HI, 0 },		// D light magenta
	{ LO, HI, HI, 0 },		// E yellow
	{ HI, HI, HI, 0 },		// F white
};

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
	else if (gameinfo.gametype == GAME_Heretic)
	{
		ST_Heretic_Init ();
	}
	else if (gameinfo.gametype == GAME_Strife)
	{
		ST_Strife_Init ();
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

	if (startup_lump < 0 || Wads.LumpLength (startup_lump) != 153648 || !ST_Util_CreateStartupWindow())
	{
		ST_Basic_Init ();
		return;
	}

	BYTE startup_screen[153648];
	union
	{
		RGBQUAD color;
		DWORD	quad;
	};

	Wads.ReadLump (startup_lump, startup_screen);

	color.rgbReserved = 0;

	StartupBitmap = ST_Util_CreateBitmap (640, 480, 4);

	// Initialize the bitmap palette.
	for (int i = 0; i < 16; ++i)
	{
		color.rgbRed = startup_screen[i*3+0];
		color.rgbGreen = startup_screen[i*3+1];
		color.rgbBlue = startup_screen[i*3+2];
		// Convert from 6-bit per component to 8-bit per component.
		quad = (quad << 2) | ((quad >> 4) & 0x03030303);
		StartupBitmap->bmiColors[i] = color;
	}

	// Fill in the bitmap data. Convert to chunky, because I can't figure out
	// if Windows actually supports planar images or not, despite the presence
	// of biPlanes in the BITMAPINFOHEADER.
	ST_Util_PlanarToChunky4 (ST_Util_BitsForBitmap(StartupBitmap), startup_screen + 48, 640, 480);

	ST_Util_SizeWindowForBitmap (1);
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
	if (StartupBitmap != NULL)
	{
		ST_Util_FreeBitmap (StartupBitmap);
		StartupBitmap = NULL;
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
				ST_Util_DrawBlock (StartupBitmap, NotchBits, x, y, ST_NOTCH_WIDTH / 2, ST_NOTCH_HEIGHT);
			}
			S_Sound (CHAN_BODY, "StartupTick", 1, ATTN_NONE);
		}
	}
	I_GetEvent ();
}

//==========================================================================
//
// ST_Heretic_Init
//
// Shows the Heretic startup screen. If the screen doesn't appear to be
// valid, it falls back to ST_Basic_Init.
//
// The loading screen is an 80x25 text screen with character data and
// attributes intermixed, which means it must be exactly 4000 bytes long.
//
//==========================================================================

static void ST_Heretic_Init ()
{
	int loading_lump = Wads.CheckNumForName ("LOADING");
	BYTE loading_screen[4000];
	BYTE *font;

	if (loading_lump < 0 || Wads.LumpLength (loading_lump) != 4000 || !ST_Util_CreateStartupWindow())
	{
		ST_Basic_Init ();
		return;
	}

	font = ST_Util_LoadFont (TEXT_FONT_NAME);
	if (font == NULL)
	{
		DestroyWindow (StartupScreen);
		ST_Basic_Init ();
		return;
	}

	Wads.ReadLump (loading_lump, loading_screen);

	// Slap the Heretic minor version on the loading screen. Heretic
	// did this inside the executable rather than coming with modified
	// LOADING screens, so we need to do the same.
	loading_screen[2*160 + 49*2] = HERETIC_MINOR_VERSION;

	// Draw the loading screen to a bitmap.
	StartupBitmap = ST_Util_AllocTextBitmap (font);
	ST_Util_DrawTextScreen (StartupBitmap, loading_screen, font);

	ThermX = THERM_X * 8;
	ThermY = THERM_Y * font[0];
	ThermWidth = THERM_LEN * 8 - 4;
	ThermHeight = font[0];

	ST_Util_FreeFont (font);

	ST_Util_SizeWindowForBitmap (1);
	LayoutMainWindow (Window, NULL);
	InvalidateRect (StartupScreen, NULL, TRUE);

	ST_Done = ST_Hexen_Done;
	ST_Progress = ST_Heretic_Progress;
	ST_NetInit = ST_Basic_NetInit;
	ST_NetProgress = ST_Basic_NetProgress;
	ST_NetMessage = ST_Basic_NetMessage;
	ST_NetDone = ST_Basic_NetDone;
	ST_NetLoop = ST_Basic_NetLoop;
}

//==========================================================================
//
// ST_Heretic_Progress
//
// Bumps the progress meter one notch.
//
//==========================================================================

static void ST_Heretic_Progress()
{
	int notch_pos;

	if (CurPos < MaxPos)
	{
		CurPos++;
		notch_pos = (CurPos * ThermWidth) / MaxPos;
		if (notch_pos != NotchPos && !(notch_pos & 3))
		{ // Time to draw another notch.
			int left = NotchPos + ThermX;
			int top = ThermY;
			int right = notch_pos + ThermX;
			int bottom = top + ThermHeight;
			ST_Util_ClearBlock (StartupBitmap, THERM_COLOR, left, top, right - left, bottom - top);
			NotchPos = notch_pos;
		}
	}
	I_GetEvent ();
}

//==========================================================================
//
// ST_Strife_Init
//
// Shows the Strife startup screen. If the screen doesn't appear to be
// valid, it falls back to ST_Basic_Init.
//
// The startup background is a raw 320x200 image, however Strife only
// actually uses 95 rows from it, starting at row 57. The rest of the image
// is discarded. (What a shame.)
//
// The peasants are raw 32x64 images. The laser dots are raw 16x16 images.
// The bot is a raw 48x48 image. All use the standard PLAYPAL.
//
//==========================================================================

static void ST_Strife_Init ()
{
	int startup_lump = Wads.CheckNumForName ("STARTUP0");
	int i;

	if (startup_lump < 0 || Wads.LumpLength (startup_lump) != 64000 || !ST_Util_CreateStartupWindow())
	{
		ST_Basic_Init ();
		return;
	}

	StartupBitmap = ST_Util_CreateBitmap (320, 200, 8);
	ST_Util_BitmapColorsFromPlaypal (StartupBitmap);

	// Fill bitmap with the startup image.
	memset (ST_Util_BitsForBitmap(StartupBitmap), 0xF0, 64000);
	FWadLump lumpr = Wads.OpenLumpNum (startup_lump);
	lumpr.Seek (57 * 320, SEEK_SET);
	lumpr.Read (ST_Util_BitsForBitmap(StartupBitmap) + 41 * 320, 95 * 320);

	// Load the animated overlays.
	for (i = 0; i < 4+2+1; ++i)
	{
		int lumpnum = Wads.CheckNumForName (StrifeStartupPicNames[i]);
		int lumplen;

		if (lumpnum < 0 || (lumplen = Wads.LumpLength (lumpnum)) != StrifeStartupPicSizes[i])
		{
			StrifeStartupPics[i] = NULL;
		}
		else
		{
			FWadLump lumpr = Wads.OpenLumpNum (lumpnum);
			StrifeStartupPics[i] = new BYTE[lumplen];
			lumpr.Read (StrifeStartupPics[i], lumplen);
		}
	}

	// Make the startup image appear.
	ST_Strife_DrawStuff (0, 0);
	ST_Util_SizeWindowForBitmap (2);
	LayoutMainWindow (Window, NULL);
	InvalidateRect (StartupScreen, NULL, TRUE);

	ST_Done = ST_Strife_Done;
	ST_Progress = ST_Strife_Progress;
	ST_NetInit = ST_Basic_NetInit;
	ST_NetProgress = ST_Basic_NetProgress;
	ST_NetMessage = ST_Basic_NetMessage;
	ST_NetDone = ST_Basic_NetDone;
	ST_NetLoop = ST_Basic_NetLoop;
}

//==========================================================================
//
// ST_Strife_Done
//
// Called just before entering graphics mode to deconstruct the startup
// screen.
//
//==========================================================================

static void ST_Strife_Done()
{
	int i;

	for (i = 0; i < 4+2+1; ++i)
	{
		if (StrifeStartupPics[i] != NULL)
		{
			delete[] StrifeStartupPics[i];
		}
		StrifeStartupPics[i] = NULL;
	}
	if (StartupScreen != NULL)
	{
		DestroyWindow (StartupScreen);
		StartupScreen = NULL;
	}
	if (StartupBitmap != NULL)
	{
		ST_Util_FreeBitmap (StartupBitmap);
		StartupBitmap = NULL;
	}
}

//==========================================================================
//
// ST_Strife_Progress
//
// Bumps the progress meter one notch.
//
//==========================================================================

static void ST_Strife_Progress()
{
	int notch_pos;

	if (CurPos < MaxPos)
	{
		CurPos++;
		notch_pos = (CurPos * (ST_LASERSPACE_WIDTH - ST_LASER_WIDTH)) / MaxPos;
		if (notch_pos != NotchPos)
		{ // Time to update.
			ST_Strife_DrawStuff (NotchPos, notch_pos);
			NotchPos = notch_pos;
		}
	}
	I_GetEvent ();
}

//==========================================================================
//
// ST_Strife_DrawStuff
//
// Draws all the moving parts of Strife's startup screen. If you're
// running off a slow drive, it can look kind of good. Otherwise, it
// borders on crazy insane fast.
//
//==========================================================================

static void ST_Strife_DrawStuff (int old_laser, int new_laser)
{
	int y;

	// Clear old laser
	ST_Util_ClearBlock (StartupBitmap, 0xF0, ST_LASERSPACE_X + old_laser,
		ST_LASERSPACE_Y, ST_LASER_WIDTH, ST_LASER_HEIGHT);
	// Draw new laser
	ST_Util_DrawBlock (StartupBitmap, StrifeStartupPics[LASER_INDEX + (new_laser & 1)],
		ST_LASERSPACE_X + new_laser, ST_LASERSPACE_Y, ST_LASER_WIDTH, ST_LASER_HEIGHT);

	// The bot jumps up and down like crazy.
	y = MAX(0, new_laser % 5 - 2);
	if (y > 0)
	{
		ST_Util_ClearBlock (StartupBitmap, 0xF0, ST_BOT_X, ST_BOT_Y, ST_BOT_WIDTH, y);
	}
	ST_Util_DrawBlock (StartupBitmap, StrifeStartupPics[BOT_INDEX], ST_BOT_X, ST_BOT_Y + y, ST_BOT_WIDTH, ST_BOT_HEIGHT);
	if (y < (5 - 1) - 2)
	{
		ST_Util_ClearBlock (StartupBitmap, 0xF0, ST_BOT_X, ST_BOT_Y + ST_BOT_HEIGHT + y, ST_BOT_WIDTH, 2 - y);
	}

	// The peasant desperately runs in place, trying to get away from the laser.
	// Yet, despite all his limb flailing, he never manages to get anywhere.
	ST_Util_DrawBlock (StartupBitmap, StrifeStartupPics[PEASANT_INDEX + (new_laser & 3)],
		ST_PEASANT_X, ST_PEASANT_Y, ST_PEASANT_WIDTH, ST_PEASANT_HEIGHT);
}

//==========================================================================
//
// ST_Endoom
//
// Shows an ENDOOM text screen
//
//==========================================================================

void ST_Endoom()
{
	if (!showendoom) exit(0);

	int endoom_lump = Wads.CheckNumForName (
		gameinfo.gametype == GAME_Doom? "ENDOOM" : 
		gameinfo.gametype == GAME_Heretic? "ENDTEXT" :
		gameinfo.gametype == GAME_Strife? "ENDSTRF" : NULL);

	BYTE endoom_screen[4000];
	BYTE *font;
	MSG mess;

	if (endoom_lump < 0 || Wads.LumpLength (endoom_lump) != 4000)
	{
		exit(0);
	}

	font = ST_Util_LoadFont (TEXT_FONT_NAME);
	if (font == NULL)
	{
		exit(0);
	}

	if (!ST_Util_CreateStartupWindow())
	{
		ST_Util_FreeFont (font);
		exit(0);
	}
	ST_Done = ST_Basic_Done;

	I_ShutdownGraphics ();
	RestoreConView ();
	S_StopMusic(true);

	Wads.ReadLump (endoom_lump, endoom_screen);

	// Draw the loading screen to a bitmap.
	StartupBitmap = ST_Util_AllocTextBitmap (font);
	ST_Util_DrawTextScreen (StartupBitmap, endoom_screen, font);
	ST_Util_FreeFont (font);

	ST_Util_SizeWindowForBitmap (1);
	LayoutMainWindow (Window, NULL);
	InvalidateRect (StartupScreen, NULL, TRUE);

	// Wait until any key has been pressed or a quit message has been received

	while (1)
	{
		if (PeekMessage (&mess, NULL, 0, 0, PM_REMOVE))
		{
			if (mess.message == WM_QUIT)
				exit (int(mess.wParam));
			if (mess.message == WM_KEYDOWN || mess.message == WM_SYSKEYDOWN || mess.message == WM_LBUTTONDOWN)
				exit(0);

			TranslateMessage (&mess);
			DispatchMessage (&mess);
		}
		else WaitMessage();
	}
}

//==========================================================================
//
// ST_Util_CreateStartupWindow
//
// Creates the static control that will draw the startup screen.
//
//==========================================================================

bool ST_Util_CreateStartupWindow ()
{
	StartupScreen = CreateWindowEx (WS_EX_NOPARENTNOTIFY, "STATIC", NULL,
		WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | SS_OWNERDRAW,
		0, 0, 0, 0, Window, NULL, g_hInst, NULL);
	if (StartupScreen == NULL)
	{
		return false;
	}
	SetWindowLong (StartupScreen, GWL_ID, IDC_STATIC_STARTUP);
	return true;
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

	GetClientRect (GameTitleWindow, &rect);
	w = StartupBitmap->bmiHeader.biWidth * scale + GetSystemMetrics (SM_CXSIZEFRAME)*2;
	h = StartupBitmap->bmiHeader.biHeight * scale + rect.bottom
		+ GetSystemMetrics (SM_CYSIZEFRAME) * 2 + GetSystemMetrics (SM_CYCAPTION);

	// Resize the window, but keep its center point the same, unless that
	// puts it partially offscreen.
	memset (&displaysettings, 0, sizeof(displaysettings));
	displaysettings.dmSize = sizeof(displaysettings);
	EnumDisplaySettings (NULL, ENUM_CURRENT_SETTINGS, &displaysettings);
	GetWindowRect (Window, &rect);
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
	MoveWindow (Window, x, y, w, h, TRUE);
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

void ST_Util_DrawBlock (BITMAPINFO *bitmap_info, const BYTE *src, int x, int y, int bytewidth, int height)
{
	int pitchshift = int(bitmap_info->bmiHeader.biBitCount == 4);
	int destpitch = bitmap_info->bmiHeader.biWidth >> pitchshift;
	BYTE *dest = ST_Util_BitsForBitmap(bitmap_info) + (x >> pitchshift) + y * destpitch;

	ST_Util_InvalidateRect (StartupScreen, bitmap_info, x, y, x + (bytewidth << pitchshift), y + height);

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
	else
	{
		for (; height > 0; --height)
		{
			memcpy (dest, src, bytewidth);
			dest += destpitch;
			src += bytewidth;
		}
	}
}

//==========================================================================
//
// ST_Util_ClearBlock
//
//==========================================================================

void ST_Util_ClearBlock (BITMAPINFO *bitmap_info, BYTE fill, int x, int y, int bytewidth, int height)
{
	int pitchshift = int(bitmap_info->bmiHeader.biBitCount == 4);
	int destpitch = bitmap_info->bmiHeader.biWidth >> pitchshift;
	BYTE *dest = ST_Util_BitsForBitmap(bitmap_info) + (x >> pitchshift) + y * destpitch;

	ST_Util_InvalidateRect (StartupScreen, bitmap_info, x, y, x + (bytewidth << pitchshift), y + height);

	while (height > 0)
	{
		memset (dest, fill, bytewidth);
		dest += destpitch;
		height--;
	}
}

//==========================================================================
//
// ST_Util_CreateBitmap
//
// Creates a BITMAPINFOHEADER, RGBQUAD, and pixel data arranged
// consecutively in memory (in other words, a normal Windows BMP file).
// The BITMAPINFOHEADER will be filled in, and the caller must fill
// in the color and pixel data.
//
// You must pass 4 or 8 for color_bits.
//
//==========================================================================

BITMAPINFO *ST_Util_CreateBitmap (int width, int height, int color_bits)
{
	DWORD size_image = (width * height) >> int(color_bits == 4);
	BITMAPINFO *bitmap_info = (BITMAPINFO *)M_Malloc (sizeof(BITMAPINFOHEADER) +
		(sizeof(RGBQUAD) << color_bits) + size_image);

	// Initialize the header.
	bitmap_info->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bitmap_info->bmiHeader.biWidth = width;
	bitmap_info->bmiHeader.biHeight = height;
	bitmap_info->bmiHeader.biPlanes = 1;
	bitmap_info->bmiHeader.biBitCount = color_bits;
	bitmap_info->bmiHeader.biCompression = 0;
	bitmap_info->bmiHeader.biSizeImage = size_image;
	bitmap_info->bmiHeader.biXPelsPerMeter = 0;
	bitmap_info->bmiHeader.biYPelsPerMeter = 0;
	bitmap_info->bmiHeader.biClrUsed = 1 << color_bits;
	bitmap_info->bmiHeader.biClrImportant = 0;

	return bitmap_info;
}

//==========================================================================
//
// ST_Util_BitsForBitmap
//
// Given a bitmap created by ST_Util_CreateBitmap, returns the start
// address for the pixel data for the bitmap.
//
//==========================================================================

BYTE *ST_Util_BitsForBitmap (BITMAPINFO *bitmap_info)
{
	return (BYTE *)bitmap_info + sizeof(BITMAPINFOHEADER) + (sizeof(RGBQUAD) << bitmap_info->bmiHeader.biBitCount);
}

//==========================================================================
//
// ST_Util_FreeBitmap
//
// Frees all the data for a bitmap created by ST_Util_CreateBitmap.
//
//==========================================================================

void ST_Util_FreeBitmap (BITMAPINFO *bitmap_info)
{
	free (bitmap_info);
}

//==========================================================================
//
// ST_Util_BitmapColorsFromPlaypal
//
// Fills the bitmap palette from the PLAYPAL lump.
//
//==========================================================================

void ST_Util_BitmapColorsFromPlaypal (BITMAPINFO *bitmap_info)
{
	BYTE playpal[768];
	int i;

	{
		FWadLump lumpr = Wads.OpenLumpName ("PLAYPAL");
		lumpr.Read (playpal, 768);
	}
	for (i = 0; i < 256; ++i)
	{
		bitmap_info->bmiColors[i].rgbBlue	= playpal[i*3+2];
		bitmap_info->bmiColors[i].rgbGreen	= playpal[i*3+1];
		bitmap_info->bmiColors[i].rgbRed	= playpal[i*3];
		bitmap_info->bmiColors[i].rgbReserved = 0;
	}
}

//==========================================================================
//
// ST_Util_InvalidateRect
//
// Invalidates the portion of the window that the specified rect of the
// bitmap appears in.
//
//==========================================================================

void ST_Util_InvalidateRect (HWND hwnd, BITMAPINFO *bitmap_info, int left, int top, int right, int bottom)
{
	RECT rect;

	GetClientRect (hwnd, &rect);
	rect.left = left * rect.right / bitmap_info->bmiHeader.biWidth - 1;
	rect.top = top * rect.bottom / bitmap_info->bmiHeader.biHeight - 1;
	rect.right = right * rect.right / bitmap_info->bmiHeader.biWidth + 1;
	rect.bottom = bottom * rect.bottom / bitmap_info->bmiHeader.biHeight + 1;
	InvalidateRect (hwnd, &rect, FALSE);
}

//==========================================================================
//
// ST_Util_LoadFont
//
// Loads a monochrome fixed-width font. Every character is one byte
// (eight pixels) wide, so we can deduce the height of each character
// by looking at the size of the font data.
//
//==========================================================================

BYTE *ST_Util_LoadFont (const char *filename)
{
	int lumpnum, lumplen, height;
	BYTE *font;
	
	lumpnum = Wads.CheckNumForFullName (filename);
	if (lumpnum < 0)
	{ // font not found
		return NULL;
	}
	lumplen = Wads.LumpLength (lumpnum);
	height = lumplen / 256;
	if (height * 256 != lumplen)
	{ // font is a bad size
		return NULL;
	}
	if (height < 6 || height > 36)
	{ // let's be reasonable here
		return NULL;
	}
	font = new BYTE[lumplen + 1];
	font[0] = height;	// Store font height in the first byte.
	Wads.ReadLump (lumpnum, font + 1);
	return font;
}

void ST_Util_FreeFont (BYTE *font)
{
	delete[] font;
}

//==========================================================================
//
// ST_Util_AllocTextBitmap
//
// Returns a bitmap properly sized to hold an 80x25 display of characters
// using the specified font.
//
//==========================================================================

BITMAPINFO *ST_Util_AllocTextBitmap (const BYTE *font)
{
	BITMAPINFO *bitmap = ST_Util_CreateBitmap (80 * 8, 25 * font[0], 4);
	memcpy (bitmap->bmiColors, TextModePalette, sizeof(TextModePalette));
	return bitmap;
}

//==========================================================================
//
// ST_Util_DrawTextScreen
//
// Draws the text screen to the bitmap. The bitmap must be the proper size
// for the font.
//
//==========================================================================

void ST_Util_DrawTextScreen (BITMAPINFO *bitmap_info, const BYTE *text_screen, const BYTE *font)
{
	int x, y;

	for (y = 0; y < 25; ++y)
	{
		for (x = 0; x < 80; ++x)
		{
			ST_Util_DrawChar (bitmap_info, font, x, y, text_screen[0], text_screen[1]);
			text_screen += 2;
		}
	}
}

//==========================================================================
//
// ST_Util_DrawChar
//
// Draws a character on the bitmap. X and Y specify the character cell,
// and fg and bg are 4-bit colors.
//
//==========================================================================

void ST_Util_DrawChar (BITMAPINFO *screen, const BYTE *font, int x, int y, BYTE charnum, BYTE attrib)
{
	const BYTE bg_left = attrib & 0xF0;
	const BYTE fg      = attrib & 0x0F;
	const BYTE fg_left = fg << 4;
	const BYTE bg      = bg_left >> 4;
	const BYTE color_array[4] = { bg_left | bg, attrib, fg_left | bg, fg_left | fg };
	const BYTE *src = font + 1 + charnum * font[0];
	int pitch = screen->bmiHeader.biWidth >> 1;
	BYTE *dest = ST_Util_BitsForBitmap(screen) + x*4 + y * font[0] * pitch;

	for (y = font[0]; y > 0; --y)
	{
		BYTE srcbyte = *src++;

		// Pixels 0 and 1
		dest[0] = color_array[(srcbyte >> 6) & 3];
		// Pixels 2 and 3
		dest[1] = color_array[(srcbyte >> 4) & 3];
		// Pixels 4 and 5
		dest[2] = color_array[(srcbyte >> 2) & 3];
		// Pixels 6 and 7
		dest[3] = color_array[(srcbyte)      & 3];
		dest += pitch;
	}
}
