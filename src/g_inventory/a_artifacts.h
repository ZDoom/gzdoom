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
	virtual void Tick () override;
	virtual void OnDestroy() override;
	virtual bool HandlePickup (AInventory *item) override;
	virtual AInventory *CreateCopy (AActor *other) override;
	virtual AInventory *CreateTossable () override;
	virtual void Serialize(FSerializer &arc) override;
	virtual void OwnerDied () override;
	virtual PalEntry GetBlend () override;
	virtual bool DrawPowerup (int x, int y) override;

	int EffectTics;
	PalEntry BlendColor;
	FNameNoInit Mode;
	double Strength;

public:
	virtual void InitEffect ();
	virtual void DoEffect () override;
	virtual void EndEffect ();
	bool isBlinking() const;

protected:
	void CallInitEffect();
	void CallEndEffect();

	friend void EndAllPowerupEffects(AInventory *item);
	friend void InitAllPowerupEffects(AInventory *item);
};

// An artifact is an item that gives the player a powerup when activated.
class APowerupGiver : public AInventory
{
	DECLARE_CLASS (APowerupGiver, AInventory)
	HAS_OBJECT_POINTERS
public:
	virtual bool Use (bool pickup) override;
	virtual void Serialize(FSerializer &arc) override;


	PClassActor *PowerupType;
	int EffectTics;			// Non-0 to override the powerup's default tics
	PalEntry BlendColor;	// Non-0 to override the powerup's default blend
	FNameNoInit Mode;		// Meaning depends on powerup - used for Invulnerability and Invisibility
	double Strength;		// Meaning depends on powerup - currently used only by Invisibility
};

class APowerInvulnerable : public APowerup
{
	DECLARE_CLASS (APowerInvulnerable, APowerup)
protected:
	virtual void InitEffect () override;
	virtual void DoEffect () override;
	virtual void EndEffect () override;
	virtual int AlterWeaponSprite (visstyle_t *vis) override;
};

class APowerStrength : public APowerup
{
	DECLARE_CLASS (APowerStrength, APowerup)
public:
	PalEntry GetBlend ();
protected:
	virtual void InitEffect () override;
	virtual void Tick () override;
	virtual bool HandlePickup (AInventory *item) override;
};

class APowerInvisibility : public APowerup
{
	DECLARE_CLASS (APowerInvisibility, APowerup)
protected:
	virtual bool HandlePickup (AInventory *item) override;
	virtual void InitEffect () override;
	virtual void DoEffect () override;
	virtual void EndEffect () override;
	virtual int AlterWeaponSprite (visstyle_t *vis) override;
};

class APowerIronFeet : public APowerup
{
	DECLARE_CLASS (APowerIronFeet, APowerup)
public:
	virtual void AbsorbDamage (int damage, FName damageType, int &newdamage) override;
	virtual void DoEffect () override;
};

class APowerMask : public APowerIronFeet
{
	DECLARE_CLASS (APowerMask, APowerIronFeet)
public:
	virtual void AbsorbDamage (int damage, FName damageType, int &newdamage) override;
	virtual void DoEffect () override;
};

class APowerSpeed : public APowerup
{
	DECLARE_CLASS (APowerSpeed, APowerup)
protected:
	virtual void Serialize(FSerializer &arc) override;
public:
	int SpeedFlags;
};

#define PSF_NOTRAIL		1

class APowerMorph : public APowerup
{
	DECLARE_CLASS( APowerMorph, APowerup )
	HAS_OBJECT_POINTERS
public:
	
	virtual void Serialize(FSerializer &arc) override;

	// Variables
	PClassPlayerPawn *PlayerClass;
	PClassActor *MorphFlash, *UnMorphFlash;
	int MorphStyle;
	player_t *MorphedPlayer;
};

#endif //__A_ARTIFACTS_H__
