//-----------------------------------------------------------------------------
//
// Copyright 1993-1996 id Software
// Copyright 1999-2016 Randy Heit
// Copyright 2002-2016 Christoph Oelckers
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


#ifndef __D_MAIN__
#define __D_MAIN__

#include "doomtype.h"
#include "gametype.h"

struct event_t;

//
// D_DoomMain()
// Not a globally visible function, just included for source reference,
// calls all startup code, parses command line options.
// If not overrided by user input, calls N_AdvanceDemo.
//

struct CRestartException
{
	char dummy;
};

void D_DoomMain (void);


void D_Display ();


//
// BASE LEVEL
//
void D_PageTicker (void);
void D_PageDrawer (void);
void D_AdvanceDemo (void);
void D_StartTitle (void);
bool D_AddFile (TArray<FString> &wadfiles, const char *file, bool check = true, int position = -1);


// [RH] Set this to something to draw an icon during the next screen refresh.
extern const char *D_DrawIcon;

// [SP] Store the capabilities of the renderer in a global variable, to prevent excessive per-frame processing
extern uint32_t r_renderercaps;


struct WadStuff
{
	WadStuff() : Type(0) {}

	FString Path;
	FString Name;
	int Type;
};

struct FIWADInfo
{
	FString Name;			// Title banner text for this IWAD
	FString Autoname;		// Name of autoload ini section for this IWAD
	FString Configname;		// Name of config section for this IWAD
	FString Required;		// Requires another IWAD
	uint32_t FgColor;			// Foreground color for title banner
	uint32_t BkColor;			// Background color for title banner
	EGameType gametype;		// which game are we playing?
	FString MapInfo;		// Base mapinfo to load
	TArray<FString> Load;	// Wads to be loaded with this one.
	TArray<FString> Lumps;	// Lump names for identification
	int flags;
	int preload;

	FIWADInfo() { flags = 0; preload = -1; FgColor = 0; BkColor= 0xc0c0c0; gametype = GAME_Doom; }
};

struct FStartupInfo
{
	FString Name;
	uint32_t FgColor;			// Foreground color for title banner
	uint32_t BkColor;			// Background color for title banner
	FString Song;
	int Type;
	enum
	{
		DefaultStartup,
		DoomStartup,
		HereticStartup,
		HexenStartup,
		StrifeStartup,
	};

};

extern FStartupInfo DoomStartupInfo;

//==========================================================================
//
// IWAD identifier class
//
//==========================================================================

class FIWadManager
{
	TArray<FIWADInfo> mIWads;
	TArray<FString> mIWadNames;
	TArray<int> mLumpsFound;

	void ParseIWadInfo(const char *fn, const char *data, int datasize);
	void ClearChecks();
	void CheckLumpName(const char *name);
	int GetIWadInfo();
	int ScanIWAD (const char *iwad);
	int CheckIWAD (const char *doomwaddir, WadStuff *wads);
	int IdentifyVersion (TArray<FString> &wadfiles, const char *iwad, const char *zdoom_wad);
public:
	void ParseIWadInfos(const char *fn);
	const FIWADInfo *FindIWAD(TArray<FString> &wadfiles, const char *iwad, const char *basewad);
	const FString *GetAutoname(unsigned int num) const
	{
		if (num < mIWads.Size()) return &mIWads[num].Autoname;
		else return NULL;
	}
	int GetIWadFlags(unsigned int num) const
	{
		if (num < mIWads.Size()) return mIWads[num].flags;
		else return false;
	}

};


#endif
