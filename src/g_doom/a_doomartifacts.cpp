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

class AInvulnerabilitySphere : public APowerup
{
	DECLARE_ACTOR (AInvulnerabilitySphere, APowerup)
public:
	bool TryPickup (AActor *toucher);
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
END_DEFAULTS

bool AInvulnerabilitySphere::TryPickup (AActor *toucher)
{
	return P_GivePower (toucher->player, pw_invulnerability);
}

const char *AInvulnerabilitySphere::PickupMessage ()
{
	return GStrings(GOTINVUL);
}

bool AInvulnerabilitySphere::ShouldRespawn ()
{
	return Super::ShouldRespawn () && (dmflags & DF_RESPAWN_SUPER);
}

// Soulsphere --------------------------------------------------------------

class ASoulsphere : public APowerup
{
	DECLARE_ACTOR (ASoulsphere, APowerup)
public:
	virtual bool TryPickup (AActor *toucher)
	{
		player_t *player = toucher->player;
		player->health += deh.SoulsphereHealth;
		if (player->health > deh.MaxSoulsphere)
			player->health = deh.MaxSoulsphere;
		player->mo->health = player->health;
		return true;
	}
protected:
	virtual const char *PickupMessage ()
	{
		return GStrings(GOTSUPER);
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
	PROP_SpawnState (0)
END_DEFAULTS

// Mega sphere --------------------------------------------------------------

class AMegasphere : public APowerup
{
	DECLARE_ACTOR (AMegasphere, APowerup)
public:
	virtual bool TryPickup (AActor *toucher)
	{
		bool gotarmor, gothealth;
		player_t *player = toucher->player;

		gotarmor = P_GiveArmor (player, (armortype_t)-deh.BlueAC, 100*deh.BlueAC);
		if ((gothealth = player->health < deh.MegasphereHealth))
		{
			player->health = deh.MegasphereHealth;
			player->mo->health = deh.MegasphereHealth;
		}

		// Use the commented-out line if you don't want to be able to get a megasphere
		// that won't do you any good.
		//return gotarmor || gothealth;
		return true;
	}
protected:
	virtual const char *PickupMessage ()
	{
		return GStrings(GOTMSPHERE);
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
	PROP_SpawnState (0)
END_DEFAULTS

// Berserk ------------------------------------------------------------------

class ABerserk : public APowerup
{
	DECLARE_ACTOR (ABerserk, APowerup)
public:
	virtual bool TryPickup (AActor *toucher)
	{
		if (P_GivePower (toucher->player, pw_strength))
		{
			if (toucher->player->readyweapon != wp_fist)
				toucher->player->pendingweapon = wp_fist;
			return true;
		}
		return false;
	}
protected:
	virtual const char *PickupMessage ()
	{
		return GStrings(GOTBERSERK);
	}
};

FState ABerserk::States[] =
{
	S_BRIGHT (PSTR, 'A',   -1, NULL 				, NULL)
};

IMPLEMENT_ACTOR (ABerserk, Doom, 2023, 134)
	PROP_Flags (MF_SPECIAL|MF_COUNTITEM)
	PROP_SpawnState (0)
END_DEFAULTS

// Invisibility -------------------------------------------------------------

class ABlurSphere : public APowerup
{
	DECLARE_ACTOR (ABlurSphere, APowerup)
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
	virtual bool TryPickup (AActor *toucher)
	{
		return P_GivePower (toucher->player, pw_invisibility);
	}
protected:
	virtual const char *PickupMessage ()
	{
		return GStrings(GOTINVIS);
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
END_DEFAULTS

// Radiation suit (aka iron feet) -------------------------------------------

class ARadSuit : public APowerup
{
	DECLARE_ACTOR (ARadSuit, APowerup)
public:
	virtual bool TryPickup (AActor *toucher)
	{
		return P_GivePower (toucher->player, pw_ironfeet);
	}
protected:
	virtual const char *PickupMessage ()
	{
		return GStrings(GOTSUIT);
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
END_DEFAULTS

// infrared -----------------------------------------------------------------

class AInfrared : public APowerup
{
	DECLARE_ACTOR (AInfrared, APowerup)
public:
	virtual bool TryPickup (AActor *toucher)
	{
		return P_GivePower (toucher->player, pw_infrared);
	}
protected:
	virtual const char *PickupMessage ()
	{
		return GStrings(GOTVISOR);
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
END_DEFAULTS

// Allmap -------------------------------------------------------------------

// Note that the allmap is a subclass of pickup, not powerup, because we
// always want it to be activated immediately on pickup.

class AAllmap : public AInventory
{
	DECLARE_ACTOR (AAllmap, AInventory)
public:
	virtual bool TryPickup (AActor *toucher)
	{
		return P_GivePower (toucher->player, pw_allmap);
	}
protected:
	virtual const char *PickupMessage ()
	{
		return GStrings(GOTMAP);
	}
	void PlayPickupSound (AActor *toucher)
	{
		S_Sound (toucher, CHAN_PICKUP, "misc/p_pkup", 1,
			toucher == NULL || toucher == players[consoleplayer].camera
			? ATTN_SURROUND : ATTN_NORM);
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
	PROP_SpawnState (0)
END_DEFAULTS

// Backpack -----------------------------------------------------------------

// The backpack is also a pickup, because there's not much point to carrying
// a backpack around unused.

class ABackpack : public AInventory
{
	DECLARE_ACTOR (ABackpack, AInventory)
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
		return GStrings(GOTBACKPACK);
	}
};

FState ABackpack::States[] =
{
	S_NORMAL (BPAK, 'A',   -1, NULL 						, NULL)
};

IMPLEMENT_ACTOR (ABackpack, Doom, 8, 144)
	PROP_HeightFixed (26)
	PROP_Flags (MF_SPECIAL)
	PROP_SpawnState (0)
END_DEFAULTS
