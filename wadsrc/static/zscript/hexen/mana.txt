// Blue mana ----------------------------------------------------------------

class Mana1 : Ammo
{
	Default
	{
		Inventory.Amount 15;
		Inventory.MaxAmount 200;
		Ammo.BackpackAmount 15;
		Ammo.BackpackMaxAmount 200;
		Radius 8;
		Height 8;
		+FLOATBOB
		Inventory.Icon "MAN1I0";
		Inventory.PickupMessage "$TXT_MANA_1";
		Tag "$AMMO_MANA1";
	}
	States
	{
	Spawn:
		MAN1 ABCDEFGHI 4 Bright;
		Loop;
	}
}

// Green mana ---------------------------------------------------------------

class Mana2 : Ammo
{
	Default
	{
		Inventory.Amount 15;
		Inventory.MaxAmount 200;
		Ammo.BackpackAmount 15;
		Ammo.BackpackMaxAmount 200;
		Radius 8;
		Height 8;
		+FLOATBOB
		Inventory.Icon "MAN2G0";
		Inventory.PickupMessage "$TXT_MANA_2";
		Tag "$AMMO_MANA2";
	}
	States
	{
	Spawn:
		MAN2 ABCDEFGHIJKLMNOP 4 Bright;
		Loop;
	}
}

// Combined mana ------------------------------------------------------------

class Mana3 : CustomInventory
{
	Default
	{
		Radius 8;
		Height 8;
		+FLOATBOB
		Inventory.PickupMessage "$TXT_MANA_BOTH";
	}
	States
	{
	Spawn:
		MAN3 ABCDEFGHIJKLMNOP 4 Bright;
		Loop;
	Pickup:
		TNT1 A 0 A_GiveInventory("Mana1", 20);
		TNT1 A 0 A_GiveInventory("Mana2", 20);
		Stop;
	}
}

// Boost Mana Artifact Krater of Might ------------------------------------

class ArtiBoostMana : CustomInventory
{
	Default
	{
		+FLOATBOB
		+COUNTITEM
		+INVENTORY.INVBAR
		+INVENTORY.FANCYPICKUPSOUND
		Inventory.PickupFlash "PickupFlash";
		Inventory.DefMaxAmount;
		Inventory.Icon "ARTIBMAN";
		Inventory.PickupSound "misc/p_pkup";
		Inventory.PickupMessage "$TXT_ARTIBOOSTMANA";
		Tag "$TAG_ARTIBOOSTMANA";
	}
	States
	{
	Spawn:
		BMAN A -1;
		Stop;
	Use:
		TNT1 A 0 A_GiveInventory("Mana1", 200);
		TNT1 A 0 A_GiveInventory("Mana2", 200);
		Stop;
	}
}
