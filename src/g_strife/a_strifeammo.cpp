#include "a_strifeglobal.h"
#include "p_local.h"
#include "a_strifeweaps.h"

// HEGrenades:		6	12
// PhGrenades:		4	8
// ClipOfBullets	??	10
// BoxOfBullets:	50	50
// MiniMissiles:	4	8
// CrateOfMissiles:	20	20
// EnergyPod:		20	40
// EnergyPack:		100
// PoisonBolts:		20
// ElectricBolts:	20

// HE-Grenade Rounds --------------------------------------------------------

FState AHEGrenadeRounds::States[] =
{
	S_NORMAL (GRN1, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (AHEGrenadeRounds, Strife, 152, 0)
	PROP_SpawnState (0)
	PROP_Flags (MF_SPECIAL)
	PROP_Flags2 (MF2_FLOORCLIP)
	PROP_StrifeType (177)
	PROP_StrifeTeaserType (170)
	PROP_StrifeTeaserType2 (174)
	PROP_Inventory_Amount (6)
	PROP_Inventory_MaxAmount (30)
	PROP_Ammo_BackpackAmount (6)
	PROP_Ammo_BackpackMaxAmount (60)
	PROP_Inventory_Icon ("I_GRN1")
	PROP_Tag ("HE-Grenade_Rounds")
END_DEFAULTS

const char *AHEGrenadeRounds::PickupMessage ()
{
	return "You picked up the HE-Grenade Rounds.";
}

// Phosphorus-Grenade Rounds ------------------------------------------------

FState APhosphorusGrenadeRounds::States[] =
{
	S_NORMAL (GRN2, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (APhosphorusGrenadeRounds, Strife, 153, 0)
	PROP_SpawnState (0)
	PROP_Flags (MF_SPECIAL)
	PROP_Flags2 (MF2_FLOORCLIP)
	PROP_StrifeType (178)
	PROP_StrifeTeaserType (171)
	PROP_StrifeTeaserType2 (175)
	PROP_Inventory_Amount (4)
	PROP_Inventory_MaxAmount (16)
	PROP_Ammo_BackpackAmount (4)
	PROP_Ammo_BackpackMaxAmount (32)
	PROP_Inventory_Icon ("I_GRN2")
	PROP_Tag ("Phoshorus-Grenade_Rounds")	// "Fire-Grenade_Rounds" in the Teaser
END_DEFAULTS

const char *APhosphorusGrenadeRounds::PickupMessage ()
{
	return "You picked up the Phoshorus-Grenade Rounds.";
}

// The teaser actually has Gas-Grenade_Rounds defined

// Clip of Bullets ----------------------------------------------------------

FState AClipOfBullets::States[] =
{
	S_NORMAL (BLIT, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (AClipOfBullets, Strife, 2007, 11)
	PROP_Flags (MF_SPECIAL)
	PROP_Flags2 (MF2_FLOORCLIP)
	PROP_SpawnState (0)
	PROP_StrifeType (179)
	PROP_StrifeTeaserType (173)
	PROP_StrifeTeaserType2 (177)
	PROP_Inventory_Amount (10)
	PROP_Inventory_MaxAmount (250)
	PROP_Ammo_BackpackAmount (10)
	PROP_Ammo_BackpackMaxAmount (500)
	PROP_Inventory_Icon ("I_BLIT")
	PROP_Tag ("clip_of_bullets")	// "bullets" in the Teaser
END_DEFAULTS

const char *AClipOfBullets::PickupMessage ()
{
	return "You picked up the clip of bullets.";
}

// Box of Bullets -----------------------------------------------------------

FState ABoxOfBullets::States[] =
{
	S_NORMAL (BBOX, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (ABoxOfBullets, Strife, 2048, 139)
	PROP_Flags (MF_SPECIAL)
	PROP_Flags2 (MF2_FLOORCLIP)
	PROP_SpawnState (0)
	PROP_StrifeType (180)
	PROP_StrifeTeaserType (174)
	PROP_StrifeTeaserType2 (178)
	PROP_Inventory_Amount (50)
	PROP_Tag ("ammo")
END_DEFAULTS

const char *ABoxOfBullets::PickupMessage ()
{
	return "You picked up the box of bullets.";
}

// Mini Missiles ------------------------------------------------------------

FState AMiniMissiles::States[] =
{
	S_NORMAL (MSSL, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (AMiniMissiles, Strife, 2010, 140)
	PROP_Flags (MF_SPECIAL)
	PROP_Flags2 (MF2_FLOORCLIP)
	PROP_SpawnState (0)
	PROP_StrifeType (181)
	PROP_StrifeTeaserType (175)
	PROP_StrifeTeaserType2 (179)
	PROP_Inventory_Amount (8)
	PROP_Inventory_MaxAmount (100)
	PROP_Ammo_BackpackAmount (4)
	PROP_Ammo_BackpackMaxAmount (200)
	PROP_Inventory_Icon ("I_ROKT")
	PROP_Tag ("mini_missiles")	//"rocket" in the Teaser
END_DEFAULTS

const char *AMiniMissiles::PickupMessage ()
{
	return "You picked up the mini missiles.";
}

// Crate of Missiles --------------------------------------------------------

FState ACrateOfMissiles::States[] =
{
	S_NORMAL (ROKT, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (ACrateOfMissiles, Strife, 2046, 141)
	PROP_Flags (MF_SPECIAL)
	PROP_Flags2 (MF2_FLOORCLIP)
	PROP_SpawnState (0)
	PROP_StrifeType (182)
	PROP_StrifeTeaserType (176)
	PROP_StrifeTeaserType2 (180)
	PROP_Inventory_Amount (20)
	PROP_Tag ("crate_of_missiles")	//"box_of_rockets" in the Teaser
END_DEFAULTS

const char *ACrateOfMissiles::PickupMessage ()
{
	return "You picked up the crate of missiles.";
}

// Energy Pod ---------------------------------------------------------------

FState AEnergyPod::States[] =
{
	S_NORMAL (BRY1, 'A', 6, NULL, &States[1]),
	S_NORMAL (BRY1, 'B', 6, NULL, &States[0])
};

IMPLEMENT_ACTOR (AEnergyPod, Strife, 2047, 75)
	PROP_Flags (MF_SPECIAL)
	PROP_Flags2 (MF2_FLOORCLIP)
	PROP_SpawnState (0)
	PROP_StrifeType (183)
	PROP_StrifeTeaserType (177)
	PROP_StrifeTeaserType2 (181)
	PROP_Inventory_Amount (20)
	PROP_Inventory_MaxAmount (400)
	PROP_Ammo_BackpackAmount (20)
	PROP_Ammo_BackpackMaxAmount (800)
	PROP_Inventory_Icon ("I_BRY1")
	PROP_Tag ("energy_pod")
END_DEFAULTS

const char *AEnergyPod::PickupMessage ()
{
	return "You picked up the energy pod.";
}

// Energy pack ---------------------------------------------------------------

FState AEnergyPack::States[] =
{
	S_NORMAL (CPAC, 'A', 6, NULL, &States[1]),
	S_NORMAL (CPAC, 'B', 6, NULL, &States[0])
};

IMPLEMENT_ACTOR (AEnergyPack, Strife, 17, 142)
	PROP_Flags (MF_SPECIAL)
	PROP_Flags2 (MF2_FLOORCLIP)
	PROP_SpawnState (0)
	PROP_StrifeType (184)
	PROP_StrifeTeaserType (178)
	PROP_StrifeTeaserType2 (182)
	PROP_Inventory_Amount (100)
	PROP_Tag ("energy_pack")
END_DEFAULTS

const char *AEnergyPack::PickupMessage ()
{
	return "You picked up the energy pack.";
}

// Poison Bolt Quiver -------------------------------------------------------

FState APoisonBolts::States[] =
{
	S_NORMAL (PQRL, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (APoisonBolts, Strife, 115, 0)
	PROP_SpawnState (0)
	PROP_Flags (MF_SPECIAL)
	PROP_Flags2 (MF2_FLOORCLIP)
	PROP_StrifeType (185)
	PROP_StrifeTeaserType (179)
	PROP_StrifeTeaserType2 (183)
	PROP_Inventory_Amount (10)
	PROP_Inventory_MaxAmount (25)
	PROP_Ammo_BackpackAmount (2)
	PROP_Ammo_BackpackMaxAmount (50)
	PROP_Inventory_Icon ("I_PQRL")
	PROP_Tag ("poison_bolts")	// "poison_arrows" in the Teaser
END_DEFAULTS

const char *APoisonBolts::PickupMessage ()
{
	return "You picked up the poison bolts.";
}

// Electric Bolt Quiver -------------------------------------------------------

FState AElectricBolts::States[] =
{
	S_NORMAL (XQRL, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (AElectricBolts, Strife, 114, 0)
	PROP_SpawnState (0)
	PROP_Flags (MF_SPECIAL)
	PROP_Flags2 (MF2_FLOORCLIP)
	PROP_StrifeType (186)
	PROP_StrifeTeaserType (180)
	PROP_StrifeTeaserType2 (184)
	PROP_Inventory_Amount (20)
	PROP_Inventory_MaxAmount (50)
	PROP_Ammo_BackpackAmount (4)
	PROP_Ammo_BackpackMaxAmount (100)
	PROP_Inventory_Icon ("I_XQRL")
	PROP_Tag ("electric_bolts")	// "electric_arrows" in the Teaser
END_DEFAULTS

const char *AElectricBolts::PickupMessage ()
{
	return "You picked up the electric bolts.";
}

// Ammo Satchel -------------------------------------------------------------

class AAmmoSatchel : public ABackpack
{
	DECLARE_ACTOR (AAmmoSatchel, ABackpack)
protected:
	virtual const char *PickupMessage ()
	{
		return "You picked up the ammo satchel";
	}
};

FState AAmmoSatchel::States[] =
{
	S_NORMAL (BKPK, 'A',   -1, NULL 						, NULL)
};

IMPLEMENT_ACTOR (AAmmoSatchel, Strife, 183, 144)
	PROP_Flags (MF_SPECIAL)
	PROP_Flags2 (MF2_FLOORCLIP)
	PROP_SpawnState (0)
	PROP_StrifeType (187)
	PROP_StrifeTeaserType (181)
	PROP_StrifeTeaserType2 (185)
	PROP_Inventory_Icon ("I_BKPK")
	PROP_Tag ("ammo_satchel")	// "Back_pack" in the Teaser
END_DEFAULTS
