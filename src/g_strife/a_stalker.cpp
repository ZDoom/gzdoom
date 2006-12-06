#include "actor.h"
#include "m_random.h"
#include "a_action.h"
#include "p_local.h"
#include "p_enemy.h"
#include "s_sound.h"

static FRandom pr_stalker ("Stalker");

void A_StalkerLookInit (AActor *);
void A_StalkerChaseDecide (AActor *);
void A_StalkerWalk (AActor *);
void A_StalkerAttack (AActor *);
void A_StalkerDrop (AActor *);

// Stalker ------------------------------------------------------------------

class AStalker : public AActor
{
	DECLARE_ACTOR (AStalker, AActor)
public:
	bool CheckMeleeRange ();
};

FState AStalker::States[] =
{
#define S_STALK_SPAWN 0
	S_NORMAL (STLK, 'A',  1, A_StalkerLookInit,		&States[S_STALK_SPAWN]),

#define S_STALK_STND_CEIL (S_STALK_SPAWN+1)
	S_NORMAL (STLK, 'A', 10, A_Look,				&States[S_STALK_STND_CEIL]),

#define S_STALK_STND_FLOOR (S_STALK_STND_CEIL+1)
	S_NORMAL (STLK, 'J', 10, A_Look,				&States[S_STALK_STND_FLOOR]),

#define S_STALK_CHASE (S_STALK_STND_FLOOR+1)
	S_NORMAL (STLK, 'A',  1, A_StalkerChaseDecide,	&States[S_STALK_CHASE+1]),
	S_NORMAL (STLK, 'A',  3, A_Chase,				&States[S_STALK_CHASE+2]),
	S_NORMAL (STLK, 'B',  3, A_Chase,				&States[S_STALK_CHASE+3]),
	S_NORMAL (STLK, 'B',  3, A_Chase,				&States[S_STALK_CHASE+4]),
	S_NORMAL (STLK, 'C',  3, A_StalkerWalk,			&States[S_STALK_CHASE+5]),
	S_NORMAL (STLK, 'C',  3, A_Chase,				&States[S_STALK_CHASE]),

#define S_STALK_ATK (S_STALK_CHASE+6)
	S_NORMAL (STLK, 'J',  3, A_FaceTarget,			&States[S_STALK_ATK+1]),
	S_NORMAL (STLK, 'K',  3, A_StalkerAttack,		&States[S_STALK_ATK+2]),

#define S_STALK_GROUND_CHASE (S_STALK_ATK+2)
	S_NORMAL (STLK, 'J',  3, A_StalkerWalk,			&States[S_STALK_GROUND_CHASE+1]),
	S_NORMAL (STLK, 'K',  3, A_Chase,				&States[S_STALK_GROUND_CHASE+2]),
	S_NORMAL (STLK, 'K',  3, A_Chase,				&States[S_STALK_GROUND_CHASE+3]),
	S_NORMAL (STLK, 'L',  3, A_StalkerWalk,			&States[S_STALK_GROUND_CHASE+4]),
	S_NORMAL (STLK, 'L',  3, A_Chase,				&States[S_STALK_GROUND_CHASE]),

#define S_STALK_PAIN (S_STALK_GROUND_CHASE+5)
	S_NORMAL (STLK, 'L',  1, A_Pain,				&States[S_STALK_CHASE]),

#define S_STALK_FLIP (S_STALK_PAIN+1)
	S_NORMAL (STLK, 'C',  2, A_StalkerDrop,			&States[S_STALK_FLIP+1]),
	S_NORMAL (STLK, 'I',  3, NULL,					&States[S_STALK_FLIP+2]),
	S_NORMAL (STLK, 'H',  3, NULL,					&States[S_STALK_FLIP+3]),
	S_NORMAL (STLK, 'G',  3, NULL,					&States[S_STALK_FLIP+4]),
	S_NORMAL (STLK, 'F',  3, NULL,					&States[S_STALK_FLIP+5]),
	S_NORMAL (STLK, 'E',  3, NULL,					&States[S_STALK_FLIP+6]),
	S_NORMAL (STLK, 'D',  3, NULL,					&States[S_STALK_GROUND_CHASE]),

#define S_STALK_DIE (S_STALK_FLIP+7)
	S_NORMAL (STLK, 'O',  4, NULL,					&States[S_STALK_DIE+1]),
	S_NORMAL (STLK, 'P',  4, A_Scream,				&States[S_STALK_DIE+2]),
	S_NORMAL (STLK, 'Q',  4, NULL,					&States[S_STALK_DIE+3]),
	S_NORMAL (STLK, 'R',  4, NULL,					&States[S_STALK_DIE+4]),
	S_NORMAL (STLK, 'S',  4, NULL,					&States[S_STALK_DIE+5]),
	S_NORMAL (STLK, 'T',  4, NULL,					&States[S_STALK_DIE+6]),
	S_NORMAL (STLK, 'U',  4, A_NoBlocking,			&States[S_STALK_DIE+7]),
	S_NORMAL (STLK, 'V',  4, NULL,					&States[S_STALK_DIE+8]),
	S_NORMAL (STLK, 'W',  4, NULL,					&States[S_STALK_DIE+9]),
	S_BRIGHT (STLK, 'X',  4, NULL,					&States[S_STALK_DIE+10]),
	S_BRIGHT (STLK, 'Y',  4, NULL,					&States[S_STALK_DIE+11]),
	S_BRIGHT (STLK, 'Z',  4, NULL,					&States[S_STALK_DIE+12]),
	S_BRIGHT (STLK, '[',  4, NULL,					NULL)
};

IMPLEMENT_ACTOR (AStalker, Strife, 186, 0)
	PROP_StrifeType (92)
	PROP_SpawnHealth (80)
	PROP_SpawnState (S_STALK_SPAWN)
	PROP_SeeState (S_STALK_CHASE)
	PROP_PainState (S_STALK_PAIN)
	PROP_PainChance (40)
	PROP_MeleeState (S_STALK_ATK)
	PROP_DeathState (S_STALK_DIE)
	PROP_SpeedFixed (16)
	PROP_RadiusFixed (31)
	PROP_HeightFixed (25)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_SPAWNCEILING|MF_NOGRAVITY|MF_DROPOFF|
				MF_NOBLOOD|MF_COUNTKILL)
	PROP_Flags2 (MF2_PASSMOBJ|MF2_PUSHWALL|MF2_MCROSS)
	PROP_Flags4 (MF4_INCOMBAT)
	PROP_MaxDropOffHeight (32)
	PROP_MinMissileChance (150)
	PROP_SeeSound ("stalker/sight")
	PROP_AttackSound ("stalker/attack")
	PROP_PainSound ("stalker/pain")
	PROP_DeathSound ("stalker/death")
	PROP_ActiveSound ("stalker/active")
	PROP_HitObituary ("$OB_STALKER")
END_DEFAULTS

void A_StalkerChaseDecide (AActor *self)
{
	if (!(self->flags & MF_NOGRAVITY))
	{
		self->SetState (&AStalker::States[S_STALK_GROUND_CHASE]);
	}
	else if (self->ceilingz - self->height > self->z)
	{
		self->SetState (&AStalker::States[S_STALK_FLIP]);
	}
}

void A_StalkerLookInit (AActor *self)
{
	if (self->flags & MF_NOGRAVITY)
	{
		if (self->state->NextState != &AStalker::States[S_STALK_STND_CEIL])
		{
			self->SetState (&AStalker::States[S_STALK_STND_CEIL]);
		}
	}
	else
	{
		if (self->state->NextState != &AStalker::States[S_STALK_STND_FLOOR])
		{
			self->SetState (&AStalker::States[S_STALK_STND_FLOOR]);
		}
	}
}

void A_StalkerDrop (AActor *self)
{
	self->flags &= ~MF_NOGRAVITY;
}

void A_StalkerAttack (AActor *self)
{
	if (self->flags & MF_NOGRAVITY)
	{
		self->SetState (&AStalker::States[S_STALK_FLIP]);
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

bool AStalker::CheckMeleeRange ()
{
	if (!(flags & MF_NOGRAVITY))
	{
		return Super::CheckMeleeRange ();
	}
	else
	{
		AActor *pl;
		fixed_t dist;
			
		if (!target)
			return false;
					
		pl = target;
		dist = P_AproxDistance (pl->x - x, pl->y - y);

		if (dist >= MELEERANGE-20*FRACUNIT + pl->radius)
			return false;

		if (!P_CheckSight (this, pl, 0))
			return false;

		return true;				
	}
}

