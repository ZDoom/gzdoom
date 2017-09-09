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
	FString Path;
	FString Name;
};

struct FIWADInfo
{
	FString Name;			// Title banner text for this IWAD
	FString Autoname;		// Name of autoload ini section for this IWAD
	FString IWadname;		// Default name this game would use - this is for IWAD detection in GAMEINFO.
	int prio = 0;			// selection priority for given IWAD name.
	FString Configname;		// Name of config section for this IWAD
	FString Required;		// Requires another IWAD
	uint32_t FgColor = 0;	// Foreground color for title banner
	uint32_t BkColor = 0xc0c0c0;		// Background color for title banner
	EGameType gametype = GAME_Doom;		// which game are we playing?
	FString MapInfo;		// Base mapinfo to load
	TArray<FString> Load;	// Wads to be loaded with this one.
	TArray<FString> Lumps;	// Lump names for identification
	int flags = 0;
};

struct FFoundWadInfo
{
	FString mFullPath;
	FString mRequiredPath;
	int mInfoIndex = -1;	// must be an index because of reallocation

	FFoundWadInfo() {}
	FFoundWadInfo(const FString &s1, const FString &s2, int index)
		: mFullPath(s1), mRequiredPath(s2), mInfoIndex(index)
	{
	}
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
	TArray<FIWADInfo> mIWadInfos;
	TArray<FString> mIWadNames;
	TArray<FString> mSearchPaths;
	TArray<FString> mOrderNames;
	TArray<FFoundWadInfo> mFoundWads;
	TArray<int> mLumpsFound;

	void ParseIWadInfo(const char *fn, const char *data, int datasize, FIWADInfo *result = nullptr);
	int ScanIWAD (const char *iwad);
	int CheckIWADInfo(const char *iwad);
	int IdentifyVersion (TArray<FString> &wadfiles, const char *iwad, const char *zdoom_wad, const char *optional_wad);
	void CollectSearchPaths();
	void AddIWADCandidates(const char *dir);
	void ValidateIWADs();
public:
	FIWadManager(const char *fn);
	const FIWADInfo *FindIWAD(TArray<FString> &wadfiles, const char *iwad, const char *basewad, const char *optionalwad);
	const FString *GetAutoname(unsigned int num) const
	{
		if (num < mIWadInfos.Size()) return &mIWadInfos[num].Autoname;
		else return NULL;
	}
	int GetIWadFlags(unsigned int num) const
	{
		if (num < mIWadInfos.Size()) return mIWadInfos[num].flags;
		else return false;
	}

};


#endif
