#ifndef __A_STRIFEGLOBAL_H__
#define __A_STRIFEGLOBAL_H__

#include "info.h"
#include "a_pickups.h"

// Base class for every humanoid in Strife that can go into
// a fire or electric death.
class ADegninOre : public AInventory
{
	DECLARE_CLASS (ADegninOre, AInventory)
public:
	bool Use (bool pickup);
};

class ACoin : public AInventory
{
	DECLARE_CLASS (ACoin, AInventory)
public:
	const char *PickupMessage ();
	bool HandlePickup (AInventory *item);
	AInventory *CreateTossable ();
	AInventory *CreateCopy (AActor *other);
};

class ADummyStrifeItem : public AInventory
{
	DECLARE_CLASS (ADummyStrifeItem, AInventory)
};

class AUpgradeStamina : public ADummyStrifeItem
{
	DECLARE_CLASS (AUpgradeStamina, ADummyStrifeItem)
public:
	bool TryPickup (AActor *&toucher);
};

class AUpgradeAccuracy : public ADummyStrifeItem
{
	DECLARE_CLASS (AUpgradeAccuracy, ADummyStrifeItem)
public:
	bool TryPickup (AActor *&toucher);
};

class ASlideshowStarter : public ADummyStrifeItem
{
	DECLARE_CLASS (ASlideshowStarter, ADummyStrifeItem)
public:
	bool TryPickup (AActor *&toucher);
};

class ASigil : public AWeapon
{
	DECLARE_CLASS (ASigil, AWeapon)
public:
	bool HandlePickup (AInventory *item);
	AInventory *CreateCopy (AActor *other);
	void Serialize (FArchive &arc);
	bool SpecialDropAction (AActor *dropper);
	static int GiveSigilPiece (AActor *daPlayer);
	void BeginPlay();

	int NumPieces, DownPieces;
};

extern const PClass *QuestItemClasses[31];

#endif
