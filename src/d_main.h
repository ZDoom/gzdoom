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
//		System specific interface stuff.
//
//-----------------------------------------------------------------------------


#ifndef __D_MAIN__
#define __D_MAIN__

#include "d_event.h"

//
// D_DoomMain()
// Not a globally visible function, just included for source reference,
// calls all startup code, parses command line options.
// If not overrided by user input, calls N_AdvanceDemo.
//
void D_DoomMain (void);

// Called by IO functions when input is detected.
void D_PostEvent (const event_t* ev);

void D_Display ();


//
// BASE LEVEL
//
void D_PageTicker (void);
void D_PageDrawer (void);
void D_AdvanceDemo (void);
void D_StartTitle (void);


// [RH] Set this to something to draw an icon during the next screen refresh.
extern const char *D_DrawIcon;


enum EIWADType
{
	IWAD_Doom2TNT,
	IWAD_Doom2Plutonia,
	IWAD_Hexen,
	IWAD_HexenDK,
	IWAD_Doom2,
	IWAD_HereticShareware,
	IWAD_HereticExtended,
	IWAD_Heretic,
	IWAD_DoomShareware,
	IWAD_UltimateDoom,
	IWAD_DoomRegistered,
	IWAD_Strife,
	IWAD_StrifeTeaser,
	IWAD_StrifeTeaser2,

	NUM_IWAD_TYPES
};

struct WadStuff
{
	WadStuff() : Type(IWAD_Doom2TNT) {}

	FString Path;
	EIWADType Type;
};

struct IWADInfo
{
	const char *Name;		// Title banner text for this IWAD
	const char *Autoname;	// Name of autoload ini section for this IWAD
	DWORD FgColor;			// Foreground color for title banner
	DWORD BkColor;			// Background color for title banner
};

extern const IWADInfo IWADInfos[NUM_IWAD_TYPES];

#endif
