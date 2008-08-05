#ifndef __A_STRIFEGLOBAL_H__
#define __A_STRIFEGLOBAL_H__

#include "dobject.h"
#include "info.h"
#include "a_pickups.h"

// Base class for every humanoid in Strife that can go into
// a fire or electric death.
class AStrifeHumanoid : public AActor
{
	DECLARE_ACTOR (AStrifeHumanoid, AActor)
};

class AStrifePuff : public AActor
{
	DECLARE_ACTOR (AStrifePuff, AActor)
};

class AFlameMissile : public AActor
{
	DECLARE_ACTOR (AFlameMissile, AActor)
};

class AAlienSpectre1 : public AActor
{
	DECLARE_ACTOR (AAlienSpectre1, AActor)
public:
	void Touch (AActor *toucher);
};

class AAlienSpectre3 : public AAlienSpectre1
{
	DECLARE_ACTOR (AAlienSpectre3, AAlienSpectre1)
public:
	void NoBlockingSet ();
};

class ADegninOre : public AInventory
{
	DECLARE_ACTOR (ADegninOre, AInventory)
public:
	void GetExplodeParms (int &damage, int &dist, bool &hurtSource);
	bool Use (bool pickup);
};

class ACoin : public AInventory
{
	DECLARE_ACTOR (ACoin, AInventory)
public:
	const char *PickupMessage ();
	bool HandlePickup (AInventory *item);
	AInventory *CreateTossable ();
	AInventory *CreateCopy (AActor *other);
};

class AOracle : public AActor
{
	DECLARE_ACTOR (AOracle, AActor)
public:
	void NoBlockingSet ();
	int TakeSpecialDamage (AActor *inflictor, AActor *source, int damage, FName damagetype);
};

class ADummyStrifeItem : public AInventory
{
	DECLARE_ACTOR (ADummyStrifeItem, AInventory)
};

class AUpgradeStamina : public ADummyStrifeItem
{
	DECLARE_STATELESS_ACTOR (AUpgradeStamina, ADummyStrifeItem)
public:
	bool TryPickup (AActor *toucher);
};

class AUpgradeAccuracy : public ADummyStrifeItem
{
	DECLARE_STATELESS_ACTOR (AUpgradeAccuracy, ADummyStrifeItem)
public:
	bool TryPickup (AActor *toucher);
};

class ASlideshowStarter : public ADummyStrifeItem
{
	DECLARE_STATELESS_ACTOR (ASlideshowStarter, ADummyStrifeItem)
public:
	bool TryPickup (AActor *toucher);
};

class AStrifeWeapon : public AWeapon
{
	DECLARE_STATELESS_ACTOR (AStrifeWeapon, AWeapon)
};

class AFlameThrower : public AStrifeWeapon
{
	DECLARE_ACTOR (AFlameThrower, AStrifeWeapon)
};

class ASigil : public AStrifeWeapon
{
	DECLARE_ACTOR (ASigil, AStrifeWeapon)
public:
	bool HandlePickup (AInventory *item);
	AInventory *CreateCopy (AActor *other);
	void Serialize (FArchive &arc);
	bool SpecialDropAction (AActor *dropper);
	static int GiveSigilPiece (AActor *daPlayer);

	int NumPieces, DownPieces;
};

extern const PClass *QuestItemClasses[31];

#endif
