// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// $Id: i_system.h,v 1.7 1998/05/03 22:33:43 killough Exp $
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
// DESCRIPTION:
//      System specific interface stuff.
//
//-----------------------------------------------------------------------------

#ifndef __I_SYSTEM__
#define __I_SYSTEM__

#include <dir.h>

#include "d_ticcmd.h"
#include "d_event.h"

// Called by DoomMain.
void I_Init(void);

// Called by startup code
// to get the ammount of memory to malloc
// for the zone management.
byte *I_ZoneBase (unsigned int *size);

// Called by D_DoomLoop,
// returns current time in tics.
extern int (*I_GetTime) (void);
extern int (*I_WaitForTic) (int);

int I_GetTimePolled (void);
int I_GetTimeFake (void);

//
// Called by D_DoomLoop,
// called before processing any tics in a frame
// (just after displaying a frame).
// Time consuming syncronous operations
// are performed here (joystick reading).
// Can call D_PostEvent.
//
void I_StartFrame (void);

//
// Called by D_DoomLoop,
// called before processing each tic in a frame.
// Quick syncronous operations are performed here.
// Can call D_PostEvent.
void I_StartTic (void);

// Asynchronous interrupt functions should maintain private queues
// that are read by the synchronous functions
// to be converted into events.

// Either returns a null ticcmd,
// or calls a loadable driver to build it.
// This ticcmd will then be modified by the gameloop
// for normal input.
ticcmd_t* I_BaseTiccmd (void);

// Called by M_Responder when quit is selected.
// Clean exit, displays sell blurb.
void I_Quit (void);

// Allocates from low memory under dos,
// just mallocs under unix
byte* I_AllocLow (int length);

void I_Tactile (int on, int off, int total);

// killough 3/20/98: add const
// killough 4/25/98: add gcc attributes
void I_FatalError(const char *error, ...) __attribute__((format(printf,1,2)));
void I_Error(const char *error, ...) __attribute((format(printf,1,2)));

void I_EndDoom(void);         // killough 2/22/98: endgame screen

// Print a console string
void I_PrintStr (int x, const char *str, int count, BOOL scroll);

// Set the title string of the startup window
void I_SetTitleString (const char *title);

#define I_PauseMouse()
#define I_ResumeMouse()

// [RH] Returns millisecond-accurate time under Win32, 1 with DJGPP
unsigned int I_MSTime (void);

// [RH] Title string to display at bottom of console during startup
extern char DoomStartupTitle[256];

// Directory searching routines

typedef struct ffblk findstate_t;

long I_FindFirst (char *filespec, findstate_t *fileinfo);
int I_FindNext (long handle, findstate_t *fileinfo);
int I_FindClose (long handle);

#define I_FindName(a)	((a)->ff_name)
#define I_FindAttr(a)	((a)->ff_attrib)

#endif
