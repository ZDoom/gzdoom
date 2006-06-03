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
//		Play functions, animation, global header.
//
//-----------------------------------------------------------------------------


#ifndef __P_LOCAL__
#define __P_LOCAL__

#ifndef __R_LOCAL__
#include "r_local.h"
#endif

#include <stdlib.h>

#define STEEPSLOPE		46341	// [RH] Minimum floorplane.c value for walking

#define MAXMORPHHEALTH	30

#define BONUSADD		6

// mapblocks are used to check movement
// against lines and things
#define MAPBLOCKUNITS	128
#define MAPBLOCKSIZE	(MAPBLOCKUNITS*FRACUNIT)
#define MAPBLOCKSHIFT	(FRACBITS+7)
#define MAPBMASK		(MAPBLOCKSIZE-1)
#define MAPBTOFRAC		(MAPBLOCKSHIFT-FRACBITS)


// MAXRADIUS is for precalculated sector block boxes
// the spider demon is larger,
// but we do not have any moving sectors nearby
#define MAXRADIUS		0/*32*FRACUNIT*/

//#define GRAVITY 		FRACUNIT
#define MAXMOVE 		(30*FRACUNIT)

#define USERANGE		(64*FRACUNIT)
#define MELEERANGE		(64*FRACUNIT)
#define MISSILERANGE	(32*64*FRACUNIT)
#define PLAYERMISSILERANGE	(8192*FRACUNIT)	// [RH] New MISSILERANGE for players

// follow a player exlusively for 3 seconds
#define BASETHRESHOLD	100



//
// P_PSPR
//
void P_SetupPsprites (player_t* curplayer);
void P_MovePsprites (player_t* curplayer);
void P_DropWeapon (player_t* player);


//
// P_USER
//
void	P_FallingDamage (AActor *ent);
void	P_PlayerThink (player_t *player);
bool	P_UndoPlayerMorph (player_t *player, bool force=false);
void	P_PredictPlayer (player_t *player);
void	P_UnPredictPlayer ();

//
// P_MOBJ
//

#define ONFLOORZ		FIXED_MIN
#define ONCEILINGZ		FIXED_MAX
#define FLOATRANDZ		(FIXED_MAX-1)

extern fixed_t FloatBobOffsets[64];
extern AActor *MissileActor;

void P_SpawnPlayer (mapthing2_t* mthing, bool startenterscripts = true);

void P_ThrustMobj (AActor *mo, angle_t angle, fixed_t move);
int P_FaceMobj (AActor *source, AActor *target, angle_t *delta);
bool P_SeekerMissile (AActor *actor, angle_t thresh, angle_t turnMax);

AActor *P_SpawnPuff (const PClass *pufftype, fixed_t x, fixed_t y, fixed_t z, angle_t dir, int updown, bool hit=false);
void	P_SpawnBlood (fixed_t x, fixed_t y, fixed_t z, angle_t dir, int damage, AActor *originator);
void	P_BloodSplatter (fixed_t x, fixed_t y, fixed_t z, AActor *originator);
void	P_BloodSplatter2 (fixed_t x, fixed_t y, fixed_t z, AActor *originator);
void	P_RipperBlood (AActor *mo, AActor *bleeder);
int		P_GetThingFloorType (AActor *thing);
void	P_ExplodeMissile (AActor *missile, line_t *explodeline);

AActor *P_SpawnMissile (AActor* source, AActor* dest, const PClass *type);
AActor *P_SpawnMissileZ (AActor* source, fixed_t z, AActor* dest, const PClass *type);
AActor *P_SpawnMissileXYZ (fixed_t x, fixed_t y, fixed_t z, AActor *source, AActor *dest, const PClass *type);
AActor *P_SpawnMissileAngle (AActor *source, const PClass *type, angle_t angle, fixed_t momz);
AActor *P_SpawnMissileAngleSpeed (AActor *source, const PClass *type, angle_t angle, fixed_t momz, fixed_t speed);
AActor *P_SpawnMissileAngleZ (AActor *source, fixed_t z, const PClass *type, angle_t angle, fixed_t momz);
AActor *P_SpawnMissileAngleZSpeed (AActor *source, fixed_t z, const PClass *type, angle_t angle, fixed_t momz, fixed_t speed, AActor *owner=NULL);
AActor *P_SpawnMissileZAimed (AActor *source, fixed_t z, AActor *dest, const PClass *type);

AActor *P_SpawnPlayerMissile (AActor* source, const PClass *type);
AActor *P_SpawnPlayerMissile (AActor *source, const PClass *type, angle_t angle);
AActor *P_SpawnPlayerMissile (AActor *source, fixed_t x, fixed_t y, fixed_t z, const PClass *type, angle_t angle);

void P_CheckFakeFloorTriggers (AActor *mo, fixed_t oldz);

//
// [RH] P_THINGS
//
#define MAX_SPAWNABLES	(256)
extern const PClass *SpawnableThings[MAX_SPAWNABLES];

bool	P_Thing_Spawn (int tid, int type, angle_t angle, bool fog, int newtid);
bool	P_Thing_Projectile (int tid, int type, const char * type_name, angle_t angle,
			fixed_t speed, fixed_t vspeed, int dest, AActor *forcedest, int gravity, int newtid,
			bool leadTarget);
bool	P_Thing_Move (int tid, int mapspot, bool fog);

//
// P_ENEMY
//
void	P_NoiseAlert (AActor* target, AActor* emmiter, bool splash);


//
// P_MAPUTL
//
typedef struct
{
	fixed_t 	x;
	fixed_t 	y;
	fixed_t 	dx;
	fixed_t 	dy;
	
} divline_t;

typedef struct
{
	fixed_t 	frac;			// along trace line
	bool	 	isaline;
	union {
		AActor *thing;
		line_t *line;
	} d;
} intercept_t;

extern TArray<intercept_t> intercepts;

typedef BOOL (*traverser_t) (intercept_t *in);

fixed_t P_AproxDistance (fixed_t dx, fixed_t dy);

//==========================================================================
//
// P_PointOnLineSide
//
// Returns 0 (front/on) or 1 (back)
// [RH] inlined, stripped down, and made more precise
//
//==========================================================================

inline int P_PointOnLineSide (fixed_t x, fixed_t y, const line_t *line)
{
	return DMulScale32 (y-line->v1->y, line->dx, line->v1->x-x, line->dy) > 0;
}

//==========================================================================
//
// P_PointOnDivlineSide
//
// Same as P_PointOnLineSide except it uses divlines
// [RH] inlined, stripped down, and made more precise
//
//==========================================================================

inline int P_PointOnDivlineSide (fixed_t x, fixed_t y, const divline_t *line)
{
	return DMulScale32 (y-line->y, line->dx, line->x-x, line->dy) > 0;
}

//==========================================================================
//
// P_MakeDivline
//
//==========================================================================

inline void P_MakeDivline (const line_t *li, divline_t *dl)
{
	dl->x = li->v1->x;
	dl->y = li->v1->y;
	dl->dx = li->dx;
	dl->dy = li->dy;
}

fixed_t P_InterceptVector (const divline_t *v2, const divline_t *v1);
int 	P_BoxOnLineSide (const fixed_t *tmbox, const line_t *ld);

extern fixed_t			opentop;
extern fixed_t			openbottom;
extern fixed_t			openrange;
extern fixed_t			lowfloor;

void	P_LineOpening (const line_t *linedef, fixed_t x, fixed_t y, fixed_t refx=FIXED_MIN, fixed_t refy=0);

BOOL P_BlockLinesIterator (int x, int y, BOOL(*func)(line_t*));
BOOL P_BlockThingsIterator (int x, int y, BOOL(*func)(AActor*), TArray<AActor *> &checkarray, AActor *start=NULL);


#define PT_ADDLINES 	1
#define PT_ADDTHINGS	2
#define PT_EARLYOUT 	4

extern divline_t		trace;

BOOL
P_PathTraverse
( fixed_t		x1,
  fixed_t		y1,
  fixed_t		x2,
  fixed_t		y2,
  int			flags,
  BOOL		(*trav) (intercept_t *));

AActor *P_BlockmapSearch (AActor *origin, int distance, AActor *(*func)(AActor *, int));
AActor *P_RoughMonsterSearch (AActor *mo, int distance);

//
// P_MAP
//

// If "floatok" true, move would be ok
// if within "tmfloorz - tmceilingz".
extern BOOL				floatok;
extern fixed_t			tmfloorz;
extern fixed_t			tmceilingz;
extern msecnode_t		*sector_list;		// phares 3/16/98
extern AActor			*BlockingMobj;
extern line_t			*BlockingLine;		// Used only by P_Move
											// This is not necessarily a *blocking* line

// For P_FindFloorCeiling
extern fixed_t			tmffloorz, tmfceilingz;
extern fixed_t			tmfdropoffz;
extern fixed_t			tmffloorpic;
extern sector_t			*tmffloorsector;
extern fixed_t			tmfceilingpic;
extern sector_t			*tmfceilingsector;


//Added by MC: tmsectortype
extern fixed_t			tmdropoffz; //Needed in b_move.c
extern sector_t			*tmsector;

extern line_t			*ceilingline;

extern TArray<line_t *> spechit;

// [RH] These are used by PIT_CheckThing and P_XYMovement to apply
// ripping damage once per tic instead of once per move.
extern bool				DoRipping;
extern AActor			*LastRipped;

BOOL	P_TestMobjLocation (AActor *mobj);
bool	P_TestMobjZ (AActor *mobj);
BOOL	P_CheckPosition (AActor *thing, fixed_t x, fixed_t y);
AActor	*P_CheckOnmobj (AActor *thing);
void	P_FakeZMovement (AActor *mo);
BOOL	P_TryMove (AActor* thing, fixed_t x, fixed_t y, BOOL dropoff, bool onfloor = false);
BOOL	P_TeleportMove (AActor* thing, fixed_t x, fixed_t y, fixed_t z, BOOL telefrag);	// [RH] Added z and telefrag parameters
void	P_PlayerStartStomp (AActor *actor);		// [RH] Stomp on things for a newly spawned player
void	P_SlideMove (AActor* mo, fixed_t tryx, fixed_t tryy, int numsteps);
bool	P_BounceWall (AActor *mo);
bool	P_CheckSight (const AActor* t1, const AActor* t2, int flags=0);
void	P_ResetSightCounters (bool full);
void	P_UseLines (player_t* player);
bool	P_UsePuzzleItem (AActor *actor, int itemType);
void	PIT_ThrustSpike (AActor *actor);
void	P_FindFloorCeiling (AActor *actor);

bool	P_ChangeSector (sector_t* sector, int crunch, int amt, int floorOrCeil);

extern	AActor*	linetarget; 	// who got hit (or NULL)
extern	AActor *PuffSpawned;	// points to last puff spawned

fixed_t P_AimLineAttack (AActor *t1, angle_t angle, fixed_t distance, fixed_t vrange=0);
void	P_LineAttack (AActor *t1, angle_t angle, fixed_t distance, int pitch, int damage, int damageType, const PClass *pufftype);
void	P_TraceBleed (int damage, fixed_t x, fixed_t y, fixed_t z, AActor *target, angle_t angle, int pitch);
void	P_TraceBleed (int damage, AActor *target, angle_t angle, int pitch);
void	P_TraceBleed (int damage, AActor *target, AActor *missile);		// missile version
void	P_TraceBleed (int damage, AActor *target);		// random direction version
void	P_RailAttack (AActor *source, int damage, int offset, int color1 = 0, int color2 = 0, float maxdiff = 0, bool silent = false);	// [RH] Shoot a railgun
bool	P_HitFloor (AActor *thing);
bool	P_HitWater (AActor *thing, sector_t *sec);
bool	P_CheckMissileSpawn (AActor *missile);

// [RH] Position the chasecam
void	P_AimCamera (AActor *t1);
extern	fixed_t CameraX, CameraY, CameraZ;
extern	sector_t *CameraSector;

// [RH] Means of death
void	P_RadiusAttack (AActor *spot, AActor *source, int damage, int distance, int damageType, bool hurtSelf, bool thrustless=false);

void	P_DelSector_List();
void	P_DelSeclist(msecnode_t *);							// phares 3/16/98
void	P_CreateSecNodeList(AActor*,fixed_t,fixed_t);		// phares 3/14/98
int		P_GetMoveFactor(const AActor *mo, int *frictionp);	// phares  3/6/98
int		P_GetFriction(const AActor *mo, int *frictionfactor);
BOOL	Check_Sides(AActor *, int, int);					// phares

// [RH] 
bool	P_CheckSlopeWalk (AActor *actor, fixed_t &xmove, fixed_t &ymove);

//
// P_SETUP
//
extern byte*			rejectmatrix;	// for fast sight rejection
extern int*				blockmaplump;	// offsets in blockmap are from here
extern int*				blockmap;
extern int				bmapwidth;
extern int				bmapheight; 	// in mapblocks
extern fixed_t			bmaporgx;
extern fixed_t			bmaporgy;		// origin of block map
extern FBlockNode**		blocklinks; 	// for thing chains



//
// P_INTER
//
void P_TouchSpecialThing (AActor *special, AActor *toucher);

void P_DamageMobj (AActor *target, AActor *inflictor, AActor *source, int damage, int mod, int flags=0);

bool P_GiveBody (AActor *actor, int num);
bool P_MorphPlayer (player_t *player, const PClass *morphClass);
void P_PoisonPlayer (player_t *player, AActor *poisoner, AActor *source, int poison);
void P_PoisonDamage (player_t *player, AActor *source, int damage, bool playPainSound);

#define DMG_NO_ARMOR		1

extern	int MeansOfDeath;


// ===== PO_MAN =====

typedef enum
{
	PODOOR_NONE,
	PODOOR_SLIDE,
	PODOOR_SWING,
} podoortype_t;

inline FArchive &operator<< (FArchive &arc, podoortype_t &type)
{
	BYTE val = (BYTE)type;
	arc << val;
	type = (podoortype_t)val;
	return arc;
}

class DPolyAction : public DThinker
{
	DECLARE_CLASS (DPolyAction, DThinker)
public:
	DPolyAction (int polyNum);
	~DPolyAction ();
	void Serialize (FArchive &arc);

	void StopInterpolation ();
protected:
	DPolyAction ();
	int m_PolyObj;
	int m_Speed;
	int m_Dist;

	void SetInterpolation ();

	friend void ThrustMobj (AActor *actor, seg_t *seg, polyobj_t *po);
};

void ThrustMobj (AActor *actor, seg_t *seg, polyobj_t *po);

class DRotatePoly : public DPolyAction
{
	DECLARE_CLASS (DRotatePoly, DPolyAction)
public:
	DRotatePoly (int polyNum);
	void Tick ();
private:
	DRotatePoly ();

	friend bool EV_RotatePoly (line_t *line, int polyNum, int speed, int byteAngle, int direction, BOOL overRide);
};

bool EV_RotatePoly (line_t *line, int polyNum, int speed, int byteAngle, int direction, BOOL overRide);

bool EV_RotatePoly (line_t *line, int polyNum, int speed, int byteAngle, int direction, BOOL overRide);

class DMovePoly : public DPolyAction
{
	DECLARE_CLASS (DMovePoly, DPolyAction)
public:
	DMovePoly (int polyNum);
	void Serialize (FArchive &arc);
	void Tick ();
protected:
	DMovePoly ();
	int m_Angle;
	fixed_t m_xSpeed; // for sliding walls
	fixed_t m_ySpeed;

	friend bool EV_MovePoly (line_t *line, int polyNum, int speed, angle_t angle, fixed_t dist, BOOL overRide);
};

bool EV_MovePoly (line_t *line, int polyNum, int speed, angle_t angle, fixed_t dist, BOOL overRide);

class DPolyDoor : public DMovePoly
{
	DECLARE_CLASS (DPolyDoor, DMovePoly)
public:
	DPolyDoor (int polyNum, podoortype_t type);
	void Serialize (FArchive &arc);
	void Tick ();
protected:
	int m_Direction;
	int m_TotalDist;
	int m_Tics;
	int m_WaitTics;
	podoortype_t m_Type;
	bool m_Close;

	friend bool EV_OpenPolyDoor (line_t *line, int polyNum, int speed, angle_t angle, int delay, int distance, podoortype_t type);
private:
	DPolyDoor ();
};

bool EV_OpenPolyDoor (line_t *line, int polyNum, int speed, angle_t angle, int delay, int distance, podoortype_t type);

// [RH] Data structure for P_SpawnMapThing() to keep track
//		of polyobject-related things.
typedef struct polyspawns_s
{
	struct polyspawns_s *next;
	fixed_t x;
	fixed_t y;
	short angle;
	short type;
} polyspawns_t;

enum
{
	PO_HEX_ANCHOR_TYPE = 3000,
	PO_HEX_SPAWN_TYPE,
	PO_HEX_SPAWNCRUSH_TYPE,

	// [RH] Thing numbers that don't conflict with Doom things
	PO_ANCHOR_TYPE = 9300,
	PO_SPAWN_TYPE,
	PO_SPAWNCRUSH_TYPE,
	PO_SPAWNHURT_TYPE
};

#define PO_LINE_START 1 // polyobj line start special
#define PO_LINE_EXPLICIT 5

extern polyobj_t *polyobjs; // list of all poly-objects on the level
extern int po_NumPolyobjs;
extern polyspawns_t *polyspawns;	// [RH] list of polyobject things to spawn


BOOL PO_MovePolyobj (int num, int x, int y);
BOOL PO_RotatePolyobj (int num, angle_t angle);
void PO_Init ();
BOOL PO_Busy (int polyobj);

//
// P_SPEC
//
#include "p_spec.h"


#endif	// __P_LOCAL__
