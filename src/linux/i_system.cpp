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

#include "errors.h"
#include <math.h>

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
#include "i_system.h"

#include "stats.h"
#include "hardware.h"

#ifdef USEASM
extern "C" BOOL STACK_ARGS CheckMMX (char *vendorid);
#endif

extern "C"
{
	BOOL		HaveRDTSC = 0;
	double		SecondsPerCycle = 1e-6;
	double		CyclesPerSecond = 1e6;
	byte		CPUFamily, CPUModel, CPUStepping;
}

static cycle_t ClockCalibration;

BOOL UseMMX;

float mb_used = 8.0;

int (*I_GetTime) (void);
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

int I_GetHeapSize (void)
{
    return (int)(mb_used*1024*1024);
}

byte *I_ZoneBase (unsigned int *size)
{
    char *p;

    p = Args.CheckValue ("-heapsize");
    if (p)
		mb_used = (float)atof (p);
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
    byte *mem;

    mem = (byte *)malloc (length);
    if (mem) {
		memset (mem,0,length);
    }
    return mem;
}


// CPhipps - believe it or not, it is possible with consecutive calls to 
// gettimeofday to receive times out of order, e.g you query the time twice and 
// the second time is earlier than the first. Cheap'n'cheerful fix here.
// NOTE: only occurs with bad kernel drivers loaded, e.g. pc speaker drv

static QWORD lasttimereply;
static QWORD basetime;

// [RH] Returns time in milliseconds
QWORD I_MSTime (void)
{
	struct timeval tv;
	struct timezone tz;
    QWORD thistimereply;

	gettimeofday(&tv, &tz);

	thistimereply = tv.tv_sec * 1000 + tv.tv_usec / 1000;

	// Fix for time problem
	if (!basetime) {
		basetime = thistimereply; thistimereply = 0;
	} else thistimereply -= basetime;

	if (thistimereply < lasttimereply)
		thistimereply = lasttimereply;

	return (lasttimereply = thistimereply);
}

//
// I_GetTime
// returns time in 1/35th second tics
//
int I_GetTimePolled (void)
{
	return I_MSTime() * TICRATE / 1000;
}

int I_WaitForTicPolled (int prevtic)
{
    int time;

    while ((time = I_GetTimePolled()) <= prevtic)
		;

    return time;
}


void I_WaitVBL (int count)
{
    // I_WaitVBL is never used to actually synchronize to the
    // vertical blank. Instead, it's used for delay purposes.
    usleep (1000000 * count / 70);
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
    if (Args.CheckParm ("-nommx"))
		UseMMX = 0;

    if (vendorid[0])
		Printf (PRINT_HIGH, "CPUID: %s  (", vendorid);

    if (UseMMX)
		Printf (PRINT_HIGH, "using MMX)\n");
    else
		Printf (PRINT_HIGH, "not using MMX)\n");

    Printf (PRINT_HIGH, "-> family %d, model %d, stepping %d\n", CPUFamily, CPUModel, CPUStepping);
    Printf (PRINT_HIGH, "-> Processor %s RDTSC\n", HaveRDTSC ? "has" : "does not have");
    if (HaveRDTSC)
    {
		clock (ClockCalibration);
		usleep (100);
		unclock (ClockCalibration);
    }
#endif

	I_GetTime = I_GetTimePolled;
	I_WaitForTic = I_WaitForTicPolled;

    I_InitSound ();
	I_InitHardware ();
}

void I_FinishClockCalibration ()
{
	Printf (PRINT_HIGH, "CPU Frequency: ~%f MHz\n", CyclesPerSecond / 1e6);
}

//
// I_Quit
//
static int has_exited;

void STACK_ARGS I_Quit (void)
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
BOOL gameisdead;

void STACK_ARGS I_FatalError (const char *error, ...)
{
    static BOOL alreadyThrown = false;
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
		throw CFatalError (errortext);
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

char DoomStartupTitle[256] = { 0 };

void I_SetTitleString (const char *title)
{
    int i;

    for (i = 0; title[i]; i++)
		DoomStartupTitle[i] = title[i] | 0x80;
}

void I_PrintStr (int xp, const char *cp, int count, BOOL scroll)
{
	char string[4096];

	memcpy (string, cp, count);
	if (scroll)
		string[count++] = '\n';
	string[count] = 0;

	fputs (string, stdout);
	fflush (stdout);
}

static const char *pattern;
static findstate_t *findstates[8];

static int select (const struct dirent *ent)
{
    return fnmatch (pattern, ent->d_name, FNM_NOESCAPE) == 0;
}

long I_FindFirst (char *filespec, findstate_t *fileinfo)
{
	char *slash = strrchr (filespec, "/");
	if (slash)
		pattern = slash+1;
	else
		pattern = filespec;

    fileinfo->current = 0;
    fileinfo->count = scandir (".", &fileinfo->namelist,
							   select, alphasort);
    if (fileinfo->count > 0)
    {
		for (int i = 0; i < 8; i++)
			if (findstates[i] == NULL)
			{
				findstates[i] = fileinfo;
				return i;
			}
    }
    return -1;
}

int I_FindNext (long handle, findstate_t *fileinfo)
{
    findstate_t *state = findstates[handle];
    if (state->current < fileinfo->count)
    {
		return ++state->current < fileinfo->count ? 0 : -1;
    }
    return -1;
}

int I_FindClose (long handle)
{
    findstates[handle]->count = 0;
    findstates[handle]->namelist = NULL;
	return 0;
}

int I_FindAttr (findstate_t *fileinfo)
{
    struct dirent *ent = fileinfo->namelist[fileinfo->current];

    return (ent->d_type == DT_DIR) ? FA_DIREC : 0;
}
