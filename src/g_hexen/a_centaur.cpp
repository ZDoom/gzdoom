/*
#include "actor.h"
#include "p_enemy.h"
#include "a_action.h"
#include "m_random.h"
#include "thingdef/thingdef.h"
*/

static FRandom pr_centaurdefend ("CentaurDefend");

//============================================================================
//
// A_CentaurDefend
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_CentaurDefend)
{
	PARAM_ACTION_PROLOGUE;

	A_FaceTarget (self);
	if (self->CheckMeleeRange() && pr_centaurdefend() < 32)
	{
		// This should unset REFLECTIVE as well
		// (unless you want the Centaur to reflect projectiles forever!)
		self->flags2&=~(MF2_REFLECTIVE|MF2_INVULNERABLE);
		self->SetState (self->MeleeState);
	}
	return 0;
}
