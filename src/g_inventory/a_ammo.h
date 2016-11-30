#pragma once
#include "a_pickups.h"

// Ammo: Something a weapon needs to operate
class PClassAmmo : public PClassInventory
{
	DECLARE_CLASS(PClassAmmo, PClassInventory)
protected:
	virtual void DeriveData(PClass *newclass);
public:
	PClassAmmo();

	int DropAmount;			// Specifies the amount for a dropped ammo item.
};

class AAmmo : public AInventory
{
	DECLARE_CLASS_WITH_META(AAmmo, AInventory, PClassAmmo)
public:
	
	virtual void Serialize(FSerializer &arc) override;
	virtual AInventory *CreateCopy (AActor *other) override;
	virtual bool HandlePickup (AInventory *item) override;
	virtual AInventory *CreateTossable () override;
	PClassActor *GetParentAmmo () const;

	int BackpackAmount, BackpackMaxAmount;
};


// A backpack gives you one clip of each ammo and doubles your
// normal maximum ammo amounts.
class ABackpackItem : public AInventory
{
	DECLARE_CLASS (ABackpackItem, AInventory)
public:
	
	virtual void Serialize(FSerializer &arc) override;
	virtual bool HandlePickup (AInventory *item) override;
	virtual AInventory *CreateCopy (AActor *other) override;
	virtual AInventory *CreateTossable () override;
	virtual void DetachFromOwner () override;

	bool bDepleted;
};


