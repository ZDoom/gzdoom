#include "actor.h"
#include "p_local.h"
#include "s_sound.h"
#include "p_enemy.h"
#include "a_doomglobal.h"
#include "a_action.h"

void A_CyberAttack (AActor *self)
{
	if (!self->target)
		return;
				
	A_FaceTarget (self);
	P_SpawnMissile (self, self->target, PClass::FindClass("Rocket"));
}

void A_Hoof (AActor *self)
{
	S_Sound (self, CHAN_BODY, "cyber/hoof", 1, ATTN_IDLE);
	A_Chase (self);
}
