#pragma once

#include "a_pickups.h"

// HealthPickup is some item that gives the player health when used.
class AHealthPickup : public AInventory
{
	DECLARE_CLASS (AHealthPickup, AInventory)
public:
	int autousemode;

	virtual void Serialize(FSerializer &arc) override;
};

