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
//		Map Objects, MObj, definition and handling.
//
//-----------------------------------------------------------------------------


#ifndef __P_MOBJ_H__
#define __P_MOBJ_H__

// Basics.
#include "tables.h"
#include "m_fixed.h"

// We need the thinker_t stuff.
#include "dthinker.h"

// We need the WAD data structure for Map things,
// from the THINGS lump.
#include "doomdata.h"

// States are tied to finite states are
//	tied to animation frames.
// Needs precompiled tables/data structures.
#include "info.h"

#include "doomdef.h"

// killough 11/98:
// For torque simulation:

#define OVERDRIVE 6
#define MAXGEAR (OVERDRIVE+16)


//
// NOTES: AActor
//
// Actors are used to tell the refresh where to draw an image,
// tell the world simulation when objects are contacted,
// and tell the sound driver how to position a sound.
//
// The refresh uses the next and prev links to follow
// lists of things in sectors as they are being drawn.
// The sprite, frame, and angle elements determine which patch_t
// is used to draw the sprite if it is visible.
// The sprite and frame values are almost always set
// from state_t structures.
// The statescr.exe utility generates the states.h and states.c
// files that contain the sprite/frame numbers from the
// statescr.txt source file.
// The xyz origin point represents a point at the bottom middle
// of the sprite (between the feet of a biped).
// This is the default origin position for patch_ts grabbed
// with lumpy.exe.
// A walking creature will have its z equal to the floor
// it is standing on.
//
// The sound code uses the x,y, and subsector fields
// to do stereo positioning of any sound effited by the AActor.
//
// The play simulation uses the blocklinks, x,y,z, radius, height
// to determine when AActors are touching each other,
// touching lines in the map, or hit by trace lines (gunshots,
// lines of sight, etc).
// The AActor->flags element has various bit flags
// used by the simulation.
//
// Every actor is linked into a single sector
// based on its origin coordinates.
// The subsector_t is found with R_PointInSubsector(x,y),
// and the sector_t can be found with subsector->sector.
// The sector links are only used by the rendering code,
// the play simulation does not care about them at all.
//
// Any actor that needs to be acted upon by something else
// in the play world (block movement, be shot, etc) will also
// need to be linked into the blockmap.
// If the thing has the MF_NOBLOCK flag set, it will not use
// the block links. It can still interact with other things,
// but only as the instigator (missiles will run into other
// things, but nothing can run into a missile).
// Each block in the grid is 128*128 units, and knows about
// every line_t that it contains a piece of, and every
// interactable actor that has its origin contained.  
//
// A valid actor is an actor that has the proper subsector_t
// filled in for its xy coordinates and is linked into the
// sector from which the subsector was made, or has the
// MF_NOSECTOR flag set (the subsector_t needs to be valid
// even if MF_NOSECTOR is set), and is linked into a blockmap
// block or has the MF_NOBLOCKMAP flag set.
// Links should only be modified by the P_[Un]SetThingPosition()
// functions.
// Do not change the MF_NO* flags while a thing is valid.
//
// Any questions?
//

enum
{
// --- mobj.flags ---

	MF_SPECIAL		= 0x00000001,	// call P_SpecialThing when touched
	MF_SOLID		= 0x00000002,
	MF_SHOOTABLE	= 0x00000004,
	MF_NOSECTOR		= 0x00000008,	// don't use the sector links
									// (invisible but touchable)
	MF_NOBLOCKMAP	= 0x00000010,	// don't use the blocklinks
									// (inert but displayable)
	MF_AMBUSH		= 0x00000020,	// not activated by sound; deaf monster
	MF_JUSTHIT		= 0x00000040,	// try to attack right back
	MF_JUSTATTACKED	= 0x00000080,	// take at least one step before attacking
	MF_SPAWNCEILING	= 0x00000100,	// hang from ceiling instead of floor
	MF_NOGRAVITY	= 0x00000200,	// don't apply gravity every tic

// movement flags
	MF_DROPOFF		= 0x00000400,	// allow jumps from high places
	MF_PICKUP		= 0x00000800,	// for players to pick up items
	MF_NOCLIP		= 0x00001000,	// player cheat
	MF_SLIDE		= 0x00002000,	// keep info about sliding along walls
	MF_FLOAT		= 0x00004000,	// allow moves to any height, no gravity
	MF_TELEPORT		= 0x00008000,	// don't cross lines or look at heights
	MF_MISSILE		= 0x00010000,	// don't hit same species, explode on block

	MF_DROPPED		= 0x00020000,	// dropped by a demon, not level spawned
	MF_SHADOW		= 0x00040000,	// actor is hard for monsters to see
	MF_NOBLOOD		= 0x00080000,	// don't bleed when shot (use puff)
	MF_CORPSE		= 0x00100000,	// don't stop moving halfway off a step
	MF_INFLOAT		= 0x00200000,	// floating to a height for a move, don't
									// auto float to target's height

	MF_COUNTKILL	= 0x00400000,	// count towards intermission kill total
	MF_COUNTITEM	= 0x00800000,	// count towards intermission item total

	MF_SKULLFLY		= 0x01000000,	// skull in flight
	MF_NOTDMATCH	= 0x02000000,	// don't spawn in death match (key cards)

	MF_TRANSLATION	= 0x0c000000,	// if 0x4 0x8 or 0xc, use a translation
	MF_TRANSSHIFT	= 26,			// table for player colormaps

	MF_UNMORPHED	= 0x10000000,	// [RH] Actor is the unmorphed version of something else
	MF_FALLING		= 0x20000000,
	MF_STEALTH		= 0x40000000,	// [RH] Andy Baker's stealth monsters
	MF_ICECORPSE	= 0x80000000,	// a frozen corpse (for blasting) [RH] was 0x800000

// --- mobj.flags2 ---

	MF2_LOGRAV			= 0x00000001,	// alternate gravity setting
	MF2_WINDTHRUST		= 0x00000002,	// gets pushed around by the wind
										// specials
	MF2_FLOORBOUNCE		= 0x00000004,	// bounces off the floor
	MF2_BLASTED			= 0x00000008,	// missile will pass through ghosts
	MF2_FLY				= 0x00000010,	// fly mode is active
	MF2_FLOORCLIP		= 0x00000020,	// if feet are allowed to be clipped
	MF2_SPAWNFLOAT		= 0x00000040,	// spawn random float z
	MF2_NOTELEPORT		= 0x00000080,	// does not teleport
	MF2_RIP				= 0x00000100,	// missile rips through solid
										// targets
	MF2_PUSHABLE		= 0x00000200,	// can be pushed by other moving
										// mobjs
	MF2_SLIDE			= 0x00000400,	// slides against walls
	MF2_ONMOBJ			= 0x00000800,	// mobj is resting on top of another
										// mobj
	MF2_PASSMOBJ		= 0x00001000,	// Enable z block checking.  If on,
										// this flag will allow the mobj to
										// pass over/under other mobjs.
	MF2_CANNOTPUSH		= 0x00002000,	// cannot push other pushable mobjs
	MF2_THRUGHOST		= 0x00004000,	// missile will pass through ghosts [RH] was 8
	MF2_BOSS			= 0x00008000,	// mobj is a major boss
	MF2_FIREDAMAGE		= 0x00010000,	// does fire damage
	MF2_NODMGTHRUST		= 0x00020000,	// does not thrust target when
										// damaging
	MF2_TELESTOMP		= 0x00040000,	// mobj can stomp another
	MF2_FLOATBOB		= 0x00080000,	// use float bobbing z movement
	MF2_IMPACT			= 0x00200000, 	// an MF_MISSILE mobj can activate SPAC_IMPACT
	MF2_PUSHWALL		= 0x00400000, 	// mobj can push walls
	MF2_MCROSS			= 0x00800000,	// can activate monster cross lines
	MF2_PCROSS			= 0x01000000,	// can activate projectile cross lines
	MF2_CANTLEAVEFLOORPIC = 0x02000000,	// stay within a certain floor type
	MF2_NONSHOOTABLE	= 0x04000000,	// mobj is totally non-shootable, 
										// but still considered solid
	MF2_INVULNERABLE	= 0x08000000,	// mobj is invulnerable
	MF2_DORMANT			= 0x10000000,	// thing is dormant
	MF2_ICEDAMAGE		= 0x20000000,	// does ice damage
	MF2_SEEKERMISSILE	= 0x40000000,	// is a seeker (for reflection)
	MF2_REFLECTIVE		= 0x80000000,	// reflects missiles

// --- mobj.flags3 ---

	MF3_FLOORHUGGER		= 0x00000001,	// Missile stays on floor
	MF3_CEILINGHUGGER	= 0x00000002,	// Missile stays on ceiling
	MF3_NORADIUSDMG		= 0x00000004,	// Actor does not take radius damage
	MF3_GHOST			= 0x00000008,	// Actor is a ghost
	MF3_ALWAYSPUFF		= 0x00000010,	// Puff always appears, even when hit nothing
	MF3_SEEISALSOACTIVE	= 0x00000020,	// Play see sound instead of active 1/2 the time
	MF3_DONTSPLASH		= 0x00000040,	// Thing doesn't make a splash
	MF3_VERYFAST		= 0x00000080,	// Don't offset missile as much when spawning
	MF3_DONTOVERLAP		= 0x00000100,	// Don't pass over/under other things with this bit set
	MF3_DONTMORPH		= 0x00000200,	// Immune to arti_egg
	MF3_DONTSQUASH		= 0x00000400,	// Death ball can't squash this actor
	MF3_EXPLOCOUNT		= 0x00000800,	// Don't explode until special2 counts to special1
	MF3_FULLVOLACTIVE	= 0x00001000,	// Active sound is played at full volume
	MF3_CLERICINVUL		= 0x00002000,	// Player who has the Cleric's invulnerability behavior
	MF3_SKYEXPLODE		= 0x00004000,	// Explode missile when hitting sky
	MF3_STAYMORPHED		= 0x00008000,	// Monster cannot unmorph
	MF3_DONTBLAST		= 0x00010000,	// Actor cannot be pushed by blasting
	MF3_CANBLAST		= 0x00020000,	// Actor is not a monster but can be blasted
	MF3_NOTARGET		= 0x00040000,	// This actor not targetted when it hurts something else
	MF3_DONTGIB			= 0x00080000,	// Don't gib this corpse
	MF3_NOBLOCKMONST	= 0x00100000,	// Can cross ML_BLOCKMONSTERS lines

// --- mobj.renderflags ---

	RF_XFLIP			= 0x0001,	// Flip sprite horizontally
	RF_YFLIP			= 0x0002,	// Flip sprite vertically
	RF_ONESIDED			= 0x0004,	// Wall/floor sprite is visible from front only
	RF_FULLBRIGHT		= 0x0010,	// Sprite is drawn at full brightness

	RF_RELMASK			= 0x0300,	// ---Relative z-coord for bound actors (these obey texture pegging)
	RF_RELABSOLUTE		= 0x0000,	// Actor z is absolute
	RF_RELUPPER			= 0x0100,	// Actor z is relative to upper part of wall
	RF_RELLOWER			= 0x0200,	// Actor z is relative to lower part of wall
	RF_RELMID			= 0x0300,	// Actor z is relative to middle part of wall

	RF_CLIPMASK			= 0x0c00,	// ---Clipping for bound actors
	RF_CLIPFULL			= 0x0000,	// Clip sprite to full height of wall
	RF_CLIPUPPER		= 0x0400,	// Clip sprite to upper part of wall
	RF_CLIPMID			= 0x0800,	// Clip sprite to mid part of wall
	RF_CLIPLOWER		= 0x0c00,	// Clip sprite to lower part of wall

	RF_SPRITETYPEMASK	= 0x7000,	// ---Different sprite types, not all implemented
	RF_FACESPRITE		= 0x0000,	// Face sprite
	RF_WALLSPRITE		= 0x1000,	// Wall sprite
	RF_FLOORSPRITE		= 0x2000,	// Floor sprite
	RF_VOXELSPRITE		= 0x3000,	// Voxel object
	RF_INVISIBLE		= 0x8000	// Don't bother drawing this actor
};

enum ERenderStyle
{
	STYLE_None,				// Do not draw
	STYLE_Normal,			// Normal; just copy the image to the screen
	STYLE_Fuzzy,			// Draw silhouette using "fuzz" effect
	STYLE_SoulTrans,		// Draw translucent with amount in r_transsouls
	STYLE_OptFuzzy,			// Draw as fuzzy or translucent, based on user preference
	STYLE_Shaded,			// Treat patch data as alpha values for alphacolor

	// The following styles can affect visibility in P_CheckSight()
	STYLE_Translucent=64,	// Draw translucent
	STYLE_Add,				// Draw additive
};

#define TRANSLUC25			(FRACUNIT/4)
#define TRANSLUC33			(FRACUNIT/3)
#define TRANSLUC50			(FRACUNIT/2)
#define TRANSLUC66			((FRACUNIT*2)/3)
#define TRANSLUC75			((FRACUNIT*3)/4)

// <wingdi.h> also #defines OPAQUE
#ifndef OPAQUE
#define OPAQUE				(FRACUNIT)
#endif

// This translucency value produces the closest match to Heretic's TINTTAB.
// ~40% of the value of the overlaid image shows through.
#define HR_SHADOW			(0x6800)

// Hexen's TINTTAB is the same as Heretic's, just reversed.
#define HX_SHADOW			(0x9800)
#define HX_ALTSHADOW		(0x6800)

class FPlayerSkin;
class FDecalBase;

inline AActor *GetDefaultByName (const char *name)
{
	return (AActor *)(TypeInfo::FindType (name)->ActorInfo->Defaults);
}

inline AActor *GetDefaultByType (const TypeInfo *type)
{
	return (AActor *)(type->ActorInfo->Defaults);
}

template<class T>
inline T *GetDefault ()
{
	return (T *)(RUNTIME_CLASS(T)->ActorInfo->Defaults);
}

// Map Object definition.
class AActor : public DThinker
{
	DECLARE_CLASS (AActor, DThinker)
	HAS_OBJECT_POINTERS
public:
	AActor () throw();
	AActor (const AActor &other) throw();
	AActor &operator= (const AActor &other);
	void Destroy ();
	~AActor ();

	void Serialize (FArchive &arc);

	static AActor *StaticSpawn (const TypeInfo *type, fixed_t x, fixed_t y, fixed_t z);

	inline AActor *GetDefault () const
	{
		return (AActor *)(RUNTIME_TYPE(this)->ActorInfo->Defaults);
	}

	// BeginPlay: Called just after the actor is created
	virtual void BeginPlay ();
	// LevelSpawned: Called after BeginPlay if this actor was spawned by the world
	virtual void LevelSpawned ();

	virtual void Activate (AActor *activator);
	virtual void Deactivate (AActor *activator);

	virtual void RunThink ();

	// Smallest yaw interval for a mapthing to be spawned with
	virtual angle_t AngleIncrements ();

	// Means-of-death for this actor
	virtual int GetMOD ();

	// Normal/ranged obituary if this actor is the attacker
	virtual const char *GetObituary ();

	// Melee obituary if this actor is the attacker
	virtual const char *GetHitObituary ();

	// Return true if the monster should use a missile attack, false for melee
	virtual bool SuggestMissileAttack (fixed_t dist);

	// Called when actor dies
	virtual void Die (AActor *source, AActor *inflictor);

	// Called by A_Explode just before exploding the actor
	virtual void PreExplode ();

	// Called by A_Explode to find out how much damage to do
	virtual void GetExplodeParms (int &damage, int &dist, bool &hurtSource);

	// Perform some special damage action. Returns the amount of damage to do.
	// Returning -1 signals the damage routine to exit immediately
	virtual int DoSpecialDamage (AActor *target, int damage);

	// Centaurs and ettins squeal when electrocuted, poisoned, or "holy"-ed
	virtual void Howl ();

	// Returns true if other should become the actor's new target
	virtual bool NewTarget (AActor *other);

	// Called by A_NoBlocking in case the actor wants to drop some presents
	virtual void NoBlockingSet ();

	// Called by A_SinkMobj
	virtual fixed_t GetSinkSpeed ();

	// Called by A_RaiseMobj
	virtual fixed_t GetRaiseSpeed ();

	// Actor just hit the floor
	virtual void HitFloor ();

	virtual void ChangeSpecial (byte special, byte data1, byte data2,
		byte data3, byte data4, byte data5);

	// Set the alphacolor field properly
	void SetShade (DWORD rgb);
	void SetShade (int r, int g, int b);

// info for drawing
// NOTE: The first member variable *must* be x.
	fixed_t	 		x,y,z;
	AActor			*snext, **sprev;	// links in sector (if needed)
	angle_t			angle;
	WORD			sprite;				// used to find patch_t and flip value
	BYTE			frame;				// sprite frame to draw
	BYTE			xscale, yscale;		// Scaling values, 63 is normal size
	BYTE			RenderStyle;		// Style to draw this actor with
	WORD			renderflags;		// Different rendering flags
	WORD			picnum;				// Draw this instead of sprite if != 0xffff
	DWORD			effects;			// [RH] see p_effect.h
	BYTE			*translation;		// Translation table (or NULL)
	fixed_t			alpha;
	DWORD			alphacolor;			// Color to draw when STYLE_Shaded

// interaction info
	fixed_t			pitch, roll;
	AActor			*bnext, **bprev;	// links in blocks (if needed)
	struct subsector_s		*subsector;
	fixed_t			floorz, ceilingz;	// closest together of contacted secs
	fixed_t			dropoffz;		// killough 11/98: the lowest floor over all contacted Sectors.

	struct sector_t	*floorsector;
	int				floorpic;			// contacted sec floorpic
	fixed_t			radius, height;		// for movement checking
	fixed_t			momx, momy, momz;	// momentums
	int				validcount;			// if == validcount, already checked
	int				tics;				// state tic counter
	FState			*state;
	int				damage;			// For missiles
	int				flags;
	int				flags2;			// Heretic flags
	int				flags3;			// Hexen/Heretic actor-dependant behavior made flagged
	int				mapflags;		// Flags from map (MTF_*)
	int				special1;		// Special info
	int				special2;		// Special info
	int 			health;
	byte			movedir;		// 0-7
	char			visdir;
	short			movecount;		// when 0, select a new dir
	AActor			*target;		// thing being chased/attacked (or NULL)
									// also the originator for missiles
	AActor			*lastenemy;		// Last known enemy -- killogh 2/15/98
	int				reactiontime;	// if non 0, don't attack yet
									// used by player to freeze a bit after
									// teleporting
	int				threshold;		// if > 0, the target will be chased
									// no matter what (even if shot)
	player_s		*player;		// only valid if type of APlayerPawn
	int				lastlook;		// player number last looked for
	WORD			SpawnPoint[3]; 	// For nightmare respawn
	WORD			SpawnAngle;
	AActor			*tracer;		// Thing being chased/attacked for tracers
	fixed_t			floorclip;		// value to use for floor clipping
	WORD			tid;			// thing identifier
	byte			special;		// special
	byte			args[5];		// special arguments

	AActor			*inext, **iprev;// Links to other mobjs in same bucket
	AActor			*goal;			// Monster's goal if not chasing anything
	FPlayerSkin		*skin;			// Sprite override
	byte			waterlevel;		// 0=none, 1=feet, 2=waist, 3=eyes
	BYTE			SpawnFlags;
	SWORD			gear;			// killough 11/98: used in torque simulation

	// a linked list of sectors where this object appears
	struct msecnode_s	*touching_sectorlist;				// phares 3/14/98

	//Added by MC:
	int id;							// Player ID (for items, # in list.)

	// [RH] Stuff that used to be part of an Actor Info
	WORD SeeSound;
	WORD AttackSound;
	WORD PainSound;
	WORD DeathSound;
	WORD ActiveSound;

	int ReactionTime;
	fixed_t Speed;
	int Mass;
	SWORD PainChance;

	FState *SpawnState;
	FState *SeeState;
	FState *PainState;
	FState *MeleeState;
	FState *MissileState;
	FState *CrashState;
	FState *DeathState;
	FState *XDeathState;
	FState *BDeathState;
	FState *IDeathState;
	FState *RaiseState;

	// [RH] Decal(s) this weapon/projectile generates on impact.
	FDecalBase *DecalGenerator;

	// Public functions
	bool IsTeammate (AActor *other);

	// ThingIDs
	static void ClearTIDHashes ();
	void AddToHash ();
	void RemoveFromHash ();

private:
	static AActor *TIDHash[128];
	static inline int TIDHASH (int key) { return key & 127; }

	friend class FActorIterator;

public:
	void LinkToWorld ();
	void UnlinkFromWorld ();
	void AdjustFloorClip ();
	void SetOrigin (fixed_t x, fixed_t y, fixed_t z);
	bool SetState (FState *newstate);
	bool SetStateNF (FState *newstate);

	static FState States[];
};

class FActorIterator
{
public:
	FActorIterator (int i) : base (NULL), id (i)
	{
	}
	AActor *Next ()
	{
		if (id == 0)
			return NULL;
		if (!base)
			base = AActor::TIDHash[id & 127];
		else
			base = base->inext;

		while (base && base->tid != id)
			base = base->inext;

		return base;
	}
private:
	AActor *base;
	int id;
};

template<class T>
class TActorIterator : public FActorIterator
{
public:
	TActorIterator (int id) : FActorIterator (id) {}
	T *Next ()
	{
		AActor *actor;
		do
		{
			actor = FActorIterator::Next ();
		} while (actor && !actor->IsKindOf (RUNTIME_CLASS(T)));
		return static_cast<T *>(actor);
	}
};

inline AActor *Spawn (const TypeInfo *type, fixed_t x, fixed_t y, fixed_t z)
{
	return AActor::StaticSpawn (type, x, y, z);
}

inline AActor *Spawn (const char *type, fixed_t x, fixed_t y, fixed_t z)
{
	return AActor::StaticSpawn (TypeInfo::FindType (type), x, y, z);
}

template<class T>
inline T *Spawn (fixed_t x, fixed_t y, fixed_t z)
{
	return static_cast<T *>(AActor::StaticSpawn (RUNTIME_CLASS(T), x, y, z));
}

#define S_FREETARGMOBJ	1

struct FSoundIndex
{
	int Index;
};

struct FSoundIndexWord
{
	WORD Index;
};

FArchive &operator<< (FArchive &arc, FSoundIndex &snd);
FArchive &operator<< (FArchive &arc, FSoundIndexWord &snd);

#define AR_SOUND(id) (*(FSoundIndex *)&(id))
#define AR_SOUNDW(id) (*(FSoundIndexWord *)&(id))

#endif // __P_MOBJ_H__
