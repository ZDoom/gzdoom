#ifndef __A_ARTIFACTS_H__
#define __A_ARTIFACTS_H__

#include "a_pickups.h"

#define INVERSECOLOR 0x00345678
#define GOLDCOLOR 0x009abcde

// [BC] More hacks!
#define REDCOLOR 0x00beefee
#define GREENCOLOR 0x00beefad

class player_t;

// A powerup is a pseudo-inventory item that applies an effect to its
// owner while it is present.
class APowerup : public AInventory
{
	DECLARE_CLASS (APowerup, AInventory)
public:
	virtual void Tick ();
	virtual void Destroy ();
	virtual bool HandlePickup (AInventory *item);
	virtual AInventory *CreateCopy (AActor *other);
	virtual AInventory *CreateTossable ();
	virtual void Serialize (FArchive &arc);
	virtual void OwnerDied ();
	virtual PalEntry GetBlend ();
	virtual bool DrawPowerup (int x, int y);

	int EffectTics;
	PalEntry BlendColor;
	FNameNoInit mode;

protected:
	virtual void InitEffect ();
	virtual void DoEffect ();
	virtual void EndEffect ();
};

// An artifact is an item that gives the player a powerup when activated.
class APowerupGiver : public AInventory
{
	DECLARE_CLASS (APowerupGiver, AInventory)
public:
	virtual bool Use (bool pickup);
	virtual void Serialize (FArchive &arc);

	const PClass *PowerupType;
	int EffectTics;			// Non-0 to override the powerup's default tics
	PalEntry BlendColor;	// Non-0 to override the powerup's default blend
	FNameNoInit mode;		// Meaning depends on powerup - currently only of use for Invulnerability
};

class APowerInvulnerable : public APowerup
{
	DECLARE_CLASS (APowerInvulnerable, APowerup)
protected:
	void InitEffect ();
	void DoEffect ();
	void EndEffect ();
	int AlterWeaponSprite (vissprite_t *vis);
};

class APowerStrength : public APowerup
{
	DECLARE_CLASS (APowerStrength, APowerup)
public:
	PalEntry GetBlend ();
protected:
	void InitEffect ();
	void Tick ();
	bool HandlePickup (AInventory *item);
};

class APowerInvisibility : public APowerup
{
	DECLARE_CLASS (APowerInvisibility, APowerup)
protected:
	void CommonInit ();
	void InitEffect ();
	void DoEffect ();
	void EndEffect ();
	int AlterWeaponSprite (vissprite_t *vis);
};

class APowerGhost : public APowerInvisibility
{
	DECLARE_CLASS (APowerGhost, APowerInvisibility)
protected:
	void InitEffect ();
	int AlterWeaponSprite (vissprite_t *vis);
};

class APowerShadow : public APowerInvisibility
{
	DECLARE_CLASS (APowerShadow, APowerInvisibility)
protected:
	bool HandlePickup (AInventory *item);
	void InitEffect ();
	int AlterWeaponSprite (vissprite_t *vis);
};

class APowerIronFeet : public APowerup
{
	DECLARE_CLASS (APowerIronFeet, APowerup)
public:
	void AbsorbDamage (int damage, FName damageType, int &newdamage);
};

class APowerMask : public APowerIronFeet
{
	DECLARE_CLASS (APowerMask, APowerIronFeet)
public:
	void AbsorbDamage (int damage, FName damageType, int &newdamage);
	void DoEffect ();
};

class APowerLightAmp : public APowerup
{
	DECLARE_CLASS (APowerLightAmp, APowerup)
protected:
	void DoEffect ();
	void EndEffect ();
};

class APowerTorch : public APowerLightAmp
{
	DECLARE_CLASS (APowerTorch, APowerLightAmp)
public:
	void Serialize (FArchive &arc);
protected:
	void DoEffect ();
	int NewTorch, NewTorchDelta;
};

class APowerFlight : public APowerup
{
	DECLARE_CLASS (APowerFlight, APowerup)
public:
	bool DrawPowerup (int x, int y);
	void Serialize (FArchive &arc);

protected:
	void InitEffect ();
	void Tick ();
	void EndEffect ();

	bool HitCenterFrame;
};

class APowerWeaponLevel2 : public APowerup
{
	DECLARE_CLASS (APowerWeaponLevel2, APowerup)
protected:
	void InitEffect ();
	void EndEffect ();
};

class APowerSpeed : public APowerup
{
	DECLARE_CLASS (APowerSpeed, APowerup)
protected:
	void DoEffect ();
	fixed_t GetSpeedFactor();
};

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
	void InitEffect ();
	void DoEffect ();
	void EndEffect ();
	void PositionAccuracy ();
	void Travelled ();
};

class APowerFrightener : public APowerup
{
	DECLARE_CLASS (APowerFrightener, APowerup)
protected:
	void InitEffect ();
	void EndEffect ();
};

class APowerTimeFreezer : public APowerup
{
	DECLARE_CLASS( APowerTimeFreezer, APowerup )
protected:
	void InitEffect( );
	void DoEffect( );
	void EndEffect( );
};

class APowerDamage : public APowerup
{
	DECLARE_CLASS( APowerDamage, APowerup )
protected:
	void InitEffect ();
	void EndEffect ();
	virtual void ModifyDamage (int damage, FName damageType, int &newdamage, bool passive);
};

class APowerProtection : public APowerup
{
	DECLARE_CLASS( APowerProtection, APowerup )
protected:
	void InitEffect ();
	void EndEffect ();
	virtual void ModifyDamage (int damage, FName damageType, int &newdamage, bool passive);
};

class APowerDrain : public APowerup
{
	DECLARE_CLASS( APowerDrain, APowerup )
protected:
	void InitEffect( );
	void EndEffect( );
};

class APowerRegeneration : public APowerup
{
	DECLARE_CLASS( APowerRegeneration, APowerup )
protected:
	void InitEffect( );
	void EndEffect( );
};

class APowerHighJump : public APowerup
{
	DECLARE_CLASS( APowerHighJump, APowerup )
protected:
	void InitEffect( );
	void EndEffect( );
};

class APowerDoubleFiringSpeed : public APowerup
{
	DECLARE_CLASS( APowerDoubleFiringSpeed, APowerup )
protected:
	void InitEffect( );
	void EndEffect( );
};

class APowerMorph : public APowerup
{
	DECLARE_CLASS( APowerMorph, APowerup )
public:
	void Serialize (FArchive &arc);

	FNameNoInit	PlayerClass, MorphFlash, UnMorphFlash;
	int MorphStyle;

protected:
	void InitEffect ();
	void EndEffect ();
	// Variables
	player_t *Player;
};

#endif //__A_ARTIFACTS_H__
