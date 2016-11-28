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


extern PClassActor *QuestItemClasses[31];

#endif
