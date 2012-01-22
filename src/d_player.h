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
#include "doomstat.h"

#include "a_artifacts.h"

// The player data structure depends on a number
// of other structs: items (internal inventory),
// animation states (closely tied to the sprites
// used to represent them, unfortunately).
#include "p_pspr.h"

// In addition, the player is just a special
// case of the generic moving object/actor.
#include "actor.h"

//Added by MC:
#include "b_bot.h"

enum
{
	APMETA_BASE = 0x95000,

	APMETA_DisplayName,		// display name (used in menus etc.)
	APMETA_SoundClass,		// sound class
	APMETA_Face,			// doom status bar face (when used)
	APMETA_ColorRange,		// skin color range
	APMETA_InvulMode,
	APMETA_HealingRadius,
	APMETA_Portrait,
	APMETA_Hexenarmor0,
	APMETA_Hexenarmor1,
	APMETA_Hexenarmor2,
	APMETA_Hexenarmor3,
	APMETA_Hexenarmor4,
	APMETA_Slot0,
	APMETA_Slot1,
	APMETA_Slot2,
	APMETA_Slot3,
	APMETA_Slot4,
	APMETA_Slot5,
	APMETA_Slot6,
	APMETA_Slot7,
	APMETA_Slot8,
	APMETA_Slot9,
};

FPlayerColorSet *P_GetPlayerColorSet(FName classname, int setnum);
void P_EnumPlayerColorSets(FName classname, TArray<int> *out);
const char *GetPrintableDisplayName(const PClass *cls);

class player_t;

class APlayerPawn : public AActor
{
	DECLARE_CLASS (APlayerPawn, AActor)
	HAS_OBJECT_POINTERS
public:
	virtual void Serialize (FArchive &arc);

	virtual void PostBeginPlay();
	virtual void Tick();
	virtual void AddInventory (AInventory *item);
	virtual void RemoveInventory (AInventory *item);
	virtual bool UseInventory (AInventory *item);

	virtual void PlayIdle ();
	virtual void PlayRunning ();
	virtual void ThrowPoisonBag ();
	virtual void TweakSpeeds (int &forwardmove, int &sidemove);
	virtual void MorphPlayerThink ();
	virtual void ActivateMorphWeapon ();
	AWeapon *PickNewWeapon (const PClass *ammotype);
	AWeapon *BestWeapon (const PClass *ammotype);
	void CheckWeaponSwitch(const PClass *ammotype);
	virtual void GiveDeathmatchInventory ();
	virtual void FilterCoopRespawnInventory (APlayerPawn *oldplayer);

	void SetupWeaponSlots ();
	void GiveDefaultInventory ();
	void PlayAttacking ();
	void PlayAttacking2 ();
	const char *GetSoundClass ();

	enum EInvulState
	{
		INVUL_Start,
		INVUL_Active,
		INVUL_Stop,
		INVUL_GetAlpha
	};

	void BeginPlay ();
	void Die (AActor *source, AActor *inflictor);

	int			crouchsprite;
	int			MaxHealth;
	int			MugShotMaxHealth;
	int			RunHealth;
	int			PlayerFlags;
	TObjPtr<AInventory> InvFirst;		// first inventory item displayed on inventory bar
	TObjPtr<AInventory> InvSel;			// selected inventory item

	// [GRB] Player class properties
	fixed_t		JumpZ;
	fixed_t		ViewHeight;
	fixed_t		ForwardMove1, ForwardMove2;
	fixed_t		SideMove1, SideMove2;
	FTextureID	ScoreIcon;
	int			SpawnMask;
	FNameNoInit	MorphWeapon;
	fixed_t		AttackZOffset;			// attack height, relative to player center
	const PClass *FlechetteType;

	// [CW] Fades for when you are being damaged.
	PalEntry DamageFade;

	bool UpdateWaterLevel (fixed_t oldz, bool splash);
	bool ResetAirSupply (bool playgasp = true);

	int GetMaxHealth() const;
};

class APlayerChunk : public APlayerPawn
{
	DECLARE_CLASS (APlayerChunk, APlayerPawn)
};

//
// PlayerPawn flags
//
enum
{
	PPF_NOTHRUSTWHENINVUL = 1,	// Attacks do not thrust the player if they are invulnerable.
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
	CF_NOCLIP			= 1 << 0,		// No clipping, walk through barriers.
	CF_GODMODE			= 1 << 1,		// No damage, no health loss.
	CF_NOVELOCITY		= 1 << 2,		// Not really a cheat, just a debug aid.
	CF_NOTARGET			= 1 << 3,		// [RH] Monsters don't target
	CF_FLY				= 1 << 4,		// [RH] Flying player
	CF_CHASECAM			= 1 << 5,		// [RH] Put camera behind player
	CF_FROZEN			= 1 << 6,		// [RH] Don't let the player move
	CF_REVERTPLEASE		= 1 << 7,		// [RH] Stick camera in player's head if (s)he moves
	CF_STEPLEFT			= 1 << 9,		// [RH] Play left footstep sound next time
	CF_FRIGHTENING		= 1 << 10,		// [RH] Scare monsters away
	CF_INSTANTWEAPSWITCH= 1 << 11,		// [RH] Switch weapons instantly
	CF_TOTALLYFROZEN	= 1 << 12,		// [RH] All players can do is press +use
	CF_PREDICTING		= 1 << 13,		// [RH] Player movement is being predicted
	CF_WEAPONREADY		= 1 << 14,		// [RH] Weapon is in the ready state and can fire its primary attack
	CF_TIMEFREEZE		= 1 << 15,		// Player has an active time freezer
	CF_DRAIN			= 1 << 16,		// Player owns a drain powerup
	CF_REGENERATION		= 1 << 17,		// Player owns a regeneration artifact
	CF_HIGHJUMP			= 1 << 18,		// more Skulltag flags. Implementation not guaranteed though. ;)
	CF_REFLECTION		= 1 << 19,
	CF_PROSPERITY		= 1 << 20,
	CF_DOUBLEFIRINGSPEED= 1 << 21,		// Player owns a double firing speed artifact
	CF_EXTREMELYDEAD	= 1 << 22,		// [RH] Reliably let the status bar know about extreme deaths.
	CF_INFINITEAMMO		= 1 << 23,		// Player owns an infinite ammo artifact
	CF_WEAPONBOBBING	= 1 << 24,		// [HW] Bob weapon while the player is moving
	CF_WEAPONREADYALT	= 1 << 25,		// Weapon can fire its secondary attack
	CF_WEAPONSWITCHOK	= 1 << 26,		// It is okay to switch away from this weapon
	CF_BUDDHA			= 1 << 27,		// [SP] Buddha mode - take damage, but don't die
} cheat_t;

#define WPIECE1		1
#define WPIECE2		2
#define WPIECE3		4

#define WP_NOCHANGE ((AWeapon*)~0)


#define MAXPLAYERNAME	15

enum
{
	GENDER_MALE,
	GENDER_FEMALE,
	GENDER_NEUTER
};

struct userinfo_t
{
	char		netname[MAXPLAYERNAME+1];
	BYTE		team;
	int			aimdist;
	int			color;
	int			colorset;
	int			skin;
	int			gender;
	bool		neverswitch;
	fixed_t		MoveBob, StillBob;
	int			PlayerClass;

	int GetAimDist() const { return (dmflags2 & DF2_NOAUTOAIM)? 0 : aimdist; }
};

FArchive &operator<< (FArchive &arc, userinfo_t &info);


//
// Extended player object info: player_t
//
class player_t
{
public:
	player_t();

	void Serialize (FArchive &arc);
	size_t FixPointers (const DObject *obj, DObject *replacement);
	size_t PropagateMark();

	void SetLogNumber (int num);
	void SetLogText (const char *text);
	void SendPitchLimits() const;

	APlayerPawn	*mo;
	BYTE		playerstate;
	ticcmd_t	cmd;
	usercmd_t	original_cmd;
	DWORD		original_oldbuttons;

	userinfo_t	userinfo;				// [RH] who is this?
	
	const PClass *cls;					// class of associated PlayerPawn

	float		DesiredFOV;				// desired field of vision
	float		FOV;					// current field of vision
	fixed_t		viewz;					// focal origin above r.z
	fixed_t		viewheight;				// base height above floor for viewz
	fixed_t		deltaviewheight;		// squat speed.
	fixed_t		bob;					// bounded/scaled total velocity

	// killough 10/98: used for realistic bobbing (i.e. not simply overall speed)
	// mo->velx and mo->vely represent true velocity experienced by player.
	// This only represents the thrust that the player applies himself.
	// This avoids anomalies with such things as Boom ice and conveyors.
	fixed_t		velx, vely;				// killough 10/98

	bool		centering;
	BYTE		turnticks;


	bool		attackdown;
	bool		usedown;
	DWORD		oldbuttons;
	int			health;					// only used between levels, mo->health
										// is used during levels

	int			inventorytics;
	BYTE		CurrentPlayerClass;		// class # for this player instance
	bool		backpack;
	
	int			frags[MAXPLAYERS];		// kills of other players
	int			fragcount;				// [RH] Cumulative frags for this player
	int			lastkilltime;			// [RH] For multikills
	BYTE		multicount;
	BYTE		spreecount;				// [RH] Keep track of killing sprees

	AWeapon	   *ReadyWeapon;
	AWeapon	   *PendingWeapon;			// WP_NOCHANGE if not changing

	int			cheats;					// bit flags
	short		refire;					// refired shots are less accurate
	short		inconsistant;
	int			killcount, itemcount, secretcount;		// for intermission
	int			damagecount, bonuscount;// for screen flashing
	int			hazardcount;			// for delayed Strife damage
	int			poisoncount;			// screen flash for poison damage
	TObjPtr<AActor>		poisoner;		// NULL for non-player actors
	TObjPtr<AActor>		attacker;		// who did damage (NULL for floors)
	int			extralight;				// so gun flashes light up areas
	short		fixedcolormap;			// can be set to REDCOLORMAP, etc.
	short		fixedlightlevel;
	pspdef_t	psprites[NUMPSPRITES];	// view sprites (gun, etc)
	int			morphTics;				// player is a chicken/pig if > 0
	const PClass *MorphedPlayerClass;		// [MH] (for SBARINFO) class # for this player instance when morphed
	int			MorphStyle;				// which effects to apply for this player instance when morphed
	const PClass *MorphExitFlash;		// flash to apply when demorphing (cache of value given to P_MorphPlayer)
	TObjPtr<AWeapon>	PremorphWeapon;		// ready weapon before morphing
	int			chickenPeck;			// chicken peck countdown
	int			jumpTics;				// delay the next jump for a moment

	int			respawn_time;			// [RH] delay respawning until this tic
	TObjPtr<AActor>		camera;			// [RH] Whose eyes this player sees through

	int			air_finished;			// [RH] Time when you start drowning

	WORD		accuracy, stamina;		// [RH] Strife stats

	FName		LastDamageType;			// [RH] For damage-specific pain and death sounds

	//Added by MC:
	angle_t		savedyaw;
	int			savedpitch;

	angle_t		angle;		// The wanted angle that the bot try to get every tic.
							//  (used to get a smoth view movement)
	TObjPtr<AActor>		dest;		// Move Destination.
	TObjPtr<AActor>		prev;		// Previous move destination.


	TObjPtr<AActor>		enemy;		// The dead meat.
	TObjPtr<AActor>		missile;	// A threatening missile that needs to be avoided.
	TObjPtr<AActor>		mate;		// Friend (used for grouping in teamplay or coop).
	TObjPtr<AActor>		last_mate;	// If bots mate disappeared (not if died) that mate is
							// pointed to by this. Allows bot to roam to it if
							// necessary.

	bool		settings_controller;	// Player can control game settings.

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

	float		BlendR;		// [RH] Final blending values
	float		BlendG;
	float		BlendB;
	float		BlendA;

	FString		LogText;	// [RH] Log for Strife

	int			MinPitch;	// Viewpitch limits (negative is up, positive is down)
	int			MaxPitch;

	SBYTE	crouching;
	SBYTE	crouchdir;
	fixed_t crouchfactor;
	fixed_t crouchoffset;
	fixed_t crouchviewdelta;

	FWeaponSlots weapons;

	// [CW] I moved these here for multiplayer conversation support.
	TObjPtr<AActor> ConversationNPC, ConversationPC;
	angle_t ConversationNPCAngle;
	bool ConversationFaceTalker;

	fixed_t GetDeltaViewHeight() const
	{
		return (mo->ViewHeight + crouchviewdelta - viewheight) >> 3;
	}

	void Uncrouch()
	{
		crouchfactor = FRACUNIT;
		crouchoffset = 0;
		crouchdir = 0;
		crouching = 0;
		crouchviewdelta = 0;
	}

	int GetSpawnClass();
};

// Bookkeeping on players - state.
extern player_t players[MAXPLAYERS];

FArchive &operator<< (FArchive &arc, player_t *&p);

void P_CheckPlayerSprites();


#define CROUCHSPEED (FRACUNIT/12)

bool P_IsPlayerTotallyFrozen(const player_t *player);

// [GRB] Custom player classes
enum
{
	PCF_NOMENU			= 1,	// Hide in new game menu
};

class FPlayerClass
{
public:
	FPlayerClass ();
	FPlayerClass (const FPlayerClass &other);
	~FPlayerClass ();

	bool CheckSkin (int skin);

	const PClass *Type;
	DWORD Flags;
	TArray<int> Skins;
};

extern TArray<FPlayerClass> PlayerClasses;

#endif // __D_PLAYER_H__
