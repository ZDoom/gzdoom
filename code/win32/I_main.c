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
#include "resource.h"

#include <stdio.h>
#include <stdarg.h>

#include "m_argv.h"
#include "d_main.h"
#include "i_system.h"
#include "c_consol.h"
#include "z_zone.h"
#include "version.h"

LRESULT CALLBACK WndProc (HWND, UINT, WPARAM, LPARAM);

// Will this work with something besides VC++?
extern int __argc;
extern char **__argv;

const char WinClassName[] = "ZDOOM WndClass";

HINSTANCE		g_hInst;
WNDCLASS		WndClass;
HWND			Window;
HINSTANCE		hInstance;

extern float mb_used;
extern UINT TimerPeriod;

int WINAPI WinMain (HINSTANCE hInstance, HINSTANCE nothing, LPSTR cmdline, int nCmdShow)
{
	LONG WinWidth, WinHeight;
	int height, width;
	RECT cRect;
	TIMECAPS tc;

	g_hInst = hInstance;
	myargc = __argc;
	myargv = __argv;

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

	Z_Init ();					// 1/18/98 killough: start up memory stuff first
	atexit (I_Quit);

#ifdef USEASM
	{
		// Disable write-protection of code segment
		// (from Win32 Demo Programming FAQ)
		DWORD OldRights;
		BYTE *pBaseOfImage = (BYTE *)GetModuleHandle(0);
		IMAGE_OPTIONAL_HEADER *pHeader = (IMAGE_OPTIONAL_HEADER *)
			(pBaseOfImage + ((IMAGE_DOS_HEADER*)pBaseOfImage)->e_lfanew +
			sizeof(IMAGE_NT_SIGNATURE) + sizeof(IMAGE_FILE_HEADER));
		if (!VirtualProtect(pBaseOfImage+pHeader->BaseOfCode,pHeader->SizeOfCode,PAGE_READWRITE,&OldRights))
			I_FatalError ("Could not make code writable\n");
	}
#endif

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
			WS_CAPTION | WS_OVERLAPPED,
			CW_USEDEFAULT, CW_USEDEFAULT, width, height,
			(HWND)   NULL,
			(HMENU)  NULL,
			(HANDLE) hInstance,
			NULL);

	if (!Window)
		I_FatalError ("Could not open window");

	GetClientRect (Window, &cRect);

	WinWidth = cRect.right;
	WinHeight = cRect.bottom;

	C_InitConsole (((WinWidth / 8) + 2) * 8, (WinHeight / 12) * 8, false);

	Printf (PRINT_HIGH, "Heapsize: %g megabytes\n", mb_used);

	D_DoomMain ();

	return 0;
}

