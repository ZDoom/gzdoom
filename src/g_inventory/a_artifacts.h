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

// An artifact is an item that gives the player a powerup when activated.
class APowerupGiver : public AInventory
{
	DECLARE_CLASS (APowerupGiver, AInventory)
	HAS_OBJECT_POINTERS
public:
	virtual void Serialize(FSerializer &arc) override;


	PClassActor *PowerupType;
	int EffectTics;			// Non-0 to override the powerup's default tics
	PalEntry BlendColor;	// Non-0 to override the powerup's default blend
	FNameNoInit Mode;		// Meaning depends on powerup - used for Invulnerability and Invisibility
	double Strength;		// Meaning depends on powerup - currently used only by Invisibility
};


#endif //__A_ARTIFACTS_H__
