// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// $Id: i_main.c,v 1.8 1998/05/15 00:34:03 killough Exp $
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
//      Main program, simply calls D_DoomMain high level loop.
//
//-----------------------------------------------------------------------------

#include <stdlib.h>
#include <sys/stat.h>
#include <signal.h>
#include <pc.h>

extern "C" {
#include "m_argv.h"
#include "d_main.h"
#include "i_system.h"
#include "c_console.h"
#include "z_zone.h"
}

// cleanup handling -- killough:
static void handler(int s)
{
	char buf[64];

	signal(s,SIG_IGN);  // Ignore future instances of this signal.

	strcpy(buf,
		 s==SIGSEGV ? "Segmentation Violation" :
		 s==SIGINT  ? "Interrupted by User" :
		 s==SIGILL  ? "Illegal Instruction" :
		 s==SIGFPE  ? "Floating Point Exception" :
		 s==SIGTERM ? "Killed" : "Terminated by signal");

	// If corrupted memory could cause crash, dump memory
	// allocation history, which points out probable causes

//	if (s==SIGSEGV || s==SIGILL || s==SIGFPE)
//		Z_DumpHistory(buf);

	I_FatalError(buf);
}

extern "C" void I_Quit(void);

int main (int argc, char **argv)
{
	myargc = argc;
	myargv = argv;

	/*
	 killough 1/98:

	 This fixes some problems with exit handling
	 during abnormal situations.

	 The old code called I_Quit() to end program,
	 while now I_Quit() is installed as an exit
	 handler and exit() is called to exit, either
	 normally or abnormally. Seg faults are caught
	 and the error handler is used, to prevent
	 being left in graphics mode or having very
	 loud SFX noise because the sound card is
	 left in an unstable state.
	*/

//	allegro_init();
	Z_Init();					// 1/18/98 killough: start up memory stuff first
	atexit(I_Quit);
	signal(SIGSEGV, handler);
	signal(SIGTERM, handler);
	signal(SIGILL,  handler);
	signal(SIGFPE,  handler);
	signal(SIGILL,  handler);
	signal(SIGINT,  handler);	// killough 3/6/98: allow CTRL-BRK during init
	signal(SIGABRT, handler);

	// [RH] Try and figure out which directory the game resides in
	_fixpath (argv[0], progdir);
    *(strrchr (progdir, '/') + 1) = 0;

	ScreenClear ();
	ScreenSetCursor (1, 0);

	// [RH] Prep the pre-game console
	{
		int cols = ScreenCols ();
		int rows = ScreenRows ();

		C_InitConsole ((cols ? cols : 80)*8, (rows ? rows - 1 : 24)*8, false);
	}
	D_DoomMain ();

	return 0;
}
