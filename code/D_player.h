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


#ifndef __D_PLAYER_H__
#define __D_PLAYER_H__

// Finally, for odd reasons, the player input
// is buffered within the player data struct,
// as commands per game tick.
#include "d_ticcmd.h"

// The player data structure depends on a number
// of other structs: items (internal inventory),
// animation states (closely tied to the sprites
// used to represent them, unfortunately).
#include "d_items.h"
#include "p_pspr.h"

// In addition, the player is just a special
// case of the generic moving object/actor.
#include "actor.h"

#include "d_netinf.h"

//Added by MC:
#include "b_bot.h"




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
	// [RH] Flying player
	CF_FLY				= 16,
	// [RH] Put camera behind player
	CF_CHASECAM			= 32,
	// [RH] Don't let the player move
	CF_FROZEN			= 64,
	// [RH] Stick camera in player's head if he moves
	CF_REVERTPLEASE		= 128
} cheat_t;


//
// Extended player object info: player_t
//
class player_s
{
public:
	void Serialize (FArchive &arc);

	AActor		*mo;
	BYTE		playerstate;
	struct ticcmd_t	cmd;

	userinfo_t	userinfo;				// [RH] who is this?
	
	float		fov;					// field of vision
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
	bool		cards[NUMCARDS];
	BOOL		backpack;
	
	int			frags[MAXPLAYERS];		// kills of other players
	int			fragcount;				// [RH] Cumulative frags for this player

	weapontype_t	readyweapon;
	weapontype_t	pendingweapon;		// wp_nochange if not changing
	int			weaponowned[NUMWEAPONS];// needs to be int for status bar
	int			ammo[NUMAMMO];
	int			maxammo[NUMAMMO];

	int			attackdown, usedown;	// true if button down last tic
	int			cheats;					// bit flags
	short		refire;					// refired shots are less accurate
	short		inconsistant;
	int			killcount, itemcount, secretcount;		// for intermission
	int			damagecount, bonuscount;// for screen flashing
	AActor		*attacker;				// who did damage (NULL for floors)
	int			extralight;				// so gun flashes light up areas
	int			fixedcolormap;			// can be set to REDCOLORMAP, etc.
	int			xviewshift;				// [RH] view shift (for earthquakes)
	pspdef_t	psprites[NUMPSPRITES];	// view sprites (gun, etc)
	int			jumpTics;				// delay the next jump for a moment

	int			respawn_time;			// [RH] delay respawning until this tic
	fixed_t		oldvelocity[3];			// [RH] Used for falling damage
	AActor		*camera;				// [RH] Whose eyes this player sees through

	int			air_finished;			// [RH] Time when you start drowning

	//Added by MC:
	angle_t		savedyaw;
	int			savedpitch;

	angle_t		angle;		// The wanted angle that the bot try to get every tic.
							//  (used to get a smoth view movement)
	AActor		*dest;		// Move Destination.
	AActor		*prev;		// Previous move destination.


	AActor		*enemy;		// The dead meat.
	AActor		*missile;	// A threathing missile that got to be avoided.
	AActor		*mate;		// Friend (used for grouping in templay or coop.
	AActor		*last_mate;	// If bots mate dissapeared (not if died) that mate is
							// pointed to by this. Allows bot to roam to it if
							// necessary.

	//Skills
	struct botskill_t	skill;

	//Tickers
	int			t_active;	// Open door, lower lift stuff, door must open and
							// lift must go down before bot does anything
							// radical like try a stuckmove
	int			t_respawn;
	int			t_strafe;
	int			t_react;
	int			t_fight;
	int			t_roam;
	int			t_rocket;

	//Misc booleans
	bool		isbot;
	bool		first_shot;	// Used for reaction skill.
	bool		sleft;		// If false, strafe is right.
	bool		allround;
	bool		redteam;	// in ctf, if true this bot is on red team, else on blue..

	fixed_t		oldx;
	fixed_t		oldy;

	//Misc
	char c_target[256];		// Target for chat.
	botchat_t chat;			// What bot will say one a tic.
};
typedef player_s player_t;

// Bookkeeping on players - state.
extern	player_s		players[MAXPLAYERS];

inline FArchive &operator<< (FArchive &arc, player_s *p)
{
	if (p)
		return arc << (BYTE)(p - players);
	else
		return arc << (BYTE)255;
}
inline FArchive &operator>> (FArchive &arc, player_s* &p)
{
	BYTE ofs;
	arc >> ofs;
	if (ofs != 255)
		p = players + ofs;
	else
		p = NULL;
	return arc;
}


//
// INTERMISSION
// Structure passed e.g. to WI_Start(wb)
//
typedef struct wbplayerstruct_s
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

typedef struct wbstartstruct_s
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

	wbplayerstruct_s	plyr[MAXPLAYERS];
} wbstartstruct_t;


#endif // __D_PLAYER_H__
