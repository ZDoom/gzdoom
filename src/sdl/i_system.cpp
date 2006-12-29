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
#include <string.h>
#include <fnmatch.h>
#include <unistd.h>

#include <stdarg.h>
#include <sys/types.h>
#include <sys/time.h>

#include "doomerrors.h"
#include <math.h>

#include "SDL.h"
#include "doomtype.h"
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
#include "i_system.h"
#include "c_dispatch.h"
#include "templates.h"

#include "stats.h"
#include "hardware.h"
#include "zstring.h"
#include "gameconfigfile.h"

EXTERN_CVAR (String, language)

#ifdef USEASM
extern "C" void STACK_ARGS CheckMMX (CPUInfo *cpu);
#endif

extern "C"
{
	double		SecondsPerCycle = 1e-8;
	double		CyclesPerSecond = 1e8;
	CPUInfo		CPU;
}

void CalculateCPUSpeed ();

DWORD LanguageIDs[4] =
{
	MAKE_ID ('e','n','u',0),
	MAKE_ID ('e','n','u',0),
	MAKE_ID ('e','n','u',0),
	MAKE_ID ('e','n','u',0)
};
	
int (*I_GetTime) (bool saveMS);
int (*I_WaitForTic) (int);

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

void I_BeginRead(void)
{
}

void I_EndRead(void)
{
}

// [RH] Returns time in milliseconds
unsigned int I_MSTime (void)
{
	return SDL_GetTicks ();
}

static DWORD TicStart;
static DWORD TicNext;

//
// I_GetTime
// returns time in 1/35th second tics
//
int I_GetTimePolled (bool saveMS)
{
	DWORD tm = SDL_GetTicks ();

	if (saveMS)
	{
		TicStart = tm;
		TicNext = Scale ((Scale (tm, TICRATE, 1000) + 1), 1000, TICRATE);
	}
	return Scale (tm, TICRATE, 1000);
}

int I_WaitForTicPolled (int prevtic)
{
    int time;

    while ((time = I_GetTimePolled(false)) <= prevtic)
		;

    return time;
}

// Returns the fractional amount of a tic passed since the most recent tic
fixed_t I_GetTimeFrac (uint32 *ms)
{
	DWORD now = SDL_GetTicks ();
	if (ms) *ms = TicNext;
	DWORD step = TicNext - TicStart;
	if (step == 0)
	{
		return FRACUNIT;
	}
	else
	{
		fixed_t frac = clamp<fixed_t> ((now - TicStart)*FRACUNIT/step, 0, FRACUNIT);
		return frac;
	}
}

void I_WaitVBL (int count)
{
    // I_WaitVBL is never used to actually synchronize to the
    // vertical blank. Instead, it's used for delay purposes.
    usleep (1000000 * count / 70);
}

//
// SetLanguageIDs
//
void SetLanguageIDs ()
{
}

//
// I_Init
//
void I_Init (void)
{
#ifndef USEASM
    memset (&CPU, 0, sizeof(CPU));
#else
	CheckMMX (&CPU);
	CalculateCPUSpeed ();
	
	// Why does Intel right-justify this string?
	char *f = CPU.CPUString, *t = f;
	
	while (*f == ' ')
	{
		++f;
	}
	if (f != t)
	{
		while (*f != 0)
		{
			*t++ = *f++;
		}
	}
#endif
	if (CPU.VendorID[0])
	{
		Printf ("CPU Vendor ID: %s\n", CPU.VendorID);
		if (CPU.CPUString[0])
		{
			Printf ("  Name: %s\n", CPU.CPUString);
		}
		if (CPU.bIsAMD)
		{
			Printf ("  Family %d (%d), Model %d, Stepping %d\n",
				CPU.Family, CPU.AMDFamily, CPU.AMDModel, CPU.AMDStepping);
		}
		else
		{
			Printf ("  Family %d, Model %d, Stepping %d\n",
				CPU.Family, CPU.Model, CPU.Stepping);
		}
		Printf ("  Features:");
		if (CPU.bMMX)		Printf (" MMX");
		if (CPU.bMMXPlus)	Printf (" MMX+");
		if (CPU.bSSE)		Printf (" SSE");
		if (CPU.bSSE2)		Printf (" SSE2");
		if (CPU.bSSE3)		Printf (" SSE3");
		if (CPU.b3DNow)		Printf (" 3DNow!");
		if (CPU.b3DNowPlus)	Printf (" 3DNow!+");
		Printf ("\n");
	}

	I_GetTime = I_GetTimePolled;
	I_WaitForTic = I_WaitForTicPolled;
    I_InitSound ();
}

void CalculateCPUSpeed ()
{
	timeval start, stop, now;
	cycle_t ClockCycles;
	DWORD usec;

	if (CPU.bRDTSC)
	{
		ClockCycles = 0;
		clock (ClockCycles);
		gettimeofday (&start, NULL);
	
		// Count cycles for at least 100 milliseconds.
		// We don't have the same accuracy we can get with the Win32
		// performance counters, so we have to time longer.
		stop.tv_usec = start.tv_usec + 100000;
		stop.tv_sec = start.tv_sec;
		if (stop.tv_usec >= 1000000)
		{
			stop.tv_usec -= 1000000;
			stop.tv_sec += 1;
		}
		do
		{
			gettimeofday (&now, NULL);
		} while (timercmp (&now, &stop, <));
	
		unclock (ClockCycles);
		gettimeofday (&now, NULL);
		usec = now.tv_usec - start.tv_usec;

		CyclesPerSecond = (double)ClockCycles * 1e6 / (double)usec;
		SecondsPerCycle = 1.0 / CyclesPerSecond;
	}
	Printf (PRINT_HIGH, "CPU Speed: ~%f MHz\n", CyclesPerSecond / 1e6);
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
    G_ClearSnapshots ();
}


//
// I_Error
//
extern FILE *Logfile;
bool gameisdead;

void STACK_ARGS I_FatalError (const char *error, ...)
{
    static bool alreadyThrown = false;
    gameisdead = true;

    if (!alreadyThrown)		// ignore all but the first message -- killough
    {
		alreadyThrown = true;
		char errortext[MAX_ERRORTEXT];
		int index;
		va_list argptr;
		va_start (argptr, error);
		index = vsprintf (errortext, error, argptr);
		va_end (argptr);

		// Record error to log (if logging)
		if (Logfile)
			fprintf (Logfile, "\n**** DIED WITH FATAL ERROR:\n%s\n", errortext);
//		throw CFatalError (errortext);
		fprintf (stderr, "%s\n", errortext);
		exit (-1);
    }

    if (!has_exited)	// If it hasn't exited yet, exit now -- killough
    {
		has_exited = 1;	// Prevent infinitely recursive exits -- killough
		exit(-1);
    }
}

void STACK_ARGS I_Error (const char *error, ...)
{
    va_list argptr;
    char errortext[MAX_ERRORTEXT];

    va_start (argptr, error);
    vsprintf (errortext, error, argptr);
    va_end (argptr);

    throw CRecoverableError (errortext);
}

void I_SetIWADInfo (const IWADInfo *info)
{
}

void I_PrintStr (const char *cp)
{
	fputs (cp, stdout);
	fflush (stdout);
}

int I_PickIWad (WadStuff *wads, int numwads, bool queryiwad, int defaultiwad)
{
	int i;
	
	printf ("Please select a game wad (or 0 to exit):\n");
	for (i = 0; i < numwads; ++i)
	{
		const char *filepart = strrchr (wads[i].Path, '/');
		if (filepart == NULL)
			filepart = wads[i].Path;
		else
			filepart++;
		printf ("%d. %s (%s)\n", i+1, IWADInfos[wads[i].Type].Name, filepart);
	}
	printf ("Which one? ");
	scanf ("%d", &i);
	if (i > numwads)
		return -1;
	return i-1;
}

bool I_WriteIniFailed ()
{
	printf ("The config file %s could not be saved:\n%s\n", GameConfig->GetPathName(), strerror(errno));
	return false;
	// return true to retry
}

static const char *pattern;

#ifdef OSF1
static int matchfile (struct dirent *ent)
#else
static int matchfile (const struct dirent *ent)
#endif
{
    return fnmatch (pattern, ent->d_name, FNM_NOESCAPE) == 0;
}

void *I_FindFirst (const char *filespec, findstate_t *fileinfo)
{
	FString dir;
	
	char *slash = strrchr (filespec, '/');
	if (slash)
	{
		pattern = slash+1;
		dir = FString(filespec, slash-filespec+1);
	}
	else
	{
		pattern = filespec;
		dir = ".";
	}

    fileinfo->current = 0;
    fileinfo->count = scandir (dir.GetChars(), &fileinfo->namelist,
							   matchfile, alphasort);
    if (fileinfo->count > 0)
    {
		return fileinfo;
    }
    return (void*)-1;
}

int I_FindNext (void *handle, findstate_t *fileinfo)
{
    findstate_t *state = (findstate_t *)handle;
    if (state->current < fileinfo->count)
    {
	    return ++state->current < fileinfo->count ? 0 : -1;
	}
	return -1;
}

int I_FindClose (void *handle)
{
	findstate_t *state = (findstate_t *)handle;
	if (handle != (void*)-1 && state->count > 0)
	{
		state->count = 0;
		free (state->namelist);
		state->namelist = NULL;
	}
	return 0;
}

int I_FindAttr (findstate_t *fileinfo)
{
	struct dirent *ent = fileinfo->namelist[fileinfo->current];

#ifdef OSF1
	return 0;	// I don't know how to detect dirs under OSF/1
#else
    return (ent->d_type == DT_DIR) ? FA_DIREC : 0;
#endif
}

// No clipboard support for Linux
void I_PutInClipboard (const char *str)
{
}

char *I_GetFromClipboard ()
{
	return NULL;
}
