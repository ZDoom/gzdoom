/*
#include "actor.h"
#include "s_sound.h"
#include "p_local.h"
#include "p_enemy.h"
#include "a_action.h"
#include "thingdef/thingdef.h"
*/

DEFINE_ACTION_FUNCTION(AActor, A_BspiAttack)
{		
	if (!self->target)
		return;

	A_FaceTarget (self);

	// launch a missile
	P_SpawnMissile (self, self->target, PClass::FindClass("ArachnotronPlasma"));
}

DEFINE_ACTION_FUNCTION(AActor, A_BabyMetal)
{
	S_Sound (self, CHAN_BODY, "baby/walk", 1, ATTN_IDLE);
	A_Chase (self);
}
