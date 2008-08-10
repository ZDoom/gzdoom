#ifndef __P_ENEMY_H__
#define __P_ENEMY_H__

#include "r_defs.h"

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

extern fixed_t xspeed[8], yspeed[8];

void P_RecursiveSound (sector_t *sec, AActor *soundtarget, bool splash, int soundblocks);
bool P_HitFriend (AActor *self);
void P_NoiseAlert (AActor *target, AActor *emmiter, bool splash=false);
bool P_CheckMeleeRange2 (AActor *actor);
bool P_Move (AActor *actor);
bool P_TryWalk (AActor *actor);
void P_NewChaseDir (AActor *actor);
bool P_LookForPlayers (AActor *actor, INTBOOL allaround);
AInventory *P_DropItem (AActor *source, const PClass *type, int special, int chance);
inline AInventory *P_DropItem (AActor *source, const char *type, int special, int chance)
{
	return P_DropItem (source, PClass::FindClass (type), special, chance);
}
void P_TossItem (AActor *item);

void A_Look (AActor *actor);
void A_Wander (AActor *actor);
void A_Look2 (AActor *actor);
void A_Chase (AActor *actor);
void A_FastChase (AActor *actor);
void A_FaceTarget (AActor *actor);
void A_MonsterRail (AActor *actor);
void A_Scream (AActor *actor);
void A_XScream (AActor *actor);
void A_Pain (AActor *actor);
void A_Die (AActor *actor);
void A_Detonate (AActor *mo);
void A_Explode (AActor *thing);
void A_Mushroom (AActor *actor);
void A_BossDeath (AActor *actor);
void A_FireScream (AActor *mo);
void A_PlayerScream (AActor *mo);
void A_ClassBossHealth (AActor *);

bool A_RaiseMobj (AActor *, fixed_t speed);
bool A_SinkMobj (AActor *, fixed_t speed);

bool CheckBossDeath (AActor *);
int P_Massacre ();
bool P_CheckMissileRange (AActor *actor);
void A_LookEx (AActor *actor);

#endif //__P_ENEMY_H__
