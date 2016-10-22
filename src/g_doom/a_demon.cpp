/*
#include "actor.h"
#include "info.h"
#include "m_random.h"
#include "p_local.h"
#include "p_enemy.h"
#include "gstrings.h"
#include "a_action.h"
#include "vm.h"
*/

static FRandom pr_sargattack ("SargAttack");

DEFINE_ACTION_FUNCTION(AActor, A_SargAttack)
{
	PARAM_SELF_PROLOGUE(AActor);

	if (!self->target)
		return 0;
				
	A_FaceTarget (self);
	if (self->CheckMeleeRange ())
	{
		int damage = ((pr_sargattack()%10)+1)*4;
		int newdam = P_DamageMobj (self->target, self, self, damage, NAME_Melee);
		P_TraceBleed (newdam > 0 ? newdam : damage, self->target, self);
	}
	return 0;
}
