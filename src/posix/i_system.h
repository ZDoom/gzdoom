//-----------------------------------------------------------------------------
//
// Copyright 1993-1996 id Software
// Copyright 1999-2016 Randy Heit
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//		System specific interface stuff.
//
//-----------------------------------------------------------------------------


#ifndef __I_SYSTEM__
#define __I_SYSTEM__

#include <dirent.h>
#include <ctype.h>

#define __solaris__ (defined(__sun) || defined(__sun__) || defined(__SRV4) || defined(__srv4__))

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
extern uint32_t LanguageIDs[4];
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

double I_GetTimeFrac (uint32_t *ms);
void I_SetFrameTime();

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

void I_Error (const char *error, ...) GCCPRINTF(1,2);
void I_FatalError (const char *error, ...) GCCPRINTF(1,2);

void addterm (void (*func)(void), const char *name);
#define atterm(t) addterm (t, #t)
void popterm ();

void I_DebugPrint (const char *cp);

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
private:
    int count;
    struct dirent **namelist;
    int current;

	friend void *I_FindFirst(const char *filespec, findstate_t *fileinfo);
	friend int I_FindNext(void *handle, findstate_t *fileinfo);
	friend const char *I_FindName(findstate_t *fileinfo);
	friend int I_FindAttr(findstate_t *fileinfo);
	friend int I_FindClose(void *handle);
};

void *I_FindFirst (const char *filespec, findstate_t *fileinfo);
int I_FindNext (void *handle, findstate_t *fileinfo);
int I_FindClose (void *handle);
int I_FindAttr (findstate_t *fileinfo); 

inline const char *I_FindName(findstate_t *fileinfo)
{
	return (fileinfo->namelist[fileinfo->current]->d_name);
}

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
