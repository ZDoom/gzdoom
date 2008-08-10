#include "actor.h"
#include "info.h"
#include "m_random.h"
#include "s_sound.h"
#include "p_local.h"
#include "p_enemy.h"
#include "doomstat.h"
#include "gstrings.h"
#include "a_action.h"
#include "thingdef/thingdef.h"

static FRandom pr_bruisattack ("BruisAttack");


DEFINE_ACTION_FUNCTION(AActor, A_BruisAttack)
{
	if (!self->target)
		return;
				
	if (self->CheckMeleeRange ())
	{
		int damage = (pr_bruisattack()%8+1)*10;
		S_Sound (self, CHAN_WEAPON, "baron/melee", 1, ATTN_NORM);
		P_DamageMobj (self->target, self, self, damage, NAME_Melee);
		P_TraceBleed (damage, self->target, self);
		return;
	}
	
	// launch a missile
	P_SpawnMissile (self, self->target, PClass::FindClass("BaronBall"));
}
