// Emacs style mode select	 -*- C++ -*- 
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
//
//-----------------------------------------------------------------------------


#include <stdlib.h>
#include <stdio.h>
#include <io.h>
#include <direct.h>
#include <string.h>

#include <stdarg.h>
#include <sys/types.h>
#include <sys/timeb.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>

#include "version.h"
#include "doomdef.h"
#include "cmdlib.h"
#include "m_argv.h"
#include "m_misc.h"
#include "i_video.h"
#include "i_sound.h"
#include "i_music.h"

#include "d_main.h"
#include "d_net.h"
#include "g_game.h"
#include "i_input.h"
#include "i_system.h"
#include "c_dispch.h"

#ifdef USEASM
BOOL STACK_ARGS CheckMMX (char *vendorid);
#endif

static void Cmd_Dir (void *plyr, int argc, char **argv);

extern HWND 			Window;
extern HDC				WinDC;
extern LONG 			OemWidth, OemHeight;
extern LONG 			WinWidth, WinHeight;
extern int				ConRows, ConCols, PhysRows;
extern char 			*Lines, *Last;

BOOL	UseMMX;
BOOL	fastdemo;

float 	mb_used = 8.0;

int (*I_GetTime) (void);

os_t OSPlatform;

void I_Tactile (int on, int off, int total)
{
  // UNUSED.
  on = off = total = 0;
}

ticcmd_t		emptycmd;
ticcmd_t*		I_BaseTiccmd(void)
{
	return &emptycmd;
}


int  I_GetHeapSize (void)
{
	return (int)(mb_used*1024*1024);
}

byte* I_ZoneBase (int *size)
{
	int i;

	i = M_CheckParm ("-heapsize");
	if (i && i < myargc - 1)
		mb_used = (float)atof (myargv[i + 1]);
	*size = (int)(mb_used*1024*1024);
	return (byte *) malloc (*size);
}

void I_BeginRead(void)
{
}

void I_EndRead(void)
{
}

byte*	I_AllocLow(int length)
{
	byte*		mem;

	mem = (byte *)malloc (length);
	if (mem) {
		memset (mem,0,length);
	}
	return mem;
}


static DWORD basetime = 0;

// [RH] Returns time in milliseconds
unsigned int I_MSTime (void)
{
	DWORD tm;

	tm = timeGetTime();
	if (!basetime)
		basetime = tm;

	return tm - basetime;
}

//
// I_GetTime
// returns time in 1/35th second tics
//
int I_GetTimeReally (void)
{
	DWORD tm;

	tm = timeGetTime();
	if (!basetime)
		basetime = tm;

	return ((tm-basetime)*TICRATE)/1000;
}

// [RH] Increments the time count every time it gets called.
//		Used only by -fastdemo (just like BOOM).
int I_GetTimeFake (void)
{
	static int tic = 0;

	return tic++;
}

void I_WaitVBL (int count)
{
	// I_WaitVBL is never used to actually synchronize to the
	// vertical blank. Instead, it's used for delay purposes.

	Sleep (1000 * count / 70);
}

// [RH] Detect the OS the game is running under
void I_DetectOS (void)
{
	OSVERSIONINFO info;
	char *osname;

	info.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	GetVersionEx (&info);

	switch (info.dwPlatformId) {
		case VER_PLATFORM_WIN32s:
			OSPlatform = os_Win32s;
			osname = "Windows 3.1 with Win32s";
			break;
		case VER_PLATFORM_WIN32_WINDOWS:
			OSPlatform = os_Win95;
			osname = info.dwMinorVersion > 0 ? "Windows 98" : "Windows 95";
			break;
		case VER_PLATFORM_WIN32_NT:
			OSPlatform = os_WinNT;
			osname = "Windows NT";
			break;
		default:
			OSPlatform = os_unknown;
			osname = "Unknown OS";
			break;
	}

	Printf ("Detected OS: %s %u.%u (build %u)\n",
			osname,
			info.dwMajorVersion, info.dwMinorVersion,
			info.dwBuildNumber & (OSPlatform == os_Win95 ? 0xffff : 0xfffffff),
			info.szCSDVersion);
	if (info.szCSDVersion[0])
		Printf ("  %s\n", info.szCSDVersion);

	if (OSPlatform == os_Win32s) {
		I_FatalError ("Sorry, Win32s is not currently supported.\n"
					  "(And probably never will be.)\n"
					  "Upgrade to a newer version of Windows.");
	} else if (OSPlatform == os_unknown) {
		Printf ("(Assuming Windows 95)\n");
		OSPlatform = os_Win95;
	}
}

//
// I_Init
//
void I_Init (void)
{
#ifndef USEASM
	UseMMX = 0;
#else
	char vendorid[13];

	vendorid[0] = vendorid[12] = 0;
	UseMMX = CheckMMX (vendorid);
	if (M_CheckParm ("-nommx"))
		UseMMX = 0;

	if (vendorid[0])
		Printf ("CPU Vendor ID: %s\n", vendorid);

	if (UseMMX)
		Printf (" - MMX detected\n");
	else
		Printf (" - MMX not detected\n");
#endif

	// [RH] Support for BOOM's -fastdemo
	if (fastdemo)
		I_GetTime = I_GetTimeFake;
	else
		I_GetTime = I_GetTimeReally;

	I_InitSound();
	I_InitInput (Window);
	C_RegisterCommand ("dir", Cmd_Dir);
}

//
// I_Quit
//
static int has_exited;

void I_Quit (void)
{
	has_exited = 1;		/* Prevent infinitely recursive exits -- killough */

	if (demorecording)
		G_CheckDemoStatus();
	M_SaveDefaults ();

	if (errortext[0]) {
		if (Window)
			ShowWindow (Window, SW_HIDE);
		MessageBox (Window, errortext, "ZDOOM Fatal Error", MB_OK|MB_ICONSTOP|MB_TASKMODAL);
	}
}


//
// I_Error
//
extern FILE *Logfile;
BOOL gameisdead;

void I_FatalError (char *error, ...)
{
	gameisdead = true;

	if (!errortext[0])		// ignore all but the first message -- killough
	{
		int index;
		va_list argptr;
		va_start (argptr, error);
		index = vsprintf (errortext, error, argptr);
		sprintf (errortext + index, "\nGetLastError = %d", GetLastError());
		va_end (argptr);
	}

	if (!has_exited)	// If it hasn't exited yet, exit now -- killough
	{
		has_exited = 1;	// Prevent infinitely recursive exits -- killough

		// [RH] Record error to log (if logging)
		if (Logfile)
			fprintf (Logfile, "\n**** DIED WITH FATAL ERROR:\n%s\n", errortext);

		exit(-1);
	}
}

void I_Error (char *error, ...)
{
	va_list argptr;

	va_start (argptr, error);
	vsprintf (errortext, error, argptr);
	va_end (argptr);

	if (errorjmpable)
		longjmp (errorjmp, 1);
	else
		I_FatalError (NULL);
}

char DoomStartupTitle[256] = { 0 };

/*
void I_PaintConsole (void)
{
	PAINTSTRUCT paint;
	HDC dc;

	if (dc = BeginPaint (Window, &paint)) {
		if (paint.rcPaint.top < OemHeight) {
			SetTextColor (dc, RGB(255,0,0));
			SetBkColor (dc, RGB(195,195,195));
			SetBkMode (dc, OPAQUE);

			TextOut (dc, 0, 0, Title, ConCols);
		}
		if (Last && paint.rcPaint.bottom > OemHeight) {
			char *row;
			int line, last, top, bottom;

			SetTextColor (dc, RGB(0,255,255));
			SetBkMode (dc, TRANSPARENT);

			top = (paint.rcPaint.top >= OemHeight) ? paint.rcPaint.top - OemHeight : 0;
			bottom = paint.rcPaint.bottom - OemHeight - 1;
			line = top / OemHeight;
			last = bottom / OemHeight;
			for (row = Last - (PhysRows - 2 - line) * (ConCols + 2); line <= last; line++) {
				TextOut (dc, 0, (line + 1) * OemHeight, row + 2, row[1]);
				row += ConCols + 2;
			}
		}
		EndPaint (Window, &paint);
	}
}
*/
void I_SetTitleString (const char *title)
{
	int i;

	for (i = 0; title[i]; i++)
		DoomStartupTitle[i] = title[i] | 0x80;
}

void I_PrintStr (int xp, const char *cp, int count, BOOL scroll) {
/*	MSG mess;
	RECT rect;

	if (count)
		TextOut (WinDC, xp * OemWidth, WinHeight - OemHeight, cp, count);
	if (scroll) {
		rect.left = 0;
		rect.top = OemHeight;
		rect.right = WinWidth;
		rect.bottom = WinHeight;
		ScrollWindowEx (Window, 0, -OemHeight, NULL, &rect, NULL, NULL, SW_ERASE|SW_INVALIDATE);
		UpdateWindow (Window);
	}
	while (PeekMessage (&mess, Window, 0, 0, PM_REMOVE)) {
		if (mess.message == WM_QUIT)
			exit (mess.wParam);
		TranslateMessage (&mess);
		DispatchMessage (&mess);
	}
*/
}

static void Cmd_Dir (void *plyr, int argc, char **argv)
{
	char dir[256], curdir[256];
	char *match;
	struct _finddata_t c_file;
	long file;

	if (!getcwd (curdir, 256)) {
		Printf ("Current path too long\n");
		return;
	}

	if (argc == 1 || chdir (argv[1])) {
		match = argc == 1 ? "./*" : argv[1];

		ExtractFilePath (match, dir);
		if (dir[0]) {
			match += strlen (dir);
		} else {
			dir[0] = '.';
			dir[1] = '/';
			dir[2] = '\0';
		}
		if (!match[0])
			match = "*";

		if (chdir (dir)) {
			Printf ("%s not found\n", dir);
			return;
		}
	} else {
		match = "*";
		strcpy (dir, argv[1]);
		if (dir[strlen(dir) - 1] != '/')
			strcat (dir, "/");
	}

	if ( (file = _findfirst (match, &c_file)) == -1)
		Printf ("Nothing matching %s%s\n", dir, match);
	else {
		Printf ("Listing of %s%s:\n", dir, match);
		do {
			Printf ("%s%s\n", c_file.name, c_file.attrib & _A_SUBDIR ? " <dir>" : "");
		} while (_findnext (file, &c_file) == 0);
		_findclose (file);
	}

	chdir (curdir);
}
