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

//============================================================================
//
// A_MWandAttack
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_MWandAttack)
{
	AActor *mo;

	mo = P_SpawnPlayerMissile (self, PClass::FindClass("MageWandMissile"));
	S_Sound (self, CHAN_WEAPON, "MageWandFire", 1, ATTN_NORM);
}
