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
	virtual void Serialize (FArchive &arc);

	virtual void Tick();
	virtual void AddInventory (AInventory *item);
	virtual void RemoveInventory (AInventory *item);
	virtual bool UseInventory (AInventory *item);

	virtual void PlayIdle ();
	virtual void PlayRunning ();
	virtual void PlayAttacking ();
	virtual void PlayAttacking2 ();
	virtual void ThrowPoisonBag ();
	virtual void GiveDefaultInventory ();
	virtual const char *GetSoundClass ();
	virtual fixed_t GetJumpZ ();
	virtual void TweakSpeeds (int &forwardmove, int &sidemove);
	virtual bool DoHealingRadius (APlayerPawn *other);
	virtual void MorphPlayerThink ();
	virtual void ActivateMorphWeapon ();
	virtual AWeapon *PickNewWeapon (const PClass *ammotype);
	virtual AWeapon *BestWeapon (const PClass *ammotype);

	enum EInvulState
	{
		INVUL_Start,
		INVUL_Active,
		INVUL_Stop,
		INVUL_GetAlpha
	};

	virtual void SpecialInvulnerabilityHandling (EInvulState state, fixed_t * pAlpha=NULL);

	void BeginPlay ();
	void Die (AActor *source, AActor *inflictor);

	fixed_t		JumpZ;				// [GRB] Variable JumpZ
	int			crouchsprite;
};

class APlayerChunk : public APlayerPawn
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
	CF_PREDICTING		= 8192,	// [RH] Player movement is being predicted
	CF_WEAPONREADY		= 16384,// [RH] Weapon is in the ready state, so bob it when walking
} cheat_t;

#define WPIECE1		1
#define WPIECE2		2
#define WPIECE3		4

enum
{
	PW_INVULNERABILITY	= 1,
	PW_INVISIBILITY		= 2,
	PW_INFRARED			= 4,

// Powerups added in Heretic
	PW_WEAPONLEVEL2		= 8,
	PW_FLIGHT			= 16,

// Powerups added in Hexen
	PW_SPEED			= 32,
	PW_MINOTAUR			= 64,
};

#define WP_NOCHANGE ((AWeapon*)~0)

//
// Extended player object info: player_t
//
class player_s
{
public:
	void Serialize (FArchive &arc);
	void FixPointers (const DObject *obj, DObject *replacement);

	void SetLogNumber (int num);
	void SetLogText (const char *text);

	APlayerPawn	*mo;
	BYTE		playerstate;
	ticcmd_t	cmd;

	userinfo_t	userinfo;				// [RH] who is this?
	
	const PClass *cls;				// class of associated PlayerPawn

	float		DesiredFOV;				// desired field of vision
	float		FOV;					// current field of vision
	fixed_t		viewz;					// focal origin above r.z
	fixed_t		viewheight;				// base height above floor for viewz
	fixed_t		defaultviewheight;		// The normal view height when standing
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
	bool		attackdown;
	int			health;					// only used between levels, mo->health
										// is used during levels

	AInventory *InvFirst;				// first inventory item displayed on inventory bar
	AInventory *InvSel;					// selected inventory item
	int			inventorytics;
	BYTE		CurrentPlayerClass;		// class # for this player instance
	int			pieces;					// Fourth Weapon pieces
	bool		backpack;
	
	int			frags[MAXPLAYERS];		// kills of other players
	int			fragcount;				// [RH] Cumulative frags for this player
	int			lastkilltime;			// [RH] For multikills
	byte		multicount;
	byte		spreecount;				// [RH] Keep track of killing sprees

	AWeapon	   *ReadyWeapon;
	AWeapon	   *PendingWeapon;			// WP_NOCHANGE if not changing

	int			cheats;					// bit flags
	BITFIELD	Powers;					// powers
	short		refire;					// refired shots are less accurate
	short		inconsistant;
	int			killcount, itemcount, secretcount;		// for intermission
	int			damagecount, bonuscount;// for screen flashing
	int			hazardcount;			// for delayed Strife damage
	int			poisoncount;			// screen flash for poison damage
	AActor		*poisoner;				// NULL for non-player actors
	AActor		*attacker;				// who did damage (NULL for floors)
	int			extralight;				// so gun flashes light up areas
	int			fixedcolormap;			// can be set to REDCOLORMAP, etc.
	int			xviewshift;				// [RH] view shift (for earthquakes)
	pspdef_t	psprites[NUMPSPRITES];	// view sprites (gun, etc)
	int			morphTics;				// player is a chicken/pig if > 0
	AWeapon		*PremorphWeapon;		// ready weapon before morphing
	int			chickenPeck;			// chicken peck countdown
	int			jumpTics;				// delay the next jump for a moment

	int			respawn_time;			// [RH] delay respawning until this tic
	AActor		*camera;				// [RH] Whose eyes this player sees through

	int			air_finished;			// [RH] Time when you start drowning

	WORD		accuracy, stamina;		// [RH] Strife stats

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

	fixed_t		oldx;
	fixed_t		oldy;

	FPlayerSkin	*skin;		// [RH] Sprite override
	float		BlendR;		// [RH] Final blending values
	float		BlendG;
	float		BlendB;
	float		BlendA;

	FString		LogText;	// [RH] Log for Strife

	signed char crouching;
	signed char	crouchdir;
	fixed_t crouchfactor;
	fixed_t crouchoffset;
	fixed_t crouchviewdelta;

	fixed_t GetDeltaViewHeight() const
	{
		return (defaultviewheight + crouchviewdelta - viewheight) >> 3;
	}

	void Uncrouch()
	{
		crouchfactor = FRACUNIT;
		crouchoffset = 0;
		crouchdir = 0;
		crouching = 0;
		crouchviewdelta = 0;
	}
};

typedef player_s player_t;

// Bookkeeping on players - state.
extern player_s players[MAXPLAYERS];

inline FArchive &operator<< (FArchive &arc, player_s *&p)
{
	return arc.SerializePointer (players, (BYTE **)&p, sizeof(*players));
}

#define CROUCHSPEED (FRACUNIT/12)
#define MAX_DN_ANGLE	56		// Max looking down angle
#define MAX_UP_ANGLE	32		// Max looking up angle

#endif // __D_PLAYER_H__
