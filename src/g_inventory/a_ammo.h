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
	
	void Serialize(FSerializer &arc);
	AInventory *CreateCopy (AActor *other);
	bool HandlePickup (AInventory *item);
	PClassActor *GetParentAmmo () const;
	AInventory *CreateTossable ();

	int BackpackAmount, BackpackMaxAmount;
};


// A backpack gives you one clip of each ammo and doubles your
// normal maximum ammo amounts.
class ABackpackItem : public AInventory
{
	DECLARE_CLASS (ABackpackItem, AInventory)
public:
	
	void Serialize(FSerializer &arc);
	bool HandlePickup (AInventory *item);
	AInventory *CreateCopy (AActor *other);
	AInventory *CreateTossable ();
	void DetachFromOwner ();

	bool bDepleted;
};


