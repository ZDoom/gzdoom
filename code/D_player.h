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
	CF_NOTARGET			= 8

} cheat_t;


//
// Extended player object info: player_t
//
typedef struct player_s
{
	mobj_t* 			mo;
	playerstate_t		playerstate;
	ticcmd_t			cmd;

	// Determine POV,
	//	including viewpoint bobbing during movement.
	// Focal origin above r.z
	fixed_t 			viewz;
	// Base height above floor for viewz.
	fixed_t 			viewheight;
	// Bob/squat speed.
	fixed_t 			deltaviewheight;
	// bounded/scaled total momentum.
	fixed_t 			bob;

	// This is only used between levels,
	// mo->health is used during levels.
	int 				health; 
	int 				armorpoints;
	// Armor type is 0-2.
	int 				armortype;		

	// Power ups. invinc and invis are tic counters.
	int 				powers[NUMPOWERS];
	BOOL 				cards[NUMCARDS];
	BOOL	 			backpack;
	
	// Frags, kills of other players.
	int 				frags[MAXPLAYERS];
	int					fragcount;			// [RH] Cumulative frags for this player

	weapontype_t		readyweapon;
	
	// Is wp_nochange if not changing.
	weapontype_t		pendingweapon;

	BOOL	 			weaponowned[NUMWEAPONS];
	int 				ammo[NUMAMMO];
	int 				maxammo[NUMAMMO];

	// True if button down last tic.
	int 				attackdown;
	int 				usedown;
	int					jumpdown;		// [RH] Don't jump like a fool

	// Bit flags, for cheats and debug.
	// See cheat_t, above.
	int 				cheats; 		

	// Refired shots are less accurate.
	int 				refire; 		

	 // For intermission stats.
	int 				killcount;
	int 				itemcount;
	int 				secretcount;

	// Hint messages.
	char*				message;		
	
	// For screen flashing (red or bright).
	int 				damagecount;
	int 				bonuscount;

	// Who did damage (NULL for floors/ceilings).
	mobj_t* 			attacker;
	
	// So gun flashes light up areas.
	int 				extralight;

	// Current PLAYPAL, ???
	//	can be set to REDCOLORMAP for pain, etc.
	int 				fixedcolormap;

	// Player skin colorshift,
	//	0-3 for which color to draw player.
	int 				colormap;		

	// Overlay view sprites (gun, etc).
	pspdef_t			psprites[NUMPSPRITES];

	// [RH] Pointer to a userinfo struct
	userinfo_t			*userinfo;

	// [RH] Tic when respawning is allowed
	int					respawn_time;

} player_t;


//
// INTERMISSION
// Structure passed e.g. to WI_Start(wb)
//
typedef struct
{
	BOOL	 	in; 	// whether the player is in game
	
	// Player stats, kills, collected items etc.
	int 		skills;
	int 		sitems;
	int 		ssecret;
	int 		stime; 
	int 		frags[MAXPLAYERS];
	int			fragcount;	// [RH] Cumulative frags for this player
	int 		score;	// current score on entry, modified on return
  
} wbplayerstruct_t;

typedef struct
{
	int 		epsd;	// episode # (0-2)

	// [RH] Name of map just finished
	char		current[8];

	// next level, [RH] actual map name
	char 		next[8];

	char		lname0[8];
	char		lname1[8];
	
	int 		maxkills;
	int 		maxitems;
	int 		maxsecret;
	int 		maxfrags;

	// the par time
	int 		partime;
	
	// index of this player in game
	int 		pnum;	

	wbplayerstruct_t	plyr[MAXPLAYERS];

} wbstartstruct_t;


#endif
//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
