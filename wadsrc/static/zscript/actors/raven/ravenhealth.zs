class CrystalVial : Health
{
	Default
	{
		+FLOATBOB
		Inventory.Amount 10;
		Inventory.PickupMessage "$TXT_ITEMHEALTH";
		Tag "$TAG_ITEMHEALTH";
	}
	States
	{
	Spawn:
		PTN1 ABC 3;
		Loop;
	}
}

