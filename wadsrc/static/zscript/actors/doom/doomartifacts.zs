// Invulnerability Sphere ---------------------------------------------------

class InvulnerabilitySphere : PowerupGiver
{
	Default
	{
		+COUNTITEM
		+INVENTORY.AUTOACTIVATE
		+INVENTORY.ALWAYSPICKUP
		+INVENTORY.BIGPOWERUP
		Inventory.MaxAmount 0;
		Powerup.Type "PowerInvulnerable";
		Powerup.Color "InverseMap";
		Inventory.PickupMessage "$GOTINVUL";
	}
	States
	{
	Spawn:
		PINV ABCD 6 Bright;
		Loop;
	}
}

// Soulsphere --------------------------------------------------------------

class Soulsphere : Health
{
	Default
	{
		+COUNTITEM;
		+INVENTORY.AUTOACTIVATE;
		+INVENTORY.ALWAYSPICKUP;
		+INVENTORY.FANCYPICKUPSOUND;
		Inventory.Amount 100;
		Inventory.MaxAmount 200;
		Inventory.PickupMessage "$GOTSUPER";
		Inventory.PickupSound "misc/p_pkup";
	}
	States
	{
	Spawn:
		SOUL ABCDCB 6 Bright;
		Loop;
	}
}

// Mega sphere --------------------------------------------------------------

class MegasphereHealth : Health	// for manipulation by Dehacked
{
	Default
	{
		Inventory.Amount 200;
		Inventory.MaxAmount 200;
		+INVENTORY.ALWAYSPICKUP
	}
}

// DeHackEd can only modify the blue armor's type, not the megasphere's.
class BlueArmorForMegasphere : BlueArmor
{
	Default
	{
		Armor.SavePercent 50;
		Armor.SaveAmount 200;
	}
}

class Megasphere : CustomInventory
{
	Default
	{
		+COUNTITEM
		+INVENTORY.ALWAYSPICKUP
		+INVENTORY.ISHEALTH
		+INVENTORY.ISARMOR
		Inventory.PickupMessage "$GOTMSPHERE";
		Inventory.PickupSound "misc/p_pkup";
	}
	States
	{
	Spawn:
		MEGA ABCD 6 BRIGHT;
		Loop;
	Pickup:
		TNT1 A 0 A_GiveInventory("BlueArmorForMegasphere", 1);
		TNT1 A 0 A_GiveInventory("MegasphereHealth", 1);
		Stop;
	}
}	

// Invisibility -------------------------------------------------------------

class BlurSphere : PowerupGiver
{
	Default
	{
		+COUNTITEM
		+VISIBILITYPULSE
		+ZDOOMTRANS
		+INVENTORY.AUTOACTIVATE
		+INVENTORY.ALWAYSPICKUP
		+INVENTORY.BIGPOWERUP
		Inventory.MaxAmount 0;
		Powerup.Type "PowerInvisibility";
		RenderStyle "Translucent";
		Inventory.PickupMessage "$GOTINVIS";
	}
	States
	{
	Spawn:
		PINS ABCD 6 Bright;
		Loop;
	}
}	

// Radiation suit (aka iron feet) -------------------------------------------

class RadSuit : PowerupGiver
{
	Default
	{
		Height 46;
		+INVENTORY.AUTOACTIVATE
		+INVENTORY.ALWAYSPICKUP
		Inventory.MaxAmount 0;
		Inventory.PickupMessage "$GOTSUIT";
		Powerup.Type "PowerIronFeet";
	}
	States
	{
	Spawn:
		SUIT A -1 Bright;
		Stop;
	}
}	

// infrared -----------------------------------------------------------------

class Infrared : PowerupGiver
{
	Default
	{
		+COUNTITEM
		+INVENTORY.AUTOACTIVATE
		+INVENTORY.ALWAYSPICKUP
		Inventory.MaxAmount 0;
		Powerup.Type "PowerLightAmp";
		Inventory.PickupMessage "$GOTVISOR";
	}
	States
	{
	Spawn:
		PVIS A 6 Bright;
		PVIS B 6;
		Loop;
	}
}
	
// Allmap -------------------------------------------------------------------

class Allmap : MapRevealer
{
	Default
	{
		+COUNTITEM
		+INVENTORY.FANCYPICKUPSOUND
		+INVENTORY.ALWAYSPICKUP
		Inventory.MaxAmount 0;
		Inventory.PickupSound "misc/p_pkup";
		Inventory.PickupMessage "$GOTMAP";
	}
	States
	{
	Spawn:
		PMAP ABCDCB 6 Bright;
		Loop;
	}
}

// Berserk ------------------------------------------------------------------

class Berserk : CustomInventory
{
	Default
	{
		+COUNTITEM
		+INVENTORY.ALWAYSPICKUP
		+INVENTORY.ISHEALTH
		Inventory.PickupMessage "$GOTBERSERK";
		Inventory.PickupSound "misc/p_pkup";
	}
	States
	{
	Spawn:
		PSTR A -1 Bright;
		Stop;
	Pickup:
		TNT1 A 0 A_GiveInventory("PowerStrength");
		TNT1 A 0 HealThing(100, 0);
		TNT1 A 0 A_SelectWeapon("Fist");
		Stop;
	}
}
	
