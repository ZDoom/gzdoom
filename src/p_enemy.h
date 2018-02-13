#ifndef __P_ENEMY_H__
#define __P_ENEMY_H__

#include "dobject.h"
#include "vectors.h"

struct sector_t;
class AActor;
class AInventory;
class PClass;


enum dirtype_t
{
	DI_EAST,
	DI_NORTHEAST,
	DI_NORTH,
	DI_NORTHWEST,
	DI_WEST,
	DI_SOUTHWEST,
	DI_SOUTH,
	DI_SOUTHEAST,
	DI_NODIR,
	NUMDIRS
};

extern double xspeed[8], yspeed[8];

enum LO_Flags
{
	LOF_NOSIGHTCHECK = 1,
	LOF_NOSOUNDCHECK = 2,
	LOF_DONTCHASEGOAL = 4,
	LOF_NOSEESOUND = 8,
	LOF_FULLVOLSEESOUND = 16,
    LOF_NOJUMP = 32,
};

struct FLookExParams
{
	DAngle Fov;
	double minDist;
	double maxDist;
	double maxHeardist;
	int flags;
	FState *seestate;
};

void P_DaggerAlert (AActor *target, AActor *emitter);
bool P_HitFriend (AActor *self);
void P_NoiseAlert (AActor *target, AActor *emmiter, bool splash=false, double maxdist=0);

bool P_CheckMeleeRange2 (AActor *actor);
bool P_Move (AActor *actor);
bool P_TryWalk (AActor *actor);
void P_NewChaseDir (AActor *actor);
AInventory *P_DropItem (AActor *source, PClassActor *type, int special, int chance);
void P_TossItem (AActor *item);
bool P_LookForPlayers (AActor *actor, INTBOOL allaround, FLookExParams *params);
void A_Weave(AActor *self, int xyspeed, int zspeed, double xydist, double zdist);
void A_Unblock(AActor *self, bool drop);

void A_BossDeath(AActor *self);

void A_Wander(AActor *self, int flags = 0);
void A_Chase(AActor *self);
void A_FaceTarget(AActor *actor);
void A_Face(AActor *self, AActor *other, DAngle max_turn = 0., DAngle max_pitch = 270., DAngle ang_offset = 0., DAngle pitch_offset = 0., int flags = 0, double z_add = 0);

bool CheckBossDeath (AActor *);
int P_Massacre (bool baddies = false);
bool P_CheckMissileRange (AActor *actor);

#define SKULLSPEED (20.)
void A_SkullAttack(AActor *self, double speed);

#endif //__P_ENEMY_H__
