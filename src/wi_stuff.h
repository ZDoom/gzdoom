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
//	Intermission.
//
//-----------------------------------------------------------------------------

#ifndef __WI_STUFF__
#define __WI_STUFF__

#include "doomdef.h"

struct FLevelLocals;

//
// INTERMISSION
// Structure passed e.g. to WI_Start(wb)
//
struct wbplayerstruct_t
{
	// Player stats, kills, collected items etc.
	int			skills;
	int			sitems;
	int			ssecret;
	int			stime;
	int			frags[MAXPLAYERS];
	int			fragcount;	// [RH] Cumulative frags for this player
};

struct wbstartstruct_t
{
	int			finished_ep;
	int			next_ep;

	FString		current;	// [RH] Name of map just finished
	FString		next;		// next level, [RH] actual map name
	FString		nextname;	// printable name for next level.
	FString		thisname;	// printable name for next level.
	FString		nextauthor;	// printable name for next level.
	FString		thisauthor;	// printable name for next level.

	FTextureID	LName0;
	FTextureID	LName1;

	int			maxkills;
	int			maxitems;
	int			maxsecret;
	int			maxfrags;

	// the par time and sucktime
	int			partime;	// in tics
	int			sucktime;	// in minutes

	// total time for the entire current game
	int			totaltime;

	// index of this player in game
	int			pnum;

	wbplayerstruct_t	plyr[MAXPLAYERS];
	
};

// Intermission stats.
// Parameters for world map / intermission.

// Called by main loop, animate the intermission.
void WI_Ticker ();

// Called by main loop,
// draws the intermission directly into the screen buffer.
void WI_Drawer ();

// Setup for an intermission screen.
void WI_Start (wbstartstruct_t *wbstartstruct);

#endif
