/*
#include "actor.h"
#include "m_random.h"
#include "a_action.h"
#include "p_local.h"
#include "p_enemy.h"
#include "s_sound.h"
#include "thingdef/thingdef.h"
*/

static FRandom pr_stalker ("Stalker");


DEFINE_ACTION_FUNCTION(AActor, A_StalkerChaseDecide)
{
	PARAM_ACTION_PROLOGUE;

	if (!(self->flags & MF_NOGRAVITY))
	{
		self->SetState (self->FindState("SeeFloor"));
	}
	else if (self->ceilingz > self->Top())
	{
		self->SetState (self->FindState("Drop"));
	}
	return 0;
}

DEFINE_ACTION_FUNCTION(AActor, A_StalkerLookInit)
{
	PARAM_ACTION_PROLOGUE;

	FState *state;
	if (self->flags & MF_NOGRAVITY)
	{
		state = self->FindState("LookCeiling");
	}
	else
	{
		state = self->FindState("LookFloor");
	}
	if (self->state->NextState != state)
	{
		self->SetState (state);
	}
	return 0;
}

DEFINE_ACTION_FUNCTION(AActor, A_StalkerDrop)
{
	PARAM_ACTION_PROLOGUE;

	self->flags5 &= ~MF5_NOVERTICALMELEERANGE;
	self->flags &= ~MF_NOGRAVITY;
	return 0;
}

DEFINE_ACTION_FUNCTION(AActor, A_StalkerAttack)
{
	PARAM_ACTION_PROLOGUE;

	if (self->flags & MF_NOGRAVITY)
	{
		self->SetState (self->FindState("Drop"));
	}
	else if (self->target != NULL)
	{
		A_FaceTarget (self);
		if (self->CheckMeleeRange ())
		{
			int damage = (pr_stalker() & 7) * 2 + 2;

			int newdam = P_DamageMobj (self->target, self, self, damage, NAME_Melee);
			P_TraceBleed (newdam > 0 ? newdam : damage, self->target, self);
		}
	}
	return 0;
}

DEFINE_ACTION_FUNCTION(AActor, A_StalkerWalk)
{
	PARAM_ACTION_PROLOGUE;

	S_Sound (self, CHAN_BODY, "stalker/walk", 1, ATTN_NORM);
	A_Chase (stack, self);
	return 0;
}

