#pragma once
#include "a_pickups.h"
#include "a_weapons.h"

class AWeaponPiece : public AInventory
{
	DECLARE_CLASS(AWeaponPiece, AInventory)
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
