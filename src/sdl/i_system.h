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

#include "d_main.h"
#include "d_ticcmd.h"
#include "d_event.h"

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

struct CPUInfo	// 92 bytes
{
	char VendorID[16];
	char CPUString[48];

	BYTE Stepping;
	BYTE Model;
	BYTE Family;
	BYTE Type;

	BYTE BrandIndex;
	BYTE CLFlush;
	BYTE CPUCount;
	BYTE APICID;

	DWORD bSSE3:1;
	DWORD DontCare1:31;

	DWORD bFPU:1;
	DWORD bVME:1;
	DWORD bDE:1;
	DWORD bPSE:1;
	DWORD bRDTSC:1;
	DWORD bMSR:1;
	DWORD bPAE:1;
	DWORD bMCE:1;
	DWORD bCX8:1;
	DWORD bAPIC:1;
	DWORD bReserved1:1;
	DWORD bSEP:1;
	DWORD bMTRR:1;
	DWORD bPGE:1;
	DWORD bMCA:1;
	DWORD bCMOV:1;
	DWORD bPAT:1;
	DWORD bPSE36:1;
	DWORD bPSN:1;
	DWORD bCFLUSH:1;
	DWORD bReserved2:1;
	DWORD bDS:1;
	DWORD bACPI:1;
	DWORD bMMX:1;
	DWORD bFXSR:1;
	DWORD bSSE:1;
	DWORD bSSE2:1;
	DWORD bSS:1;
	DWORD bHTT:1;
	DWORD bTM:1;
	DWORD bReserved3:1;
	DWORD bPBE:1;

	DWORD DontCare2:22;
	DWORD bMMXPlus:1;		// AMD's MMX extensions
	DWORD bMMXAgain:1;		// Just a copy of bMMX above
	DWORD DontCare3:6;
	DWORD b3DNowPlus:1;
	DWORD b3DNow:1;

	BYTE AMDStepping;
	BYTE AMDModel;
	BYTE AMDFamily;
	BYTE bIsAMD;

	BYTE DataL1LineSize;
	BYTE DataL1LinesPerTag;
	BYTE DataL1Associativity;
	BYTE DataL1SizeKB;
};


extern "C" {
	extern CPUInfo CPU;
}

// Called by DoomMain.
void I_Init (void);

// Called by D_DoomLoop,
// returns current time in tics.
extern int (*I_GetTime) (bool saveMS);

// like I_GetTime, except it waits for a new tic before returning
extern int (*I_WaitForTic) (int);

int I_GetTimePolled (bool saveMS);
int I_GetTimeFake (void);

fixed_t I_GetTimeFrac (uint32 *ms);


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
struct IWADInfo;
void I_SetIWADInfo (const IWADInfo *info);

// Pick from multiple IWADs to use
int I_PickIWad (WadStuff *wads, int numwads, bool queryiwad, int defaultiwad);

// The ini could not be saved at exit
bool I_WriteIniFailed ();

// [RH] Returns millisecond-accurate time
unsigned int I_MSTime (void);


// Directory searching routines

typedef struct
{
    int count;
    struct dirent **namelist;
    int current;
} findstate_t;

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
