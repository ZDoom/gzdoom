#include "info.h"
#include "a_pickups.h"
#include "a_artifacts.h"
#include "gstrings.h"
#include "p_local.h"
#include "p_enemy.h"
#include "s_sound.h"
#include "ravenshared.h"

void A_Summon (AActor *);

// Dark Servant Artifact ----------------------------------------------------

class AArtiDarkServant : public AInventory
{
	DECLARE_ACTOR (AArtiDarkServant, AInventory)
public:
	bool Use (bool pickup);
};

FState AArtiDarkServant::States[] =
{
#define S_ARTI_SUMMON 0
	S_NORMAL (SUMN, 'A',  350, NULL					    , &States[S_ARTI_SUMMON]),
};

IMPLEMENT_ACTOR (AArtiDarkServant, Hexen, 86, 16)
	PROP_Flags (MF_SPECIAL|MF_COUNTITEM)
	PROP_Flags2 (MF2_FLOATBOB)
	PROP_SpawnState (S_ARTI_SUMMON)
	PROP_Inventory_RespawnTics (30+4200)
	PROP_Inventory_DefMaxAmount
	PROP_Inventory_PickupFlash (1)
	PROP_Inventory_FlagsSet (IF_INVBAR|IF_FANCYPICKUPSOUND)
	PROP_Inventory_Icon ("ARTISUMN")
	PROP_Inventory_PickupSound ("misc/p_pkup")
	PROP_Inventory_PickupMessage("$TXT_ARTISUMMON")
END_DEFAULTS


//============================================================================
//
// Activate the summoning artifact
//
//============================================================================

bool AArtiDarkServant::Use (bool pickup)
{
	AActor *mo = P_SpawnPlayerMissile (Owner, PClass::FindClass ("SummoningDoll"));
	if (mo)
	{
		mo->target = Owner;
		mo->tracer = Owner;
		mo->momz = 5*FRACUNIT;
	}
	return true;
}

//============================================================================
//
// A_Summon
//
//============================================================================

void A_Summon (AActor *actor)
{
	AMinotaurFriend *mo;

	mo = Spawn<AMinotaurFriend> (actor->x, actor->y, actor->z, ALLOW_REPLACE);
	if (mo)
	{
		if (P_TestMobjLocation(mo) == false || !actor->tracer)
		{ // Didn't fit - change back to artifact
			mo->Destroy ();
			AActor *arti = Spawn<AArtiDarkServant> (actor->x, actor->y, actor->z, ALLOW_REPLACE);
			if (arti) arti->flags |= MF_DROPPED;
			return;
		}

		mo->StartTime = level.maptime;
		if (actor->tracer->flags & MF_CORPSE)
		{	// Master dead
			mo->tracer = NULL;		// No master
		}
		else
		{
			mo->tracer = actor->tracer;		// Pointer to master
			AInventory *power = Spawn<APowerMinotaur> (0, 0, 0, NO_REPLACE);
			power->TryPickup (actor->tracer);
			if (actor->tracer->player != NULL)
			{
				mo->FriendPlayer = int(actor->tracer->player - players + 1);
			}
		}

		// Make smoke puff
		Spawn ("MinotaurSmoke", actor->x, actor->y, actor->z, ALLOW_REPLACE);
		S_Sound (actor, CHAN_VOICE, mo->ActiveSound, 1, ATTN_NORM);
	}
}
