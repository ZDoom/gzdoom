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

#include "doomtype.h"

struct ticcmd_t;
struct WadStuff;

// Index values into the LanguageIDs array
enum
{
	LANGIDX_UserPreferred,
	LANGIDX_UserDefault,
	LANGIDX_SysPreferred,
	LANGIDX_SysDefault
};
extern uint32 LanguageIDs[4];
extern void SetLanguageIDs ();

// [RH] Detects the OS the game is running under.
void I_DetectOS (void);

typedef enum {
	os_unknown,
	os_Win95,
	os_WinNT4,
	os_Win2k
} os_t;

extern os_t OSPlatform;

// Helper template so that we can access newer Win32 functions with a single static
// variable declaration. If this were C++11 it could be totally transparent.
template<typename Proto>
class TOptWin32Proc
{
	static Proto GetOptionalWin32Proc(const char* module, const char* function)
	{
		HMODULE hmodule = GetModuleHandle(module);
		if (hmodule == NULL)
			return NULL;

		return (Proto)GetProcAddress(hmodule, function);
	}

public:
	const Proto Call;

	TOptWin32Proc(const char* module, const char* function)
		: Call(GetOptionalWin32Proc(module, function)) {}

	// Wrapper object can be tested against NULL, but not directly called.
	operator const void*() const { return Call; }
};

// Called by DoomMain.
void I_Init (void);

// Called by D_DoomLoop, returns current time in tics.
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

void atterm (void (*func)(void));
void popterm ();

// Set the mouse cursor. The texture must be 32x32.
class FTexture;
bool I_SetCursor(FTexture *cursor);

// Repaint the pre-game console
void I_PaintConsole (void);

// Print a console string
void I_PrintStr (const char *cp);

// Set the title string of the startup window
void I_SetIWADInfo ();

// Pick from multiple IWADs to use
int I_PickIWad (WadStuff *wads, int numwads, bool queryiwad, int defaultiwad);

// The ini could not be saved at exit
bool I_WriteIniFailed ();

// [RH] Returns millisecond-accurate time
unsigned int I_MSTime (void);
unsigned int I_FPSTime();

// [RH] Used by the display code to set the normal window procedure
void I_SetWndProc();

// [RH] Checks the registry for Steam's install path, so we can scan its
// directories for IWADs if the user purchased any through Steam.
TArray<FString> I_GetSteamPath();

// [GZ] Same deal for GOG paths
TArray<FString> I_GetGogPaths();

// Damn Microsoft for doing Get/SetWindowLongPtr half-assed. Instead of
// giving them proper prototypes under Win32, they are just macros for
// Get/SetWindowLong, meaning they take LONGs and not LONG_PTRs.
#ifdef _WIN64
typedef long long WLONG_PTR;
#elif _MSC_VER
typedef _W64 long WLONG_PTR;
#else
typedef long WLONG_PTR;
#endif

// Wrapper for GetLongPathName
FString I_GetLongPathName(FString shortpath);

// Directory searching routines

// Mirror WIN32_FIND_DATAA in <winbase.h>
#ifndef MAX_PATH
#define MAX_PATH 260
#endif
#ifndef PATH_MAX
#define PATH_MAX 260
#endif

struct findstate_t
{
	DWORD Attribs;
	DWORD Times[3*2];
	DWORD Size[2];
	DWORD Reserved[2];
	char Name[MAX_PATH];
	char AltName[14];
};

void *I_FindFirst (const char *filespec, findstate_t *fileinfo);
int I_FindNext (void *handle, findstate_t *fileinfo);
int I_FindClose (void *handle);

#define I_FindName(a)	((a)->Name)
#define I_FindAttr(a)	((a)->Attribs)

#define FA_RDONLY	0x00000001
#define FA_HIDDEN	0x00000002
#define FA_SYSTEM	0x00000004
#define FA_DIREC	0x00000010
#define FA_ARCH		0x00000020

#endif
