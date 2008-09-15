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
	if (!self->target)
		return;
				
	A_FaceTarget (self);
	P_SpawnMissile (self, self->target, PClass::FindClass("Rocket"));
}

DEFINE_ACTION_FUNCTION(AActor, A_Hoof)
{
	S_Sound (self, CHAN_BODY, "cyber/hoof", 1, ATTN_IDLE);
	A_Chase (self);
}
