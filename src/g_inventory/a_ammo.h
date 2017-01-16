#pragma once
#include "a_pickups.h"

class AAmmo : public AInventory
{
	DECLARE_CLASS (AAmmo, AInventory)
public:
	
	virtual void Serialize(FSerializer &arc) override;
	PClassActor *GetParentAmmo () const;

	int BackpackAmount, BackpackMaxAmount, DropAmount;
};

