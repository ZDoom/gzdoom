/*
** i_main.cpp
** System-specific startup code. Eventually calls D_DoomMain.
**
**---------------------------------------------------------------------------
** Copyright 1998-2009 Randy Heit
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
#include <mmsystem.h>
#include <objbase.h>
#include <commctrl.h>
#include <richedit.h>

#include <processenv.h>
#include <shellapi.h>

#ifdef _MSC_VER
#pragma warning(disable:4244)
#endif

#ifdef _MSC_VER
#include <eh.h>
#include <new.h>
#include <crtdbg.h>
#endif
#include "resource.h"

#include "engineerrors.h"
#include "hardware.h"

#include "m_argv.h"
#include "i_module.h"
#include "c_console.h"
#include "version.h"
#include "i_input.h"
#include "filesystem.h"
#include "cmdlib.h"
#include "s_soundinternal.h"
#include "vm.h"
#include "i_system.h"
#include "gstrings.h"
#include "s_music.h"

#include "stats.h"
#include "st_start.h"
#include "i_interface.h"
#include "startupinfo.h"
#include "printf.h"

#include "i_mainwindow.h"

// MACROS ------------------------------------------------------------------

// The main window's title.
#ifdef _M_X64
#define X64 " 64-bit"
#else
#define X64 ""
#endif

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

LRESULT CALLBACK WndProc (HWND, UINT, WPARAM, LPARAM);
void CreateCrashLog (const char *custominfo, DWORD customsize);
void DisplayCrashLog ();
void DestroyCustomCursor();
int GameMain();

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern EXCEPTION_POINTERS CrashPointers;
extern UINT TimerPeriod;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// The command line arguments.
FArgs *Args;

HINSTANCE		g_hInst;
HANDLE			MainThread;
DWORD			MainThreadID;
HANDLE			StdOut;
bool			FancyStdOut, AttachedStdOut;

// CODE --------------------------------------------------------------------


//==========================================================================
//
// I_SetIWADInfo
//
//==========================================================================

void I_SetIWADInfo()
{
	// Make the startup banner show itself
	mainwindow.UpdateLayout();
}

//==========================================================================
//
// DoMain
//
//==========================================================================

int DoMain (HINSTANCE hInstance)
{
	LONG WinWidth, WinHeight;
	int height, width, x, y;
	RECT cRect;
	TIMECAPS tc;
	DEVMODE displaysettings;

	// Do not use the multibyte __argv here because we want UTF-8 arguments
	// and those can only be done by converting the Unicode variants.
	Args = new FArgs();
	auto argc = __argc;
	auto wargv = __wargv;
	for (int i = 0; i < argc; i++)
	{
		Args->AppendArg(FString(wargv[i]));
	}

	if (Args->CheckParm("-stdout"))
	{
		// As a GUI application, we don't normally get a console when we start.
		// If we were run from the shell and are on XP+, we can attach to its
		// console. Otherwise, we can create a new one. If we already have a
		// stdout handle, then we have been redirected and should just use that
		// handle instead of creating a console window.

		StdOut = GetStdHandle(STD_OUTPUT_HANDLE);
		if (StdOut != NULL)
		{
			// It seems that running from a shell always creates a std output
			// for us, even if it doesn't go anywhere. (Running from Explorer
			// does not.) If we can get file information for this handle, it's
			// a file or pipe, so use it. Otherwise, pretend it wasn't there
			// and find a console to use instead.
			BY_HANDLE_FILE_INFORMATION info;
			if (!GetFileInformationByHandle(StdOut, &info))
			{
				StdOut = NULL;
			}
		}
		if (StdOut == NULL)
		{
			if (AttachConsole(ATTACH_PARENT_PROCESS))
			{
				StdOut = GetStdHandle(STD_OUTPUT_HANDLE);
				DWORD foo; WriteFile(StdOut, "\n", 1, &foo, NULL);
				AttachedStdOut = true;
			}
			if (StdOut == NULL && AllocConsole())
			{
				StdOut = GetStdHandle(STD_OUTPUT_HANDLE);
			}

			// Deprecated stuff for legacy consoles. As of now this is still relevant, but this code can be removed once Windows 7 is no longer relevant.
			CONSOLE_FONT_INFOEX cfi;
			cfi.cbSize = sizeof(cfi);
			if (GetCurrentConsoleFontEx(StdOut, false, &cfi))
			{
				if (*cfi.FaceName == 0)	// If the face name is empty, the default (useless) raster font is actoive.
				{
					//cfi.dwFontSize = { 8, 14 };
					wcscpy(cfi.FaceName, L"Lucida Console");
					cfi.FontFamily = FF_DONTCARE;
					SetCurrentConsoleFontEx(StdOut, false, &cfi);
				}
			}
			FancyStdOut = true;
		}
	}

	// Set the timer to be as accurate as possible
	if (timeGetDevCaps (&tc, sizeof(tc)) != TIMERR_NOERROR)
		TimerPeriod = 1;	// Assume minimum resolution of 1 ms
	else
		TimerPeriod = tc.wPeriodMin;

	timeBeginPeriod (TimerPeriod);
	atexit([](){ timeEndPeriod(TimerPeriod); });

	// Figure out what directory the program resides in.
	WCHAR progbuff[1024];
	if (GetModuleFileNameW(nullptr, progbuff, sizeof progbuff) == 0)
	{
		MessageBoxA(nullptr, "Fatal", "Could not determine program location.", MB_ICONEXCLAMATION|MB_OK);
		exit(-1);
	}

	progbuff[1023] = '\0';
	if (auto lastsep = wcsrchr(progbuff, '\\'))
	{
		lastsep[1] = '\0';
	}

	progdir = progbuff;
	FixPathSeperator(progdir);

	HDC screenDC = GetDC(0);
	int dpi = GetDeviceCaps(screenDC, LOGPIXELSX);
	ReleaseDC(0, screenDC);
	width = (512 * dpi + 96 / 2) / 96;
	height = (384 * dpi + 96 / 2) / 96;

	// Many Windows structures that specify their size do so with the first
	// element. DEVMODE is not one of those structures.
	memset (&displaysettings, 0, sizeof(displaysettings));
	displaysettings.dmSize = sizeof(displaysettings);
	EnumDisplaySettings (NULL, ENUM_CURRENT_SETTINGS, &displaysettings);
	x = (displaysettings.dmPelsWidth - width) / 2;
	y = (displaysettings.dmPelsHeight - height) / 2;

	if (Args->CheckParm ("-0"))
	{
		x = y = 0;
	}

	/* create window */
	FStringf caption("" GAMENAME " %s " X64 " (%s)", GetVersionString(), GetGitTime());
	mainwindow.Create(caption, x, y, width, height);

	GetClientRect (mainwindow.GetHandle(), &cRect);

	WinWidth = cRect.right;
	WinHeight = cRect.bottom;

	CoInitialize (NULL);
	atexit ([](){ CoUninitialize(); }); // beware of calling convention.

	int ret = GameMain ();
	mainwindow.CheckForRestart();

	DestroyCustomCursor();
	if (ret == 1337) // special exit code for 'norun'.
	{
		if (!batchrun)
		{
			if (FancyStdOut && !AttachedStdOut)
			{ // Outputting to a new console window: Wait for a keypress before quitting.
				DWORD bytes;
				HANDLE stdinput = GetStdHandle(STD_INPUT_HANDLE);

				ShowWindow(mainwindow.GetHandle(), SW_HIDE);
				WriteFile(StdOut, "Press any key to exit...", 24, &bytes, NULL);
				FlushConsoleInputBuffer(stdinput);
				SetConsoleMode(stdinput, 0);
				ReadConsole(stdinput, &bytes, 1, &bytes, NULL);
			}
			else if (StdOut == NULL)
			{
				mainwindow.ShowErrorPane(nullptr);
			}
		}
	}
	return ret;
}

void I_ShowFatalError(const char *msg)
{
	I_ShutdownGraphics ();
	mainwindow.RestoreConView();
	S_StopMusic(true);

	if (CVMAbortException::stacktrace.IsNotEmpty())
	{
		Printf("%s", CVMAbortException::stacktrace.GetChars());
	}

	if (!batchrun)
	{
		mainwindow.ShowErrorPane(msg);
	}
	else
	{
		Printf("%s\n", msg);
	}
}

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

//==========================================================================
//
// SleepForever
//
//==========================================================================

void SleepForever ()
{
	Sleep (INFINITE);
}

//==========================================================================
//
// ExitMessedUp
//
// An exception occurred while exiting, so don't do any standard processing.
// Just die.
//
//==========================================================================

LONG WINAPI ExitMessedUp (LPEXCEPTION_POINTERS foo)
{
	ExitProcess (1000);
}

//==========================================================================
//
// ExitFatally
//
//==========================================================================

void CALLBACK ExitFatally (ULONG_PTR dummy)
{
	SetUnhandledExceptionFilter (ExitMessedUp);
	I_ShutdownGraphics ();
	mainwindow.RestoreConView ();
	DisplayCrashLog ();
	exit(-1);
}

#ifndef _M_ARM64
//==========================================================================
//
// CatchAllExceptions
//
//==========================================================================

namespace
{
	CONTEXT MainThreadContext;
}

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
	if (sysCallbacks.CrashInfo && custominfo) sysCallbacks.CrashInfo(custominfo, 16384, "\r\n");
	CreateCrashLog (custominfo, (DWORD)strlen(custominfo));

	// If the main thread crashed, then make it clean up after itself.
	// Otherwise, put the crashing thread to sleep and signal the main thread to clean up.
	if (GetCurrentThreadId() == MainThreadID)
	{
#ifdef _M_X64
		*info->ContextRecord = MainThreadContext;
#else
		info->ContextRecord->Eip = (DWORD_PTR)ExitFatally;
#endif // _M_X64
	}
	else
	{
#ifndef _M_X64
		info->ContextRecord->Eip = (DWORD_PTR)SleepForever;
#else
		info->ContextRecord->Rip = (DWORD_PTR)SleepForever;
#endif
		QueueUserAPC (ExitFatally, MainThread, 0);
	}
	return EXCEPTION_CONTINUE_EXECUTION;
}
#else // !_M_ARM64
// stub this function for ARM64
LONG WINAPI CatchAllExceptions (LPEXCEPTION_POINTERS info)
{
	return EXCEPTION_CONTINUE_EXECUTION;
}
#endif // !_M_ARM64

//==========================================================================
//
// infiniterecursion
//
// Debugging routine for testing the crash logger.
//
//==========================================================================

#ifdef _DEBUG
static void infiniterecursion(int foo)
{
	if (foo)
	{
		infiniterecursion(foo);
	}
}
#endif

// Setting this to 'true' allows getting the standard notification for a crash
// which offers the very important feature to open a debugger and see the crash in context right away.
CUSTOM_CVAR(Bool, disablecrashlog, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	SetUnhandledExceptionFilter(!*self ? CatchAllExceptions : nullptr);
}

//==========================================================================
//
// WinMain
//
//==========================================================================

int WINAPI wWinMain (HINSTANCE hInstance, HINSTANCE nothing, LPWSTR cmdline, int nCmdShow)
{
	g_hInst = hInstance;

	InitCommonControls ();			// Load some needed controls and be pretty under XP

	// We need to load riched20.dll so that we can create the control.
	if (NULL == LoadLibraryA ("riched20.dll"))
	{
		// This should only happen on basic Windows 95 installations, but since we
		// don't support Windows 95, we have no obligation to provide assistance in
		// getting it installed.
		MessageBoxA(NULL, "Could not load riched20.dll", GAMENAME " Error", MB_OK | MB_ICONSTOP);
		return 0;
	}

#if !defined(__GNUC__) && defined(_DEBUG)
	if (__argc == 2 && __wargv != nullptr && wcscmp (__wargv[1], L"TestCrash") == 0)
	{
		__try
		{
			*(int *)0 = 0;
		}
		__except(CrashPointers = *GetExceptionInformation(),
			CreateCrashLog ("TestCrash", 9), EXCEPTION_EXECUTE_HANDLER)
		{
		}
		DisplayCrashLog ();
		return 0;
	}
	if (__argc == 2 && __wargv != nullptr && wcscmp (__wargv[1], L"TestStackCrash") == 0)
	{
		__try
		{
			infiniterecursion(1);
		}
		__except(CrashPointers = *GetExceptionInformation(),
			CreateCrashLog ("TestStackCrash", 14), EXCEPTION_EXECUTE_HANDLER)
		{
		}
		DisplayCrashLog ();
		return 0;
	}
#endif

	MainThread = INVALID_HANDLE_VALUE;
	DuplicateHandle (GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &MainThread,
		0, FALSE, DUPLICATE_SAME_ACCESS);
	MainThreadID = GetCurrentThreadId();

#ifndef _DEBUG
	if (MainThread != INVALID_HANDLE_VALUE)
	{
#ifndef _M_ARM64
		SetUnhandledExceptionFilter (CatchAllExceptions);
#endif

#ifdef _M_X64
		static bool setJumpResult = false;
		RtlCaptureContext(&MainThreadContext);
		if (setJumpResult)
		{
			ExitFatally(0);
			return 0;
		}
		setJumpResult = true;
#endif // _M_X64
	}
#endif

#if defined(_DEBUG) && defined(_MSC_VER)
	// Uncomment this line to make the Visual C++ CRT check the heap before
	// every allocation and deallocation. This will be slow, but it can be a
	// great help in finding problem areas.
	//_CrtSetDbgFlag (_CRTDBG_ALLOC_MEM_DF | _CRTDBG_CHECK_ALWAYS_DF);

	// Enable leak checking at exit.
	_CrtSetDbgFlag (_CrtSetDbgFlag(0) | _CRTDBG_LEAK_CHECK_DF);

	// Use this to break at a specific allocation number.
	//_crtBreakAlloc = 227524;
#endif

	int ret = DoMain (hInstance);

	CloseHandle (MainThread);
	MainThread = INVALID_HANDLE_VALUE;
	return ret;
}

// each platform has its own specific version of this function.
void I_SetWindowTitle(const char* caption)
{
	mainwindow.SetWindowTitle(caption);
}
