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

#include "a_artifacts.h"

// The player data structure depends on a number
// of other structs: items (internal inventory),
// animation states (closely tied to the sprites
// used to represent them, unfortunately).
#include "p_pspr.h"

// In addition, the player is just a special
// case of the generic moving object/actor.
#include "actor.h"

#include "d_netinf.h"

//Added by MC:
#include "b_bot.h"

class player_s;

class APlayerPawn : public AActor
{
	DECLARE_STATELESS_ACTOR (APlayerPawn, AActor)
public:
	virtual void PlayIdle ();
	virtual void PlayRunning ();
	virtual void PlayAttacking ();
	virtual void PlayAttacking2 ();
	virtual bool HealOther (player_s *pawn);	// returns true if effective
	virtual void ThrowPoisonBag ();
	virtual const TypeInfo *GetDropType ();
	virtual void GiveDefaultInventory ();
	virtual int GetAutoArmorSave ();
	virtual fixed_t GetArmorIncrement (int armortype);
	virtual int GetArmorMax ();
	virtual const char *GetSoundClass ();
	virtual fixed_t GetJumpZ ();
	virtual void TweakSpeeds (int &forwardmove, int &sidemove);
	virtual bool DoHealingRadius (APlayerPawn *other);

	virtual void NoBlockingSet ();

	void BeginPlay ();
};

class APlayerChunk : APlayerPawn
{
	DECLARE_STATELESS_ACTOR (APlayerChunk, APlayerPawn)
};

//
// Player states.
//
typedef enum
{
	PST_LIVE,	// Playing or camping.
	PST_DEAD,	// Dead on the ground, view follows killer.
	PST_REBORN,	// Ready to restart/respawn???
	PST_ENTER	// [BC] Entered the game
} playerstate_t;


//
// Player internal flags, for cheats and debug.
//
typedef enum
{
	CF_NOCLIP			= 1,	// No clipping, walk through barriers.
	CF_GODMODE			= 2,	// No damage, no health loss.
	CF_NOMOMENTUM		= 4,	// Not really a cheat, just a debug aid.
	CF_NOTARGET			= 8,	// [RH] Monsters don't target
	CF_FLY				= 16,	// [RH] Flying player
	CF_CHASECAM			= 32,	// [RH] Put camera behind player
	CF_FROZEN			= 64,	// [RH] Don't let the player move
	CF_REVERTPLEASE		= 128,	// [RH] Stick camera in player's head if (s)he moves
	CF_STEPLEFT			= 512,	// [RH] Play left footstep sound next time
	CF_FRIGHTENING		= 1024,	// [RH] Scare monsters away
	CF_INSTANTWEAPSWITCH= 2048,	// [RH] Switch weapons instantly
	CF_TOTALLYFROZEN	= 4096, // [RH] All players can do is press +use
} cheat_t;

#define WPIECE1		1
#define WPIECE2		2
#define WPIECE3		4

//
// Extended player object info: player_t
//
class player_s
{
public:
	void Serialize (FArchive &arc);
	void FixPointers (const DObject *obj);

	bool UseAmmo (bool noCheck=false);

	APlayerPawn	*mo;
	BYTE		playerstate;
	ticcmd_t	cmd;

	userinfo_t	userinfo;				// [RH] who is this?
	FWeaponSlots WeaponSlots;			// Weapon slot assignments for this player
	
	const TypeInfo *cls;				// class of associated PlayerPawn

	float		DesiredFOV;				// desired field of vision
	float		FOV;					// current field of vision
	fixed_t		viewz;					// focal origin above r.z
	fixed_t		viewheight;				// base height above floor for viewz
	fixed_t		deltaviewheight;		// squat speed.
	fixed_t		bob;					// bounded/scaled total momentum

	// killough 10/98: used for realistic bobbing (i.e. not simply overall speed)
	// mo->momx and mo->momy represent true momenta experienced by player.
	// This only represents the thrust that the player applies himself.
	// This avoids anomolies with such things as Boom ice and conveyors.
	fixed_t		momx, momy;				// killough 10/98

	bool		centering;
	byte		turnticks;
	short		oldbuttons;
	int			health;					// only used between levels, mo->health
										// is used during levels
	int			armortype;				// armor type is 0-2
	int			armorpoints[NUMARMOR];

	int			inventorytics;
	WORD		inventory[NUMINVENTORYSLOTS];
	BYTE		CurrentPlayerClass;		// class # for this player instance
	artitype_t	readyArtifact;
	int			artifactCount;
	int			inventorySlotNum;
	int			powers[NUMPOWERS];
	bool		keys[NUMKEYS];
	int			pieces;					// Fourth Weapon pieces
	bool		backpack;
	
	int			frags[MAXPLAYERS];		// kills of other players
	int			fragcount;				// [RH] Cumulative frags for this player
	int			lastkilltime;			// [RH] For multikills
	byte		multicount;
	byte		spreecount;				// [RH] Keep track of killing sprees

	weapontype_t	readyweapon;
	weapontype_t	pendingweapon;		// wp_nochange if not changing
	bool		weaponowned[NUMWEAPONS];
	int			ammo[NUMAMMO];
	int			maxammo[NUMAMMO];

	int			cheats;					// bit flags
	short		refire;					// refired shots are less accurate
	short		inconsistant;
	int			killcount, itemcount, secretcount;		// for intermission
	int			damagecount, bonuscount;// for screen flashing
	int			mstaffcount;			// for mage's bloodscourge flash
	int			cholycount;				// for cleric's wraithverge flash
	int			flamecount;				// for flame thrower duration
	int			poisoncount;			// screen flash for poison damage
	AActor		*poisoner;				// NULL for non-player actors
	AActor		*attacker;				// who did damage (NULL for floors)
	int			extralight;				// so gun flashes light up areas
	int			fixedcolormap;			// can be set to REDCOLORMAP, etc.
	int			xviewshift;				// [RH] view shift (for earthquakes)
	pspdef_t	psprites[NUMPSPRITES];	// view sprites (gun, etc)
	int			morphTics;				// player is a chicken/pig if > 0
	int			chickenPeck;			// chicken peck countdown
	AActor		*rain1;					// active rain maker 1
	AActor		*rain2;					// active rain maker 2
	int			jumpTics;				// delay the next jump for a moment

	int			respawn_time;			// [RH] delay respawning until this tic
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

	FPlayerSkin	*skin;		// [RH] Sprite override
	float		BlendR;		// [RH] Final blending values
	float		BlendG;
	float		BlendB;
	float		BlendA;
};

typedef player_s player_t;

// Bookkeeping on players - state.
extern player_s players[MAXPLAYERS];

inline FArchive &operator<< (FArchive &arc, player_s *&p)
{
	return arc.SerializePointer (players, (BYTE **)&p, sizeof(*players));
}

#endif // __D_PLAYER_H__
