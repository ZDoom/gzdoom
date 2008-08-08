#include "actor.h"
#include "m_random.h"
#include "a_action.h"
#include "p_local.h"
#include "p_enemy.h"
#include "s_sound.h"
#include "a_strifeglobal.h"

static FRandom pr_templar ("Templar");

void A_TemplarAttack (AActor *self)
{
	int damage;
	angle_t angle;
	int pitch;
	int pitchdiff;

	if (self->target == NULL)
		return;

	S_Sound (self, CHAN_WEAPON, "templar/shoot", 1, ATTN_NORM);
	A_FaceTarget (self);
	pitch = P_AimLineAttack (self, self->angle, MISSILERANGE);

	for (int i = 0; i < 10; ++i)
	{
		damage = (pr_templar() & 4) * 2;
		angle = self->angle + (pr_templar.Random2() << 19);
		pitchdiff = pr_templar.Random2() * 332063;
		P_LineAttack (self, angle, MISSILERANGE+64*FRACUNIT, pitch+pitchdiff, damage, NAME_Disintegrate, NAME_MaulerPuff);
	}
}
