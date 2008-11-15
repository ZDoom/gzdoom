/*
#include "actor.h"
#include "gi.h"
#include "m_random.h"
#include "s_sound.h"
#include "d_player.h"
#include "a_action.h"
#include "p_local.h"
#include "a_action.h"
#include "p_pspr.h"
#include "gstrings.h"
#include "a_hexenglobal.h"
#include "thingdef/thingdef.h"
*/

static FRandom pr_smoke ("MWandSmoke");

void A_MWandAttack (AActor *actor);

// Wand Missile -------------------------------------------------------------

class AMageWandMissile : public AFastProjectile
{
	DECLARE_CLASS(AMageWandMissile, AFastProjectile)
public:
	void Effect ();
};

IMPLEMENT_CLASS (AMageWandMissile)

void AMageWandMissile::Effect ()
{
	fixed_t hitz;

	//if (pr_smoke() < 128)	// [RH] I think it looks better if it's consistent
	{
		hitz = z-8*FRACUNIT;
		if (hitz < floorz)
		{
			hitz = floorz;
		}
		Spawn ("MageWandSmoke", x, y, hitz, ALLOW_REPLACE);
	}
}

//============================================================================
//
// A_MWandAttack
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_MWandAttack)
{
	AActor *mo;

	mo = P_SpawnPlayerMissile (self, RUNTIME_CLASS(AMageWandMissile));
	S_Sound (self, CHAN_WEAPON, "MageWandFire", 1, ATTN_NORM);
}
