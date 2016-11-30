#pragma once

#include "a_pickups.h"

// Health is some item that gives the player health when picked up.
class PClassHealth : public PClassInventory
{
	DECLARE_CLASS(PClassHealth, PClassInventory)
protected:
public:
	PClassHealth();
	virtual void DeriveData(PClass *newclass);

	FString LowHealthMessage;
	int LowHealth;
};

class AHealth : public AInventory
{
	DECLARE_CLASS_WITH_META(AHealth, AInventory, PClassHealth)

public:
	int PrevHealth;
	virtual bool TryPickup (AActor *&other) override;
	virtual FString PickupMessage () override;
};

// HealthPickup is some item that gives the player health when used.
class AHealthPickup : public AInventory
{
	DECLARE_CLASS (AHealthPickup, AInventory)
public:
	int autousemode;

	
	virtual void Serialize(FSerializer &arc) override;
	virtual AInventory *CreateCopy (AActor *other) override;
	virtual AInventory *CreateTossable () override;
	virtual bool HandlePickup (AInventory *item) override;
	virtual bool Use (bool pickup) override;
};

