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
//
//
//-----------------------------------------------------------------------------


#ifndef __D_PLAYER__
#define __D_PLAYER__


// The player data structure depends on a number
// of other structs: items (internal inventory),
// animation states (closely tied to the sprites
// used to represent them, unfortunately).
#include "d_items.h"
#include "p_pspr.h"

// In addition, the player is just a special
// case of the generic moving object/actor.
#include "p_mobj.h"

// Finally, for odd reasons, the player input
// is buffered within the player data struct,
// as commands per game tick.
#include "d_ticcmd.h"

#include "d_netinf.h"





//
// Player states.
//
typedef enum
{
	// Playing or camping.
	PST_LIVE,
	// Dead on the ground, view follows killer.
	PST_DEAD,
	// Ready to restart/respawn???
	PST_REBORN			

} playerstate_t;


//
// Player internal flags, for cheats and debug.
//
typedef enum
{
	// No clipping, walk through barriers.
	CF_NOCLIP			= 1,
	// No damage, no health loss.
	CF_GODMODE			= 2,
	// Not really a cheat, just a debug aid.
	CF_NOMOMENTUM		= 4,
	// [RH] Monsters don't target
	CF_NOTARGET			= 8,
	// [RH] Put camera behind player
	CF_CHASECAM			= 16,
	// [RH] Don't let the player move
	CF_FROZEN			= 32,
	// [RH] Stick camera in player's head if he moves
	CF_REVERTPLEASE		= 64
} cheat_t;


//
// Extended player object info: player_t
//
typedef struct player_s
{
	mobj_t *mo;
	playerstate_t playerstate;
	ticcmd_t cmd;

	userinfo_t	userinfo;				// [RH] who is this?
	
	fixed_t		viewz;					// focal origin above r.z
	fixed_t		viewheight;				// base height above floor for viewz
	fixed_t		deltaviewheight;		// squat speed.
	fixed_t		bob;					// bounded/scaled total momentum

	// killough 10/98: used for realistic bobbing (i.e. not simply overall speed)
	// mo->momx and mo->momy represent true momenta experienced by player.
	// This only represents the thrust that the player applies himself.
	// This avoids anomolies with such things as Boom ice and conveyors.
	fixed_t		momx, momy;				// killough 10/98

	int			health;					// only used between levels, mo->health
										// is used during levels
	int			armorpoints;
	int			armortype;				// armor type is 0-2


	int			powers[NUMPOWERS];		// invinc and invis are tic counters
	BOOL		cards[NUMCARDS];
	BOOL		backpack;
	
	int			frags[MAXPLAYERS];		// kills of other players
	int			fragcount;				// [RH] Cumulative frags for this player

	weapontype_t	readyweapon;
	weapontype_t	pendingweapon;		// wp_nochange if not changing
	BOOL		weaponowned[NUMWEAPONS];
	int			ammo[NUMAMMO];
	int			maxammo[NUMAMMO];

	int			attackdown, usedown;	// true if button down last tic
	int			cheats;					// bit flags
	int			refire;					// refired shots are less accurate
	int			killcount, itemcount, secretcount;		// for intermission
	int			damagecount, bonuscount;// for screen flashing
	mobj_t		*attacker;				// who did damage (NULL for floors
	int			extralight;				// so gun flashes light up areas
	int			fixedcolormap;			// can be set to REDCOLORMAP, etc.
	int			xviewshift;				// [RH] view shift (for earthquakes)
	pspdef_t	psprites[NUMPSPRITES];	// view sprites (gun, etc)
	int			jumpTics;				// delay the next jump for a moment

	int			respawn_time;			// [RH] delay respawning until this tic
	fixed_t		oldvelocity[3];			// [RH] Used for falling damage
	mobj_t		*camera;				// [RH] Whose eyes this player sees through
} player_t;


//
// INTERMISSION
// Structure passed e.g. to WI_Start(wb)
//
typedef struct
{
	BOOL		in;			// whether the player is in game
	
	// Player stats, kills, collected items etc.
	int			skills;
	int			sitems;
	int			ssecret;
	int			stime;
	int			frags[MAXPLAYERS];
	int			fragcount;	// [RH] Cumulative frags for this player
	int			score;		// current score on entry, modified on return

} wbplayerstruct_t;

typedef struct
{
	int			epsd;	// episode # (0-2)

	char		current[8];	// [RH] Name of map just finished
	char		next[8];	// next level, [RH] actual map name

	char		lname0[8];
	char		lname1[8];
	
	int			maxkills;
	int			maxitems;
	int			maxsecret;
	int			maxfrags;

	// the par time
	int			partime;
	
	// index of this player in game
	int			pnum;	

	wbplayerstruct_t	plyr[MAXPLAYERS];
} wbstartstruct_t;


#endif
