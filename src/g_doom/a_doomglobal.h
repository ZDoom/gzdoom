#ifndef __A_DOOMGLOBAL_H__
#define __A_DOOMGLOBAL_H__

#include "dobject.h"
#include "info.h"

class ABossBrain : public AActor
{
	DECLARE_ACTOR (ABossBrain, AActor)
};

class AExplosiveBarrel : public AActor
{
	DECLARE_ACTOR (AExplosiveBarrel, AActor)
public:
	int GetMOD () { return MOD_BARREL; }
};

class ABulletPuff : public AActor
{
	DECLARE_ACTOR (ABulletPuff, AActor)
public:
	void BeginPlay ();
};

class ARocket : public AActor
{
	DECLARE_ACTOR (ARocket, AActor)
public:
	void BeginPlay ();
	int GetMOD () { return MOD_ROCKET; }
};

class AArchvile : public AActor
{
	DECLARE_ACTOR (AArchvile, AActor)
public:
	bool SuggestMissileAttack (fixed_t dist);
	const char *GetObituary ();
};

class APainElemental : public AActor
{
	DECLARE_ACTOR (APainElemental, AActor)
public:
	void Tick ();
};

class ALostSoul : public AActor
{
	DECLARE_ACTOR (ALostSoul, AActor)
public:
	bool SuggestMissileAttack (fixed_t dist);
	void Die (AActor *source, AActor *inflictor);
	const char *GetObituary ();
};

class APlasmaBall : public AActor
{
	DECLARE_ACTOR (APlasmaBall, AActor)
	int GetMOD () { return MOD_PLASMARIFLE; }
};

class ABFGBall : public AActor
{
	DECLARE_ACTOR (ABFGBall, AActor)
public:
	int GetMOD () { return MOD_BFG_BOOM; }
};

class AScriptedMarine : public AActor
{
	DECLARE_ACTOR (AScriptedMarine, AActor)
public:
	enum EMarineWeapon
	{
		WEAPON_Dummy,
		WEAPON_Fist,
		WEAPON_BerserkFist,
		WEAPON_Chainsaw,
		WEAPON_Pistol,
		WEAPON_Shotgun,
		WEAPON_SuperShotgun,
		WEAPON_Chaingun,
		WEAPON_RocketLauncher,
		WEAPON_PlasmaRifle,
		WEAPON_Railgun,
		WEAPON_BFG
	};

	void Activate (AActor *activator);
	void Deactivate (AActor *activator);
	void Tick ();
	void SetWeapon (EMarineWeapon);
};

#endif //__A_DOOMGLOBAL_H__
