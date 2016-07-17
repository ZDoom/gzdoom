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

class player_t;

// Standard pre-defined skin colors
struct FPlayerColorSet
{
	struct ExtraRange
	{
		BYTE RangeStart, RangeEnd;	// colors to remap
		BYTE FirstColor, LastColor;	// colors to map to
	};

	FName Name;			// Name of this color

	int Lump;			// Lump to read the translation from, otherwise use next 2 fields
	BYTE FirstColor, LastColor;		// Describes the range of colors to use for the translation

	BYTE RepresentativeColor;		// A palette entry representative of this translation,
									// for map arrows and status bar backgrounds and such
	BYTE NumExtraRanges;
	ExtraRange Extra[6];
};

typedef TMap<int, FPlayerColorSet> FPlayerColorSetMap;
typedef TMap<FName, PalEntry> PainFlashList;

class PClassPlayerPawn : public PClassActor
{
	DECLARE_CLASS(PClassPlayerPawn, PClassActor);
protected:
public:
	PClassPlayerPawn();
	virtual void DeriveData(PClass *newclass);
	void EnumColorSets(TArray<int> *out);
	FPlayerColorSet *GetColorSet(int setnum) { return ColorSets.CheckKey(setnum); }
	void SetPainFlash(FName type, PalEntry color);
	bool GetPainFlash(FName type, PalEntry *color) const;
	virtual void ReplaceClassRef(PClass *oldclass, PClass *newclass);

	FString DisplayName;	// Display name (used in menus, etc.)
	FString SoundClass;		// Sound class
	FString Face;			// Doom status bar face (when used)
	FString Portrait;
	FString Slot[10];
	FName InvulMode;
	FName HealingRadiusType;
	double HexenArmor[5];
	BYTE ColorRangeStart;	// Skin color range
	BYTE ColorRangeEnd;
	FPlayerColorSetMap ColorSets;
	PainFlashList PainFlashes;
};
FString GetPrintableDisplayName(PClassPlayerPawn *cls);

class APlayerPawn : public AActor
{
	DECLARE_CLASS_WITH_META(APlayerPawn, AActor, PClassPlayerPawn)
	HAS_OBJECT_POINTERS
public:
	virtual void Serialize (FArchive &arc);

	virtual void PostBeginPlay();
	virtual void Tick();
	virtual void AddInventory (AInventory *item);
	virtual void RemoveInventory (AInventory *item);
	virtual bool UseInventory (AInventory *item);
	virtual void MarkPrecacheSounds () const;

	virtual void PlayIdle ();
	virtual void PlayRunning ();
	virtual void ThrowPoisonBag ();
	virtual void TweakSpeeds (double &forwardmove, double &sidemove);
	virtual void MorphPlayerThink ();
	virtual void ActivateMorphWeapon ();
	AWeapon *PickNewWeapon (PClassAmmo *ammotype);
	AWeapon *BestWeapon (PClassAmmo *ammotype);
	void CheckWeaponSwitch(PClassAmmo *ammotype);
	virtual void GiveDeathmatchInventory ();
	virtual void FilterCoopRespawnInventory (APlayerPawn *oldplayer);

	void SetupWeaponSlots ();
	void GiveDefaultInventory ();
	void PlayAttacking ();
	void PlayAttacking2 ();
	const char *GetSoundClass () const;

	enum EInvulState
	{
		INVUL_Start,
		INVUL_Active,
		INVUL_Stop,
		INVUL_GetAlpha
	};

	void BeginPlay ();
	void Die (AActor *source, AActor *inflictor, int dmgflags);

	int			crouchsprite;
	int			MaxHealth;
	int			MugShotMaxHealth;
	int			RunHealth;
	int			PlayerFlags;
	TObjPtr<AInventory> InvFirst;		// first inventory item displayed on inventory bar
	TObjPtr<AInventory> InvSel;			// selected inventory item

	// [GRB] Player class properties
	double		JumpZ;
	double		GruntSpeed;
	double		FallingScreamMinSpeed, FallingScreamMaxSpeed;
	double		ViewHeight;
	double		ForwardMove1, ForwardMove2;
	double		SideMove1, SideMove2;
	FTextureID	ScoreIcon;
	int			SpawnMask;
	FNameNoInit	MorphWeapon;
	double		AttackZOffset;			// attack height, relative to player center
	double		UseRange;				// [NS] Distance at which player can +use
	double		AirCapacity;			// Multiplier for air supply underwater.
	PClassActor *FlechetteType;


	// [CW] Fades for when you are being damaged.
	PalEntry DamageFade;

	bool UpdateWaterLevel (bool splash);
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
	PPF_CANSUPERMORPH = 2,		// Being remorphed into this class can give you a Tome of Power
	PPF_CROUCHABLEMORPH = 4,	// This morphed player can crouch
};

//
// Player states.
//
typedef enum
{
	PST_LIVE,	// Playing or camping.
	PST_DEAD,	// Dead on the ground, view follows killer.
	PST_REBORN,	// Ready to restart/respawn???
	PST_ENTER,	// [BC] Entered the game
	PST_GONE	// Player has left the game
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
	CF_INTERPVIEW		= 1 << 14,		// [RH] view was changed outside of input, so interpolate one frame
	CF_DRAIN			= 1 << 16,		// Player owns a drain powerup
	CF_HIGHJUMP			= 1 << 18,		// more Skulltag flags. Implementation not guaranteed though. ;)
	CF_REFLECTION		= 1 << 19,
	CF_PROSPERITY		= 1 << 20,
	CF_DOUBLEFIRINGSPEED= 1 << 21,		// Player owns a double firing speed artifact
	CF_EXTREMELYDEAD	= 1 << 22,		// [RH] Reliably let the status bar know about extreme deaths.
	CF_INFINITEAMMO		= 1 << 23,		// Player owns an infinite ammo artifact
	CF_BUDDHA2			= 1 << 24,		// [MC] Absolute buddha. No voodoo can kill it either.
	CF_GODMODE2			= 1 << 25,		// [MC] Absolute godmode. No voodoo can kill it either.
	CF_BUDDHA			= 1 << 27,		// [SP] Buddha mode - take damage, but don't die
	CF_NOCLIP2			= 1 << 30,		// [RH] More Quake-like noclip
} cheat_t;

enum
{
	WF_WEAPONREADY		= 1 << 0,		// [RH] Weapon is in the ready state and can fire its primary attack
	WF_WEAPONBOBBING	= 1 << 1,		// [HW] Bob weapon while the player is moving
	WF_WEAPONREADYALT	= 1 << 2,		// Weapon can fire its secondary attack
	WF_WEAPONSWITCHOK	= 1 << 3,		// It is okay to switch away from this weapon
	WF_DISABLESWITCH	= 1 << 4,		// Disable weapon switching completely
	WF_WEAPONRELOADOK	= 1 << 5,		// [XA] Okay to reload this weapon.
	WF_WEAPONZOOMOK		= 1 << 6,		// [XA] Okay to use weapon zoom function.
	WF_REFIRESWITCHOK	= 1 << 7,		// Mirror WF_WEAPONSWITCHOK for A_ReFire
	WF_USER1OK			= 1 << 8,		// [MC] Allow pushing of custom state buttons 1-4
	WF_USER2OK			= 1 << 9,
	WF_USER3OK			= 1 << 10,
	WF_USER4OK			= 1 << 11,
};

#define WPIECE1		1
#define WPIECE2		2
#define WPIECE3		4

#define WP_NOCHANGE ((AWeapon*)~0)


#define MAXPLAYERNAME	15

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

	PClassPlayerPawn *Type;
	DWORD Flags;
	TArray<int> Skins;
};

extern TArray<FPlayerClass> PlayerClasses;

// User info (per-player copies of each CVAR_USERINFO cvar)
enum
{
	GENDER_MALE,
	GENDER_FEMALE,
	GENDER_NEUTER
};

struct userinfo_t : TMap<FName,FBaseCVar *>
{
	~userinfo_t();

	double GetAimDist() const
	{
		if (dmflags2 & DF2_NOAUTOAIM)
		{
			return 0;
		}

		float aim = *static_cast<FFloatCVar *>(*CheckKey(NAME_Autoaim));
		if (aim > 35 || aim < 0)
		{
			return 35.;
		}
		else
		{
			return aim;
		}
	}
	const char *GetName() const
	{
		return *static_cast<FStringCVar *>(*CheckKey(NAME_Name));
	}
	int GetTeam() const
	{
		return *static_cast<FIntCVar *>(*CheckKey(NAME_Team));
	}
	int GetColorSet() const
	{
		return *static_cast<FIntCVar *>(*CheckKey(NAME_ColorSet));
	}
	uint32 GetColor() const
	{
		return *static_cast<FColorCVar *>(*CheckKey(NAME_Color));
	}
	bool GetNeverSwitch() const
	{
		return *static_cast<FBoolCVar *>(*CheckKey(NAME_NeverSwitchOnPickup));
	}
	double GetMoveBob() const
	{
		return *static_cast<FFloatCVar *>(*CheckKey(NAME_MoveBob));
	}
	double GetStillBob() const
	{
		return *static_cast<FFloatCVar *>(*CheckKey(NAME_StillBob));
	}
	int GetPlayerClassNum() const
	{
		return *static_cast<FIntCVar *>(*CheckKey(NAME_PlayerClass));
	}
	PClassPlayerPawn *GetPlayerClassType() const
	{
		return PlayerClasses[GetPlayerClassNum()].Type;
	}
	int GetSkin() const
	{
		return *static_cast<FIntCVar *>(*CheckKey(NAME_Skin));
	}
	int GetGender() const
	{
		return *static_cast<FIntCVar *>(*CheckKey(NAME_Gender));
	}
	bool GetNoAutostartMap() const
	{
		return *static_cast<FBoolCVar *>(*CheckKey(NAME_Wi_NoAutostartMap));
	}

	void Reset();
	int TeamChanged(int team);
	int SkinChanged(const char *skinname, int playerclass);
	int SkinNumChanged(int skinnum);
	int GenderChanged(const char *gendername);
	int PlayerClassChanged(const char *classname);
	int PlayerClassNumChanged(int classnum);
	uint32 ColorChanged(const char *colorname);
	uint32 ColorChanged(uint32 colorval);
	int ColorSetChanged(int setnum);
};

void ReadUserInfo(FArchive &arc, userinfo_t &info, FString &skin);
void WriteUserInfo(FArchive &arc, userinfo_t &info);

//
// Extended player object info: player_t
//
class player_t
{
public:
	player_t();
	~player_t();
	player_t &operator= (const player_t &p);

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
	
	PClassPlayerPawn *cls;				// class of associated PlayerPawn

	float		DesiredFOV;				// desired field of vision
	float		FOV;					// current field of vision
	double		viewz;					// focal origin above r.z
	double		viewheight;				// base height above floor for viewz
	double		deltaviewheight;		// squat speed.
	double		bob;					// bounded/scaled total velocity

	// killough 10/98: used for realistic bobbing (i.e. not simply overall speed)
	// mo->velx and mo->vely represent true velocity experienced by player.
	// This only represents the thrust that the player applies himself.
	// This avoids anomalies with such things as Boom ice and conveyors.
	DVector2 Vel;

	bool		centering;
	BYTE		turnticks;


	bool		attackdown;
	bool		usedown;
	DWORD		oldbuttons;
	int			health;					// only used between levels, mo->health
										// is used during levels

	int			inventorytics;
	BYTE		CurrentPlayerClass;		// class # for this player instance

	int			frags[MAXPLAYERS];		// kills of other players
	int			fragcount;				// [RH] Cumulative frags for this player
	int			lastkilltime;			// [RH] For multikills
	BYTE		multicount;
	BYTE		spreecount;				// [RH] Keep track of killing sprees
	WORD		WeaponState;

	AWeapon	   *ReadyWeapon;
	AWeapon	   *PendingWeapon;			// WP_NOCHANGE if not changing
	TObjPtr<DPSprite> psprites; // view sprites (gun, etc)

	int			cheats;					// bit flags
	int			timefreezer;			// Player has an active time freezer
	short		refire;					// refired shots are less accurate
	short		inconsistant;
	bool		waiting;
	int			killcount, itemcount, secretcount;		// for intermission
	int			damagecount, bonuscount;// for screen flashing
	int			hazardcount;			// for delayed Strife damage
	int			hazardinterval;			// Frequency of damage infliction
	FName		hazardtype;				// Damage type of last hazardous damage encounter.
	int			poisoncount;			// screen flash for poison damage
	FName		poisontype;				// type of poison damage to apply
	FName		poisonpaintype;			// type of Pain state to enter for poison damage
	TObjPtr<AActor>		poisoner;		// NULL for non-player actors
	TObjPtr<AActor>		attacker;		// who did damage (NULL for floors)
	int			extralight;				// so gun flashes light up areas
	short		fixedcolormap;			// can be set to REDCOLORMAP, etc.
	short		fixedlightlevel;
	int			morphTics;				// player is a chicken/pig if > 0
	PClassPlayerPawn *MorphedPlayerClass;		// [MH] (for SBARINFO) class # for this player instance when morphed
	int			MorphStyle;				// which effects to apply for this player instance when morphed
	PClassActor *MorphExitFlash;		// flash to apply when demorphing (cache of value given to P_MorphPlayer)
	TObjPtr<AWeapon>	PremorphWeapon;		// ready weapon before morphing
	int			chickenPeck;			// chicken peck countdown
	int			jumpTics;				// delay the next jump for a moment
	bool		onground;				// Identifies if this player is on the ground or other object

	int			respawn_time;			// [RH] delay respawning until this tic
	TObjPtr<AActor>		camera;			// [RH] Whose eyes this player sees through

	int			air_finished;			// [RH] Time when you start drowning

	FName		LastDamageType;			// [RH] For damage-specific pain and death sounds

	TObjPtr<AActor> MUSINFOactor;		// For MUSINFO purposes
	SBYTE		MUSINFOtics;

	bool		settings_controller;	// Player can control game settings.
	SBYTE		crouching;
	SBYTE		crouchdir;

	//Added by MC:
	TObjPtr<DBot> Bot;

	float		BlendR;		// [RH] Final blending values
	float		BlendG;
	float		BlendB;
	float		BlendA;

	FString		LogText;	// [RH] Log for Strife

	DAngle			MinPitch;	// Viewpitch limits (negative is up, positive is down)
	DAngle			MaxPitch;

	double crouchfactor;
	double crouchoffset;
	double crouchviewdelta;

	FWeaponSlots weapons;

	// [CW] I moved these here for multiplayer conversation support.
	TObjPtr<AActor> ConversationNPC, ConversationPC;
	DAngle ConversationNPCAngle;
	bool ConversationFaceTalker;

	double GetDeltaViewHeight() const
	{
		return (mo->ViewHeight + crouchviewdelta - viewheight) / 8;
	}

	void Uncrouch()
	{
		if (crouchfactor != 1)
		{
			crouchfactor = 1;
			crouchoffset = 0;
			crouchdir = 0;
			crouching = 0;
			crouchviewdelta = 0;
			viewheight = mo->ViewHeight;
		}
	}
	
	bool CanCrouch() const
	{
		return morphTics == 0 || mo->PlayerFlags & PPF_CROUCHABLEMORPH;
	}

	int GetSpawnClass();

	// PSprite layers
	void TickPSprites();
	void DestroyPSprites();
	DPSprite *FindPSprite(int layer);
	// Used ONLY for compatibility with the old hardcoded layers.
	// Make sure that a state is properly set after calling this unless
	// you are 100% sure the context already implies the layer exists.
	DPSprite *GetPSprite(PSPLayers layer);
};

// Bookkeeping on players - state.
extern player_t players[MAXPLAYERS];

FArchive &operator<< (FArchive &arc, player_t *&p);

void P_CheckPlayerSprite(AActor *mo, int &spritenum, DVector2 &scale);

inline void AActor::SetFriendPlayer(player_t *player)
{
	if (player == NULL)
	{
		FriendPlayer = 0;
	}
	else
	{
		FriendPlayer = int(player - players) + 1;
	}
}

inline bool AActor::IsNoClip2() const
{
	if (player != NULL && player->mo == this)
	{
		return (player->cheats & CF_NOCLIP2) != 0;
	}
	return false;
}

#define CROUCHSPEED (1./12)

bool P_IsPlayerTotallyFrozen(const player_t *player);

#endif // __D_PLAYER_H__
