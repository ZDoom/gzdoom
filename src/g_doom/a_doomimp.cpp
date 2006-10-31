#include "actor.h"
#include "info.h"
#include "m_random.h"
#include "s_sound.h"
#include "p_local.h"
#include "p_enemy.h"
#include "gstrings.h"
#include "a_action.h"

static FRandom pr_troopattack ("TroopAttack");

void A_TroopAttack (AActor *);

//
// A_TroopAttack
//
void A_TroopAttack (AActor *self)
{
	if (!self->target)
		return;
				
	A_FaceTarget (self);
	if (self->CheckMeleeRange ())
	{
		int damage = (pr_troopattack()%8+1)*3;
		S_Sound (self, CHAN_WEAPON, "imp/melee", 1, ATTN_NORM);
		P_DamageMobj (self->target, self, self, damage, NAME_Melee);
		P_TraceBleed (damage, self->target, self);
		return;
	}
	
	// launch a missile
	P_SpawnMissile (self, self->target, PClass::FindClass("DoomImpBall"));
}
