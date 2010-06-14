/*
#include "actor.h"
#include "info.h"
#include "p_local.h"
#include "p_spec.h"
#include "p_enemy.h"
#include "a_action.h"
#include "thingdef/thingdef.h"
*/

//
// A_KeenDie
// DOOM II special, map 32.
// Uses special tag 666 by default.
//
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_KeenDie)
{
	A_Unblock(self, false);
	
	// scan the remaining thinkers to see if all Keens are dead
	AActor *other;
	TThinkerIterator<AActor> iterator;
	const PClass *matchClass = self->GetClass ();

	while ( (other = iterator.Next ()) )
	{
		if (other != self && other->health > 0 && other->IsA (matchClass))
		{
			// other Keen not dead
			return;
		}
	}

	ACTION_PARAM_START(1);
	ACTION_PARAM_INT(doortag, 0);

	EV_DoDoor (DDoor::doorOpen, NULL, NULL, doortag, 2*FRACUNIT, 0, 0, 0);
}


