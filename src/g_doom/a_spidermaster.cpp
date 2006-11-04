#include "templates.h"
#include "actor.h"
#include "m_random.h"
#include "s_sound.h"
#include "p_local.h"
#include "p_enemy.h"
#include "a_action.h"

static FRandom pr_spidrefire ("SpidRefire");

void A_SpidRefire (AActor *self)
{
	// keep firing unless target got out of sight
	A_FaceTarget (self);

	if (pr_spidrefire() < 10)
		return;

	if (!self->target
		|| P_HitFriend (self)
		|| self->target->health <= 0
		|| !P_CheckSight (self, self->target, 0) )
	{
		self->SetState (self->SeeState);
	}
}

void A_Metal (AActor *self)
{
	S_Sound (self, CHAN_BODY, "spider/walk", 1, ATTN_IDLE);
	A_Chase (self);
}
