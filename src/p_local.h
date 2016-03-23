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

#include <float.h>
#include "doomtype.h"
#include "tables.h"
#include "vectors.h"

const double NO_VALUE = FLT_MAX;

class player_t;
class AActor;
struct FPlayerStart;
class PClassActor;
struct fixedvec3;
class APlayerPawn;
struct line_t;
struct sector_t;
struct msecnode_t;
struct secplane_t;
struct FCheckPosition;
struct FTranslatedLineTarget;

#include <stdlib.h>

#define STEEPSLOPE		46342	// [RH] Minimum floorplane.c value for walking

#define BONUSADD		6

// mapblocks are used to check movement
// against lines and things
#define MAPBLOCKUNITS	128
#define MAPBLOCKSIZE	(MAPBLOCKUNITS*FRACUNIT)
#define MAPBLOCKSHIFT	(FRACBITS+7)
#define MAPBMASK		(MAPBLOCKSIZE-1)
#define MAPBTOFRAC		(MAPBLOCKSHIFT-FRACBITS)

// Inspired by Maes
extern int bmapnegx;
extern int bmapnegy;

inline int GetSafeBlockX(int blockx)
{
	blockx >>= MAPBLOCKSHIFT;
	return (blockx <= bmapnegx) ? blockx & 0x1FF : blockx;
}
inline int GetSafeBlockX(long long blockx)
{
	blockx >>= MAPBLOCKSHIFT;
	return int((blockx <= bmapnegx) ? blockx & 0x1FF : blockx);
}

inline int GetSafeBlockY(int blocky)
{
	blocky >>= MAPBLOCKSHIFT;
	return (blocky <= bmapnegy) ? blocky & 0x1FF: blocky;
}
inline int GetSafeBlockY(long long blocky)
{
	blocky >>= MAPBLOCKSHIFT;
	return int((blocky <= bmapnegy) ? blocky & 0x1FF: blocky);
}

//#define GRAVITY 		FRACUNIT
#define _f_MAXMOVE 		(30*FRACUNIT)
#define MAXMOVE 		(30.)

#define TALKRANGE		(128.)
#define USERANGE		(64*FRACUNIT)

#define MELEERANGE		(64.)
#define SAWRANGE		(64.+(1./65536.))	// use meleerange + 1 so the puff doesn't skip the flash (i.e. plays all states)
#define MISSILERANGE	(32*64.)
#define PLAYERMISSILERANGE	(8192.)	// [RH] New MISSILERANGE for players

// follow a player exlusively for 3 seconds
// No longer used.
// #define BASETHRESHOLD	100


//
// P_PSPR
//
void P_SetupPsprites (player_t* curplayer, bool startweaponup);
void P_MovePsprites (player_t* curplayer);
void P_DropWeapon (player_t* player);


//
// P_USER
//
void	P_FallingDamage (AActor *ent);
void	P_PlayerThink (player_t *player);
void	P_PredictPlayer (player_t *player);
void	P_UnPredictPlayer ();
void	P_PredictionLerpReset();

//
// P_MOBJ
//

#define SPF_TEMPPLAYER		1	// spawning a short-lived dummy player
#define SPF_WEAPONFULLYUP	2	// spawn with weapon already raised

APlayerPawn *P_SpawnPlayer (FPlayerStart *mthing, int playernum, int flags=0);

int P_FaceMobj (AActor *source, AActor *target, DAngle *delta);
bool P_SeekerMissile (AActor *actor, double thresh, double turnMax, bool precise = false, bool usecurspeed=false);

enum EPuffFlags
{
	PF_HITTHING = 1,
	PF_MELEERANGE = 2,
	PF_TEMPORARY = 4,
	PF_HITTHINGBLEED = 8,
	PF_NORANDOMZ = 16
};

AActor *P_SpawnPuff(AActor *source, PClassActor *pufftype, const DVector3 &pos, DAngle hitdir, DAngle particledir, int updown, int flags = 0, AActor *vict = NULL);
inline AActor *P_SpawnPuff(AActor *source, PClassActor *pufftype, const fixedvec3 &pos, angle_t hitdir, angle_t particledir, int updown, int flags = 0, AActor *vict = NULL)
{
	DVector3 _pos(FIXED2DBL(pos.x), FIXED2DBL(pos.y), FIXED2DBL(pos.z));
	return P_SpawnPuff(source, pufftype, _pos, ANGLE2DBL(hitdir), ANGLE2DBL(particledir), updown, flags, vict);
}
void	P_SpawnBlood (const DVector3 &pos, DAngle angle, int damage, AActor *originator);
void	P_BloodSplatter (const DVector3 &pos, AActor *originator, DAngle hitangle);
void	P_BloodSplatter2 (const DVector3 &pos, AActor *originator, DAngle hitangle);
void	P_RipperBlood (AActor *mo, AActor *bleeder);
int		P_GetThingFloorType (AActor *thing);
void	P_ExplodeMissile (AActor *missile, line_t *explodeline, AActor *target);

AActor *P_OldSpawnMissile(AActor *source, AActor *owner, AActor *dest, PClassActor *type);
AActor *P_SpawnMissile (AActor* source, AActor* dest, PClassActor *type, AActor* owner = NULL);
AActor *P_SpawnMissileZ (AActor* source, fixed_t z, AActor* dest, PClassActor *type);
inline AActor *P_SpawnMissileZ(AActor* source, double z, AActor* dest, PClassActor *type)
{
	return P_SpawnMissileZ(source, FLOAT2FIXED(z), dest, type);
}

AActor *P_SpawnMissileXYZ (fixed_t x, fixed_t y, fixed_t z, AActor *source, AActor *dest, PClassActor *type, bool checkspawn = true, AActor *owner = NULL);
inline AActor *P_SpawnMissileXYZ(const fixedvec3 &pos, AActor *source, AActor *dest, PClassActor *type, bool checkspawn = true, AActor *owner = NULL)
{
	return P_SpawnMissileXYZ(pos.x, pos.y, pos.z, source, dest, type, checkspawn, owner);
}
inline AActor *P_SpawnMissileXYZ(const DVector3 &pos, AActor *source, AActor *dest, PClassActor *type, bool checkspawn = true, AActor *owner = NULL)
{
	return P_SpawnMissileXYZ(FLOAT2FIXED(pos.X), FLOAT2FIXED(pos.Y), FLOAT2FIXED(pos.Z), source, dest, type, checkspawn, owner);
}
AActor *P_SpawnMissileAngle (AActor *source, PClassActor *type, angle_t angle, fixed_t vz);
inline AActor *P_SpawnMissileAngle(AActor *source, PClassActor *type, DAngle angle, double vz)
{
	return P_SpawnMissileAngle(source, type, angle.BAMs(), FLOAT2FIXED(vz));
}
AActor *P_SpawnMissileAngleSpeed (AActor *source, PClassActor *type, angle_t angle, fixed_t vz, fixed_t speed);
AActor *P_SpawnMissileAngleZ (AActor *source, fixed_t z, PClassActor *type, angle_t angle, fixed_t vz);
inline AActor *P_SpawnMissileAngleZ(AActor *source, double z, PClassActor *type, DAngle angle, double vz)
{
	return P_SpawnMissileAngleZ(source, FLOAT2FIXED(z), type, angle.BAMs(), FLOAT2FIXED(vz));
}
AActor *P_SpawnMissileAngleZSpeed (AActor *source, fixed_t z, PClassActor *type, angle_t angle, fixed_t vz, fixed_t speed, AActor *owner=NULL, bool checkspawn = true);
inline AActor *P_SpawnMissileAngleZSpeed(AActor *source, double z, PClassActor *type, DAngle angle, double vz, double speed, AActor *owner = NULL, bool checkspawn = true)
{
	return P_SpawnMissileAngleZSpeed(source, FLOAT2FIXED(z), type, angle.BAMs(), FLOAT2FIXED(vz), FLOAT2FIXED(speed), owner, checkspawn);
}
AActor *P_SpawnMissileZAimed (AActor *source, fixed_t z, AActor *dest, PClassActor *type);
inline AActor *P_SpawnMissileZAimed(AActor *source, double z, AActor *dest, PClassActor *type)
{
	return P_SpawnMissileZAimed(source, FLOAT2FIXED(z), dest, type);
}

AActor *P_SpawnPlayerMissile (AActor* source, PClassActor *type);
AActor *P_SpawnPlayerMissile (AActor *source, PClassActor *type, DAngle angle);

AActor *P_SpawnPlayerMissile (AActor *source, double x, double y, double z, PClassActor *type, DAngle angle, 
							  FTranslatedLineTarget *pLineTarget = NULL, AActor **MissileActor = NULL, bool nofreeaim = false, bool noautoaim = false, int aimflags = 0);


void P_CheckFakeFloorTriggers (AActor *mo, fixed_t oldz, bool oldz_has_viewheight=false);

AActor *P_SpawnSubMissile (AActor *source, PClassActor *type, AActor *target);	// Strife uses it


//
// [RH] P_THINGS
//
extern FClassMap SpawnableThings;
extern FClassMap StrifeTypes;

bool	P_Thing_Spawn (int tid, AActor *source, int type, DAngle angle, bool fog, int newtid);
bool	P_Thing_Projectile (int tid, AActor *source, int type, const char * type_name, DAngle angle,
			fixed_t speed, fixed_t vspeed, int dest, AActor *forcedest, int gravity, int newtid,
			bool leadTarget);
bool	P_MoveThing(AActor *source, const DVector3 &pos, bool fog);
bool	P_Thing_Move (int tid, AActor *source, int mapspot, bool fog);
int		P_Thing_Damage (int tid, AActor *whofor0, int amount, FName type);
void	P_Thing_SetVelocity(AActor *actor, const DVector3 &vec, bool add, bool setbob);
void P_RemoveThing(AActor * actor);
bool P_Thing_Raise(AActor *thing, AActor *raiser);
bool P_Thing_CanRaise(AActor *thing);
PClassActor *P_GetSpawnableType(int spawnnum);
void InitSpawnablesFromMapinfo();
int P_Thing_Warp(AActor *caller, AActor *reference, fixed_t xofs, fixed_t yofs, fixed_t zofs, angle_t angle, int flags, fixed_t heightoffset, fixed_t radiusoffset, angle_t pitch);

enum WARPF
{
	WARPF_ABSOLUTEOFFSET = 0x1,
	WARPF_ABSOLUTEANGLE  = 0x2,
	WARPF_USECALLERANGLE = 0x4,

	WARPF_NOCHECKPOSITION = 0x8,

	WARPF_INTERPOLATE       = 0x10,
	WARPF_WARPINTERPOLATION = 0x20,
	WARPF_COPYINTERPOLATION = 0x40,

	WARPF_STOP             = 0x80,
	WARPF_TOFLOOR          = 0x100,
	WARPF_TESTONLY         = 0x200,
	WARPF_ABSOLUTEPOSITION = 0x400,
	WARPF_BOB              = 0x800,
	WARPF_MOVEPTR          = 0x1000,
	WARPF_USEPTR           = 0x2000,
	WARPF_USETID           = 0x2000,
	WARPF_COPYVELOCITY		= 0x4000,
	WARPF_COPYPITCH			= 0x8000,
};




AActor *P_BlockmapSearch (AActor *mo, int distance, AActor *(*check)(AActor*, int, void *), void *params = NULL);
AActor *P_RoughMonsterSearch (AActor *mo, int distance, bool onlyseekable=false);

//
// P_MAP
//


// If "floatok" true, move would be ok
// if within "tmfloorz - tm_f_ceilingz()".
extern msecnode_t		*sector_list;		// phares 3/16/98

struct spechit_t
{
	line_t *line;
	fixedvec2 oldrefpos;
	fixedvec2 refpos;
};

extern TArray<spechit_t> spechit;
extern TArray<spechit_t> portalhit;


bool	P_TestMobjLocation (AActor *mobj);
bool	P_TestMobjZ (AActor *mobj, bool quick=true, AActor **pOnmobj = NULL);
bool	P_CheckPosition (AActor *thing, fixed_t x, fixed_t y, FCheckPosition &tm, bool actorsonly=false);
bool	P_CheckPosition (AActor *thing, fixed_t x, fixed_t y, bool actorsonly=false);
inline bool P_CheckPosition(AActor *thing, const fixedvec3 &pos, bool actorsonly = false)
{
	return P_CheckPosition(thing, pos.x, pos.y, actorsonly);
}
inline bool P_CheckPosition(AActor *thing, const DVector3 &pos, bool actorsonly = false)
{
	return P_CheckPosition(thing, FLOAT2FIXED(pos.X), FLOAT2FIXED(pos.Y), actorsonly);
}
AActor	*P_CheckOnmobj (AActor *thing);
void	P_FakeZMovement (AActor *mo);
bool	P_TryMove (AActor* thing, fixed_t x, fixed_t y, int dropoff, const secplane_t * onfloor, FCheckPosition &tm, bool missileCheck = false);
bool	P_TryMove (AActor* thing, fixed_t x, fixed_t y, int dropoff, const secplane_t * onfloor = NULL);
inline bool	P_TryMove(AActor* thing, double x, double y, int dropoff, const secplane_t * onfloor = NULL)
{
	return P_TryMove(thing, FLOAT2FIXED(x), FLOAT2FIXED(y), dropoff, onfloor);
}
inline bool	P_TryMove(AActor* thing, const DVector2 &pos, int dropoff, const secplane_t * onfloor = NULL)
{
	return P_TryMove(thing, FLOAT2FIXED(pos.X), FLOAT2FIXED(pos.Y), dropoff, onfloor);
}
inline bool	P_TryMove(AActor* thing, const DVector2 &pos, int dropoff, const secplane_t * onfloor, FCheckPosition &tm, bool missileCheck = false)
{
	return P_TryMove(thing, FLOAT2FIXED(pos.X), FLOAT2FIXED(pos.Y), dropoff, onfloor, tm, missileCheck);
}
bool	P_CheckMove(AActor *thing, fixed_t x, fixed_t y);
void	P_ApplyTorque(AActor *mo);
bool	P_TeleportMove (AActor* thing, fixed_t x, fixed_t y, fixed_t z, bool telefrag, bool modifyactor = true);	// [RH] Added z and telefrag parameters
inline bool	P_TeleportMove(AActor* thing, const fixedvec3 &pos, bool telefrag, bool modifyactor = true)
{
	return P_TeleportMove(thing, pos.x, pos.y, pos.z, telefrag, modifyactor);
}
inline bool	P_TeleportMove(AActor* thing, const DVector3 &pos, bool telefrag, bool modifyactor = true)
{
	return P_TeleportMove(thing, FLOAT2FIXED(pos.X), FLOAT2FIXED(pos.Y), FLOAT2FIXED(pos.Z), telefrag, modifyactor);
}
void	P_PlayerStartStomp (AActor *actor, bool mononly=false);		// [RH] Stomp on things for a newly spawned player
void	P_SlideMove (AActor* mo, fixed_t tryx, fixed_t tryy, int numsteps);
bool	P_BounceWall (AActor *mo);
bool	P_BounceActor (AActor *mo, AActor *BlockingMobj, bool ontop);
bool	P_CheckSight (AActor *t1, AActor *t2, int flags=0);

enum ESightFlags
{
	SF_IGNOREVISIBILITY=1,
	SF_SEEPASTSHOOTABLELINES=2,
	SF_SEEPASTBLOCKEVERYTHING=4,
	SF_IGNOREWATERBOUNDARY=8
};

void	P_ResetSightCounters (bool full);
bool	P_TalkFacing (AActor *player);
void	P_UseLines (player_t* player);
bool	P_UsePuzzleItem (AActor *actor, int itemType);

enum
{
	FFCF_ONLYSPAWNPOS = 1,
	FFCF_SAMESECTOR = 2,
	FFCF_ONLY3DFLOORS = 4,	// includes 3D midtexes
	FFCF_3DRESTRICT = 8,	// ignore 3D midtexes and floors whose floorz are above thing's z
	FFCF_NOPORTALS = 16,	// ignore portals (considers them impassable.)
	FFCF_NOFLOOR = 32,
	FFCF_NOCEILING = 64,
	FFCF_RESTRICTEDPORTAL = 128,	// current values in the iterator's return are through a restricted portal type (i.e. some features are blocked.)
	FFCF_NODROPOFF = 256,			// Caller does not need a dropoff (saves some time when checking portals)
};
void	P_FindFloorCeiling (AActor *actor, int flags=0);

bool	P_ChangeSector (sector_t* sector, int crunch, int amt, int floorOrCeil, bool isreset);

DAngle P_AimLineAttack(AActor *t1, DAngle angle, double distance, FTranslatedLineTarget *pLineTarget = NULL, DAngle vrange = 0., int flags = 0, AActor *target = NULL, AActor *friender = NULL);

enum	// P_AimLineAttack flags
{
	ALF_FORCENOSMART = 1,
	ALF_CHECK3D = 2,
	ALF_CHECKNONSHOOTABLE = 4,
	ALF_CHECKCONVERSATION = 8,
	ALF_NOFRIENDS = 16,
	ALF_PORTALRESTRICT = 32,	// only work through portals with a global offset (to be used for stuff that cannot remember the calculated FTranslatedLineTarget info)
};

enum	// P_LineAttack flags
{
	LAF_ISMELEEATTACK = 1,
	LAF_NORANDOMPUFFZ = 2,
	LAF_NOIMPACTDECAL = 4
};

AActor *P_LineAttack(AActor *t1, DAngle angle, double distance, DAngle pitch, int damage, FName damageType, PClassActor *pufftype, int flags = 0, FTranslatedLineTarget *victim = NULL, int *actualdamage = NULL);
AActor *P_LineAttack(AActor *t1, DAngle angle, double distance, DAngle pitch, int damage, FName damageType, FName pufftype, int flags = 0, FTranslatedLineTarget *victim = NULL, int *actualdamage = NULL);

void	P_TraceBleed (int damage, fixed_t x, fixed_t y, fixed_t z, AActor *target, angle_t angle, int pitch);
inline void	P_TraceBleed(int damage, const fixedvec3 &pos, AActor *target, angle_t angle, int pitch)
{
	P_TraceBleed(damage, pos.x, pos.y, pos.z, target, angle, pitch);
}
void	P_TraceBleed (int damage, AActor *target, angle_t angle, int pitch);
inline void	P_TraceBleed(int damage, AActor *target, DAngle angle, DAngle pitch)
{
	P_TraceBleed(damage, target, angle.BAMs(), pitch.BAMs());
}
void	P_TraceBleed (int damage, AActor *target, AActor *missile);		// missile version
void	P_TraceBleed(int damage, FTranslatedLineTarget *t, AActor *puff);		// hitscan version
void	P_TraceBleed (int damage, AActor *target);		// random direction version
bool	P_HitFloor (AActor *thing);
bool	P_HitWater (AActor *thing, sector_t *sec, const DVector3 &pos, bool checkabove = false, bool alert = true, bool force = false);
inline bool	P_HitWater(AActor *thing, sector_t *sec, const fixedvec3 &pos, bool checkabove = false, bool alert = true, bool force = false)
{
	DVector3 fpos(FIXED2DBL(pos.x), FIXED2DBL(pos.y), FIXED2DBL(pos.z));
	return P_HitWater(thing, sec, fpos, checkabove, alert, force);
}
void	P_CheckSplash(AActor *self, double distance);
void	P_RailAttack (AActor *source, int damage, int offset_xy, fixed_t offset_z = 0, int color1 = 0, int color2 = 0, double maxdiff = 0, int flags = 0, PClassActor *puff = NULL, angle_t angleoffset = 0, angle_t pitchoffset = 0, fixed_t distance = 8192*FRACUNIT, int duration = 0, double sparsity = 1.0, double drift = 1.0, PClassActor *spawnclass = NULL, int SpiralOffset = 270);	// [RH] Shoot a railgun

enum	// P_RailAttack / A_RailAttack / A_CustomRailgun / P_DrawRailTrail flags
{	
	RAF_SILENT = 1,
	RAF_NOPIERCE = 2,
	RAF_EXPLICITANGLE = 4,
	RAF_FULLBRIGHT = 8,
	RAF_CENTERZ = 16,
};


bool	P_CheckMissileSpawn(AActor *missile, double maxdist);

void	P_PlaySpawnSound(AActor *missile, AActor *spawner);

// [RH] Position the chasecam
void	P_AimCamera (AActor *t1, fixed_t &x, fixed_t &y, fixed_t &z, sector_t *&sec, bool &unlinked);

// [RH] Means of death
enum
{
	RADF_HURTSOURCE = 1,
	RADF_NOIMPACTDAMAGE = 2,
	RADF_SOURCEISSPOT = 4,
	RADF_NODAMAGE = 8,
};
void	P_RadiusAttack (AActor *spot, AActor *source, int damage, int distance, 
						FName damageType, int flags, int fulldamagedistance=0);

void	P_DelSector_List();
void	P_DelSeclist(msecnode_t *);							// phares 3/16/98
msecnode_t*	P_DelSecnode(msecnode_t *);
void	P_CreateSecNodeList(AActor*,fixed_t,fixed_t);		// phares 3/14/98
int		P_GetMoveFactor(const AActor *mo, int *frictionp);	// phares  3/6/98
inline double 	P_GetMoveFactor(const AActor *mo, double *frictionp)
{
	int rv, fp;
	rv = P_GetMoveFactor(mo, &fp);
	*frictionp = FIXED2DBL(fp);
	return FIXED2DBL(rv);
}

int		P_GetFriction(const AActor *mo, int *frictionfactor);
bool	Check_Sides(AActor *, int, int);					// phares

// [RH] 
const secplane_t * P_CheckSlopeWalk (AActor *actor, fixed_t &xmove, fixed_t &ymove);

//
// P_SETUP
//
extern BYTE*			rejectmatrix;	// for fast sight rejection



//
// P_INTER
//
void P_TouchSpecialThing (AActor *special, AActor *toucher);
int  P_DamageMobj (AActor *target, AActor *inflictor, AActor *source, int damage, FName mod, int flags=0, DAngle angle = 0.);
void P_PoisonMobj (AActor *target, AActor *inflictor, AActor *source, int damage, int duration, int period, FName type);
bool P_GiveBody (AActor *actor, int num, int max=0);
bool P_PoisonPlayer (player_t *player, AActor *poisoner, AActor *source, int poison);
void P_PoisonDamage (player_t *player, AActor *source, int damage, bool playPainSound);

enum EDmgFlags
{
	DMG_NO_ARMOR = 1,
	DMG_INFLICTOR_IS_PUFF = 2,
	DMG_THRUSTLESS = 4,
	DMG_FORCED = 8,
	DMG_NO_FACTOR = 16,
	DMG_PLAYERATTACK = 32,
	DMG_FOILINVUL = 64,
	DMG_FOILBUDDHA = 128,
	DMG_NO_PROTECT = 256,
	DMG_USEANGLE = 512,
};


//
// P_SPEC
//
bool P_AlignFlat (int linenum, int side, int fc);

#endif	// __P_LOCAL__
