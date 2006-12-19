// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// $Log:$
//
// DESCRIPTION:
//	Main program, simply calls D_DoomMain high level loop.
//
//-----------------------------------------------------------------------------


#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0501
#include <windows.h>
#include <mmsystem.h>
#include <objbase.h>
#include <commctrl.h>

//#include <wtsapi32.h>
#define NOTIFY_FOR_THIS_SESSION 0

#include <malloc.h>
#include "m_alloc.h"
#ifdef _MSC_VER
#include <eh.h>
#include <new.h>
#include <crtdbg.h>
#endif
#include "resource.h"

#include <stdio.h>
#include <stdarg.h>
#include <math.h>

#define USE_WINDOWS_DWORD
#include "doomerrors.h"
#include "hardware.h"

#include "doomtype.h"
#include "m_argv.h"
#include "d_main.h"
#include "i_system.h"
#include "c_console.h"
#include "version.h"
#include "i_video.h"
#include "i_sound.h"
#include "i_input.h"
#include "autosegs.h"
#include "w_wad.h"
#include "templates.h"

#include "stats.h"
#include "st_start.h"

#include <assert.h>

#define WINDOW_TITLE GAMESIG " " DOTVERSIONSTR " (" __DATE__ ")"
LRESULT CALLBACK WndProc (HWND, UINT, WPARAM, LPARAM);

void CreateCrashLog (char *custominfo, DWORD customsize);
extern void DisplayCrashLog ();
extern EXCEPTION_POINTERS CrashPointers;

// Will this work with something besides VC++?
// Answer: yes it will.
// Which brings up the question, what won't it work with?
//
//extern int __argc;
//extern char **__argv;

DArgs Args;

const char WinClassName[] = "ZDoomMainWindow";

HINSTANCE		g_hInst;
DWORD			SessionID;
HANDLE			MainThread;
DWORD			MainThreadID;

// The main window
HWND			Window;

// The subwindows used for startup and error output
HWND			ConWindow, GameTitleWindow;
HWND			ErrorPane, ProgressBar, NetStartPane;

HFONT			GameTitleFont;
LONG			GameTitleFontHeight;

HMODULE			hwtsapi32;		// handle to wtsapi32.dll

extern UINT TimerPeriod;
extern HCURSOR TheArrowCursor, TheInvisibleCursor;

#define MAX_TERMS	32
void (*TermFuncs[MAX_TERMS])(void);
static int NumTerms;

//===========================================================================
//
// atterm
//
// Our own atexit because atexit can be problematic under Linux, though I
// forget the circumstances that cause trouble.
//
//===========================================================================

void atterm (void (*func)(void))
{
	// Make sure this function wasn't already registered.
	for (int i = 0; i < NumTerms; ++i)
	{
		if (TermFuncs[i] == func)
		{
			return;
		}
	}
	if (NumTerms == MAX_TERMS)
	{
		func ();
		I_FatalError ("Too many exit functions registered.\nIncrease MAX_TERMS in i_main.cpp");
	}
	TermFuncs[NumTerms++] = func;
}

//===========================================================================
//
// popterm
//
// Removes the most recently register atterm function.
//
//===========================================================================

void popterm ()
{
	if (NumTerms)
		NumTerms--;
}

//===========================================================================
//
// call_terms
//
//===========================================================================

static void STACK_ARGS call_terms (void)
{
	while (NumTerms > 0)
	{
		TermFuncs[--NumTerms]();
	}
}

#ifdef _MSC_VER
static int STACK_ARGS NewFailure (size_t size)
{
	I_FatalError ("Failed to allocate %d bytes from process heap", size);
	return 0;
}
#endif

//===========================================================================
//
// UnCOM
//
// Called by atterm if CoInitialize() succeeded.
//
//===========================================================================

static void UnCOM (void)
{
	CoUninitialize ();
}

//===========================================================================
//
// UnWTS
//
// Called by atterm if RegisterSessionNotification() succeeded.
//
//===========================================================================

static void UnWTS (void)
{
	if (hwtsapi32 != 0)
	{
		typedef BOOL (WINAPI *ursn)(HWND);
		ursn unreg = (ursn)GetProcAddress (hwtsapi32, "WTSUnRegisterSessionNotification");
		if (unreg != 0)
		{
			unreg (Window);
		}
		FreeLibrary (hwtsapi32);
		hwtsapi32 = 0;
	}
}

//===========================================================================
//
// LayoutErrorPane
//
// Lays out the error pane to the desired width, returning the required
// height.
//
//===========================================================================

static int LayoutErrorPane (HWND pane, int w)
{
	HWND ctl;
	RECT margin, rectc;
	int cxicon, cyicon;
	const char *text;
	int textheight, textwidth, padding;

	// Determine margin sizes.
	SetRect (&margin, 7, 7, 0, 0);
	MapDialogRect (pane, &margin);

	// Get size of icon.
	cxicon = GetSystemMetrics (SM_CXICON);
	cyicon = GetSystemMetrics (SM_CYICON);

	// Determine size of text control.
	text = (const char *)(LONG_PTR)GetWindowLongPtr (pane, DWLP_USER);
	textwidth = w - margin.left*3 - cxicon;
	textheight = 0;
	if (text != NULL)
	{
		HWND ctl;
		HDC dc;
		HGDIOBJ oldfont;
		int len;

		ctl = GetDlgItem (pane, IDC_ERRORTEXT);
		rectc.top = rectc.left = 0;
		rectc.right = textwidth;
		rectc.bottom = 0;
		len = (int)strlen(text);
		dc = GetDC (pane);
		oldfont = SelectObject (dc, (HFONT)SendMessage (ctl, WM_GETFONT, 0, 0));
		DrawText (dc, text, len, &rectc, DT_CALCRECT | DT_EDITCONTROL | DT_NOPREFIX | DT_EXPANDTABS | DT_WORDBREAK | DT_NOCLIP);
		SelectObject (dc, oldfont);
		ReleaseDC (pane, dc);
		textheight = rectc.bottom;
	}

	// Fill the text box to the determined size. If it is shorter than the icon,
	// center it vertically.
	padding = MAX (cyicon - textheight, 0);
	ctl = GetDlgItem (pane, IDC_ERRORTEXT);
	MoveWindow (ctl, margin.left*2 + cxicon, margin.top + padding/2, textwidth, textheight, TRUE);
	InvalidateRect (ctl, NULL, TRUE);
	textheight += padding;

	// Center the Okay button horizontally, just underneath the text box.
	ctl = GetDlgItem (pane, IDOK);
	GetClientRect (ctl, &rectc);	// Find out how big it is.
	MoveWindow (ctl, (w - rectc.right) / 2, margin.top*2 + textheight, rectc.right, rectc.bottom, TRUE);
	InvalidateRect (ctl, NULL, TRUE);

	// Return the needed height for this layout
	return margin.top*3 + textheight + rectc.bottom;
}

//===========================================================================
//
// LayoutNetStartPane
//
// Lays out the network startup pane to the specified width, returning
// its required height.
//
//===========================================================================

int LayoutNetStartPane (HWND pane, int w)
{
	HWND ctl;
	RECT margin, rectc;
	int staticheight, barheight;

	// Determine margin sizes.
	SetRect (&margin, 7, 7, 0, 0);
	MapDialogRect (pane, &margin);

	// Stick the message text in the upper left corner.
	ctl = GetDlgItem (pane, IDC_NETSTARTMESSAGE);
	GetClientRect (ctl, &rectc);
	MoveWindow (ctl, margin.left, margin.top, rectc.right, rectc.bottom, TRUE);

	// Stick the count text in the upper right corner.
	ctl = GetDlgItem (pane, IDC_NETSTARTCOUNT);
	GetClientRect (ctl, &rectc);
	MoveWindow (ctl, w - rectc.right - margin.left, margin.top, rectc.right, rectc.bottom, TRUE);
	staticheight = rectc.bottom;

	// Stretch the progress bar to fill the entire width.
	ctl = GetDlgItem (pane, IDC_NETSTARTPROGRESS);
	barheight = GetSystemMetrics (SM_CYVSCROLL);
	MoveWindow (ctl, margin.left, margin.top*2 + staticheight, w - margin.left*2, barheight, TRUE);

	// Center the abort button underneath the progress bar.
	ctl = GetDlgItem (pane, IDCANCEL);
	GetClientRect (ctl, &rectc);
	MoveWindow (ctl, (w - rectc.right) / 2, margin.top*3 + staticheight + barheight, rectc.right, rectc.bottom, TRUE);

	return margin.top*4 + staticheight + barheight + rectc.bottom;
}

//===========================================================================
//
// LayoutMainWindow
//
// Lays out the main window with the game title and log controls and
// possibly the error pane and progress bar.
//
//===========================================================================

void LayoutMainWindow (HWND hWnd, HWND pane)
{
	RECT rect;
	int errorpaneheight = 0;
	int bannerheight = 0;
	int progressheight = 0;
	int netpaneheight = 0;
	int w, h;

	GetClientRect (hWnd, &rect);
	w = rect.right;
	h = rect.bottom;

	if (pane != NULL)
	{
		errorpaneheight = LayoutErrorPane (pane, w);
		SetWindowPos (pane, HWND_TOP, 0, 0, w, errorpaneheight, 0);
	}
	if (DoomStartupInfo != NULL)
	{
		bannerheight = GameTitleFontHeight + 5;
		MoveWindow (GameTitleWindow, 0, errorpaneheight, w, bannerheight, TRUE);
		InvalidateRect (GameTitleWindow, NULL, FALSE);
	}
	if (ProgressBar != NULL)
	{
		// Base the height of the progress bar on the height of a scroll bar
		// arrow, just as in the progress bar example.
		progressheight = GetSystemMetrics (SM_CYVSCROLL);
		MoveWindow (ProgressBar, 0, h - progressheight, w, progressheight, TRUE);
	}
	if (NetStartPane != NULL)
	{
		netpaneheight = LayoutNetStartPane (NetStartPane, w);
		SetWindowPos (NetStartPane, HWND_TOP, 0, h - progressheight - netpaneheight, w, netpaneheight, SWP_SHOWWINDOW);
		RECT foo;
		GetClientRect (NetStartPane, &foo);
		foo.bottom = foo.bottom;;
	}
	// The log window uses whatever space is left.
	MoveWindow (ConWindow, 0, errorpaneheight + bannerheight, w,
		h - bannerheight - errorpaneheight - progressheight - netpaneheight, TRUE);
}

//===========================================================================
//
// LConProc
//
// The main window's WndProc during startup. During gameplay, the WndProc
// in i_input.cpp is used instead.
//
//===========================================================================

LRESULT CALLBACK LConProc (HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	HWND view;
	HDC hdc;
	HBRUSH hbr;
	HGDIOBJ oldfont;
	RECT rect;
	int titlelen;
	SIZE size;
	LOGFONT lf;
	TEXTMETRIC tm;
	HINSTANCE inst = (HINSTANCE)(LONG_PTR)GetWindowLongPtr(hWnd, GWLP_HINSTANCE);
	HFONT font;
	DRAWITEMSTRUCT *drawitem;

	switch (msg)
	{
	case WM_CREATE:
		// Create game title static control
		memset (&lf, 0, sizeof(lf));
		hdc = GetDC (hWnd);
		lf.lfHeight = -MulDiv(12, GetDeviceCaps(hdc, LOGPIXELSY), 72);
		lf.lfCharSet = ANSI_CHARSET;
		lf.lfWeight = FW_BOLD;
		lf.lfPitchAndFamily = VARIABLE_PITCH | FF_ROMAN;
		strcpy (lf.lfFaceName, "Trebuchet MS");
		GameTitleFont = CreateFontIndirect (&lf);

		font = GameTitleFont != NULL ? GameTitleFont : (HFONT)GetStockObject (DEFAULT_GUI_FONT);
		oldfont = SelectObject (hdc, font);
		GetTextMetrics (hdc, &tm);
		SelectObject (hdc, oldfont);
		ReleaseDC (hWnd, hdc);

		GameTitleFontHeight = tm.tmHeight;

		// Create log read-only edit control
		view = CreateWindowEx (WS_EX_NOPARENTNOTIFY | WS_EX_CLIENTEDGE, "EDIT", NULL,
			WS_CHILD | WS_VISIBLE | WS_VSCROLL |
			ES_LEFT | ES_MULTILINE | WS_CLIPSIBLINGS,
			0, 0, 0, 0,
			hWnd, NULL, inst, NULL);
		if (view == NULL)
		{
			return -1;
		}
		SendMessage (view, WM_SETFONT, (WPARAM)GetStockObject (DEFAULT_GUI_FONT), FALSE);
		SendMessage (view, EM_SETREADONLY, TRUE, 0);
		SendMessage (view, EM_SETLIMITTEXT, 0, 0);
		ConWindow = view;

		view = CreateWindowEx (WS_EX_NOPARENTNOTIFY, "STATIC", NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | SS_OWNERDRAW, 0, 0, 0, 0, hWnd, NULL, inst, NULL);
		if (view == NULL)
		{
			return -1;
		}
		GameTitleWindow = view;

		return 0;

	case WM_SIZE:
		if (wParam != SIZE_MAXHIDE && wParam != SIZE_MAXSHOW)
		{
			LayoutMainWindow (hWnd, ErrorPane);
		}
		return 0;

	case WM_DRAWITEM:
		if (DoomStartupInfo != NULL)
		{
			const PalEntry *c;

			// Draw the game title strip at the top of the window.
			drawitem = (LPDRAWITEMSTRUCT)lParam;

			// Draw the background.
			rect = drawitem->rcItem;
			rect.bottom -= 1;
			c = (const PalEntry *)&DoomStartupInfo->BkColor;
			hbr = CreateSolidBrush (RGB(c->r,c->g,c->b));
			FillRect (drawitem->hDC, &drawitem->rcItem, hbr);
			DeleteObject (hbr);

			// Calculate width of the title string.
			SetTextAlign (drawitem->hDC, TA_TOP);
			oldfont = SelectObject (drawitem->hDC, GameTitleFont != NULL ? GameTitleFont : (HFONT)GetStockObject (DEFAULT_GUI_FONT));
			titlelen = (int)strlen (DoomStartupInfo->Name);
			GetTextExtentPoint32 (drawitem->hDC, DoomStartupInfo->Name, titlelen, &size);

			// Draw the title.
			c = (const PalEntry *)&DoomStartupInfo->FgColor;
			SetTextColor (drawitem->hDC, RGB(c->r,c->g,c->b));
			SetBkMode (drawitem->hDC, TRANSPARENT);
			TextOut (drawitem->hDC, rect.left + (rect.right - rect.left - size.cx) / 2, 2, DoomStartupInfo->Name, titlelen);
			SelectObject (drawitem->hDC, oldfont);
			return TRUE;
		}
		return FALSE;

	case WM_CLOSE:
		PostQuitMessage (0);
		break;

	case WM_CTLCOLORSTATIC:
		return (LRESULT)GetStockObject (WHITE_BRUSH);

	case WM_DESTROY:
		if (GameTitleFont != NULL)
		{
			DeleteObject (GameTitleFont);
		}
		break;
	}
	return DefWindowProc (hWnd, msg, wParam, lParam);
}

//===========================================================================
//
// ErrorPaneProc
//
// DialogProc for the error pane.
//
//===========================================================================

INT_PTR CALLBACK ErrorPaneProc (HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	HWND ctl;

	switch (msg)
	{
	case WM_INITDIALOG:
		SetWindowLongPtr (hDlg, DWLP_USER, lParam);

		// Set the static control to the error text.
		ctl = GetDlgItem (hDlg, IDC_ERRORTEXT);
		SetWindowText (ctl, (LPCSTR)lParam);

		// Set the icon to the system default error icon.
		ctl = GetDlgItem (hDlg, IDC_ICONPIC);
		SendMessage (ctl, STM_SETICON, (WPARAM)LoadIcon (NULL, IDI_ERROR), 0);

		// Appear in the main window.
		LayoutMainWindow (GetParent (hDlg), hDlg);

		// Make sure the last line of output is visible in the log window.
		SendMessage (ConWindow, EM_LINESCROLL, 0, SendMessage (ConWindow, EM_GETLINECOUNT, 0, 0) -
			SendMessage (ConWindow, EM_GETFIRSTVISIBLELINE, 0, 0));
		return TRUE;

	case WM_COMMAND:
		// There is only one button, and it's "Ok" and makes us quit.
		if (HIWORD(wParam) == BN_CLICKED)
		{
			PostQuitMessage (0);
			return TRUE;
		}
		break;
	}
	return FALSE;
}

//===========================================================================
//
// I_SetWndProc
//
// Sets the main WndProc, hides all the child windows, and starts up
// in-game input.
//
//===========================================================================

void I_SetWndProc()
{
	if (GetWindowLongPtr (Window, GWLP_USERDATA) == 0)
	{
		SetWindowLongPtr (Window, GWLP_USERDATA, 1);
		SetWindowLongPtr (Window, GWLP_WNDPROC, (WLONG_PTR)WndProc);
		ShowWindow (ConWindow, SW_HIDE);
		ShowWindow (GameTitleWindow, SW_HIDE);
		I_InitInput (Window);
	}
}

//===========================================================================
//
// RestoreConView
//
// Returns the main window to its startup state.
//
//===========================================================================

void RestoreConView()
{
	// Make sure the window has a frame in case it was fullscreened.
	SetWindowLongPtr (Window, GWL_STYLE, WS_VISIBLE|WS_OVERLAPPEDWINDOW);
	if (GetWindowLong (Window, GWL_EXSTYLE) & WS_EX_TOPMOST)
	{
		SetWindowPos (Window, HWND_BOTTOM, 0, 0, 512, 384,
			SWP_DRAWFRAME | SWP_NOCOPYBITS | SWP_NOMOVE);
		SetWindowPos (Window, HWND_TOP, 0, 0, 0, 0, SWP_NOCOPYBITS | SWP_NOMOVE | SWP_NOSIZE);
	}
	else
	{
		SetWindowPos (Window, NULL, 0, 0, 512, 384,
			SWP_DRAWFRAME | SWP_NOCOPYBITS | SWP_NOMOVE | SWP_NOZORDER);
	}

	SetWindowLongPtr (Window, GWLP_WNDPROC, (WLONG_PTR)LConProc);
	ShowWindow (ConWindow, SW_SHOW);
	ShowWindow (GameTitleWindow, SW_SHOW);
	// Make sure the progress bar isn't visible.
	ST_Done();
}

//===========================================================================
//
// ShowErrorPane
//
// Shows an error message, preferably in the main window, but it can
// use a normal message box too.
//
//===========================================================================

void ShowErrorPane(const char *text)
{
	size_t len = strlen(text);

	ErrorPane = CreateDialogParam (g_hInst, MAKEINTRESOURCE(IDD_ERRORPANE), Window, ErrorPaneProc, (LPARAM)text);

	if (ErrorPane == NULL)
	{
		MessageBox (Window, text,
			"ZDOOM Fatal Error", MB_OK|MB_ICONSTOP|MB_TASKMODAL);
	}
	else
	{
		BOOL bRet;
		MSG msg;

		SetWindowText (Window, "Fatal Error - " WINDOW_TITLE);

		while ((bRet = GetMessage(&msg, NULL, 0, 0)) != 0)
		{
			if (bRet == -1)
			{
				MessageBox (Window, text,
					"ZDOOM Fatal Error", MB_OK|MB_ICONSTOP|MB_TASKMODAL);
				return;
			}
			else if (!IsDialogMessage (ErrorPane, &msg))
			{
				TranslateMessage (&msg);
				DispatchMessage (&msg);
			}
		}
	}
}

//===========================================================================
//
// DoMain
//
//===========================================================================

void DoMain (HINSTANCE hInstance)
{
	LONG WinWidth, WinHeight;
	int height, width, x, y;
	RECT cRect;
	TIMECAPS tc;

	try
	{
#ifdef _MSC_VER
		_set_new_handler (NewFailure);
#endif

		Args.SetArgs (__argc, __argv);

		// Under XP, get our session ID so we can know when the user changes/locks sessions.
		// Since we need to remain binary compatible with older versions of Windows, we
		// need to extract the ProcessIdToSessionId function from kernel32.dll manually.
		HMODULE kernel = GetModuleHandle ("kernel32.dll");

		// NASM does not support creating writeable code sections (even though this
		// is a perfectly valid configuration for Microsoft's COFF format), so I
		// need to make the self-modifying code writeable after it's already loaded.
#ifdef USEASM
	{
		BYTE *module = (BYTE *)GetModuleHandle (NULL);
		IMAGE_DOS_HEADER *dosHeader = (IMAGE_DOS_HEADER *)module;
		IMAGE_NT_HEADERS *ntHeaders = (IMAGE_NT_HEADERS *)(module + dosHeader->e_lfanew);
		IMAGE_SECTION_HEADER *sections = IMAGE_FIRST_SECTION (ntHeaders);
		int i;
		LPVOID *start = NULL;
		SIZE_T size = 0;
		DWORD oldprotect;

		for (i = 0; i < ntHeaders->FileHeader.NumberOfSections; ++i)
		{
			if (memcmp (sections[i].Name, ".rtext\0", 8) == 0)
			{
				start = (LPVOID *)(sections[i].VirtualAddress + module);
				size = sections[i].Misc.VirtualSize;
				break;
			}
		}

		// I think these pages need to be mapped PAGE_EXECUTE_WRITECOPY (based on the
		// description of PAGE_WRITECOPY), but PAGE_EXECUTE_READWRITE seems to work
		// just as well; two instances of the program can be running with different
		// resolutions at the same time either way. Perhaps the file mappings for
		// executables are created with PAGE_WRITECOPY, so any attempts to give them
		// write access are automatically transformed to copy-on-write?
		//
		// This used to be PAGE_EXECUTE_WRITECOPY until Timmie found out Win9x doesn't
		// support it, although the MSDN does not indicate it.
		if (!VirtualProtect (start, size, PAGE_EXECUTE_READWRITE, &oldprotect))
		{
			I_FatalError ("The self-modifying code section code not be made writeable.");
		}
	}
#endif

		// Set the timer to be as accurate as possible
		if (timeGetDevCaps (&tc, sizeof(tc)) != TIMERR_NOERROR)
			TimerPeriod = 1;	// Assume minimum resolution of 1 ms
		else
			TimerPeriod = tc.wPeriodMin;

		timeBeginPeriod (TimerPeriod);

		/*
		killough 1/98:

		This fixes some problems with exit handling
		during abnormal situations.

		The old code called I_Quit() to end program,
		while now I_Quit() is installed as an exit
		handler and exit() is called to exit, either
		normally or abnormally.
		*/

		atexit (call_terms);

		atterm (I_Quit);

		// Figure out what directory the program resides in.
		GetModuleFileName (NULL, progdir, 1024);
		*(strrchr (progdir, '\\') + 1) = 0;
		FixPathSeperator (progdir);
/*
		height = GetSystemMetrics (SM_CYFIXEDFRAME) * 2 +
				GetSystemMetrics (SM_CYCAPTION) + 12 * 32;
		width  = GetSystemMetrics (SM_CXFIXEDFRAME) * 2 + 8 * 78;
*/
		width = 512;
		height = 384;

		DEVMODE displaysettings = { sizeof(displaysettings) };
		EnumDisplaySettings (NULL, ENUM_CURRENT_SETTINGS, &displaysettings);
		x = (displaysettings.dmPelsWidth - width) / 2;
		y = (displaysettings.dmPelsHeight - height) / 2;
#if _DEBUG
		x = y = 0;
#endif

		TheInvisibleCursor = LoadCursor (hInstance, MAKEINTRESOURCE(IDC_INVISIBLECURSOR));
		TheArrowCursor = LoadCursor (NULL, IDC_ARROW);

		WNDCLASS WndClass;
		WndClass.style			= 0;
		WndClass.lpfnWndProc	= LConProc;
		WndClass.cbClsExtra		= 0;
		WndClass.cbWndExtra		= 0;
		WndClass.hInstance		= hInstance;
		WndClass.hIcon			= LoadIcon (hInstance, MAKEINTRESOURCE(IDI_ICON1));
		WndClass.hCursor		= TheArrowCursor;
		WndClass.hbrBackground	= NULL;
		WndClass.lpszMenuName	= NULL;
		WndClass.lpszClassName	= (LPCTSTR)WinClassName;
		
		/* register this new class with Windows */
		if (!RegisterClass((LPWNDCLASS)&WndClass))
			I_FatalError ("Could not register window class");
		
		/* create window */
		Window = CreateWindowEx(
				WS_EX_APPWINDOW,
				(LPCTSTR)WinClassName,
				(LPCTSTR)WINDOW_TITLE,
				WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_CLIPCHILDREN,
				x, y, width, height,
				(HWND)   NULL,
				(HMENU)  NULL,
						hInstance,
				NULL);

		if (!Window)
			I_FatalError ("Could not open window");

		if (kernel != 0)
		{
			typedef BOOL (WINAPI *pts)(DWORD, DWORD *);
			pts pidsid = (pts)GetProcAddress (kernel, "ProcessIdToSessionId");
			if (pidsid != 0)
			{
				if (!pidsid (GetCurrentProcessId(), &SessionID))
				{
					SessionID = 0;
				}
				hwtsapi32 = LoadLibraryA ("wtsapi32.dll");
				if (hwtsapi32 != 0)
				{
					FARPROC reg = GetProcAddress (hwtsapi32, "WTSRegisterSessionNotification");
					if (reg == 0 || !((BOOL(WINAPI *)(HWND, DWORD))reg) (Window, NOTIFY_FOR_THIS_SESSION))
					{
						FreeLibrary (hwtsapi32);
						hwtsapi32 = 0;
					}
					else
					{
						atterm (UnWTS);
					}
				}
			}
		}

		GetClientRect (Window, &cRect);

		WinWidth = cRect.right;
		WinHeight = cRect.bottom;

		CoInitialize (NULL);
		atterm (UnCOM);

		C_InitConsole (((WinWidth / 8) + 2) * 8, (WinHeight / 12) * 8, false);

		I_DetectOS ();
		D_DoomMain ();
	}
	catch (class CDoomError &error)
	{
		I_ShutdownGraphics ();
		RestoreConView ();
		if (error.GetMessage ())
		{
			ShowErrorPane (error.GetMessage());
		}
		exit (-1);
	}
}

//===========================================================================
//
// DoomSpecificInfo
//
// Called by the crash logger to get application-specific information.
//
//===========================================================================

void DoomSpecificInfo (char *buffer)
{
	const char *arg;
	int i;

	buffer += wsprintf (buffer, "ZDoom version " DOTVERSIONSTR " (" __DATE__ ")\r\n");
	buffer += wsprintf (buffer, "\r\nCommand line: %s\r\n", GetCommandLine());

	for (i = 0; (arg = Wads.GetWadName (i)) != NULL; ++i)
	{
		buffer += wsprintf (buffer, "\r\nWad %d: %s", i, arg);
	}

	if (gamestate != GS_LEVEL && gamestate != GS_TITLELEVEL)
	{
		buffer += wsprintf (buffer, "\r\n\r\nNot in a level.");
	}
	else
	{
		char name[9];

		strncpy (name, level.mapname, 8);
		name[8] = 0;
		buffer += wsprintf (buffer, "\r\n\r\nCurrent map: %s", name);

		if (!viewactive)
		{
			buffer += wsprintf (buffer, "\r\n\r\nView not active.");
		}
		else
		{
			buffer += wsprintf (buffer, "\r\n\r\nviewx = %d", viewx);
			buffer += wsprintf (buffer, "\r\nviewy = %d", viewy);
			buffer += wsprintf (buffer, "\r\nviewz = %d", viewz);
			buffer += wsprintf (buffer, "\r\nviewangle = %x", viewangle);
		}
	}
	*buffer++ = '\r';
	*buffer++ = '\n';
	*buffer++ = '\0';
}

extern FILE *Logfile;

// Here is how the error logging system works.
//
// To catch exceptions that occur in secondary threads, CatchAllExceptions is
// set as the UnhandledExceptionFilter for this process. It records the state
// of the thread at the time of the crash using CreateCrashLog and then queues
// an APC on the primary thread. When the APC executes, it raises a software
// exception that gets caught by the __try/__except block in WinMain.
// I_GetEvent calls SleepEx to put the primary thread in a waitable state
// periodically so that the APC has a chance to execute.
//
// Exceptions on the primary thread are caught by the __try/__except block in
// WinMain. Not only does it record the crash information, it also shuts
// everything down and displays a dialog with the information present. If a
// console log is being produced, the information will also be appended to it.
//
// If a debugger is running, CatchAllExceptions never executes, so secondary
// thread exceptions will always be caught by the debugger. For the primary
// thread, IsDebuggerPresent is called to determine if a debugger is present.
// Note that this function is not present on Windows 95, so we cannot
// statically link to it.
//
// To make this work with MinGW, you will need to use inline assembly
// because GCC offers no native support for Windows' SEH.

//===========================================================================
//
// SleepForever
//
//===========================================================================

void SleepForever ()
{
	Sleep (INFINITE);
}

//===========================================================================
//
// ExitMessedUp
//
// An exception occurred while exiting, so don't do any standard processing.
// Just die.
//
//===========================================================================

LONG WINAPI ExitMessedUp (LPEXCEPTION_POINTERS foo)
{
	ExitProcess (1000);
}

//===========================================================================
//
// ExitFatally
//
//===========================================================================

void CALLBACK ExitFatally (ULONG_PTR dummy)
{
	SetUnhandledExceptionFilter (ExitMessedUp);
	I_ShutdownGraphics ();
	RestoreConView ();
	DisplayCrashLog ();
	exit (-1);
}

//===========================================================================
//
// CatchAllExceptions
//
//===========================================================================

LONG WINAPI CatchAllExceptions (LPEXCEPTION_POINTERS info)
{
#ifdef _DEBUG
	if (info->ExceptionRecord->ExceptionCode == EXCEPTION_BREAKPOINT)
	{
		return EXCEPTION_CONTINUE_SEARCH;
	}
#endif

	static bool caughtsomething = false;

	if (caughtsomething) return EXCEPTION_EXECUTE_HANDLER;
	caughtsomething = true;

	char *custominfo = (char *)HeapAlloc (GetProcessHeap(), 0, 16384);

	CrashPointers = *info;
	DoomSpecificInfo (custominfo);
	CreateCrashLog (custominfo, (DWORD)strlen(custominfo));

	// If the main thread crashed, then make it clean up after itself.
	// Otherwise, put the crashing thread to sleep and signal the main thread to clean up.
	if (GetCurrentThreadId() == MainThreadID)
	{
		info->ContextRecord->Eip = (DWORD_PTR)ExitFatally;
	}
	else
	{
		info->ContextRecord->Eip = (DWORD_PTR)SleepForever;
		QueueUserAPC (ExitFatally, MainThread, 0);
	}
	return EXCEPTION_CONTINUE_EXECUTION;
}

//===========================================================================
//
// infiniterecursion
//
// Debugging routine for testing the crash logger.
//
//===========================================================================

#ifdef _DEBUG
static void infiniterecursion(int foo)
{
	if (foo)
	{
		infiniterecursion(foo);
	}
}
#endif

//===========================================================================
//
// WinMain
//
//===========================================================================

int WINAPI WinMain (HINSTANCE hInstance, HINSTANCE nothing, LPSTR cmdline, int nCmdShow)
{
	g_hInst = hInstance;

	InitCommonControls ();	// Be pretty under XP

#if !defined(__GNUC__) && defined(_DEBUG)
	if (__argc == 2 && strcmp (__argv[1], "TestCrash") == 0)
	{
		__try
		{
			*(int *)0 = 0;
		}
		__except(CrashPointers = *GetExceptionInformation(),
			CreateCrashLog (__argv[1], 9), EXCEPTION_EXECUTE_HANDLER)
		{
		}
		DisplayCrashLog ();
		exit (0);
	}
	if (__argc == 2 && strcmp (__argv[1], "TestStackCrash") == 0)
	{
		__try
		{
			infiniterecursion(1);
		}
		__except(CrashPointers = *GetExceptionInformation(),
			CreateCrashLog (__argv[1], 14), EXCEPTION_EXECUTE_HANDLER)
		{
		}
		DisplayCrashLog ();
		exit (0);
	}
#endif

	MainThread = INVALID_HANDLE_VALUE;
	DuplicateHandle (GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &MainThread,
		0, FALSE, DUPLICATE_SAME_ACCESS);
	MainThreadID = GetCurrentThreadId();

#ifndef _DEBUG
	if (MainThread != INVALID_HANDLE_VALUE)
	{
		SetUnhandledExceptionFilter (CatchAllExceptions);
	}
#endif

#if defined(_DEBUG) && defined(_MSC_VER)
	// Uncomment this line to make the Visual C++ CRT check the heap before
	// every allocation and deallocation. This will be slow, but it can be a
	// great help in finding problem areas.
	//_CrtSetDbgFlag (_CRTDBG_ALLOC_MEM_DF | _CRTDBG_CHECK_ALWAYS_DF);
	_CrtSetDbgFlag (_CrtSetDbgFlag(0) | _CRTDBG_LEAK_CHECK_DF);
#endif

	DoMain (hInstance);

	CloseHandle (MainThread);
	MainThread = INVALID_HANDLE_VALUE;
	return 0;
}

//===========================================================================
//
// CCMD crashout
//
// Debugging routine for testing the crash logger.
// Useless in a debug build, because that doesn't enable the crash logger.
//
//===========================================================================

#ifndef _DEBUG
#include "c_dispatch.h"
CCMD (crashout)
{
	*(int *)0 = 0;
}
#endif
