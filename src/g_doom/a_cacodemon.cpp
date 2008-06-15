#include "actor.h"
#include "info.h"
#include "m_random.h"
#include "p_local.h"
#include "p_enemy.h"
#include "gstrings.h"
#include "a_action.h"
#include "s_sound.h"

static FRandom pr_headattack ("HeadAttack");

void A_HeadAttack (AActor *self)
{
	if (!self->target)
		return;
				
	A_FaceTarget (self);
	if (self->CheckMeleeRange ())
	{
		int damage = (pr_headattack()%6+1)*10;
		S_Sound (self, CHAN_WEAPON, self->AttackSound, 1, ATTN_NORM);
		P_DamageMobj (self->target, self, self, damage, NAME_Melee);
		P_TraceBleed (damage, self->target, self);
		return;
	}
	
	// launch a missile
	P_SpawnMissile (self, self->target, PClass::FindClass("CacodemonBall"));
}
