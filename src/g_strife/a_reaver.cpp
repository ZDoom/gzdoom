/*
#include "actor.h"
#include "p_enemy.h"
#include "a_action.h"
#include "s_sound.h"
#include "m_random.h"
#include "p_local.h"
#include "a_strifeglobal.h"
#include "thingdef/thingdef.h"
*/

static FRandom pr_reaverattack ("ReaverAttack");

DEFINE_ACTION_FUNCTION(AActor, A_ReaverRanged)
{
	if (self->target != NULL)
	{
		angle_t bangle;
		int pitch;

		A_FaceTarget (self);
		S_Sound (self, CHAN_WEAPON, "reaver/attack", 1, ATTN_NORM);
		bangle = self->angle;
		pitch = P_AimLineAttack (self, bangle, MISSILERANGE);

		for (int i = 0; i < 3; ++i)
		{
			angle_t angle = bangle + (pr_reaverattack.Random2() << 20);
			int damage = ((pr_reaverattack() & 7) + 1) * 3;
			P_LineAttack (self, angle, MISSILERANGE, pitch, damage, NAME_Hitscan, NAME_StrifePuff);
		}
	}
}
