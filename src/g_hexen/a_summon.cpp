/*
#include "info.h"
#include "a_pickups.h"
#include "a_artifacts.h"
#include "gstrings.h"
#include "p_local.h"
#include "s_sound.h"
#include "ravenshared.h"
#include "thingdef/thingdef.h"
#include "g_level.h"
*/

void A_Summon (AActor *);

// Dark Servant Artifact ----------------------------------------------------

class AArtiDarkServant : public AInventory
{
	DECLARE_CLASS (AArtiDarkServant, AInventory)
public:
	bool Use (bool pickup);
};

IMPLEMENT_CLASS (AArtiDarkServant)

//============================================================================
//
// Activate the summoning artifact
//
//============================================================================

bool AArtiDarkServant::Use (bool pickup)
{
	AActor *mo = P_SpawnPlayerMissile (Owner, PClass::FindActor("SummoningDoll"));
	if (mo)
	{
		mo->target = Owner;
		mo->tracer = Owner;
		mo->Vel.Z = 5;
	}
	return true;
}

//============================================================================
//
// A_Summon
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_Summon)
{
	PARAM_ACTION_PROLOGUE;

	AMinotaurFriend *mo;

	mo = Spawn<AMinotaurFriend>(self->Pos(), ALLOW_REPLACE);
	if (mo)
	{
		if (P_TestMobjLocation(mo) == false || !self->tracer)
		{ // Didn't fit - change back to artifact
			mo->Destroy();
			AActor *arti = Spawn<AArtiDarkServant>(self->Pos(), ALLOW_REPLACE);
			if (arti) arti->flags |= MF_DROPPED;
			return 0;
		}

		mo->StartTime = level.maptime;
		if (self->tracer->flags & MF_CORPSE)
		{	// Master dead
			mo->tracer = NULL;		// No master
		}
		else
		{
			mo->tracer = self->tracer;		// Pointer to master
			AInventory *power = Spawn<APowerMinotaur>();
			power->CallTryPickup(self->tracer);
			mo->SetFriendPlayer(self->tracer->player);
		}

		// Make smoke puff
		Spawn("MinotaurSmoke", self->Pos(), ALLOW_REPLACE);
		S_Sound(self, CHAN_VOICE, mo->ActiveSound, 1, ATTN_NORM);
	}
	return 0;
}
