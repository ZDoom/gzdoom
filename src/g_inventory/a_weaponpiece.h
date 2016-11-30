#pragma once
#include "a_pickups.h"
#include "a_weapons.h"

// 
class PClassWeaponPiece : public PClassInventory
{
	DECLARE_CLASS(PClassWeaponPiece, PClassInventory)
protected:
public:
	virtual void ReplaceClassRef(PClass *oldclass, PClass *newclass);
};

class AWeaponPiece : public AInventory
{
	DECLARE_CLASS_WITH_META(AWeaponPiece, AInventory, PClassWeaponPiece)
	HAS_OBJECT_POINTERS
protected:
	bool PrivateShouldStay ();
public:
	
	virtual void Serialize(FSerializer &arc) override;
	virtual bool TryPickup (AActor *&toucher) override;
	virtual bool TryPickupRestricted (AActor *&toucher) override;
	virtual bool ShouldStay () override;
	virtual FString PickupMessage () override;
	virtual void PlayPickupSound (AActor *toucher) override;

	int PieceValue;
	PClassActor *WeaponClass;
	TObjPtr<AWeapon> FullWeapon;
};

// an internal class to hold the information for player class independent weapon piece handling
// [BL] Needs to be available for SBarInfo to check weaponpieces
class AWeaponHolder : public AInventory
{
	DECLARE_CLASS(AWeaponHolder, AInventory)

public:
	int PieceMask;
	PClassActor * PieceWeapon;

	
	virtual void Serialize(FSerializer &arc) override;
};
