#include "actor.h"
#include "p_enemy.h"
#include "a_action.h"
#include "m_random.h"

static FRandom pr_centaurdefend ("CentaurDefend");

//============================================================================
//
// A_CentaurDefend
//
//============================================================================

void A_CentaurDefend (AActor *actor)
{
	A_FaceTarget (actor);
	if (actor->CheckMeleeRange() && pr_centaurdefend() < 32)
	{
		// This should unset REFLECTIVE as well
		// (unless you want the Centaur to reflect projectiles forever!)
		A_UnSetReflectiveInvulnerable (actor);
		actor->SetState (actor->MeleeState);
	}
}
