
// Egg missile --------------------------------------------------------------

class EggFX : MorphProjectile
{
	Default
	{
		Radius 8;
		Height 8;
		Speed 18;
		MorphProjectile.PlayerClass "ChickenPlayer";
		MorphProjectile.MonsterClass "Chicken";
		MorphProjectile.MorphStyle MRF_UNDOBYTOMEOFPOWER;
	}
	States
	{
	Spawn:
		EGGM ABCDE 4;
		Loop;
	Death:
		FX01 FFGH 3 Bright;
		Stop;
	}
}
	

// Morph Ovum ----------------------------------------------------------------

class ArtiEgg : CustomInventory
{
	Default
	{
		+COUNTITEM
		+FLOATBOB
		+INVENTORY.INVBAR
		Inventory.PickupFlash "PickupFlash";
		+INVENTORY.FANCYPICKUPSOUND
		Inventory.Icon "ARTIEGGC";
		Inventory.PickupSound "misc/p_pkup";
		Inventory.PickupMessage "$TXT_ARTIEGG";
		Inventory.DefMaxAmount;
		Tag "$TAG_ARTIEGG";
	}
	States
	{
	Spawn:
		EGGC ABCB 6;
		Loop;
	Use:
		TNT1 A 0
		{
			for (double i = -15; i <= 15; i += 7.5)	A_FireProjectile("EggFX", i, false, 0, 0, FPF_AIMATANGLE);
		}
		Stop;
	}
}
	
// Pork missile --------------------------------------------------------------

class PorkFX : MorphProjectile
{
	Default
	{
		Radius 8;
		Height 8;
		Speed 18;
		MorphProjectile.PlayerClass "PigPlayer";
		MorphProjectile.MonsterClass "Pig";
		MorphProjectile.MorphStyle MRF_UNDOBYTOMEOFPOWER|MRF_UNDOBYCHAOSDEVICE;
	}
	States
	{
	Spawn:
		PRKM ABCDE 4;
		Loop;
	Death:
		FHFX IJKL 3 Bright;
		Stop;
	}
}

// Porkalator ---------------------------------------------------------------

class ArtiPork : CustomInventory
{
	Default
	{
		+COUNTITEM
		+FLOATBOB
		+INVENTORY.INVBAR
		Inventory.PickupFlash "PickupFlash";
		+INVENTORY.FANCYPICKUPSOUND
		Inventory.Icon "ARTIPORK";
		Inventory.PickupSound "misc/p_pkup";
		Inventory.PickupMessage "$TXT_ARTIEGG2";
		Inventory.DefMaxAmount;
		Tag "$TAG_ARTIPORK";
	}
	States
	{
	Spawn:
		PORK ABCDEFGH 5;
		Loop;
	Use:
		TNT1 A 0
		{
			for (double i = -15; i <= 15; i += 7.5)	A_FireProjectile("PorkFX", i, false, 0, 0, FPF_AIMATANGLE);
		}
		Stop;
	}
}

