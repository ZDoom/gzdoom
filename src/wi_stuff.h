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
//	Intermission.
//
//-----------------------------------------------------------------------------

#ifndef __WI_STUFF__
#define __WI_STUFF__

#include "doomdef.h"

class FTexture;

//
// INTERMISSION
// Structure passed e.g. to WI_Start(wb)
//
struct wbplayerstruct_t
{
	bool		in;			// whether the player is in game

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

	FTexture	*LName0;
	FTexture	*LName1;

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
extern wbstartstruct_t wminfo;


// Called by main loop, animate the intermission.
void WI_Ticker ();

// Called by main loop,
// draws the intermission directly into the screen buffer.
void WI_Drawer ();

// Setup for an intermission screen.
void WI_Start (wbstartstruct_t *wbstartstruct);

#endif
