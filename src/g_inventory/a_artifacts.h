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
	virtual void Destroy () override;
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

protected:
	virtual void InitEffect ();
	virtual void DoEffect () override;
	virtual void EndEffect ();

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

class APowerLightAmp : public APowerup
{
	DECLARE_CLASS (APowerLightAmp, APowerup)
protected:
	virtual void DoEffect () override;
	virtual void EndEffect () override;
};

class APowerTorch : public APowerLightAmp
{
	DECLARE_CLASS (APowerTorch, APowerLightAmp)
public:
	
	virtual void Serialize(FSerializer &arc) override;
protected:
	virtual void DoEffect () override;
	int NewTorch, NewTorchDelta;
};

class APowerFlight : public APowerup
{
	DECLARE_CLASS (APowerFlight, APowerup)
public:
	virtual bool DrawPowerup (int x, int y) override;
	virtual void Serialize(FSerializer &arc) override;

protected:
	virtual void InitEffect () override;
	virtual void Tick () override;
	virtual void EndEffect () override;

private:
	bool HitCenterFrame;
};

class APowerWeaponLevel2 : public APowerup
{
	DECLARE_CLASS (APowerWeaponLevel2, APowerup)
protected:
	virtual void InitEffect () override;
	virtual void EndEffect () override;
};

class APowerSpeed : public APowerup
{
	DECLARE_CLASS (APowerSpeed, APowerup)
protected:
	virtual void DoEffect () override;
	
	virtual void Serialize(FSerializer &arc) override;
public:
	int SpeedFlags;
};

#define PSF_NOTRAIL		1

class APowerMinotaur : public APowerup
{
	DECLARE_CLASS (APowerMinotaur, APowerup)
};

class APowerScanner : public APowerup
{
	DECLARE_CLASS (APowerScanner, APowerup)
};

class APowerTargeter : public APowerup
{
	DECLARE_CLASS (APowerTargeter, APowerup)
protected:
	virtual void InitEffect () override;
	virtual void DoEffect () override;
	virtual void EndEffect () override;
	void PositionAccuracy ();
	virtual void Travelled () override;
	virtual void AttachToOwner(AActor *other) override;
	virtual bool HandlePickup(AInventory *item) override;
};

class APowerFrightener : public APowerup
{
	DECLARE_CLASS (APowerFrightener, APowerup)
protected:
	virtual void InitEffect () override;
	virtual void EndEffect () override;
};

class APowerBuddha : public APowerup
{
	DECLARE_CLASS (APowerBuddha, APowerup)
protected:
	virtual void InitEffect () override;
	virtual void EndEffect () override;
};

class APowerTimeFreezer : public APowerup
{
	DECLARE_CLASS( APowerTimeFreezer, APowerup )
protected:
	virtual void InitEffect() override;
	virtual void DoEffect() override;
	virtual void EndEffect() override;
};

class APowerDamage : public APowerup
{
	DECLARE_CLASS( APowerDamage, APowerup )
protected:
	virtual void InitEffect () override;
	virtual void EndEffect () override;
	virtual void ModifyDamage (int damage, FName damageType, int &newdamage, bool passive) override;
};

class APowerProtection : public APowerup
{
	DECLARE_CLASS( APowerProtection, APowerup )
protected:
	virtual void InitEffect () override;
	virtual void EndEffect () override;
	virtual void ModifyDamage (int damage, FName damageType, int &newdamage, bool passive) override;
};

class APowerDrain : public APowerup
{
	DECLARE_CLASS( APowerDrain, APowerup )
protected:
	virtual void InitEffect() override;
	virtual void EndEffect() override;
};

class APowerRegeneration : public APowerup
{
	DECLARE_CLASS( APowerRegeneration, APowerup )
protected:
	virtual void DoEffect() override;
};

class APowerHighJump : public APowerup
{
	DECLARE_CLASS( APowerHighJump, APowerup )
protected:
	virtual void InitEffect() override;
	virtual void EndEffect() override;
};

class APowerDoubleFiringSpeed : public APowerup
{
	DECLARE_CLASS( APowerDoubleFiringSpeed, APowerup )
protected:
	virtual void InitEffect() override;
	virtual void EndEffect() override;
};

class APowerInfiniteAmmo : public APowerup
{
	DECLARE_CLASS( APowerInfiniteAmmo, APowerup )
protected:
	virtual void InitEffect() override;
	virtual void EndEffect() override;
};

class APowerMorph : public APowerup
{
	DECLARE_CLASS( APowerMorph, APowerup )
	HAS_OBJECT_POINTERS
public:
	
	virtual void Serialize(FSerializer &arc) override;
	void SetNoCallUndoMorph() { bInUndoMorph = true; }

	// Variables
	PClassPlayerPawn *PlayerClass;
	PClassActor *MorphFlash, *UnMorphFlash;
	int MorphStyle;
	player_t *MorphedPlayer;
	bool bInUndoMorph;	// Because P_UndoPlayerMorph() can call EndEffect recursively

protected:
	virtual void InitEffect () override;
	virtual void EndEffect () override;
};

#endif //__A_ARTIFACTS_H__
