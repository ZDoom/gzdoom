/*
#include "templates.h"
#include "actor.h"
#include "m_random.h"
#include "s_sound.h"
#include "p_local.h"
#include "p_enemy.h"
#include "a_action.h"
#include "thingdef/thingdef.h"
*/

static FRandom pr_spidrefire ("SpidRefire");

DEFINE_ACTION_FUNCTION(AActor, A_SpidRefire)
{
	PARAM_ACTION_PROLOGUE;

	// keep firing unless target got out of sight
	A_FaceTarget (self);

	if (pr_spidrefire() < 10)
		return 0;

	if (!self->target
		|| P_HitFriend (self)
		|| self->target->health <= 0
		|| !P_CheckSight (self, self->target, SF_SEEPASTBLOCKEVERYTHING|SF_SEEPASTSHOOTABLELINES))
	{
		self->SetState (self->SeeState);
	}
	return 0;
}

DEFINE_ACTION_FUNCTION(AActor, A_Metal)
{
	PARAM_ACTION_PROLOGUE;

	S_Sound (self, CHAN_BODY, "spider/walk", 1, ATTN_IDLE);
	A_Chase (stack, self);
	return 0;
}
