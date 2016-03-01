/*
#include "actor.h"
#include "p_local.h"
#include "s_sound.h"
#include "p_enemy.h"
#include "a_action.h"
#include "thingdef/thingdef.h"
*/

DEFINE_ACTION_FUNCTION(AActor, A_CyberAttack)
{
	PARAM_ACTION_PROLOGUE;

	if (!self->target)
		return 0;
				
	A_FaceTarget (self);
	P_SpawnMissile (self, self->target, PClass::FindActor("Rocket"));
	return 0;
}

DEFINE_ACTION_FUNCTION(AActor, A_Hoof)
{
	PARAM_ACTION_PROLOGUE;

	S_Sound (self, CHAN_BODY, "cyber/hoof", 1, ATTN_IDLE);
	A_Chase (stack, self);
	return 0;
}
