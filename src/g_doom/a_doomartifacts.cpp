#include "info.h"
#include "a_pickups.h"
#include "d_player.h"
#include "p_local.h"
#include "gi.h"
#include "gstrings.h"
#include "s_sound.h"
#include "m_random.h"
#include "p_local.h"
#include "p_spec.h"
#include "p_lnspec.h"
#include "p_enemy.h"
#include "p_effect.h"

// Invulnerability Sphere ---------------------------------------------------

class AInvulnerabilitySphere : public APowerupGiver
{
	DECLARE_ACTOR (AInvulnerabilitySphere, APowerupGiver)
public:
	bool ShouldRespawn ();
protected:
	const char *PickupMessage ();
};

FState AInvulnerabilitySphere::States[] =
{
	S_BRIGHT (PINV, 'A',	6, NULL 				, &States[1]),
	S_BRIGHT (PINV, 'B',	6, NULL 				, &States[2]),
	S_BRIGHT (PINV, 'C',	6, NULL 				, &States[3]),
	S_BRIGHT (PINV, 'D',	6, NULL 				, &States[0])
};

IMPLEMENT_ACTOR (AInvulnerabilitySphere, Doom, 2022, 133)
	PROP_Flags (MF_SPECIAL|MF_COUNTITEM)
	PROP_SpawnState (0)
	PROP_Inventory_FlagsSet (IF_AUTOACTIVATE|IF_ALWAYSPICKUP)
	PROP_Inventory_MaxAmount (0)
	PROP_PowerupGiver_Powerup ("PowerInvulnerable")
END_DEFAULTS

const char *AInvulnerabilitySphere::PickupMessage ()
{
	return GStrings("GOTINVUL");
}

bool AInvulnerabilitySphere::ShouldRespawn ()
{
	return Super::ShouldRespawn () && (dmflags & DF_RESPAWN_SUPER);
}

// Soulsphere --------------------------------------------------------------

class ASoulsphere : public AInventory
{
	DECLARE_ACTOR (ASoulsphere, AInventory)
public:
	virtual bool Use (bool pickup)
	{
		player_t *player = Owner->player;
		player->health += deh.SoulsphereHealth;
		if (player->health > deh.MaxSoulsphere)
			player->health = deh.MaxSoulsphere;
		player->mo->health = player->health;
		return true;
	}
protected:
	virtual const char *PickupMessage ()
	{
		return GStrings("GOTSUPER");
	}
};

FState ASoulsphere::States[] =
{
	S_BRIGHT (SOUL, 'A',	6, NULL 				, &States[1]),
	S_BRIGHT (SOUL, 'B',	6, NULL 				, &States[2]),
	S_BRIGHT (SOUL, 'C',	6, NULL 				, &States[3]),
	S_BRIGHT (SOUL, 'D',	6, NULL 				, &States[4]),
	S_BRIGHT (SOUL, 'C',	6, NULL 				, &States[5]),
	S_BRIGHT (SOUL, 'B',	6, NULL 				, &States[0])
};

IMPLEMENT_ACTOR (ASoulsphere, Doom, 2013, 25)
	PROP_Flags (MF_SPECIAL|MF_COUNTITEM)
	PROP_Inventory_FlagsSet (IF_AUTOACTIVATE|IF_ALWAYSPICKUP|IF_FANCYPICKUPSOUND)
	PROP_Inventory_MaxAmount (0)
	PROP_SpawnState (0)
	PROP_Inventory_PickupSound ("misc/p_pkup")
END_DEFAULTS

// Mega sphere --------------------------------------------------------------

class AMegasphere : public APowerupGiver
{
	DECLARE_ACTOR (AMegasphere, APowerupGiver)
public:
	virtual bool Use (bool pickup)
	{
		player_t *player = Owner->player;

		ABasicArmorPickup *armor = static_cast<ABasicArmorPickup *> (Spawn ("BlueArmor", 0,0,0));
		if (!armor->TryPickup (Owner))
		{
			armor->Destroy ();
		}
		if (player->health < deh.MegasphereHealth)
		{
			player->health = deh.MegasphereHealth;
			player->mo->health = deh.MegasphereHealth;
		}
		return true;
	}
protected:
	virtual const char *PickupMessage ()
	{
		return GStrings("GOTMSPHERE");
	}
};

FState AMegasphere::States[] =
{
	S_BRIGHT (MEGA, 'A',	6, NULL 				, &States[1]),
	S_BRIGHT (MEGA, 'B',	6, NULL 				, &States[2]),
	S_BRIGHT (MEGA, 'C',	6, NULL 				, &States[3]),
	S_BRIGHT (MEGA, 'D',	6, NULL 				, &States[0])
};

IMPLEMENT_ACTOR (AMegasphere, Doom, 83, 132)
	PROP_Flags (MF_SPECIAL|MF_COUNTITEM)
	PROP_Inventory_FlagsSet (IF_AUTOACTIVATE|IF_ALWAYSPICKUP)
	PROP_Inventory_MaxAmount (0)
	PROP_SpawnState (0)
END_DEFAULTS

// Berserk ------------------------------------------------------------------

class ABerserk : public APowerupGiver
{
	DECLARE_ACTOR (ABerserk, APowerupGiver)
public:
	virtual bool Use (bool pickup)
	{
		P_GiveBody (Owner, -100);
		if (Super::Use (pickup))
		{
			const TypeInfo *fistType = TypeInfo::FindType ("Fist");
			if (Owner->player->ReadyWeapon == NULL ||
				Owner->player->ReadyWeapon->GetClass() != fistType)
			{
				AInventory *fist = Owner->FindInventory (fistType);
				if (fist != NULL)
				{
					Owner->player->PendingWeapon = static_cast<AWeapon *> (fist);
				}
			}
			return true;
		}
		return false;
	}
protected:
	virtual const char *PickupMessage ()
	{
		return GStrings("GOTBERSERK");
	}
};

FState ABerserk::States[] =
{
	S_BRIGHT (PSTR, 'A',   -1, NULL 				, NULL)
};

IMPLEMENT_ACTOR (ABerserk, Doom, 2023, 134)
	PROP_Flags (MF_SPECIAL|MF_COUNTITEM)
	PROP_SpawnState (0)
	PROP_Inventory_FlagsSet (IF_AUTOACTIVATE|IF_ALWAYSPICKUP)
	PROP_Inventory_MaxAmount (0)
	PROP_PowerupGiver_Powerup ("PowerStrength")
END_DEFAULTS

// Invisibility -------------------------------------------------------------

class ABlurSphere : public APowerupGiver
{
	DECLARE_ACTOR (ABlurSphere, APowerupGiver)
public:
	virtual void PostBeginPlay ()
	{
		Super::PostBeginPlay ();
		effects |= FX_VISIBILITYPULSE;
		visdir = -1;
	}
	virtual bool ShouldRespawn ()
	{
		return Super::ShouldRespawn () && (dmflags & DF_RESPAWN_SUPER);
	}
protected:
	virtual const char *PickupMessage ()
	{
		return GStrings("GOTINVIS");
	}
};

FState ABlurSphere::States[] =
{
	S_BRIGHT (PINS, 'A',	6, NULL 				, &States[1]),
	S_BRIGHT (PINS, 'B',	6, NULL 				, &States[2]),
	S_BRIGHT (PINS, 'C',	6, NULL 				, &States[3]),
	S_BRIGHT (PINS, 'D',	6, NULL 				, &States[0])
};

IMPLEMENT_ACTOR (ABlurSphere, Doom, 2024, 135)
	PROP_Flags (MF_SPECIAL|MF_COUNTITEM)
	PROP_RenderStyle (STYLE_Translucent)
	PROP_SpawnState (0)
	PROP_Inventory_FlagsSet (IF_AUTOACTIVATE|IF_ALWAYSPICKUP)
	PROP_Inventory_MaxAmount (0)
	PROP_PowerupGiver_Powerup ("PowerInvisibility")
END_DEFAULTS

// Radiation suit (aka iron feet) -------------------------------------------

class ARadSuit : public APowerupGiver
{
	DECLARE_ACTOR (ARadSuit, APowerupGiver)
protected:
	virtual const char *PickupMessage ()
	{
		return GStrings("GOTSUIT");
	}
};

FState ARadSuit::States[] =
{
	S_BRIGHT (SUIT, 'A',   -1, NULL 				, NULL)
};

IMPLEMENT_ACTOR (ARadSuit, Doom, 2025, 136)
	PROP_HeightFixed (46)
	PROP_Flags (MF_SPECIAL)
	PROP_SpawnState (0)
	PROP_Inventory_FlagsSet (IF_AUTOACTIVATE|IF_ALWAYSPICKUP)
	PROP_Inventory_MaxAmount (0)
	PROP_PowerupGiver_Powerup ("PowerIronFeet")
END_DEFAULTS

// infrared -----------------------------------------------------------------

class AInfrared : public APowerupGiver
{
	DECLARE_ACTOR (AInfrared, APowerupGiver)
protected:
	virtual const char *PickupMessage ()
	{
		return GStrings("GOTVISOR");
	}
};

FState AInfrared::States[] =
{
	S_BRIGHT (PVIS, 'A',	6, NULL 				, &States[1]),
	S_NORMAL (PVIS, 'B',	6, NULL 				, &States[0])
};

IMPLEMENT_ACTOR (AInfrared, Doom, 2045, 138)
	PROP_Flags (MF_SPECIAL|MF_COUNTITEM)
	PROP_SpawnState (0)
	PROP_Inventory_FlagsSet (IF_AUTOACTIVATE|IF_ALWAYSPICKUP)
	PROP_Inventory_MaxAmount (0)
	PROP_PowerupGiver_Powerup ("PowerLightAmp")
END_DEFAULTS

// Allmap -------------------------------------------------------------------

class AAllmap : public AMapRevealer
{
	DECLARE_ACTOR (AAllmap, AMapRevealer)
protected:
	const char *PickupMessage ()
	{
		return GStrings("GOTMAP");
	}
};

FState AAllmap::States[] =
{
	S_BRIGHT (PMAP, 'A',	6, NULL 				, &States[1]),
	S_BRIGHT (PMAP, 'B',	6, NULL 				, &States[2]),
	S_BRIGHT (PMAP, 'C',	6, NULL 				, &States[3]),
	S_BRIGHT (PMAP, 'D',	6, NULL 				, &States[4]),
	S_BRIGHT (PMAP, 'C',	6, NULL 				, &States[5]),
	S_BRIGHT (PMAP, 'B',	6, NULL 				, &States[0])
};

IMPLEMENT_ACTOR (AAllmap, Doom, 2026, 137)
	PROP_Flags (MF_SPECIAL|MF_COUNTITEM)
	PROP_Inventory_FlagsSet (IF_ALWAYSPICKUP|IF_FANCYPICKUPSOUND)
	PROP_Inventory_MaxAmount (0)
	PROP_SpawnState (0)
	PROP_Inventory_PickupSound ("misc/p_pkup")
END_DEFAULTS
