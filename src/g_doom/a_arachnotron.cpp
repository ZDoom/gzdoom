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
	PARAM_ACTION_PROLOGUE;

	if (!self->target)
		return 0;

	A_FaceTarget (self);

	// launch a missile
	P_SpawnMissile (self, self->target, PClass::FindActor("ArachnotronPlasma"));
	return 0;
}

DEFINE_ACTION_FUNCTION(AActor, A_BabyMetal)
{
	PARAM_ACTION_PROLOGUE;
	S_Sound (self, CHAN_BODY, "baby/walk", 1, ATTN_IDLE);
	A_Chase (stack, self);
	return 0;
}
