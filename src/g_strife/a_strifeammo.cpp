#include "a_pickups.h"
#include "p_local.h"

// HE-Grenade Rounds --------------------------------------------------------

class AHEGrenadeRounds : public AAmmo
{
	DECLARE_ACTOR (AHEGrenadeRounds, AAmmo);
public:
	virtual bool TryPickup (AActor *toucher)
	{
		if (flags & MF_DROPPED)
			return P_GiveAmmo (toucher->player, am_hegrenade, 6);
		else
			return P_GiveAmmo (toucher->player, am_hegrenade, 12);
	}
	virtual ammotype_t GetAmmoType () const
	{
		return am_hegrenade;
	}
protected:
	virtual const char *PickupMessage ()
	{
		return "You picked up the HE-Grenade Rounds";
	}
};

FState AHEGrenadeRounds::States[] =
{
	S_NORMAL (GRN1, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (AHEGrenadeRounds, Strife, 152, 0)
	PROP_SpawnState (0)
	PROP_Flags (MF_SPECIAL)
	PROP_Tag ("HE-Grenade_Rounds")
END_DEFAULTS

// Phosphorus-Grenade Rounds ------------------------------------------------

class APhosphorusGrenadeRounds : public AAmmo
{
	DECLARE_ACTOR (APhosphorusGrenadeRounds, AAmmo);
public:
	virtual bool TryPickup (AActor *toucher)
	{
		if (flags & MF_DROPPED)
			return P_GiveAmmo (toucher->player, am_phgrenade, 4);
		else
			return P_GiveAmmo (toucher->player, am_phgrenade, 8);
	}
	virtual ammotype_t GetAmmoType () const
	{
		return am_phgrenade;
	}
protected:
	virtual const char *PickupMessage ()
	{
		return "You picked up the HE-Grenade Rounds";
	}
};

FState APhosphorusGrenadeRounds::States[] =
{
	S_NORMAL (GRN2, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (APhosphorusGrenadeRounds, Strife, 153, 0)
	PROP_SpawnState (0)
	PROP_Flags (MF_SPECIAL)
	PROP_Tag ("HE-Grenade_Rounds")
END_DEFAULTS

// Clip of Bullets ----------------------------------------------------------

class AClipOfBullets : public AAmmo
{
	DECLARE_ACTOR (AClipOfBullets, AAmmo)
public:
	virtual bool TryPickup (AActor *toucher)
	{
		if (flags & MF_DROPPED)
			return P_GiveAmmo (toucher->player, am_clip, 5);
		else
			return P_GiveAmmo (toucher->player, am_clip, 10);
	}
	virtual ammotype_t GetAmmoType () const
	{
		return am_clip;
	}
protected:
	virtual const char *PickupMessage ()
	{
		return "You picked up the clip of bullets";
	}
};

FState AClipOfBullets::States[] =
{
	S_NORMAL (BLIT, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (AClipOfBullets, Strife, 2007, 11)
	PROP_Flags (MF_SPECIAL)
	PROP_SpawnState (0)
	PROP_Tag ("clip_of_bullets")
END_DEFAULTS

// Box of Bullets -----------------------------------------------------------

class ABoxOfBullets : public AAmmo
{
	DECLARE_ACTOR (ABoxOfBullets, AAmmo)
public:
	virtual bool TryPickup (AActor *toucher)
	{
		return P_GiveAmmo (toucher->player, am_clip, 50);
	}
	virtual ammotype_t GetAmmoType () const
	{
		return am_clip;
	}
protected:
	virtual const char *PickupMessage ()
	{
		return "You picked up the box of bullets";
	}
};

FState ABoxOfBullets::States[] =
{
	S_NORMAL (BBOX, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (ABoxOfBullets, Strife, 2048, 139)
	PROP_Flags (MF_SPECIAL)
	PROP_SpawnState (0)
	PROP_Tag ("ammo")
END_DEFAULTS

// Mini Missiles ------------------------------------------------------------

class AMiniMissiles : public AAmmo
{
	DECLARE_ACTOR (AMiniMissiles, AAmmo)
public:
	virtual bool TryPickup (AActor *toucher)
	{
		if (flags & MF_DROPPED)
			return P_GiveAmmo (toucher->player, am_misl, 4);
		else
			return P_GiveAmmo (toucher->player, am_misl, 8);
	}
	virtual ammotype_t GetAmmoType () const
	{
		return am_misl;
	}
protected:
	virtual const char *PickupMessage ()
	{
		return "You picked up the mini missiles";
	}
};

FState AMiniMissiles::States[] =
{
	S_NORMAL (MSSL, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (AMiniMissiles, Strife, 2010, 140)
	PROP_Flags (MF_SPECIAL)
	PROP_SpawnState (0)
	PROP_Tag ("mini_missiles")
END_DEFAULTS

// Crate of Missiles --------------------------------------------------------

class ACrateOfMissiles : public AAmmo
{
	DECLARE_ACTOR (ACrateOfMissiles, AAmmo)
public:
	virtual bool TryPickup (AActor *toucher)
	{
		return P_GiveAmmo (toucher->player, am_misl, 20);
	}
	virtual ammotype_t GetAmmoType () const
	{
		return am_misl;
	}
protected:
	virtual const char *PickupMessage ()
	{
		return "You picked up the crate of missiles";
	}
};

FState ACrateOfMissiles::States[] =
{
	S_NORMAL (ROKT, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (ACrateOfMissiles, Strife, 2046, 141)
	PROP_Flags (MF_SPECIAL)
	PROP_SpawnState (0)
	PROP_Tag ("crate_of_missiles")
END_DEFAULTS

// Energy Pod ---------------------------------------------------------------

class AEnergyPod : public AAmmo
{
	DECLARE_ACTOR (AEnergyPod, AAmmo)
public:
	virtual bool TryPickup (AActor *toucher)
	{
		if (flags & MF_DROPPED)
			return P_GiveAmmo (toucher->player, am_cell, 20);
		else
			return P_GiveAmmo (toucher->player, am_cell, 40);
	}
	virtual ammotype_t GetAmmoType () const
	{
		return am_cell;
	}
protected:
	virtual const char *PickupMessage ()
	{
		return "You picked up the energy pod";
	}
};

FState AEnergyPod::States[] =
{
	S_NORMAL (BRY1, 'A', 6, NULL, &States[1]),
	S_NORMAL (BRY1, 'B', 6, NULL, &States[0])
};

IMPLEMENT_ACTOR (AEnergyPod, Strife, 2047, 75)
	PROP_Flags (MF_SPECIAL)
	PROP_SpawnState (0)
	PROP_Tag ("energy_pod")
END_DEFAULTS

// Energy pack ---------------------------------------------------------------

class AEnergyPack : public AAmmo
{
	DECLARE_ACTOR (AEnergyPack, AAmmo)
public:
	virtual bool TryPickup (AActor *toucher)
	{
		return P_GiveAmmo (toucher->player, am_cell, 100);
	}
	virtual ammotype_t GetAmmoType () const
	{
		return am_cell;
	}
protected:
	virtual const char *PickupMessage ()
	{
		return "You picked up the energy pack";
	}
};

FState AEnergyPack::States[] =
{
	S_NORMAL (CPAC, 'A', 6, NULL, &States[1]),
	S_NORMAL (CPAC, 'B', 6, NULL, &States[0])
};

IMPLEMENT_ACTOR (AEnergyPack, Strife, 17, 142)
	PROP_Flags (MF_SPECIAL)
	PROP_SpawnState (0)
	PROP_Tag ("energy_pack")
END_DEFAULTS

// Poison Bolt Quiver -------------------------------------------------------

class APoisonBolts : public AAmmo
{
	DECLARE_ACTOR (APoisonBolts, AAmmo)
public:
	virtual bool TryPickup (AActor *toucher)
	{
		return P_GiveAmmo (toucher->player, am_poisonbolt, 20);
	}
	virtual ammotype_t GetAmmoType () const
	{
		return am_poisonbolt;
	}
protected:
	virtual const char *PickupMessage ()
	{
		return "You picked up the poison bolts";
	}
};

FState APoisonBolts::States[] =
{
	S_NORMAL (PQRL, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (APoisonBolts, Strife, 115, 0)
	PROP_SpawnState (0)
	PROP_Flags (MF_SPECIAL)
	PROP_Tag ("poison_bolts")
END_DEFAULTS

// Electric Bolt Quiver -------------------------------------------------------

class AElectricBolts : public AAmmo
{
	DECLARE_ACTOR (AElectricBolts, AAmmo)
public:
	virtual bool TryPickup (AActor *toucher)
	{
		return P_GiveAmmo (toucher->player, am_electricbolt, 20);
	}
	virtual ammotype_t GetAmmoType () const
	{
		return am_electricbolt;
	}
protected:
	virtual const char *PickupMessage ()
	{
		return "You picked up the electric bolts";
	}
};

FState AElectricBolts::States[] =
{
	S_NORMAL (XQRL, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (AElectricBolts, Strife, 114, 0)
	PROP_SpawnState (0)
	PROP_Flags (MF_SPECIAL)
	PROP_Tag ("electric_bolts")
END_DEFAULTS

// Ammo Satchel -------------------------------------------------------------

class AAmmoSatchel : public AInventory
{
	DECLARE_ACTOR (AAmmoSatchel, AInventory)
public:
	virtual bool TryPickup (AActor *toucher)
	{
		player_t *player = toucher->player;
		int i;

		if (!player->backpack)
		{
			for (i = 0; i < NUMAMMO; i++)
				player->maxammo[i] *= 2;
			player->backpack = true;
		}
		for (i = 0; i < NUMAMMO; i++)
			P_GiveAmmo (player, (ammotype_t)i, clipammo[i]);
		return true;
	}
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

IMPLEMENT_ACTOR (AAmmoSatchel, Strife, 8, 144)
	PROP_Flags (MF_SPECIAL)
	PROP_SpawnState (0)
	PROP_Tag ("ammo_satchel")
END_DEFAULTS
