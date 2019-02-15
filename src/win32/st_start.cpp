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

// How many ms elapse between blinking text flips. On a standard VGA
// adapter, the characters are on for 16 frames and then off for another 16.
// The number here therefore corresponds roughly to the blink rate on a
// 60 Hz display.
#define BLINK_PERIOD			267

// TYPES -------------------------------------------------------------------

class FBasicStartupScreen : public FStartupScreen
{
public:
	FBasicStartupScreen(int max_progress, bool show_bar);
	~FBasicStartupScreen();

	void Progress();
	void NetInit(const char *message, int num_players);
	void NetProgress(int count);
	void NetMessage(const char *format, ...);	// cover for printf
	void NetDone();
	bool NetLoop(bool (*timer_callback)(void *), void *userdata);
protected:
	LRESULT NetMarqueeMode;
	int NetMaxPos, NetCurPos;
};

class FGraphicalStartupScreen : public FBasicStartupScreen
{
public:
	FGraphicalStartupScreen(int max_progress);
	~FGraphicalStartupScreen();
};

class FHereticStartupScreen : public FGraphicalStartupScreen
{
public:
	FHereticStartupScreen(int max_progress, HRESULT &hr);

	void Progress();
	void LoadingStatus(const char *message, int colors);
	void AppendStatusLine(const char *status);
protected:
	int ThermX, ThermY, ThermWidth, ThermHeight;
	int HMsgY, SMsgX;
};

class FHexenStartupScreen : public FGraphicalStartupScreen
{
public:
	FHexenStartupScreen(int max_progress, HRESULT &hr);
	~FHexenStartupScreen();

	void Progress();
	void NetProgress(int count);
	void NetDone();

	// Hexen's notch graphics, converted to chunky pixels.
	uint8_t * NotchBits;
	uint8_t * NetNotchBits;
};

class FStrifeStartupScreen : public FGraphicalStartupScreen
{
public:
	FStrifeStartupScreen(int max_progress, HRESULT &hr);
	~FStrifeStartupScreen();

	void Progress();
protected:
	void DrawStuff(int old_laser, int new_laser);

	uint8_t *StartupPics[4+2+1];
};

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

void RestoreConView();
void LayoutMainWindow (HWND hWnd, HWND pane);
int LayoutNetStartPane (HWND pane, int w);

bool ST_Util_CreateStartupWindow ();
void ST_Util_SizeWindowForBitmap (int scale);
BITMAPINFO *ST_Util_CreateBitmap (int width, int height, int color_bits);
uint8_t *ST_Util_BitsForBitmap (BITMAPINFO *bitmap_info);
void ST_Util_FreeBitmap (BITMAPINFO *bitmap_info);
void ST_Util_BitmapColorsFromPlaypal (BITMAPINFO *bitmap_info);
void ST_Util_PlanarToChunky4 (uint8_t *dest, const uint8_t *src, int width, int height);
void ST_Util_DrawBlock (BITMAPINFO *bitmap_info, const uint8_t *src, int x, int y, int bytewidth, int height);
void ST_Util_ClearBlock (BITMAPINFO *bitmap_info, uint8_t fill, int x, int y, int bytewidth, int height);
void ST_Util_InvalidateRect (HWND hwnd, BITMAPINFO *bitmap_info, int left, int top, int right, int bottom);
uint8_t *ST_Util_LoadFont (const char *filename);
void ST_Util_FreeFont (uint8_t *font);
BITMAPINFO *ST_Util_AllocTextBitmap (const uint8_t *font);
void ST_Util_DrawTextScreen (BITMAPINFO *bitmap_info, const uint8_t *text_screen, const uint8_t *font);
void ST_Util_UpdateTextBlink (BITMAPINFO *bitmap_info, const uint8_t *text_screen, const uint8_t *font, bool blink_on);
void ST_Util_DrawChar (BITMAPINFO *screen, const uint8_t *font, int x, int y, uint8_t charnum, uint8_t attrib);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static INT_PTR CALLBACK NetStartPaneProc (HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern HINSTANCE g_hInst;
extern HWND Window, ConWindow, ProgressBar, NetStartPane, StartupScreen, GameTitleWindow;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

FStartupScreen *StartScreen;
BITMAPINFO *StartupBitmap;

CUSTOM_CVAR(Int, showendoom, 0, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (self < 0) self = 0;
	else if (self > 2) self=2;
}

// PRIVATE DATA DEFINITIONS ------------------------------------------------

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
	{  0, LO, MD, 0 },		// 6 brown
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
	HRESULT hr;

	if (!Args->CheckParm("-nostartup"))
	{
		if (DoomStartupInfo.Type == FStartupInfo::HexenStartup ||
			(gameinfo.gametype == GAME_Hexen && DoomStartupInfo.Type == FStartupInfo::DefaultStartup))
		{
			scr = new FHexenStartupScreen(max_progress, hr);
		}
		else if (DoomStartupInfo.Type == FStartupInfo::HereticStartup ||
			(gameinfo.gametype == GAME_Heretic && DoomStartupInfo.Type == FStartupInfo::DefaultStartup))
		{
			scr = new FHereticStartupScreen(max_progress, hr);
		}
		else if (DoomStartupInfo.Type == FStartupInfo::StrifeStartup ||
			(gameinfo.gametype == GAME_Strife && DoomStartupInfo.Type == FStartupInfo::DefaultStartup))
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
		ProgressBar = CreateWindowEx(0, PROGRESS_CLASS,
			NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
			0, 0, 0, 0,
			Window, 0, g_hInst, NULL);
		SendMessage (ProgressBar, PBM_SETRANGE, 0, MAKELPARAM(0,MaxPos));
		LayoutMainWindow (Window, NULL);
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
	if (ProgressBar != NULL)
	{
		DestroyWindow (ProgressBar);
		ProgressBar = NULL;
		LayoutMainWindow (Window, NULL);
	}
	KillTimer(Window, 1337);
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
		SendMessage (ProgressBar, PBM_SETPOS, CurPos, 0);
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
// FHexenStartupScreen Constructor
//
// Shows the Hexen startup screen. If the screen doesn't appear to be
// valid, it sets hr for a failure.
//
// The startup graphic is a planar, 4-bit 640x480 graphic preceded by a
// 16 entry (48 byte) VGA palette.
//
//==========================================================================

FHexenStartupScreen::FHexenStartupScreen(int max_progress, HRESULT &hr)
: FGraphicalStartupScreen(max_progress)
{
	int startup_lump = Wads.CheckNumForName ("STARTUP");
	int netnotch_lump = Wads.CheckNumForName ("NETNOTCH");
	int notch_lump = Wads.CheckNumForName ("NOTCH");
	hr = E_FAIL;

	if (startup_lump < 0 || Wads.LumpLength (startup_lump) != 153648 || !ST_Util_CreateStartupWindow() ||
		netnotch_lump < 0 || Wads.LumpLength (netnotch_lump) != ST_NETNOTCH_WIDTH / 2 * ST_NETNOTCH_HEIGHT ||
		notch_lump < 0 || Wads.LumpLength (notch_lump) != ST_NOTCH_WIDTH / 2 * ST_NOTCH_HEIGHT)
	{
		NetNotchBits = NotchBits = NULL;
		return;
	}

	NetNotchBits = new uint8_t[ST_NETNOTCH_WIDTH / 2 * ST_NETNOTCH_HEIGHT];
	Wads.ReadLump (netnotch_lump, NetNotchBits);
	NotchBits = new uint8_t[ST_NOTCH_WIDTH / 2 * ST_NOTCH_HEIGHT];
 	Wads.ReadLump (notch_lump, NotchBits);

	uint8_t startup_screen[153648];
	union
	{
		RGBQUAD color;
		uint32_t	quad;
	} c;

	Wads.ReadLump (startup_lump, startup_screen);

	c.color.rgbReserved = 0;

	StartupBitmap = ST_Util_CreateBitmap (640, 480, 4);

	// Initialize the bitmap palette.
	for (int i = 0; i < 16; ++i)
	{
		c.color.rgbRed = startup_screen[i*3+0];
		c.color.rgbGreen = startup_screen[i*3+1];
		c.color.rgbBlue = startup_screen[i*3+2];
		// Convert from 6-bit per component to 8-bit per component.
		c.quad = (c.quad << 2) | ((c.quad >> 4) & 0x03030303);
		StartupBitmap->bmiColors[i] = c.color;
	}

	// Fill in the bitmap data. Convert to chunky, because I can't figure out
	// if Windows actually supports planar images or not, despite the presence
	// of biPlanes in the BITMAPINFOHEADER.
	ST_Util_PlanarToChunky4 (ST_Util_BitsForBitmap(StartupBitmap), startup_screen + 48, 640, 480);

	ST_Util_SizeWindowForBitmap (1);
	LayoutMainWindow (Window, NULL);
	InvalidateRect (StartupScreen, NULL, TRUE);

	if (!batchrun)
	{
		if (DoomStartupInfo.Song.IsNotEmpty())
		{
			S_ChangeMusic(DoomStartupInfo.Song.GetChars(), true, true);
		}
		else
		{
			S_ChangeMusic("orb", true, true);
		}
	}
	hr = S_OK;
}

//==========================================================================
//
// FHexenStartupScreen Deconstructor
//
// Frees the notch pictures.
//
//==========================================================================

FHexenStartupScreen::~FHexenStartupScreen()
{
	if (NotchBits)
		delete[] NotchBits;
	if (NetNotchBits)
		delete[] NetNotchBits;
}

//==========================================================================
//
// FHexenStartupScreen :: Progress
//
// Bumps the progress meter one notch.
//
//==========================================================================

void FHexenStartupScreen::Progress()
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
// FHexenStartupScreen :: NetProgress
//
// Draws the red net noches in addition to the normal progress bar.
//
//==========================================================================

void FHexenStartupScreen::NetProgress(int count)
{
	int oldpos = NetCurPos;
	int x, y;

	FGraphicalStartupScreen::NetProgress (count);
	if (NetMaxPos != 0 && NetCurPos > oldpos)
	{
		for (; oldpos < NetCurPos && oldpos < ST_MAX_NETNOTCHES; ++oldpos)
		{
			x = ST_NETPROGRESS_X + ST_NETNOTCH_WIDTH * oldpos;
			y = ST_NETPROGRESS_Y;
			ST_Util_DrawBlock (StartupBitmap, NetNotchBits, x, y, ST_NETNOTCH_WIDTH / 2, ST_NETNOTCH_HEIGHT);
		}
		S_Sound (CHAN_BODY, "misc/netnotch", 1, ATTN_NONE);
		I_GetEvent ();
	}
}

//==========================================================================
//
// FHexenStartupScreen :: NetDone
//
// Aside from the standard processing, also plays a sound.
//
//==========================================================================

void FHexenStartupScreen::NetDone()
{
	S_Sound (CHAN_BODY, "PickupWeapon", 1, ATTN_NORM);
	FGraphicalStartupScreen::NetDone();
}

//==========================================================================
//
// FHereticStartupScreen Constructor
//
// Shows the Heretic startup screen. If the screen doesn't appear to be
// valid, it returns a failure code in hr.
//
// The loading screen is an 80x25 text screen with character data and
// attributes intermixed, which means it must be exactly 4000 bytes long.
//
//==========================================================================

FHereticStartupScreen::FHereticStartupScreen(int max_progress, HRESULT &hr)
: FGraphicalStartupScreen(max_progress)
{
	int loading_lump = Wads.CheckNumForName ("LOADING");
	uint8_t loading_screen[4000];
	uint8_t *font;

	hr = E_FAIL;
	if (loading_lump < 0 || Wads.LumpLength (loading_lump) != 4000 || !ST_Util_CreateStartupWindow())
	{
		return;
	}

	font = ST_Util_LoadFont (TEXT_FONT_NAME);
	if (font == NULL)
	{
		DestroyWindow (StartupScreen);
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
	HMsgY = 7;
	SMsgX = 1;

	ST_Util_FreeFont (font);

	ST_Util_SizeWindowForBitmap (1);
	LayoutMainWindow (Window, NULL);
	InvalidateRect (StartupScreen, NULL, TRUE);
	hr = S_OK;
}

//==========================================================================
//
// FHereticStartupScreen::Progress
//
// Bumps the progress meter one notch.
//
//==========================================================================

void FHereticStartupScreen::Progress()
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
// FHereticStartupScreen :: LoadingStatus
//
// Prints text in the center box of the startup screen.
//
//==========================================================================

void FHereticStartupScreen::LoadingStatus(const char *message, int colors)
{
	uint8_t *font = ST_Util_LoadFont (TEXT_FONT_NAME);
	if (font != NULL)
	{
		int x;

		for (x = 0; message[x] != '\0'; ++x)
		{
			ST_Util_DrawChar (StartupBitmap, font, 17 + x, HMsgY, message[x], colors);
		}
		ST_Util_InvalidateRect (StartupScreen, StartupBitmap, 17 * 8, HMsgY * font[0], (17 + x) * 8, HMsgY * font[0] + font[0]);
		ST_Util_FreeFont (font);
		HMsgY++;
		I_GetEvent ();
	}
}

//==========================================================================
//
// FHereticStartupScreen :: AppendStatusLine
//
// Appends text to Heretic's status line.
//
//==========================================================================

void FHereticStartupScreen::AppendStatusLine(const char *status)
{
	uint8_t *font = ST_Util_LoadFont (TEXT_FONT_NAME);
	if (font != NULL)
	{
		int x;

		for (x = 0; status[x] != '\0'; ++x)
		{
			ST_Util_DrawChar (StartupBitmap, font, SMsgX + x, 24, status[x], 0x1f);
		}
		ST_Util_InvalidateRect (StartupScreen, StartupBitmap, SMsgX * 8, 24 * font[0], (SMsgX + x) * 8, 25 * font[0]);
		ST_Util_FreeFont (font);
		SMsgX += x;
		I_GetEvent ();
	}
}

//==========================================================================
//
// FStrifeStartupScreen Constructor
//
// Shows the Strife startup screen. If the screen doesn't appear to be
// valid, it returns a failure code in hr.
//
// The startup background is a raw 320x200 image, however Strife only
// actually uses 95 rows from it, starting at row 57. The rest of the image
// is discarded. (What a shame.)
//
// The peasants are raw 32x64 images. The laser dots are raw 16x16 images.
// The bot is a raw 48x48 image. All use the standard PLAYPAL.
//
//==========================================================================

FStrifeStartupScreen::FStrifeStartupScreen(int max_progress, HRESULT &hr)
: FGraphicalStartupScreen(max_progress)
{
	int startup_lump = Wads.CheckNumForName ("STARTUP0");
	int i;

	hr = E_FAIL;
	for (i = 0; i < 4+2+1; ++i)
	{
		StartupPics[i] = NULL;
	}

	if (startup_lump < 0 || Wads.LumpLength (startup_lump) != 64000 || !ST_Util_CreateStartupWindow())
	{
		return;
	}

	StartupBitmap = ST_Util_CreateBitmap (320, 200, 8);
	ST_Util_BitmapColorsFromPlaypal (StartupBitmap);

	// Fill bitmap with the startup image.
	memset (ST_Util_BitsForBitmap(StartupBitmap), 0xF0, 64000);
	auto lumpr = Wads.OpenLumpReader (startup_lump);
	lumpr.Seek (57 * 320, FileReader::SeekSet);
	lumpr.Read (ST_Util_BitsForBitmap(StartupBitmap) + 41 * 320, 95 * 320);

	// Load the animated overlays.
	for (i = 0; i < 4+2+1; ++i)
	{
		int lumpnum = Wads.CheckNumForName (StrifeStartupPicNames[i]);
		int lumplen;

		if (lumpnum >= 0 && (lumplen = Wads.LumpLength (lumpnum)) == StrifeStartupPicSizes[i])
		{
			auto lumpr = Wads.OpenLumpReader (lumpnum);
			StartupPics[i] = new uint8_t[lumplen];
			lumpr.Read (StartupPics[i], lumplen);
		}
	}

	// Make the startup image appear.
	DrawStuff (0, 0);
	ST_Util_SizeWindowForBitmap (2);
	LayoutMainWindow (Window, NULL);
	InvalidateRect (StartupScreen, NULL, TRUE);

	hr = S_OK;
}

//==========================================================================
//
// FStrifeStartupScreen Deconstructor
//
// Frees the strife pictures.
//
//==========================================================================

FStrifeStartupScreen::~FStrifeStartupScreen()
{
	for (int i = 0; i < 4+2+1; ++i)
	{
		if (StartupPics[i] != NULL)
		{
			delete[] StartupPics[i];
		}
		StartupPics[i] = NULL;
	}
}

//==========================================================================
//
// FStrifeStartupScreen :: Progress
//
// Bumps the progress meter one notch.
//
//==========================================================================

void FStrifeStartupScreen::Progress()
{
	int notch_pos;

	if (CurPos < MaxPos)
	{
		CurPos++;
		notch_pos = (CurPos * (ST_LASERSPACE_WIDTH - ST_LASER_WIDTH)) / MaxPos;
		if (notch_pos != NotchPos && !(notch_pos & 1))
		{ // Time to update.
			DrawStuff (NotchPos, notch_pos);
			NotchPos = notch_pos;
		}
	}
	I_GetEvent ();
}

//==========================================================================
//
// FStrifeStartupScreen :: DrawStuff
//
// Draws all the moving parts of Strife's startup screen. If you're
// running off a slow drive, it can look kind of good. Otherwise, it
// borders on crazy insane fast.
//
//==========================================================================

void FStrifeStartupScreen::DrawStuff(int old_laser, int new_laser)
{
	int y;

	// Clear old laser
	ST_Util_ClearBlock (StartupBitmap, 0xF0, ST_LASERSPACE_X + old_laser,
		ST_LASERSPACE_Y, ST_LASER_WIDTH, ST_LASER_HEIGHT);
	// Draw new laser
	ST_Util_DrawBlock (StartupBitmap, StartupPics[LASER_INDEX + (new_laser & 1)],
		ST_LASERSPACE_X + new_laser, ST_LASERSPACE_Y, ST_LASER_WIDTH, ST_LASER_HEIGHT);

	// The bot jumps up and down like crazy.
	y = MAX(0, (new_laser >> 1) % 5 - 2);
	if (y > 0)
	{
		ST_Util_ClearBlock (StartupBitmap, 0xF0, ST_BOT_X, ST_BOT_Y, ST_BOT_WIDTH, y);
	}
	ST_Util_DrawBlock (StartupBitmap, StartupPics[BOT_INDEX], ST_BOT_X, ST_BOT_Y + y, ST_BOT_WIDTH, ST_BOT_HEIGHT);
	if (y < (5 - 1) - 2)
	{
		ST_Util_ClearBlock (StartupBitmap, 0xF0, ST_BOT_X, ST_BOT_Y + ST_BOT_HEIGHT + y, ST_BOT_WIDTH, 2 - y);
	}

	// The peasant desperately runs in place, trying to get away from the laser.
	// Yet, despite all his limb flailing, he never manages to get anywhere.
	ST_Util_DrawBlock (StartupBitmap, StartupPics[PEASANT_INDEX + ((new_laser >> 1) & 3)],
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
	if (showendoom == 0) exit(0);

	if (gameinfo.Endoom.Len() == 0) 
	{
		exit(0);
	}

	int endoom_lump = Wads.CheckNumForFullName (gameinfo.Endoom, true);

	uint8_t endoom_screen[4000];
	uint8_t *font;
	MSG mess;
	BOOL bRet;
	bool blinking = false, blinkstate = false;
	int i;

	if (endoom_lump < 0 || Wads.LumpLength (endoom_lump) != 4000)
	{
		exit(0);
	}

	if (Wads.GetLumpFile(endoom_lump) == Wads.GetIwadNum() && showendoom == 2)
	{
		// showendoom==2 means to show only lumps from PWADs.
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

	I_ShutdownGraphics ();
	RestoreConView ();
	S_StopMusic(true);

	Wads.ReadLump (endoom_lump, endoom_screen);

	// Draw the loading screen to a bitmap.
	StartupBitmap = ST_Util_AllocTextBitmap (font);
	ST_Util_DrawTextScreen (StartupBitmap, endoom_screen, font);

	// Make the title banner go away.
	if (GameTitleWindow != NULL)
	{
		DestroyWindow (GameTitleWindow);
		GameTitleWindow = NULL;
	}

	ST_Util_SizeWindowForBitmap (1);
	LayoutMainWindow (Window, NULL);
	InvalidateRect (StartupScreen, NULL, TRUE);

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
			ST_Util_FreeFont (font);
			exit (int(bRet == 0 ? mess.wParam : 0));
		}
		else if (blinking && mess.message == WM_TIMER && mess.hwnd == Window && mess.wParam == 0x5A15A)
		{
			ST_Util_UpdateTextBlink (StartupBitmap, endoom_screen, font, blinkstate);
			blinkstate = !blinkstate;
		}
		TranslateMessage (&mess);
		DispatchMessage (&mess);
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
	StartupScreen = CreateWindowEx (WS_EX_NOPARENTNOTIFY, L"STATIC", NULL,
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

	if (GameTitleWindow != NULL)
	{
		GetClientRect (GameTitleWindow, &rect);
	}
	else
	{
		rect.bottom = 0;
	}
	RECT sizerect = { 0, 0, StartupBitmap->bmiHeader.biWidth * scale,
		StartupBitmap->bmiHeader.biHeight * scale + rect.bottom };
	AdjustWindowRectEx(&sizerect, WS_VISIBLE|WS_OVERLAPPEDWINDOW, FALSE, WS_EX_APPWINDOW);
	w = sizerect.right - sizerect.left;
	h = sizerect.bottom - sizerect.top;

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

void ST_Util_PlanarToChunky4 (uint8_t *dest, const uint8_t *src, int width, int height)
{
	int y, x;
	const uint8_t *src1, *src2, *src3, *src4;
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

void ST_Util_DrawBlock (BITMAPINFO *bitmap_info, const uint8_t *src, int x, int y, int bytewidth, int height)
{
	if (src == NULL)
	{
		return;
	}

	int pitchshift = int(bitmap_info->bmiHeader.biBitCount == 4);
	int destpitch = bitmap_info->bmiHeader.biWidth >> pitchshift;
	uint8_t *dest = ST_Util_BitsForBitmap(bitmap_info) + (x >> pitchshift) + y * destpitch;

	ST_Util_InvalidateRect (StartupScreen, bitmap_info, x, y, x + (bytewidth << pitchshift), y + height);

	if (bytewidth == 8)
	{ // progress notches
		for (; height > 0; --height)
		{
			((uint32_t *)dest)[0] = ((const uint32_t *)src)[0];
			((uint32_t *)dest)[1] = ((const uint32_t *)src)[1];
			dest += destpitch;
			src += 8;
		}
	}
	else if (bytewidth == 2)
	{ // net progress notches
		for (; height > 0; --height)
		{
			*((uint16_t *)dest) = *((const uint16_t *)src);
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

void ST_Util_ClearBlock (BITMAPINFO *bitmap_info, uint8_t fill, int x, int y, int bytewidth, int height)
{
	int pitchshift = int(bitmap_info->bmiHeader.biBitCount == 4);
	int destpitch = bitmap_info->bmiHeader.biWidth >> pitchshift;
	uint8_t *dest = ST_Util_BitsForBitmap(bitmap_info) + (x >> pitchshift) + y * destpitch;

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
	uint32_t size_image = (width * height) >> int(color_bits == 4);
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

uint8_t *ST_Util_BitsForBitmap (BITMAPINFO *bitmap_info)
{
	return (uint8_t *)bitmap_info + sizeof(BITMAPINFOHEADER) + (sizeof(RGBQUAD) << bitmap_info->bmiHeader.biBitCount);
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
	M_Free (bitmap_info);
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
	uint8_t playpal[768];

	ReadPalette(Wads.CheckNumForName("PLAYPAL"), playpal);
	for (int i = 0; i < 256; ++i)
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

uint8_t *ST_Util_LoadFont (const char *filename)
{
	int lumpnum, lumplen, height;
	uint8_t *font;
	
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
	font = new uint8_t[lumplen + 1];
	font[0] = height;	// Store font height in the first byte.
	Wads.ReadLump (lumpnum, font + 1);
	return font;
}

void ST_Util_FreeFont (uint8_t *font)
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

BITMAPINFO *ST_Util_AllocTextBitmap (const uint8_t *font)
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

void ST_Util_DrawTextScreen (BITMAPINFO *bitmap_info, const uint8_t *text_screen, const uint8_t *font)
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

void ST_Util_DrawChar (BITMAPINFO *screen, const uint8_t *font, int x, int y, uint8_t charnum, uint8_t attrib)
{
	const uint8_t bg_left = attrib & 0x70;
	const uint8_t fg      = attrib & 0x0F;
	const uint8_t fg_left = fg << 4;
	const uint8_t bg      = bg_left >> 4;
	const uint8_t color_array[4] = { (uint8_t)(bg_left | bg), (uint8_t)(attrib & 0x7F), (uint8_t)(fg_left | bg), (uint8_t)(fg_left | fg) };
	const uint8_t *src = font + 1 + charnum * font[0];
	int pitch = screen->bmiHeader.biWidth >> 1;
	uint8_t *dest = ST_Util_BitsForBitmap(screen) + x*4 + y * font[0] * pitch;

	for (y = font[0]; y > 0; --y)
	{
		uint8_t srcbyte = *src++;

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

//==========================================================================
//
// ST_Util_UpdateTextBlink
//
// Draws the parts of the text screen that blink to the bitmap. The bitmap
// must be the proper size for the font.
//
//==========================================================================

void ST_Util_UpdateTextBlink (BITMAPINFO *bitmap_info, const uint8_t *text_screen, const uint8_t *font, bool on)
{
	int x, y;

	for (y = 0; y < 25; ++y)
	{
		for (x = 0; x < 80; ++x)
		{
			if (text_screen[1] & 0x80)
			{
				ST_Util_DrawChar (bitmap_info, font, x, y, on ? text_screen[0] : ' ', text_screen[1]);
				ST_Util_InvalidateRect (Window, bitmap_info, x*8, y*font[0], x*8+8, y*font[0]+font[0]);
			}
			text_screen += 2;
		}
	}
}
