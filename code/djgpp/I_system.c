// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// $Id: i_system.c,v 1.14 1998/05/03 22:33:13 killough Exp $
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
//
// DESCRIPTION:
//
//-----------------------------------------------------------------------------

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <stdarg.h>
#include <go32.h>
#include <dpmi.h>
#include <gppconio.h>
#include <pc.h>
#include <dos.h>

#include "version.h"
#include "doomdef.h"
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

#include "w_wad.h"
#include "z_zone.h"

#define TRUE 1
#define FALSE 0
typedef unsigned long DWORD;

#include "../../midas/src/midas/midasdll.h"

#ifdef USEASM
BOOL MIDAS_CALL CheckMMX (char *vendorid);
#endif

BOOL UseMMX;
BOOL	fastdemo;

float 	mb_used = 6.0;

int (*I_GetTime) (void);


void I_Tactile (int on, int off, int total)
{
  // UNUSED.
  on = off = total = 0;
}

static ticcmd_t emptycmd;
ticcmd_t *I_BaseTiccmd (void)
{
	return &emptycmd;
}


int I_GetHeapSize (void)
{
	return (int)(mb_used*1024*1024);
}

byte *I_ZoneBase (int *size)
{
	int heap;
	int i;
	byte *ptr;

	heap = _go32_dpmi_remaining_physical_memory ();	// get memory info
	printf ("DPMI memory: 0x%x, ", heap);
	i = M_CheckParm ("-heapsize");
	if (i && i < myargc - 1) {
		heap = (mb_used*1024*1024);
	} else {
		heap -= 2 * 1024 * 1024 + 0x10000;	// leave at least 2 megs free
	}

	do
	{
		heap -= 0x10000;                // leave 64k alone
		if (heap > 0x800000)
			heap = 0x800000;
		ptr = malloc (heap);
	} while (!ptr);

	printf ("0x%x allocated for zone\n", heap);
	if (heap < 0x180000)
		I_FatalError ("Insufficient DPMI memory!");

	*size = heap;
	mb_used = ((float)heap)/1024/1024;
	return ptr;
}

byte *I_AllocLow (int length)
{
	byte *mem;

	mem = (byte *)malloc (length);
	if (mem) {
		memset (mem,0,length);
	}
	return mem;
}

// Under Win32, this actually does something.
unsigned int I_MSTime (void)
{
	return 1;
}

//
// I_GetTime
// returns time in 1/70th second tics
//
static volatile int counter;

static void MIDAS_CALL TimerCallback (void)
{
	counter++;
}
static void TimerEnd (void) { }

int I_GetTimeReally (void)
{
	return counter;
}

// [RH] Increments the time count every time it gets called.
//		Used only by -fastdemo (just like BOOM).
int I_GetTimeFake (void)
{
	static int tic = 0;

	return tic++;
}


void I_WaitVBL(int count)
{
	// I_WaitVBL is never used to actually synchronize to the
	// vertical blank. Instead, it's used for delay purposes.

	delay (1000 * count / 70);
}

void I_BeginRead(void)
{
}

void I_EndRead(void)
{
}

void I_Shutdown(void)
{
}

void I_Init(void)
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

	// [RH] Support for BOOM's -fastdemo
	if (fastdemo)
		I_GetTime = I_GetTimeFake;
	else
		I_GetTime = I_GetTimeReally;

	I_InitSound();

	// Add a MIDAS timer callback 
	_go32_dpmi_lock_data ((void *)&counter, sizeof(counter));
	_go32_dpmi_lock_code (TimerCallback, (long)TimerEnd - (long)TimerCallback);

	if (!MIDASsetTimerCallbacks (TICRATE*1000, FALSE, TimerCallback, NULL, NULL))
		I_FatalError ("Could not add timer callback!");

	I_InitInput ();
	atexit (I_Shutdown);
}

//
// I_Quit
//

extern BOOL demorecording;

static int has_exited;

void I_Quit (void)
{
	has_exited = 1;		/* Prevent infinitely recursive exits -- killough */

	if (demorecording)
		G_CheckDemoStatus();
	M_SaveDefaults ();

	if (errortext[0])
		fprintf (stderr, "%s\n", errortext);
	else
		I_EndDoom();
}

//
// I_Error
//
extern FILE *Logfile;
BOOL gameisdead;

void I_FatalError(const char *error, ...) // killough 3/20/98: add const
{
	gameisdead = true;

	if (!errortext[0])		// ignore all but the first message -- killough
	{
		va_list argptr;
		va_start(argptr,error);
		vsprintf(errortext,error,argptr);
		va_end(argptr);
	}

	if (!has_exited)	// If it hasn't exited yet, exit now -- killough
	{
		has_exited=1;	// Prevent infinitely recursive exits -- killough

		// [RH] Record error to log (if logging)
		if (Logfile)
			fprintf (Logfile, "\n**** DIED WITH FATAL ERROR:\n%s\n", errortext);
		exit(-1);
	}
}

void I_Error (const char *error, ...)
{
	va_list argptr;

	va_start (argptr, error);
	vsprintf (errortext, error, argptr);
	va_end (argptr);

	if (errorjmpable)
		longjmp (errorjmp, 1);
	else
		I_FatalError ("humph");
}

// killough 2/22/98: Add support for ENDBOOM, which is PC-specific
// [RH] Use ENDOOM if ENDBOOM not found.

void I_EndDoom(void)
{
	int lump = W_CheckNumForName ("ENDOOM");

	if (lump != -1)
	{
		byte *scr = W_CacheLumpNum (lump, PU_CACHE);
		ScreenUpdate (scr);
		ScreenSetCursor (23, 0);
	}
}

char DoomStartupTitle[256] = { 0 };

void I_SetTitleString (const char *title)
{
	int i;

	for (i = 0; title[i]; i++)
		DoomStartupTitle[i] = title[i] | 0x80;
}

void I_PrintStr (int xp, const char *cp, int count, BOOL scroll) {
	char string[4096];

	memcpy (string, cp, count);
	if (scroll)
		string[count++] = '\n';
	string[count] = 0;

	fputs (string, stdout);
        fflush (stdout);
}
