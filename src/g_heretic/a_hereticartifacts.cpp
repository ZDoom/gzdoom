#include "info.h"
#include "a_pickups.h"
#include "a_artifacts.h"
#include "gstrings.h"
#include "p_local.h"
#include "p_enemy.h"
#include "s_sound.h"

// Tome of power ------------------------------------------------------------

class AArtiTomeOfPower : public APowerupGiver
{
	DECLARE_ACTOR (AArtiTomeOfPower, APowerupGiver)
public:
	bool Use (bool pickup);
};

FState AArtiTomeOfPower::States[] =
{
	S_NORMAL (PWBK, 'A',  350, NULL, &States[0])
};

IMPLEMENT_ACTOR (AArtiTomeOfPower, Heretic, 86, 134)
	PROP_Flags (MF_SPECIAL|MF_COUNTITEM)
	PROP_Flags2 (MF2_FLOATBOB)
	PROP_SpawnState (0)
	PROP_Inventory_FlagsSet (IF_PICKUPFLASH)
	PROP_Inventory_Icon ("ARTIPWBK")
	PROP_PowerupGiver_Powerup ("PowerWeaponLevel2")
	PROP_Inventory_PickupMessage("$TXT_ARTITOMEOFPOWER")
END_DEFAULTS

bool AArtiTomeOfPower::Use (bool pickup)
{
	if (Owner->player->morphTics)
	{ // Attempt to undo chicken
		if (P_UndoPlayerMorph (Owner->player) == false)
		{ // Failed
			P_DamageMobj (Owner, NULL, NULL, 1000000, NAME_Telefrag);
		}
		else
		{ // Succeeded
			Owner->player->morphTics = 0;
			S_Sound (Owner, CHAN_VOICE, "*evillaugh", 1, ATTN_IDLE);
		}
		return true;
	}
	else
	{
		return Super::Use (pickup);
	}
}

// Time bomb ----------------------------------------------------------------

class AActivatedTimeBomb : public AActor
{
	DECLARE_ACTOR (AActivatedTimeBomb, AActor)
public:
	void PreExplode ()
	{
		z += 32*FRACUNIT;
		alpha = OPAQUE;
	}
};

FState AActivatedTimeBomb::States[] =
{
	S_NORMAL (FBMB, 'A',   10, NULL 	, &States[1]),
	S_NORMAL (FBMB, 'B',   10, NULL 	, &States[2]),
	S_NORMAL (FBMB, 'C',   10, NULL 	, &States[3]),
	S_NORMAL (FBMB, 'D',   10, NULL 	, &States[4]),
	S_NORMAL (FBMB, 'E',	6, A_Scream , &States[5]),
	S_BRIGHT (XPL1, 'A',	4, A_Explode, &States[6]),
	S_BRIGHT (XPL1, 'B',	4, NULL 	, &States[7]),
	S_BRIGHT (XPL1, 'C',	4, NULL 	, &States[8]),
	S_BRIGHT (XPL1, 'D',	4, NULL 	, &States[9]),
	S_BRIGHT (XPL1, 'E',	4, NULL 	, &States[10]),
	S_BRIGHT (XPL1, 'F',	4, NULL 	, NULL)
};

IMPLEMENT_ACTOR (AActivatedTimeBomb, Heretic, -1, 72)
	PROP_Flags (MF_NOGRAVITY)
	PROP_RenderStyle (STYLE_Translucent)
	PROP_Alpha (HR_SHADOW)

	PROP_SpawnState (0)

	PROP_DeathSound ("misc/timebomb")
END_DEFAULTS

class AArtiTimeBomb : public AInventory
{
	DECLARE_ACTOR (AArtiTimeBomb, AInventory)
public:
	bool Use (bool pickup);
};

FState AArtiTimeBomb::States[] =
{
	S_NORMAL (FBMB, 'E',  350, NULL, &States[0]),
};

IMPLEMENT_ACTOR (AArtiTimeBomb, Heretic, 34, 72)
	PROP_Flags (MF_SPECIAL|MF_COUNTITEM)
	PROP_Flags2 (MF2_FLOATBOB)
	PROP_SpawnState (0)
	PROP_Inventory_DefMaxAmount
	PROP_Inventory_FlagsSet (IF_INVBAR|IF_PICKUPFLASH|IF_FANCYPICKUPSOUND)
	PROP_Inventory_Icon ("ARTIFBMB")
	PROP_Inventory_PickupSound ("misc/p_pkup")
	PROP_Inventory_PickupMessage("$TXT_ARTIFIREBOMB")
END_DEFAULTS

bool AArtiTimeBomb::Use (bool pickup)
{
	angle_t angle = Owner->angle >> ANGLETOFINESHIFT;
	AActor *mo = Spawn<AActivatedTimeBomb> (
		Owner->x + 24*finecosine[angle],
		Owner->y + 24*finesine[angle],
		Owner->z - Owner->floorclip, ALLOW_REPLACE);
	mo->target = Owner;
	return true;
}

