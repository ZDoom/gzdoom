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
// DESCRIPTION:
//		System specific interface stuff.
//
//-----------------------------------------------------------------------------


#ifndef __I_SYSTEM__
#define __I_SYSTEM__

#include <dirent.h>
#include <ctype.h>

#include "doomtype.h"

struct ticcmd_t;
struct WadStuff;

#ifndef SHARE_DIR
#define SHARE_DIR "/usr/local/share/"
#endif

// Index values into the LanguageIDs array
enum
{
	LANGIDX_UserPreferred,
	LANGIDX_UserDefault,
	LANGIDX_SysPreferred,
	LANGIDX_SysDefault
};
extern DWORD LanguageIDs[4];
extern void SetLanguageIDs ();

// Called by DoomMain.
void I_Init (void);

// Called by D_DoomLoop,
// returns current time in tics.
extern int (*I_GetTime) (bool saveMS);

// like I_GetTime, except it waits for a new tic before returning
extern int (*I_WaitForTic) (int);

// Freezes tic counting temporarily. While frozen, calls to I_GetTime()
// will always return the same value. This does not affect I_MSTime().
// You must also not call I_WaitForTic() while freezing time, since the
// tic will never arrive (unless it's the current one).
extern void (*I_FreezeTime) (bool frozen);

fixed_t I_GetTimeFrac (uint32 *ms);

// Return a seed value for the RNG.
unsigned int I_MakeRNGSeed();


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
ticcmd_t *I_BaseTiccmd (void);


// Called by M_Responder when quit is selected.
// Clean exit, displays sell blurb.
void I_Quit (void);


void I_Tactile (int on, int off, int total);

void STACK_ARGS I_Error (const char *error, ...) GCCPRINTF(1,2);
void STACK_ARGS I_FatalError (const char *error, ...) GCCPRINTF(1,2);

void addterm (void (*func)(void), const char *name);
#define atterm(t) addterm (t, #t)
void popterm ();

// Print a console string
void I_PrintStr (const char *str);

// Set the title string of the startup window
void I_SetIWADInfo ();

// Pick from multiple IWADs to use
int I_PickIWad (WadStuff *wads, int numwads, bool queryiwad, int defaultiwad);

// [RH] Checks the registry for Steam's install path, so we can scan its
// directories for IWADs if the user purchased any through Steam.
TArray<FString> I_GetSteamPath();

TArray<FString> I_GetGogPaths();

// The ini could not be saved at exit
bool I_WriteIniFailed ();

// [RH] Returns millisecond-accurate time
unsigned int I_MSTime (void);
unsigned int I_FPSTime();

class FTexture;
bool I_SetCursor(FTexture *);

// Directory searching routines

struct findstate_t
{
    int count;
    struct dirent **namelist;
    int current;
};

void *I_FindFirst (const char *filespec, findstate_t *fileinfo);
int I_FindNext (void *handle, findstate_t *fileinfo);
int I_FindClose (void *handle);
int I_FindAttr (findstate_t *fileinfo); 

#define I_FindName(a)	((a)->namelist[(a)->current]->d_name)

#define FA_RDONLY	1
#define FA_HIDDEN	2
#define FA_SYSTEM	4
#define FA_DIREC	8
#define FA_ARCH		16

static inline char *strlwr(char *str)
{
	char *ptr = str;
	while(*ptr)
	{
		*ptr = tolower(*ptr);
		++ptr;
	}
	return str;
}

#endif
