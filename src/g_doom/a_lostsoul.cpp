/*
#include "templates.h"
#include "actor.h"
#include "info.h"
#include "m_random.h"
#include "s_sound.h"
#include "p_local.h"
#include "p_enemy.h"
#include "gi.h"
#include "gstrings.h"
#include "a_action.h"
#include "thingdef/thingdef.h"
*/

 FRandom pr_oldsoul ("BetaLostSoul");

//
// SkullAttack
// Fly at the player like a missile.
//

void A_SkullAttack(AActor *self, fixed_t speed)
{
	AActor *dest;
	angle_t an;
	int dist;

	if (!self->target)
		return;
				
	dest = self->target;		
	self->flags |= MF_SKULLFLY;

	S_Sound (self, CHAN_VOICE, self->AttackSound, 1, ATTN_NORM);
	A_FaceTarget (self);
	an = self->angle >> ANGLETOFINESHIFT;
	self->velx = FixedMul (speed, finecosine[an]);
	self->vely = FixedMul (speed, finesine[an]);
	dist = self->AproxDistance (dest);
	dist = dist / speed;
	
	if (dist < 1)
		dist = 1;
	self->velz = (dest->Z() + (dest->height>>1) - self->Z()) / dist;
}

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_SkullAttack)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_FIXED_OPT(speed) { speed = SKULLSPEED; }

	if (speed <= 0)
		speed = SKULLSPEED;
	A_SkullAttack(self, speed);
	return 0;
}

DEFINE_ACTION_FUNCTION(AActor, A_BetaSkullAttack)
{
	PARAM_ACTION_PROLOGUE;

	int damage;
	if (!self || !self->target || self->target->GetSpecies() == self->GetSpecies())
		return 0;
	S_Sound (self, CHAN_WEAPON, self->AttackSound, 1, ATTN_NORM);
	A_FaceTarget(self);
	damage = (pr_oldsoul()%8+1)*self->GetMissileDamage(0,1);
	P_DamageMobj(self->target, self, self, damage, NAME_None);
	return 0;
}



//==========================================================================
//
// CVAR transsouls
//
// How translucent things drawn with STYLE_SoulTrans are. Normally, only
// Lost Souls have this render style, but a dehacked patch could give other
// things this style. Values less than 0.25 will automatically be set to
// 0.25 to ensure some degree of visibility. Likewise, values above 1.0 will
// be set to 1.0, because anything higher doesn't make sense.
//
//==========================================================================

CUSTOM_CVAR (Float, transsouls, 0.75f, CVAR_ARCHIVE)
{
	if (self < 0.25f)
	{
		self = 0.25f;
	}
	else if (self > 1.f)
	{
		self = 1.f;
	}
}
