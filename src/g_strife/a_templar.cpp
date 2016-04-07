/*
#include "actor.h"
#include "m_random.h"
#include "a_action.h"
#include "p_local.h"
#include "p_enemy.h"
#include "s_sound.h"
#include "a_strifeglobal.h"
#include "thingdef/thingdef.h"
*/

static FRandom pr_templar ("Templar");

DEFINE_ACTION_FUNCTION(AActor, A_TemplarAttack)
{
	PARAM_ACTION_PROLOGUE;

	int damage;
	DAngle angle;
	DAngle pitch;

	if (self->target == NULL)
		return 0;

	S_Sound (self, CHAN_WEAPON, "templar/shoot", 1, ATTN_NORM);
	A_FaceTarget (self);
	pitch = P_AimLineAttack (self, self->Angles.Yaw, MISSILERANGE);

	for (int i = 0; i < 10; ++i)
	{
		damage = (pr_templar() & 4) * 2;
		angle = self->Angles.Yaw + pr_templar.Random2() * (11.25 / 256);
		P_LineAttack (self, angle, MISSILERANGE+64., pitch + pr_templar.Random2() * (7.097 / 256), damage, NAME_Hitscan, NAME_MaulerPuff);
	}
	return 0;
}
