#ifndef __A_ARTIFACTS_H__
#define __A_ARTIFACTS_H__

#include "farchive.h"
#include "a_pickups.h"

#define INVERSECOLOR 0x00345678
#define GOLDCOLOR 0x009abcde

// [BC] More hacks!
#define REDCOLOR 0x00beefee
#define GREENCOLOR 0x00beefad

#define STREAM_ENUM(e) \
	inline FArchive &operator<< (FArchive &arc, e &i) \
	{ \
		BYTE val = (BYTE)i; \
		arc << val; \
		i = (e)val; \
		return arc; \
	}

class player_s;

// A powerup is a pseudo-inventory item that applies an effect to its
// owner while it is present.
class APowerup : public AInventory
{
	DECLARE_STATELESS_ACTOR (APowerup, AInventory)
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
	DECLARE_STATELESS_ACTOR (APowerupGiver, AInventory)
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
	DECLARE_STATELESS_ACTOR (APowerInvulnerable, APowerup)
protected:
	void InitEffect ();
	void DoEffect ();
	void EndEffect ();
	int AlterWeaponSprite (vissprite_t *vis);
};

class APowerStrength : public APowerup
{
	DECLARE_STATELESS_ACTOR (APowerStrength, APowerup)
public:
	PalEntry GetBlend ();
protected:
	void InitEffect ();
	void Tick ();
	bool HandlePickup (AInventory *item);
};

class APowerInvisibility : public APowerup
{
	DECLARE_STATELESS_ACTOR (APowerInvisibility, APowerup)
protected:
	void InitEffect ();
	void DoEffect ();
	void EndEffect ();
	int AlterWeaponSprite (vissprite_t *vis);
};

class APowerGhost : public APowerInvisibility
{
	DECLARE_STATELESS_ACTOR (APowerGhost, APowerInvisibility)
protected:
	void InitEffect ();
	int AlterWeaponSprite (vissprite_t *vis);
};

class APowerShadow : public APowerInvisibility
{
	DECLARE_STATELESS_ACTOR (APowerShadow, APowerInvisibility)
protected:
	bool HandlePickup (AInventory *item);
	void InitEffect ();
	int AlterWeaponSprite (vissprite_t *vis);
};

class APowerIronFeet : public APowerup
{
	DECLARE_STATELESS_ACTOR (APowerIronFeet, APowerup)
public:
	void AbsorbDamage (int damage, FName damageType, int &newdamage);
};

class APowerMask : public APowerIronFeet
{
	DECLARE_STATELESS_ACTOR (APowerMask, APowerIronFeet)
public:
	void AbsorbDamage (int damage, FName damageType, int &newdamage);
	void DoEffect ();
};

class APowerLightAmp : public APowerup
{
	DECLARE_STATELESS_ACTOR (APowerLightAmp, APowerup)
protected:
	void DoEffect ();
	void EndEffect ();
};

class APowerTorch : public APowerLightAmp
{
	DECLARE_STATELESS_ACTOR (APowerTorch, APowerLightAmp)
public:
	void Serialize (FArchive &arc);
protected:
	void DoEffect ();
	int NewTorch, NewTorchDelta;
};

class APowerFlight : public APowerup
{
	DECLARE_STATELESS_ACTOR (APowerFlight, APowerup)
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
	DECLARE_STATELESS_ACTOR (APowerWeaponLevel2, APowerup)
protected:
	void InitEffect ();
	void EndEffect ();
};

class APowerSpeed : public APowerup
{
	DECLARE_STATELESS_ACTOR (APowerSpeed, APowerup)
protected:
	void DoEffect ();
	fixed_t GetSpeedFactor();
};

class APowerMinotaur : public APowerup
{
	DECLARE_STATELESS_ACTOR (APowerMinotaur, APowerup)
};

class APowerScanner : public APowerup
{
	DECLARE_STATELESS_ACTOR (APowerScanner, APowerup)
};

class APowerTargeter : public APowerup
{
	DECLARE_ACTOR (APowerTargeter, APowerup)
protected:
	void InitEffect ();
	void DoEffect ();
	void EndEffect ();
	void PositionAccuracy ();
	void Travelled ();
};

class APowerFrightener : public APowerup
{
	DECLARE_STATELESS_ACTOR (APowerFrightener, APowerup)
protected:
	void InitEffect ();
	void EndEffect ();
};

class APowerTimeFreezer : public APowerup
{
	DECLARE_STATELESS_ACTOR( APowerTimeFreezer, APowerup )
protected:
	void InitEffect( );
	void DoEffect( );
	void EndEffect( );
};

class APowerDamage : public APowerup
{
	DECLARE_STATELESS_ACTOR( APowerDamage, APowerup )
protected:
	void InitEffect ();
	void EndEffect ();
	virtual void ModifyDamage (int damage, FName damageType, int &newdamage, bool passive);
};

class APowerProtection : public APowerup
{
	DECLARE_STATELESS_ACTOR( APowerProtection, APowerup )
protected:
	void InitEffect ();
	void EndEffect ();
	virtual void ModifyDamage (int damage, FName damageType, int &newdamage, bool passive);
};

class APowerDrain : public APowerup
{
	DECLARE_STATELESS_ACTOR( APowerDrain, APowerup )
protected:
	void InitEffect( );
	void EndEffect( );
};

class APowerRegeneration : public APowerup
{
	DECLARE_STATELESS_ACTOR( APowerRegeneration, APowerup )
protected:
	void InitEffect( );
	void EndEffect( );
};

class APowerHighJump : public APowerup
{
	DECLARE_STATELESS_ACTOR( APowerHighJump, APowerup )
protected:
	void InitEffect( );
	void EndEffect( );
};




class player_s;

#endif //__A_ARTIFACTS_H__
