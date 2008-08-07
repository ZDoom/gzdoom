#include "actor.h"
#include "m_random.h"
#include "a_action.h"
#include "p_local.h"
#include "p_enemy.h"
#include "s_sound.h"

static FRandom pr_stalker ("Stalker");


void A_StalkerChaseDecide (AActor *self)
{
	if (!(self->flags & MF_NOGRAVITY))
	{
		self->SetState (self->FindState("SeeFloor"));
	}
	else if (self->ceilingz - self->height > self->z)
	{
		self->SetState (self->FindState("Drop"));
	}
}

void A_StalkerLookInit (AActor *self)
{
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
}

void A_StalkerDrop (AActor *self)
{
	self->flags5 &= ~MF5_NOVERTICALMELEERANGE;
	self->flags &= ~MF_NOGRAVITY;
}

void A_StalkerAttack (AActor *self)
{
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

			P_DamageMobj (self->target, self, self, damage, NAME_Melee);
			P_TraceBleed (damage, self->target, self);
		}
	}
}

void A_StalkerWalk (AActor *self)
{
	S_Sound (self, CHAN_BODY, "stalker/walk", 1, ATTN_NORM);
	A_Chase (self);
}

