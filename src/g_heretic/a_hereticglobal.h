#ifndef __A_HERETICGLOBAL_H__
#define __A_HERETICGLOBAL_H__

#include "info.h"
#include "a_pickups.h"

class AHereticWeapon : public AWeapon
{
	DECLARE_STATELESS_ACTOR (AHereticWeapon, AWeapon)
};

class APhoenixFX1 : public AActor
{
	DECLARE_ACTOR (APhoenixFX1, AActor)
public:
	int DoSpecialDamage (AActor *target, int damage);
};

class APhoenixPuff : public AActor
{
	DECLARE_ACTOR (APhoenixPuff, AActor)
};

void P_DSparilTeleport (AActor *actor);

#endif //__A_HERETICGLOBAL_H__
