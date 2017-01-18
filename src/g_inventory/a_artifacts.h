#ifndef __A_ARTIFACTS_H__
#define __A_ARTIFACTS_H__

#include "a_pickups.h"

class player_t;

// A powerup is a pseudo-inventory item that applies an effect to its
// owner while it is present.
class APowerup : public AInventory
{
	DECLARE_CLASS (APowerup, AInventory)
public:
	virtual void Serialize(FSerializer &arc) override;

	int EffectTics;
	PalEntry BlendColor;
	FNameNoInit Mode;
	double Strength;
	int Colormap;

public:
	friend void EndAllPowerupEffects(AInventory *item);
	friend void InitAllPowerupEffects(AInventory *item);
};
#endif //__A_ARTIFACTS_H__
