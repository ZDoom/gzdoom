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
#define _WIN32_WINNT 0x0500
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
#include "gccseh.h"
#include "autosegs.h"
#include "w_wad.h"

#include "stats.h"


#include <assert.h>

LRESULT CALLBACK WndProc (HWND, UINT, WPARAM, LPARAM);

extern void CreateCrashLog (char *(*userCrashInfo)(char *text, char *maxtext));
extern void DisplayCrashLog ();
extern EXCEPTION_POINTERS CrashPointers;
extern char *CrashText;

// Will this work with something besides VC++?
// Answer: yes it will.
// Which brings up the question, what won't it work with?
//
//extern int __argc;
//extern char **__argv;

DArgs Args;

const char WinClassName[] = "ZDOOM WndClass";
const char ConClassName[] = "ZDOOM Logview";

HINSTANCE		g_hInst;
WNDCLASS		WndClass;
HWND			Window, ConWindow;
DWORD			SessionID;
HANDLE			MainThread;

HMODULE			hwtsapi32;		// handle to wtsapi32.dll

BOOL			(*pIsDebuggerPresent)(VOID);

extern UINT TimerPeriod;
extern HCURSOR TheArrowCursor, TheInvisibleCursor;

#define MAX_TERMS	16
void (STACK_ARGS *TermFuncs[MAX_TERMS])(void);
static int NumTerms;

void atterm (void (STACK_ARGS *func)(void))
{
	if (NumTerms == MAX_TERMS)
		I_FatalError ("Too many exit functions registered.\nIncrease MAX_TERMS in i_main.cpp");
	TermFuncs[NumTerms++] = func;
}

void popterm ()
{
	if (NumTerms)
		NumTerms--;
}

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

static void STACK_ARGS UnCOM (void)
{
	CoUninitialize ();
}

static void STACK_ARGS UnWTS (void)
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

LRESULT CALLBACK LConProc (HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	LPMINMAXINFO minmax;
	HWND view;

	switch (msg)
	{
	case WM_SIZE:
		if (wParam != SIZE_MAXHIDE && wParam != SIZE_MAXSHOW)
		{
			MoveWindow ((HWND)GetWindowLongPtr (hWnd, GWLP_USERDATA),
				0, 0, LOWORD(lParam), HIWORD(lParam), TRUE);
		}
		return 0;

	case WM_GETMINMAXINFO:
		minmax = (LPMINMAXINFO)lParam;
		minmax->ptMinTrackSize.x *= 10;
		minmax->ptMinTrackSize.y *= 8;
		break;
	
	case WM_CREATE:
		view = CreateWindow ("EDIT", NULL,
			WS_CHILD | WS_VISIBLE | WS_VSCROLL |
			ES_LEFT | ES_MULTILINE,
			0, 0, 0, 0,
			hWnd, NULL, (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE), NULL);
		if (view == NULL)
		{
			return -1;
		}
		SetWindowLongPtr (hWnd, GWLP_USERDATA, (LONG_PTR)view);
		SendMessage (view, WM_SETFONT, (WPARAM)GetStockObject (DEFAULT_GUI_FONT), FALSE);
		SendMessage (view, EM_SETREADONLY, TRUE, 0);
		return 0;
	}
	return DefWindowProc (hWnd, msg, wParam, lParam);
}

void I_HideLogWindow ()
{
	if (ConWindow != NULL && 0)
	{
		ShowWindow (ConWindow, SW_HIDE);
	}
}

void DoMain (HINSTANCE hInstance)
{
	LONG WinWidth, WinHeight;
	int height, width;
	RECT cRect;
	TIMECAPS tc;

	try
	{
#ifdef _MSC_VER
		_set_new_handler (NewFailure);
#endif

		g_hInst = hInstance;
		Args.SetArgs (__argc, __argv);

		// Under XP, get our session ID so we can know when the user changes/locks sessions.
		// Since we need to remain binary compatible with older versions of Windows, we
		// need to extract the ProcessIdToSessionId function from kernel32.dll manually.
		HMODULE kernel = LoadLibraryA ("kernel32.dll");
		if (kernel != 0)
		{
			pIsDebuggerPresent = (BOOL(*)())GetProcAddress (kernel, "IsDebuggerPresent");
		}

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

		// Let me see fancy visual styles under XP
		InitCommonControls ();

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

		height = GetSystemMetrics (SM_CYFIXEDFRAME) * 2 +
				GetSystemMetrics (SM_CYCAPTION) + 12 * 32;
		width  = GetSystemMetrics (SM_CXFIXEDFRAME) * 2 + 8 * 78;

		TheInvisibleCursor = LoadCursor (hInstance, MAKEINTRESOURCE(IDC_INVISIBLECURSOR));
		TheArrowCursor = LoadCursor (NULL, IDC_ARROW);

		WndClass.style			= 0;
		WndClass.lpfnWndProc	= WndProc;
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
		Window = CreateWindow((LPCTSTR)WinClassName,
				(LPCTSTR) "ZDOOM " DOTVERSIONSTR " (" __DATE__ ")",
				WS_OVERLAPPEDWINDOW,
				0/*CW_USEDEFAULT*/, 1/*CW_USEDEFAULT*/, width, height,
				(HWND)   NULL,
				(HMENU)  NULL,
						hInstance,
				NULL);

		if (!Window)
			I_FatalError ("Could not open window");

		WndClass.lpfnWndProc = LConProc;
		WndClass.lpszClassName = (LPCTSTR)ConClassName;
		if (RegisterClass ((LPWNDCLASS)&WndClass))
		{
			ConWindow = CreateWindowEx (
				WS_EX_PALETTEWINDOW & (~WS_EX_TOPMOST),
				(LPCTSTR)ConClassName,
				(LPCTSTR) "ZDoom Startup Viewer",
				WS_OVERLAPPEDWINDOW/* | WS_VISIBLE*/,
				CW_USEDEFAULT, CW_USEDEFAULT,
				512, 384,
				Window, NULL, hInstance, NULL);
		}

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
			FreeLibrary (kernel);
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
		I_ShutdownHardware ();
		SetWindowPos (Window, NULL, 0, 0, 0, 0, SWP_HIDEWINDOW);
		if (ConWindow != NULL)
		{
			ShowWindow (ConWindow, SW_SHOW);
		}
		if (error.GetMessage ())
			MessageBox (Window, error.GetMessage(),
				"ZDOOM Fatal Error", MB_OK|MB_ICONSTOP|MB_TASKMODAL);
		exit (-1);
	}
}

char *DoomSpecificInfo (char *text, char *maxtext)
{
	const char *arg;
	int i;

	text += wsprintf (text, "\r\nZDoom version " DOTVERSIONSTR "\r\n");

	text += wsprintf (text, "\r\nCommand line:\r\n");
	for (i = 0; i < Args.NumArgs(); ++i)
	{
		arg = Args.GetArg(i);
		if (text + strlen(arg) + 4 >= maxtext)
			goto done;
		text += wsprintf (text, " %s", arg);
	}

	arg = Wads.GetWadName (FWadCollection::IWAD_FILENUM);
	if (arg != NULL)
	{
		if (text + strlen(arg) + 10 >= maxtext)
			goto done;
		text += wsprintf (text, "\r\nIWAD: %s", arg);
	}

	if (gamestate != GS_LEVEL && gamestate != GS_TITLELEVEL)
	{
		if (text + 32 < maxtext)
			text += wsprintf (text, "\r\n\r\nNot in a level.");
	}
	else
	{
		char name[9];
		if (text + 32 < maxtext)
		{
			strncpy (name, level.mapname, 8);
			name[8] = 0;
			text += wsprintf (text, "\r\n\r\nCurrent map: %s", name);
		}
		if (!viewactive)
		{
			if (text + 32 < maxtext)
				text += wsprintf (text, "\r\n\r\nView not active.");
		}
		else
		{
			if (text + 32 < maxtext)
				text += wsprintf (text, "\r\n\r\nviewx = %d", viewx);
			if (text + 32 < maxtext)
				text += wsprintf (text, "\r\nviewy = %d", viewy);
			if (text + 32 < maxtext)
				text += wsprintf (text, "\r\nviewz = %d", viewz);
			if (text + 32 < maxtext)
				text += wsprintf (text, "\r\nviewangle = %u", viewangle);
		}
	}

done:
	*text++ = '\r';
	*text++ = '\n';
	*text = 0;
	return text;
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

#ifndef __GNUC__
void SleepForever ()
{
	Sleep (INFINITE);
}

void CALLBACK TimeToDie (ULONG_PTR dummy)
{
	RaiseException (0xE0000000+'D'+'i'+'e'+'!', EXCEPTION_NONCONTINUABLE, 0, NULL);
}

LONG WINAPI CatchAllExceptions (LPEXCEPTION_POINTERS info)
{
	CrashPointers = *info;

#ifdef _DEBUG
	if (info->ExceptionRecord->ExceptionCode == EXCEPTION_BREAKPOINT)
	{
		return EXCEPTION_CONTINUE_SEARCH;
	}
#endif

	CreateCrashLog (DoomSpecificInfo);
	QueueUserAPC (TimeToDie, MainThread, 0);

	// Put the crashing thread to sleep until the process exits.
	info->ContextRecord->Eip = (DWORD)SleepForever;
	return EXCEPTION_CONTINUE_EXECUTION;
	//return EXCEPTION_EXECUTE_HANDLER;
}
#endif

int WINAPI WinMain (HINSTANCE hInstance, HINSTANCE nothing, LPSTR cmdline, int nCmdShow)
{
#ifdef REGEXEPEEK
	InitAutoSegMarkers ();
#endif

	MainThread = INVALID_HANDLE_VALUE;
	DuplicateHandle (GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &MainThread,
		0, FALSE, DUPLICATE_SAME_ACCESS);

#ifdef _DEBUG
	// Uncomment this line to make the Visual C++ CRT check the heap before
	// every allocation and deallocation. This will be slow, but it can be a
	// great help in finding problem areas.
	//_CrtSetDbgFlag (_CRTDBG_ALLOC_MEM_DF | _CRTDBG_CHECK_ALWAYS_DF);
#endif

#if !defined(__GNUC__) && !defined(_DEBUG)
	if (MainThread != INVALID_HANDLE_VALUE)
	{
		SetUnhandledExceptionFilter (CatchAllExceptions);
	}

	__try
	{
		DoMain (hInstance);
	}
	__except (pIsDebuggerPresent && pIsDebuggerPresent() ? EXCEPTION_CONTINUE_SEARCH :
		(CrashPointers = *GetExceptionInformation(), CreateCrashLog (DoomSpecificInfo), EXCEPTION_EXECUTE_HANDLER))
	{
		SetUnhandledExceptionFilter (NULL);
		I_ShutdownHardware ();
		SetWindowPos (Window, NULL, 0, 0, 0, 0, SWP_HIDEWINDOW);
		call_terms ();
		if (CrashText != NULL && Logfile != NULL)
		{
			fprintf (Logfile, "**** EXCEPTION CAUGHT ****\n%s", CrashText);
		}
		DisplayCrashLog ();
	}
#else
	// GCC is not nice enough to support SEH, so we can't gather crash info with it.
	// It could probably be faked with inline assembly, but that's too much trouble.
	DoMain (hInstance);
#endif
	CloseHandle (MainThread);
	MainThread = INVALID_HANDLE_VALUE;
	return 0;
}
