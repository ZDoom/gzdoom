//-----------------------------------------------------------------------------
//
// Copyright 1993-1996 id Software
// Copyright 1994-1996 Raven Software
// Copyright 1998-1998 Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
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


#ifndef __D_PLAYER_H__
#define __D_PLAYER_H__

// Finally, for odd reasons, the player input
// is buffered within the player data struct,
// as commands per game tick.
#include "d_protocol.h"
#include "doomstat.h"

#include "a_weapons.h"

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
		uint8_t RangeStart, RangeEnd;	// colors to remap
		uint8_t FirstColor, LastColor;	// colors to map to
	};

	FName Name;			// Name of this color

	int Lump;			// Lump to read the translation from, otherwise use next 2 fields
	uint8_t FirstColor, LastColor;		// Describes the range of colors to use for the translation

	uint8_t RepresentativeColor;		// A palette entry representative of this translation,
									// for map arrows and status bar backgrounds and such
	uint8_t NumExtraRanges;
	ExtraRange Extra[6];
};

typedef TArray<std::tuple<PClass*, FName, PalEntry>> PainFlashList;
typedef TArray<std::tuple<PClass*, int, FPlayerColorSet>> ColorSetList;

extern PainFlashList PainFlashes;
extern ColorSetList ColorSets;

FString GetPrintableDisplayName(PClassActor *cls);

class APlayerPawn : public AActor
{
	DECLARE_CLASS(APlayerPawn, AActor)
	HAS_OBJECT_POINTERS
public:
	
	virtual void Serialize(FSerializer &arc);

	virtual void PostBeginPlay() override;
	virtual void Tick() override;
	virtual void AddInventory (AInventory *item) override;
	virtual void RemoveInventory (AInventory *item) override;
	virtual bool UseInventory (AInventory *item) override;
	virtual void MarkPrecacheSounds () const override;
	virtual void BeginPlay () override;
	virtual void Die (AActor *source, AActor *inflictor, int dmgflags) override;
	virtual bool UpdateWaterLevel (bool splash) override;

	bool ResetAirSupply (bool playgasp = true);
	int GetMaxHealth(bool withupgrades = false) const;
	void ActivateMorphWeapon ();
	AWeapon *PickNewWeapon (PClassActor *ammotype);
	AWeapon *BestWeapon (PClassActor *ammotype);
	void GiveDeathmatchInventory ();
	void FilterCoopRespawnInventory (APlayerPawn *oldplayer);

	void SetupWeaponSlots ();
	void GiveDefaultInventory ();

	// These are virtual on the script side only.
	void PlayIdle();
	void PlayAttacking2 ();

	const char *GetSoundClass () const;

	enum EInvulState
	{
		INVUL_Start,
		INVUL_Active,
		INVUL_Stop,
		INVUL_GetAlpha
	};


	int			crouchsprite;
	int			MaxHealth;
	int			BonusHealth;

	int			MugShotMaxHealth;
	int			RunHealth;
	int			PlayerFlags;
	double		FullHeight;
	TObjPtr<AInventory*> InvFirst;		// first inventory item displayed on inventory bar
	TObjPtr<AInventory*> InvSel;			// selected inventory item

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

	// [SP] ViewBob Multiplier
	double		ViewBob;

	// Former class properties that were moved into the object to get rid of the meta class.
	FNameNoInit SoundClass;		// Sound class
	FNameNoInit Face;			// Doom status bar face (when used)
	FNameNoInit Portrait;
	FNameNoInit Slot[10];
	double HexenArmor[5];
	uint8_t ColorRangeStart;	// Skin color range
	uint8_t ColorRangeEnd;

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
	CF_EXTREMELYDEAD	= 1 << 22,		// [RH] Reliably let the status bar know about extreme deaths.
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

// The VM cannot deal with this as an invalid pointer because it performs a read barrier on every object pointer read.
// This doesn't have to point to a valid weapon, though, because WP_NOCHANGE is never dereferenced, but it must point to a valid object
// and the class descriptor just works fine for that.
extern AWeapon *WP_NOCHANGE;


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

	PClassActor *Type;
	uint32_t Flags;
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
	// Same but unfiltered.
	double GetAutoaim() const
	{
		return *static_cast<FFloatCVar *>(*CheckKey(NAME_Autoaim));
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
	uint32_t GetColor() const
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
	float GetWBobSpeed() const
	{
		return *static_cast<FFloatCVar *>(*CheckKey(NAME_WBobSpeed));
	}
	int GetPlayerClassNum() const
	{
		return *static_cast<FIntCVar *>(*CheckKey(NAME_PlayerClass));
	}
	bool GetClassicFlight() const
	{
		return *static_cast<FBoolCVar *>(*CheckKey(NAME_ClassicFlight));
	}
	PClassActor *GetPlayerClassType() const
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
	uint32_t ColorChanged(const char *colorname);
	uint32_t ColorChanged(uint32_t colorval);
	int ColorSetChanged(int setnum);
};

void ReadUserInfo(FSerializer &arc, userinfo_t &info, FString &skin);
void WriteUserInfo(FSerializer &arc, userinfo_t &info);

//
// Extended player object info: player_t
//
class player_t
{
public:
	player_t();
	~player_t();
	player_t &operator= (const player_t &p);

	void Serialize(FSerializer &arc);
	size_t FixPointers (const DObject *obj, DObject *replacement);
	size_t PropagateMark();

	void SetLogNumber (int num);
	void SetLogText (const char *text);
	void SendPitchLimits() const;

	APlayerPawn	*mo;
	uint8_t		playerstate;
	ticcmd_t	cmd;
	usercmd_t	original_cmd;
	uint32_t		original_oldbuttons;

	userinfo_t	userinfo;				// [RH] who is this?
	
	PClassActor *cls;				// class of associated PlayerPawn

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
	uint8_t		turnticks;


	bool		attackdown;
	bool		usedown;
	uint32_t		oldbuttons;
	int			health;					// only used between levels, mo->health
										// is used during levels

	int			inventorytics;
	uint8_t		CurrentPlayerClass;		// class # for this player instance

	int			frags[MAXPLAYERS];		// kills of other players
	int			fragcount;				// [RH] Cumulative frags for this player
	int			lastkilltime;			// [RH] For multikills
	uint8_t		multicount;
	uint8_t		spreecount;				// [RH] Keep track of killing sprees
	uint16_t		WeaponState;

	AWeapon	   *ReadyWeapon;
	AWeapon	   *PendingWeapon;			// WP_NOCHANGE if not changing
	TObjPtr<DPSprite*> psprites; // view sprites (gun, etc)

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
	TObjPtr<AActor*>		poisoner;		// NULL for non-player actors
	TObjPtr<AActor*>		attacker;		// who did damage (NULL for floors)
	int			extralight;				// so gun flashes light up areas
	short		fixedcolormap;			// can be set to REDCOLORMAP, etc.
	short		fixedlightlevel;
	int			morphTics;				// player is a chicken/pig if > 0
	PClassActor *MorphedPlayerClass;		// [MH] (for SBARINFO) class # for this player instance when morphed
	int			MorphStyle;				// which effects to apply for this player instance when morphed
	PClassActor *MorphExitFlash;		// flash to apply when demorphing (cache of value given to P_MorphPlayer)
	TObjPtr<AWeapon*>	PremorphWeapon;		// ready weapon before morphing
	int			chickenPeck;			// chicken peck countdown
	int			jumpTics;				// delay the next jump for a moment
	bool		onground;				// Identifies if this player is on the ground or other object

	int			respawn_time;			// [RH] delay respawning until this tic
	TObjPtr<AActor*>		camera;			// [RH] Whose eyes this player sees through

	int			air_finished;			// [RH] Time when you start drowning

	FName		LastDamageType;			// [RH] For damage-specific pain and death sounds

	TObjPtr<AActor*> MUSINFOactor;		// For MUSINFO purposes
	int8_t		MUSINFOtics;

	bool		settings_controller;	// Player can control game settings.
	int8_t		crouching;
	int8_t		crouchdir;

	//Added by MC:
	TObjPtr<DBot*> Bot;

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
	TObjPtr<AActor*> ConversationNPC, ConversationPC;
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
			viewheight = mo ? mo->ViewHeight : 0;
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

	bool GetPainFlash(FName type, PalEntry *color) const;

	// [Nash] set player FOV
	void SetFOV(float fov);
	bool HasWeaponsInSlot(int slot) const;
	bool Resurrect();
};

// Bookkeeping on players - state.
extern player_t players[MAXPLAYERS];

void P_CheckPlayerSprite(AActor *mo, int &spritenum, DVector2 &scale);
void EnumColorSets(PClassActor *pc, TArray<int> *out);
FPlayerColorSet *GetColorSet(PClassActor *pc, int setnum);

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

bool P_IsPlayerTotallyFrozen(const player_t *player);

#endif // __D_PLAYER_H__
