#ifndef __A_HEXENGLOBAL_H__
#define __A_HEXENGLOBAL_H__

#include "d_player.h"

class ALightning : public AActor
{
	DECLARE_CLASS (ALightning, AActor)
public:
	int SpecialMissileHit (AActor *victim);
};

class APoisonCloud : public AActor
{
	DECLARE_ACTOR (APoisonCloud, AActor)
public:
	void GetExplodeParms (int &damage, int &distance, bool &hurtSource);
	int DoSpecialDamage (AActor *target, int damage);
	void BeginPlay ();
};

class AHeresiarch : public AActor
{
	DECLARE_ACTOR (AHeresiarch, AActor)
public:
	const PClass *StopBall;

	void Serialize (FArchive &arc);
	void Die (AActor *source, AActor *inflictor);
};

class AHolySpirit : public AActor
{
	DECLARE_CLASS (AHolySpirit, AActor)
public:
	bool Slam (AActor *thing);
	bool SpecialBlastHandling (AActor *source, fixed_t strength);
	bool IsOkayToAttack (AActor *link);
};

class AFighterWeapon : public AWeapon
{
	DECLARE_STATELESS_ACTOR (AFighterWeapon, AWeapon);
public:
	bool TryPickup (AActor *toucher);
};

class AClericWeapon : public AWeapon
{
	DECLARE_STATELESS_ACTOR (AClericWeapon, AWeapon);
public:
	bool TryPickup (AActor *toucher);
};

class AMageWeapon : public AWeapon
{
	DECLARE_STATELESS_ACTOR (AMageWeapon, AWeapon);
public:
	bool TryPickup (AActor *toucher);
};

class ASwitchableDecoration : public AActor
{
	DECLARE_STATELESS_ACTOR (ASwitchableDecoration, AActor)
public:
	void Activate (AActor *activator);
	void Deactivate (AActor *activator);
};

#endif //__A_HEXENGLOBAL_H__
