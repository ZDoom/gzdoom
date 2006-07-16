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

// Mega sphere --------------------------------------------------------------

class AMegasphere : public APowerupGiver
{
	DECLARE_ACTOR (AMegasphere, APowerupGiver)
public:
	virtual bool Use (bool pickup)
	{
		player_t *player = Owner->player;

		ABasicArmorPickup *armor = static_cast<ABasicArmorPickup *> (Spawn ("BlueArmor", 0,0,0, NO_REPLACE));
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
	PROP_Inventory_PickupMessage("$GOTMSPHERE")
END_DEFAULTS

// Berserk ------------------------------------------------------------------

class ABerserk : public APowerupGiver
{
	DECLARE_ACTOR (ABerserk, APowerupGiver)
public:
	virtual bool Use (bool pickup)
	{
		P_GiveBody (Owner, 100);
		if (Super::Use (pickup))
		{
			const PClass *fistType = PClass::FindClass ("Fist");
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
	PROP_Inventory_PickupMessage("$GOTBERSERK")
END_DEFAULTS

