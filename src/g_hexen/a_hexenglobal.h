#ifndef __A_HEXENGLOBAL_H__
#define __A_HEXENGLOBAL_H__

class ALightning : public AActor
{
	DECLARE_STATELESS_ACTOR (ALightning, AActor)
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

class ABishop : public AActor
{
	DECLARE_ACTOR (ABishop, AActor)
public:
	void GetExplodeParms (int &damage, int &distance, bool &hurtSource);
	bool NewTarget (AActor *other);
};

class AHolySpirit : public AActor
{
	DECLARE_ACTOR (AHolySpirit, AActor)
public:
	bool Slam (AActor *thing);
	bool SpecialBlastHandling (AActor *source, fixed_t strength);
	bool IsOkayToAttack (AActor *link);
};

class AHammerPuff : public AActor
{
	DECLARE_ACTOR (AHammerPuff, AActor)
public:
	void BeginPlay ();
};

class ACentaur : public AActor
{
	DECLARE_ACTOR (ACentaur, AActor)
public:
	void Howl ();
	bool AdjustReflectionAngle (AActor *thing, angle_t &angle);
};

#endif //__A_HEXENGLOBAL_H__
