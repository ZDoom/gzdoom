#ifndef __A_HEXENGLOBAL_H__
#define __A_HEXENGLOBAL_H__

#include "d_player.h"

void AdjustPlayerAngle(AActor *pmo, FTranslatedLineTarget *t);

class AHolySpirit : public AActor
{
	DECLARE_CLASS (AHolySpirit, AActor)
public:
	bool Slam (AActor *thing);
	bool SpecialBlastHandling (AActor *source, double strength);
};

class AFighterWeapon : public AWeapon
{
	DECLARE_CLASS (AFighterWeapon, AWeapon);
public:
};

class AClericWeapon : public AWeapon
{
	DECLARE_CLASS (AClericWeapon, AWeapon);
public:
};

class AMageWeapon : public AWeapon
{
	DECLARE_CLASS (AMageWeapon, AWeapon);
public:
};

class AArtiPoisonBag : public AInventory
{
	DECLARE_CLASS (AArtiPoisonBag, AInventory)
public:
	bool HandlePickup (AInventory *item);
	AInventory *CreateCopy (AActor *other);
	void BeginPlay ();
};

#endif //__A_HEXENGLOBAL_H__
