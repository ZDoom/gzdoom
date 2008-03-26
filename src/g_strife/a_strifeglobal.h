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

// Sigil/Spectral projectiles -----------------------------------------------

class ASpectralLightningBase : public AActor
{
	DECLARE_ACTOR (ASpectralLightningBase, AActor)
public:
	void GetExplodeParms (int &damage, int &dist, bool &hurtSource);
};

class ASpectralLightningDeath1 : public ASpectralLightningBase
{
	DECLARE_STATELESS_ACTOR (ASpectralLightningDeath1, ASpectralLightningBase)
};

class ASpectralLightningDeath2 : public ASpectralLightningBase
{
	DECLARE_STATELESS_ACTOR (ASpectralLightningDeath2, ASpectralLightningBase)
};

class ASpectralLightningDeathShort : public ASpectralLightningBase
{
	DECLARE_STATELESS_ACTOR (ASpectralLightningDeathShort, ASpectralLightningBase)
};

class ASpectralLightningBall1 : public ASpectralLightningBase
{
	DECLARE_ACTOR (ASpectralLightningBall1, ASpectralLightningBase)
};

class ASpectralLightningBall2 : public ASpectralLightningBall1
{
	DECLARE_STATELESS_ACTOR (ASpectralLightningBall2, ASpectralLightningBall1)
};

class ASpectralLightningH1 : public ASpectralLightningBase
{
	DECLARE_ACTOR (ASpectralLightningH1, ASpectralLightningBase)
};

class ASpectralLightningH2 : public ASpectralLightningH1
{
	DECLARE_STATELESS_ACTOR (ASpectralLightningH2, ASpectralLightningH1)
};

class ASpectralLightningH3 : public ASpectralLightningH1
{
	DECLARE_STATELESS_ACTOR (ASpectralLightningH3, ASpectralLightningH1)
};

class ASpectralLightningHTail : public AActor
{
	DECLARE_ACTOR (ASpectralLightningHTail, AActor)
};

class ASpectralLightningBigBall1 : public ASpectralLightningDeath2
{
	DECLARE_ACTOR (ASpectralLightningBigBall1, ASpectralLightningDeath2)
};

class ASpectralLightningBigBall2 : public ASpectralLightningBigBall1
{
	DECLARE_STATELESS_ACTOR (ASpectralLightningBigBall2, ASpectralLightningBigBall1)
};

class ASpectralLightningV1 : public ASpectralLightningDeathShort
{
	DECLARE_ACTOR (ASpectralLightningV1, ASpectralLightningDeathShort)
};

class ASpectralLightningV2 : public ASpectralLightningV1
{
	DECLARE_STATELESS_ACTOR (ASpectralLightningV2, ASpectralLightningV1)
};

class ASpectralLightningSpot : public ASpectralLightningDeath1
{
	DECLARE_ACTOR (ASpectralLightningSpot, ASpectralLightningDeath1)
};

class ASpectralLightningBigV1 : public ASpectralLightningDeath1
{
	DECLARE_ACTOR (ASpectralLightningBigV1, ASpectralLightningDeath1)
};

class ASpectralLightningBigV2 : public ASpectralLightningBigV1
{
	DECLARE_STATELESS_ACTOR (ASpectralLightningBigV2, ASpectralLightningBigV1)
};


#endif
