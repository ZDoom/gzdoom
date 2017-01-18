#pragma once

#include "a_pickups.h"

// Armor absorbs some damage for the player.
class AArmor : public AInventory
{
	DECLARE_CLASS (AArmor, AInventory)
};

// Hexen armor consists of four separate armor types plus a conceptual armor
// type (the player himself) that work together as a single armor.
class AHexenArmor : public AArmor
{
	DECLARE_CLASS (AHexenArmor, AArmor)
public:
	
	virtual void Serialize(FSerializer &arc) override;
	virtual AInventory *CreateCopy (AActor *other) override;
	virtual AInventory *CreateTossable () override;
	virtual bool HandlePickup (AInventory *item) override;
	virtual void AbsorbDamage (int damage, FName damageType, int &newdamage) override;
	virtual void DepleteOrDestroy() override;

	double Slots[5];
	double SlotsIncrement[4];

protected:
	bool AddArmorToSlot (AActor *actor, int slot, int amount);
};

