#pragma once
#include "a_pickups.h"

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
	
	void Serialize(FSerializer &arc);
	bool TryPickup (AActor *&toucher);
	bool TryPickupRestricted (AActor *&toucher);
	bool ShouldStay ();
	virtual FString PickupMessage ();
	virtual void PlayPickupSound (AActor *toucher);

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

	
	void Serialize(FSerializer &arc);
};
