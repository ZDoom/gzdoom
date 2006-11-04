#include "actor.h"
#include "s_sound.h"
#include "p_local.h"
#include "p_enemy.h"
#include "a_doomglobal.h"
#include "a_action.h"

void A_BspiAttack (AActor *self)
{		
	if (!self->target)
		return;

	A_FaceTarget (self);

	// launch a missile
	P_SpawnMissile (self, self->target, PClass::FindClass("ArachnotronPlasma"));
}

void A_BabyMetal (AActor *self)
{
	S_Sound (self, CHAN_BODY, "baby/walk", 1, ATTN_IDLE);
	A_Chase (self);
}
