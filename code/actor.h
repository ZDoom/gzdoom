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

// --- mobj.flags ---

#define	MF_SPECIAL		1			// call P_SpecialThing when touched
#define	MF_SOLID		2
#define	MF_SHOOTABLE	4
#define	MF_NOSECTOR		8			// don't use the sector links
									// (invisible but touchable)
#define	MF_NOBLOCKMAP	16			// don't use the blocklinks
									// (inert but displayable)
#define	MF_AMBUSH		32			// not activated by sound; deaf monster
#define	MF_JUSTHIT		64			// try to attack right back
#define	MF_JUSTATTACKED	128			// take at least one step before attacking
#define	MF_SPAWNCEILING	256			// hang from ceiling instead of floor
#define	MF_NOGRAVITY	512			// don't apply gravity every tic

// movement flags
#define	MF_DROPOFF		0x00000400	// allow jumps from high places
#define	MF_PICKUP		0x00000800	// for players to pick up items
#define	MF_NOCLIP		0x00001000	// player cheat
#define	MF_SLIDE		0x00002000	// keep info about sliding along walls
#define	MF_FLOAT		0x00004000	// allow moves to any height, no gravity
#define	MF_TELEPORT		0x00008000	// don't cross lines or look at heights
#define MF_MISSILE		0x00010000	// don't hit same species, explode on block

#define	MF_DROPPED		0x00020000	// dropped by a demon, not level spawned
#define	MF_SHADOW		0x00040000	// use fuzzy draw (shadow demons / spectres)
#define	MF_NOBLOOD		0x00080000	// don't bleed when shot (use puff)
#define	MF_CORPSE		0x00100000	// don't stop moving halfway off a step
#define	MF_INFLOAT		0x00200000	// floating to a height for a move, don't
									// auto float to target's height

#define	MF_COUNTKILL	0x00400000	// count towards intermission kill total
#define	MF_COUNTITEM	0x00800000	// count towards intermission item total

#define	MF_SKULLFLY		0x01000000	// skull in flight
#define	MF_NOTDMATCH	0x02000000	// don't spawn in death match (key cards)

#define	MF_TRANSLATION	0x0c000000	// if 0x4 0x8 or 0xc, use a translation
#define	MF_TRANSSHIFT	26			// tablefor player colormaps

#define	MF_STEALTH		0x40000000	// [RH] Andy Baker's stealth monsters
#define	MF_ICECORPSE	0x80000000	// a frozen corpse (for blasting) [RH] was 0x800000




// --- mobj.flags2 ---

#define MF2_LOGRAV			0x00000001	// alternate gravity setting
#define MF2_WINDTHRUST		0x00000002	// gets pushed around by the wind
										// specials
#define MF2_FLOORBOUNCE		0x00000004	// bounces off the floor
#define MF2_BLASTED			0x00000008	// missile will pass through ghosts
#define MF2_FLY				0x00000010	// fly mode is active
#define MF2_FLOORCLIP		0x00000020	// if feet are allowed to be clipped
#define MF2_SPAWNFLOAT		0x00000040	// spawn random float z
#define MF2_NOTELEPORT		0x00000080	// does not teleport
#define MF2_RIP				0x00000100	// missile rips through solid
										// targets
#define MF2_PUSHABLE		0x00000200	// can be pushed by other moving
										// mobjs
#define MF2_SLIDE			0x00000400	// slides against walls
#define MF2_ONMOBJ			0x00000800	// mobj is resting on top of another
										// mobj
#define MF2_PASSMOBJ		0x00001000	// Enable z block checking.  If on,
										// this flag will allow the mobj to
										// pass over/under other mobjs.
#define MF2_CANNOTPUSH		0x00002000	// cannot push other pushable mobjs
//#define MF2_DROPPED			0x00004000	// dropped by a demon [RH] use MF_DROPPED instead
#define MF2_THRUGHOST		0x00004000	// missile will pass through ghosts [RH] was 8
#define MF2_BOSS			0x00008000	// mobj is a major boss
#define MF2_FIREDAMAGE		0x00010000	// does fire damage
#define MF2_NODMGTHRUST		0x00020000	// does not thrust target when
										// damaging
#define MF2_TELESTOMP		0x00040000	// mobj can stomp another
#define MF2_FLOATBOB		0x00080000	// use float bobbing z movement
#define MF2_DONTDRAW		0x00100000	// don't generate a vissprite
#define MF2_IMPACT			0x00200000 	// an MF_MISSILE mobj can activate
								 		// SPAC_IMPACT
#define MF2_PUSHWALL		0x00400000 	// mobj can push walls
#define MF2_MCROSS			0x00800000	// can activate monster cross lines
#define MF2_PCROSS			0x01000000	// can activate projectile cross lines
#define MF2_CANTLEAVEFLOORPIC 0x02000000 // stay within a certain floor type
#define MF2_NONSHOOTABLE	0x04000000	// mobj is totally non-shootable, 
										// but still considered solid
#define MF2_INVULNERABLE	0x08000000	// mobj is invulnerable
#define MF2_DORMANT			0x10000000	// thing is dormant
#define MF2_ICEDAMAGE		0x20000000  // does ice damage
#define MF2_SEEKERMISSILE	0x40000000	// is a seeker (for reflection)
#define MF2_REFLECTIVE		0x80000000	// reflects missiles


#define TRANSLUC25			(FRACUNIT/4)
#define TRANSLUC33			(FRACUNIT/3)
#define TRANSLUC50			(FRACUNIT/2)
#define TRANSLUC66			((FRACUNIT*2)/3)
#define TRANSLUC75			((FRACUNIT*3)/4)

// Map Object definition.
class AActor : public DThinker
{
	DECLARE_SERIAL (AActor, DThinker)
public:
	AActor ();
	AActor (const AActor &other);
	AActor &operator= (const AActor &other);
	AActor (fixed_t x, fixed_t y, fixed_t z, mobjtype_t type);
	~AActor ();

	virtual void RunThink ();

// info for drawing
	fixed_t	 		x,y,z;
	AActor			*snext, **sprev;	// links in sector (if needed)
	angle_t			angle;
	spritenum_t		sprite;				// used to find patch_t and flip value
	int				frame;				// might be ord with FF_FULLBRIGHT
	DWORD			effects;			// [RH] see p_effect.h

// interaction info
	fixed_t			pitch, roll;
	AActor			*bnext, **bprev;	// links in blocks (if needed)
	struct subsector_s		*subsector;
	fixed_t			floorz, ceilingz;	// closest together of contacted secs
//	fixed_t			floorpic;			// contacted sec floorpic
	fixed_t			radius, height;		// for movement checking
	fixed_t			momx, momy, momz;	// momentums
	int				validcount;			// if == validcount, already checked
	mobjtype_t		type;
	mobjinfo_t		*info;				// &mobjinfo[mobj->type]
	int				tics;				// state tic counter
	state_t			*state;
	int				damage;			// For missiles
	int				flags;
	int				flags2;			// Heretic flags
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
	player_s		*player;		// only valid if type == MT_PLAYER
	int				lastlook;		// player number last looked for
	mapthing2_t		spawnpoint; 	// For nightmare respawn
	AActor			*tracer;		// Thing being chased/attacked for tracers
//	fixed_t			floorclip;		// value to use for floor clipping
//	int				archiveNum;		// Identity during archive
	short			tid;			// thing identifier
	byte			special;		// special
	byte			args[5];		// special arguments

	AActor			*inext, *iprev;	// Links to other mobjs in same bucket
	AActor			*goal;			// Monster's goal if not chasing anything
	unsigned		targettic;		// Avoid missiles blowing up in your face
	byte			*translation;	// Translation table (or NULL)
	fixed_t			translucency;	// 65536=fully opaque, 0=fully invisible
	byte			waterlevel;		// 0=none, 1=feet, 2=waist, 3=eyes

	// a linked list of sectors where this object appears
	struct msecnode_s	*touching_sectorlist;				// phares 3/14/98

	//Added by MC:
	int id;							// Player ID (for items, # in list.)

	// Public functions
	bool IsTeammate (AActor *other);

	// ThingIDs
	static void ClearTIDHashes ();
	void AddToHash ();
	void RemoveFromHash ();
	AActor *FindByTID (int tid) const;
	static AActor *FindByTID (const AActor *first, int tid);
	AActor *FindGoal (int tid, int kind) const;
	static AActor *FindGoal (const AActor *first, int tid, int kind);

private:
	static AActor *TIDHash[128];
	static inline int TIDHASH (int key) { return key & 127; }

public:
	void LinkToWorld ();
	void UnlinkFromWorld ();
	void SetOrigin (fixed_t x, fixed_t y, fixed_t z);

};



#endif // __P_MOBJ_H__
