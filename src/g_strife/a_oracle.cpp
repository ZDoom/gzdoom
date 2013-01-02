/*
#include "actor.h"
#include "a_action.h"
#include "a_strifeglobal.h"
#include "p_enemy.h"
#include "r_defs.h"
#include "thingdef/thingdef.h"
*/


DEFINE_ACTION_FUNCTION(AActor, A_WakeOracleSpectre)
{
	TThinkerIterator<AActor> it(NAME_AlienSpectre3);
	AActor *spectre = it.Next();

	if (spectre != NULL && spectre->health > 0 && self->target != spectre)
	{
		spectre->Sector->SoundTarget = spectre->LastHeard = self->LastHeard;
		spectre->target = self->target;
		spectre->SetState (spectre->SeeState);
	}
}

