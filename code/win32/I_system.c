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

extern HWND Window;

BOOL UseMMX;
UINT TimerPeriod;
UINT TimerEventID;
HANDLE NewTicArrived;

float mb_used = 8.0;

int (*I_GetTime) (void);
int (*I_WaitForTic) (int);

os_t OSPlatform;

void I_Tactile (int on, int off, int total)
{
  // UNUSED.
  on = off = total = 0;
}

ticcmd_t emptycmd;
ticcmd_t *I_BaseTiccmd(void)
{
	return &emptycmd;
}

int I_GetHeapSize (void)
{
	return (int)(mb_used*1024*1024);
}

byte *I_ZoneBase (int *size)
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

byte *I_AllocLow(int length)
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
int I_GetTimePolled (void)
{
	DWORD tm;

	tm = timeGetTime();
	if (!basetime)
		basetime = tm;

	return ((tm-basetime)*TICRATE)/1000;
}

int I_WaitForTicPolled (int prevtic)
{
	int time;

	while ((time = I_GetTimePolled()) <= prevtic)
		;

	return time;
}


static int tics;

int I_GetTimeEventDriven (void)
{
	return tics;
}

int I_WaitForTicEvent (int prevtic)
{
	if (prevtic >= tics)
		do
		{
			DWORD res = WaitForSingleObject (NewTicArrived, 5000);
			if (res == WAIT_TIMEOUT)
				I_FatalError ("Something happened to the timer event.\nPlease try restarting the game.");
		} while (prevtic >= tics);

	return tics;
}

void CALLBACK TimerTicked (UINT id, UINT msg, DWORD user, DWORD dw1, DWORD dw2)
{
	tics++;
	SetEvent (NewTicArrived);
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

	Printf (PRINT_HIGH, "OS: %s %u.%u (build %u)\n",
			osname,
			info.dwMajorVersion, info.dwMinorVersion,
			info.dwBuildNumber & (OSPlatform == os_Win95 ? 0xffff : 0xfffffff),
			info.szCSDVersion);
	if (info.szCSDVersion[0])
		Printf (PRINT_HIGH, "  %s\n", info.szCSDVersion);

	if (OSPlatform == os_Win32s) {
		I_FatalError ("Sorry, Win32s is not currently supported.\n"
					  "(And probably never will be.)\n"
					  "Upgrade to a newer version of Windows.");
	} else if (OSPlatform == os_unknown) {
		Printf (PRINT_HIGH, "(Assuming Windows 95)\n");
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
		Printf (PRINT_HIGH, "CPUID: %s  (", vendorid);

	if (UseMMX)
		Printf (PRINT_HIGH, "using MMX)\n");
	else
		Printf (PRINT_HIGH, "not using MMX)\n");
#endif

	// Use a timer event if possible
	NewTicArrived = CreateEvent (NULL, FALSE, FALSE, NULL);
	if (NewTicArrived)
	{
		TimerEventID = timeSetEvent
			(
				1000/TICRATE,
				0,
				TimerTicked,
				0,
				TIME_PERIODIC
			);
	}
	if (TimerEventID != 0)
	{
		I_GetTime = I_GetTimeEventDriven;
		I_WaitForTic = I_WaitForTicEvent;
	}
	else
	{	// Otherwise, busy-loop with timeGetTime
		I_GetTime = I_GetTimePolled;
		I_WaitForTic = I_WaitForTicPolled;
	}

	I_InitSound();
	I_InitInput (Window);
}

//
// I_Quit
//
static int has_exited;

void STACK_ARGS I_Quit (void)
{
	has_exited = 1;		/* Prevent infinitely recursive exits -- killough */

	if (TimerEventID)
		timeKillEvent (TimerEventID);
	if (NewTicArrived)
		CloseHandle (NewTicArrived);

	timeEndPeriod (TimerPeriod);

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

void STACK_ARGS I_FatalError (char *error, ...)
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

void STACK_ARGS I_Error (char *error, ...)
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

void I_SetTitleString (const char *title)
{
	int i;

	for (i = 0; title[i]; i++)
		DoomStartupTitle[i] = title[i] | 0x80;
}

void I_PrintStr (int xp, const char *cp, int count, BOOL scroll) {
	// used in the DOS version
}

long I_FindFirst (char *filespec, findstate_t *fileinfo)
{
	return _findfirst (filespec, fileinfo);
}
int I_FindNext (long handle, findstate_t *fileinfo)
{
	return _findnext (handle, fileinfo);
}

int I_FindClose (long handle)
{
	return _findclose (handle);
}
