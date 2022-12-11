
// Health -------------------------------------------------------------------

class ArtiHealth : HealthPickup
{
	Default
	{
		Health 25;
		+COUNTITEM
		+FLOATBOB
		Inventory.PickupFlash "PickupFlash";
		+INVENTORY.FANCYPICKUPSOUND
		Inventory.Icon "ARTIPTN2";
		Inventory.PickupSound "misc/p_pkup";
		Inventory.PickupMessage "$TXT_ARTIHEALTH";
		Tag "$TAG_ARTIHEALTH";
		HealthPickup.Autouse 1;
	}
	States
	{
	Spawn:
		PTN2 ABC 4;
		Loop;
	}
}

// Super health -------------------------------------------------------------

class ArtiSuperHealth : HealthPickup
{
	Default
	{
		Health 100;
		+COUNTITEM
		+FLOATBOB
		Inventory.PickupFlash "PickupFlash";
		+INVENTORY.FANCYPICKUPSOUND
		Inventory.Icon "ARTISPHL";
		Inventory.PickupSound "misc/p_pkup";
		Inventory.PickupMessage "$TXT_ARTISUPERHEALTH";
		Tag "$TAG_ARTISUPERHEALTH";
		HealthPickup.Autouse 2;
	}
	States
	{
	Spawn:
		SPHL A 350;
		Loop;
	}
}

// Flight -------------------------------------------------------------------

class ArtiFly : PowerupGiver
{
	Default
	{
		+COUNTITEM
		+FLOATBOB
		Inventory.PickupFlash "PickupFlash";
		Inventory.InterHubAmount 0;
		Inventory.RespawnTics 4230;
		Inventory.Icon "ARTISOAR";
		Inventory.PickupMessage "$TXT_ARTIFLY";
		Tag "$TAG_ARTIFLY";
		Powerup.Type "PowerFlight";
	}
	States
	{
	Spawn:
		SOAR ABCB 5;
		Loop;
	}
}

// Invulnerability Heretic (Ring of invincibility) --------------------------

class ArtiInvulnerability : PowerupGiver
{
	Default
	{
		+COUNTITEM
		+FLOATBOB
		Inventory.PickupFlash "PickupFlash";
		Inventory.RespawnTics 4230;
		Inventory.Icon "ARTIINVU";
		Inventory.PickupMessage "$TXT_ARTIINVULNERABILITY";
		Tag "$TAG_ARTIINVULNERABILITY";
		Powerup.Type "PowerInvulnerable";
		Powerup.Color "GoldMap";
	}
	States
	{
	Spawn:
		INVU ABCD 3;
		Loop;
	}
}
	
// Invulnerability Hexen (Icon of the defender) -----------------------------

class ArtiInvulnerability2 : PowerupGiver
{
	Default
	{
		+COUNTITEM
		+FLOATBOB
		Inventory.PickupFlash "PickupFlash";
		Inventory.RespawnTics 4230;
		Inventory.Icon "ARTIDEFN";
		Inventory.PickupMessage "$TXT_ARTIINVULNERABILITY2";
		Powerup.Type "PowerInvulnerable";
		Tag "$TAG_ARTIDEFENDER";
	}
	States
	{
	Spawn:
		DEFN ABCD 3;
		Loop;
	}
}
	
// Torch --------------------------------------------------------------------

class ArtiTorch : PowerupGiver
{
	Default
	{
		+COUNTITEM
		+FLOATBOB
		Inventory.PickupFlash "PickupFlash";
		Inventory.Icon "ARTITRCH";
		Inventory.PickupMessage "$TXT_ARTITORCH";
		Tag "$TAG_ARTITORCH";
		Powerup.Type "PowerTorch";
	}
	States
	{
	Spawn:
		TRCH ABC 3 Bright;
		Loop;
	}
}
