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
#include <windows.h>
#include <mmsystem.h>
#include <eh.h>
#include "resource.h"

#include <stdio.h>
#include <stdarg.h>
#include <math.h>
#include <new.h>

#include "errors.h"
#include "hardware.h"

#include "doomtype.h"
#include "m_argv.h"
#include "d_main.h"
#include "i_system.h"
#include "c_console.h"
#include "z_zone.h"
#include "version.h"
#include "i_video.h"
#include "i_sound.h"

#include "stats.h"

LRESULT CALLBACK WndProc (HWND, UINT, WPARAM, LPARAM);

// Will this work with something besides VC++?
//extern int __argc;
//extern char **__argv;

DArgs Args;

const char WinClassName[] = "ZDOOM WndClass";

HINSTANCE		g_hInst;
WNDCLASS		WndClass;
HWND			Window;
HINSTANCE		hInstance;

extern float mb_used;
extern UINT TimerPeriod;

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

static int STACK_ARGS NewFailure (size_t size)
{
	I_FatalError ("Failed to allocate %d bytes from system heap\n(Try using a smaller -heapsize)");
	return 0;
}

int WINAPI WinMain (HINSTANCE hInstance, HINSTANCE nothing, LPSTR cmdline, int nCmdShow)
{
	LONG WinWidth, WinHeight;
	int height, width;
	RECT cRect;
	TIMECAPS tc;

	_set_new_handler (NewFailure);

	try
	{
		g_hInst = hInstance;
		Args.SetArgs (__argc, __argv);

		// Set the timer to be as accurate as possible
		if (timeGetDevCaps (&tc, sizeof(tc) != TIMERR_NOERROR))
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

		Z_Init ();					// 1/18/98 killough: start up memory stuff first
		atterm (I_Quit);

		// Figure out what directory the program resides in.
		GetModuleFileName (NULL, progdir, 1024);
		*(strrchr (progdir, '\\') + 1) = 0;
		FixPathSeperator (progdir);

		height = GetSystemMetrics (SM_CYFIXEDFRAME) * 2 +
				 GetSystemMetrics (SM_CYCAPTION) + 12 * 32;
		width  = GetSystemMetrics (SM_CXFIXEDFRAME) * 2 + 8 * 78;

		WndClass.style			= 0;
		WndClass.lpfnWndProc	= WndProc;
		WndClass.cbClsExtra		= 0;
		WndClass.cbWndExtra		= 0;
		WndClass.hInstance		= hInstance;
		WndClass.hIcon			= LoadIcon (hInstance, MAKEINTRESOURCE(IDI_ICON1));
		WndClass.hCursor		= LoadCursor (NULL, IDC_ARROW);
		WndClass.hbrBackground	= NULL;
		WndClass.lpszMenuName	= NULL;
		WndClass.lpszClassName	= (LPCTSTR)WinClassName;
		
		/* register this new class with Windoze */
		if (!RegisterClass((LPWNDCLASS)&WndClass))
			I_FatalError ("Could not register window class");
		
		/* create window */
		Window = CreateWindow((LPCTSTR)WinClassName,
				(LPCTSTR) "ZDOOM (" __DATE__ ")",
				WS_OVERLAPPEDWINDOW,
				0/*CW_USEDEFAULT*/, 1/*CW_USEDEFAULT*/, width, height,
				(HWND)   NULL,
				(HMENU)  NULL,
						 hInstance,
				NULL);

		if (!Window)
			I_FatalError ("Could not open window");

		GetClientRect (Window, &cRect);

		WinWidth = cRect.right;
		WinHeight = cRect.bottom;

		C_InitConsole (((WinWidth / 8) + 2) * 8, (WinHeight / 12) * 8, false);

		Printf (PRINT_HIGH, "Heapsize: %g megabytes\n", mb_used);

		I_DetectOS ();

		D_DoomMain ();
	}
	catch (CDoomError &error)
	{
		I_ShutdownHardware ();
		SetWindowPos (Window, NULL, 0, 0, 0, 0, SWP_HIDEWINDOW);
		if (error.GetMessage ())
			MessageBox (Window, error.GetMessage(),
				"ZDOOM Fatal Error", MB_OK|MB_ICONSTOP|MB_TASKMODAL);
		exit (-1);
	}
#ifndef _DEBUG
	catch (...)
	{
		// If an exception is thrown, be sure to do a proper shutdown.
		// This is especially important if we are in fullscreen mode,
		// because the OS will only show the alert box if we are in
		// windowed mode. Graphics gets shut down first in case something
		// goes wrong calling the cleanup functions.
		I_ShutdownHardware ();
		SetWindowPos (Window, NULL, 0, 0, 0, 0, SWP_HIDEWINDOW);
		call_terms ();
		// Now let somebody who understands the exception deal with it.
		throw;
	}
#endif
	return 0;
}

