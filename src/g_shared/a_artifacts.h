#ifndef __A_ARTIFACTS_H__
#define __A_ARTIFACTS_H__

#include "farchive.h"
#include "a_pickups.h"

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
	virtual PalEntry GetBlend ();
	virtual bool DrawPowerup (int x, int y);

	int EffectTics;
	PalEntry BlendColor;

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
	virtual bool Use ();
	virtual void Serialize (FArchive &arc);

	const TypeInfo *PowerupType;
	int EffectTics;		// Non-0 to override the powerup's default tics
protected:
	void PlayPickupSound (AActor *toucher);
};

class APowerInvulnerable : public APowerup
{
	DECLARE_STATELESS_ACTOR (APowerInvulnerable, APowerup)
protected:
	void InitEffect ();
	void DoEffect ();
	void EndEffect ();
};

class APowerStrength : public APowerup
{
	DECLARE_STATELESS_ACTOR (APowerStrength, APowerup)
public:
	PalEntry GetBlend ();
protected:
	void InitEffect ();
	void DoEffect ();
};

class APowerInvisibility : public APowerup
{
	DECLARE_STATELESS_ACTOR (APowerInvisibility, APowerup)
protected:
	void InitEffect ();
	void EndEffect ();
	void AlterWeaponSprite (vissprite_t *vis);
};

class APowerIronFeet : public APowerup
{
	DECLARE_STATELESS_ACTOR (APowerIronFeet, APowerup)
};

class APowerMask : public APowerIronFeet
{
	DECLARE_STATELESS_ACTOR (APowerMask, APowerIronFeet)
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
	void DoEffect ();
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
	void InitEffect ();
	void DoEffect ();
	void EndEffect ();
};

class APowerMinotaur : public APowerup
{
	DECLARE_STATELESS_ACTOR (APowerMinotaur, APowerup)
};

class APowerScanner : public APowerup
{
	DECLARE_STATELESS_ACTOR (APowerScanner, APowerup)
};

class player_s;

#endif //__A_ARTIFACTS_H__
