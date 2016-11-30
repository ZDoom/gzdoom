#pragma once

#include "a_pickups.h"

// Armor absorbs some damage for the player.
class AArmor : public AInventory
{
	DECLARE_CLASS (AArmor, AInventory)
};

// Basic armor absorbs a specific percent of the damage. You should
// never pickup a BasicArmor. Instead, you pickup a BasicArmorPickup
// or BasicArmorBonus and those gives you BasicArmor when it activates.
class ABasicArmor : public AArmor
{
	DECLARE_CLASS (ABasicArmor, AArmor)
public:
	
	virtual void Serialize(FSerializer &arc) override;
	virtual void Tick () override;
	virtual AInventory *CreateCopy (AActor *other) override;
	virtual bool HandlePickup (AInventory *item) override;
	virtual void AbsorbDamage (int damage, FName damageType, int &newdamage) override;

	int AbsorbCount;
	double SavePercent;
	int MaxAbsorb;
	int MaxFullAbsorb;
	int BonusCount;
	FNameNoInit ArmorType;
	int ActualSaveAmount;
};

// BasicArmorPickup replaces the armor you have.
class ABasicArmorPickup : public AArmor
{
	DECLARE_CLASS (ABasicArmorPickup, AArmor)
public:
	
	virtual void Serialize(FSerializer &arc) override;
	virtual AInventory *CreateCopy (AActor *other) override;
	virtual bool Use (bool pickup) override;

	double SavePercent;
	int MaxAbsorb;
	int MaxFullAbsorb;
	int SaveAmount;
};

// BasicArmorBonus adds to the armor you have.
class ABasicArmorBonus : public AArmor
{
	DECLARE_CLASS (ABasicArmorBonus, AArmor)
public:
	
	virtual void Serialize(FSerializer &arc) override;
	virtual AInventory *CreateCopy (AActor *other) override;
	virtual bool Use (bool pickup) override;

	double SavePercent;	// The default, for when you don't already have armor
	int MaxSaveAmount;
	int MaxAbsorb;
	int MaxFullAbsorb;
	int SaveAmount;
	int BonusCount;
	int BonusMax;
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

