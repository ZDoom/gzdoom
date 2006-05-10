
class AWeaponPiece : public AInventory
{
	DECLARE_CLASS (AWeaponPiece, AInventory)
protected:
	bool PrivateShouldStay ();
public:
	void Serialize (FArchive &arc);
	bool TryPickup (AActor *toucher);
	bool ShouldStay ();
	virtual const char *PickupMessage ();
	virtual void PlayPickupSound (AActor *toucher);

	int PieceValue;
	const PClass * WeaponClass;
	AWeapon * FullWeapon;
};
