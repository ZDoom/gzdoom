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

#endif //__A_DOOMGLOBAL_H__
