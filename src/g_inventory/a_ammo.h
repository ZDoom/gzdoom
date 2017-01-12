#pragma once
#include "a_pickups.h"

class AAmmo : public AInventory
{
	DECLARE_CLASS(AAmmo, AInventory)
public:
	
	virtual void Serialize(FSerializer &arc) override;
	virtual AInventory *CreateCopy (AActor *other) override;
	virtual bool HandlePickup (AInventory *item) override;
	virtual AInventory *CreateTossable () override;
	PClassActor *GetParentAmmo () const;

	int BackpackAmount, BackpackMaxAmount, DropAmount;
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


