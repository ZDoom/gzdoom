#include "actor.h"
#include "m_random.h"
#include "a_action.h"
#include "p_local.h"
#include "p_enemy.h"
#include "s_sound.h"

static FRandom pr_inq ("Inquisitor");

void A_InquisitorWalk (AActor *);
void A_InquisitorDecide (AActor *);
void A_InquisitorAttack (AActor *);
void A_InquisitorJump (AActor *);
void A_InquisitorCheckLand (AActor *);
void A_TossArm (AActor *);
void A_ShutUp (AActor *);

void A_ReaverRanged (AActor *);
void A_TossGib (AActor *);
void A_Countdown (AActor *);

// Inquisitor ---------------------------------------------------------------

class AInquisitor : public AActor
{
	DECLARE_ACTOR (AInquisitor, AActor)
};

FState AInquisitor::States[] =
{
#define S_INQ_STND 0
	S_NORMAL (ROB3, 'A', 10, A_Look,				&States[S_INQ_STND+1]),
	S_NORMAL (ROB3, 'B', 10, A_Look,				&States[S_INQ_STND]),

#define S_INQ_CHASE (S_INQ_STND+2)
	S_NORMAL (ROB3, 'B',  3, A_InquisitorWalk,		&States[S_INQ_CHASE+1]),
	S_NORMAL (ROB3, 'B',  3, A_Chase,				&States[S_INQ_CHASE+2]),
	S_NORMAL (ROB3, 'C',  4, A_Chase,				&States[S_INQ_CHASE+3]),
	S_NORMAL (ROB3, 'C',  4, A_Chase,				&States[S_INQ_CHASE+4]),
	S_NORMAL (ROB3, 'D',  4, A_Chase,				&States[S_INQ_CHASE+5]),
	S_NORMAL (ROB3, 'D',  4, A_Chase,				&States[S_INQ_CHASE+6]),
	S_NORMAL (ROB3, 'E',  3, A_InquisitorWalk,		&States[S_INQ_CHASE+7]),
	S_NORMAL (ROB3, 'E',  3, A_InquisitorDecide,	&States[S_INQ_CHASE]),

#define S_INQ_ATK (S_INQ_CHASE+8)
	S_NORMAL (ROB3, 'A',  2, A_InquisitorDecide,	&States[S_INQ_ATK+1]),
	S_NORMAL (ROB3, 'F',  6, A_FaceTarget,			&States[S_INQ_ATK+2]),
	S_BRIGHT (ROB3, 'G',  8, A_ReaverRanged,		&States[S_INQ_ATK+3]),
	S_NORMAL (ROB3, 'G',  8, A_ReaverRanged,		&States[S_INQ_CHASE]),

#define S_INQ_ATK2 (S_INQ_ATK+4)
	S_NORMAL (ROB3, 'K', 12, A_FaceTarget,			&States[S_INQ_ATK2+1]),
	S_BRIGHT (ROB3, 'J',  6, A_InquisitorAttack,	&States[S_INQ_ATK2+2]),
	S_NORMAL (ROB3, 'K', 12, NULL,					&States[S_INQ_CHASE]),

#define S_INQ_BAR (S_INQ_ATK2+3)
	S_BRIGHT (ROB3, 'H',  8, A_InquisitorJump,		&States[S_INQ_BAR+1]),
	S_BRIGHT (ROB3, 'I',  4, A_InquisitorCheckLand,	&States[S_INQ_BAR+2]),
	S_BRIGHT (ROB3, 'H',  4, A_InquisitorCheckLand,	&States[S_INQ_BAR+1]),

#define S_INQ_DIE (S_INQ_BAR+3)
	S_NORMAL (ROB3, 'L',  0, A_ShutUp,				&States[S_INQ_DIE+1]),
	S_NORMAL (ROB3, 'L',  4, A_TossGib,				&States[S_INQ_DIE+2]),
	S_NORMAL (ROB3, 'M',  4, A_Scream,				&States[S_INQ_DIE+3]),
	S_NORMAL (ROB3, 'N',  4, A_TossGib,				&States[S_INQ_DIE+4]),
	S_BRIGHT (ROB3, 'O',  4, A_ExplodeAndAlert,		&States[S_INQ_DIE+5]),
	S_BRIGHT (ROB3, 'P',  4, A_TossGib,				&States[S_INQ_DIE+6]),
	S_BRIGHT (ROB3, 'Q',  4, A_NoBlocking,			&States[S_INQ_DIE+7]),
	S_NORMAL (ROB3, 'R',  4, A_TossGib,				&States[S_INQ_DIE+8]),
	S_NORMAL (ROB3, 'S',  4, A_TossGib,				&States[S_INQ_DIE+9]),
	S_NORMAL (ROB3, 'T',  4, A_TossGib,				&States[S_INQ_DIE+10]),
	S_NORMAL (ROB3, 'U',  4, A_TossGib,				&States[S_INQ_DIE+11]),
	S_NORMAL (ROB3, 'V',  4, A_TossGib,				&States[S_INQ_DIE+12]),
	S_BRIGHT (ROB3, 'W',  4, A_ExplodeAndAlert,		&States[S_INQ_DIE+13]),
	S_BRIGHT (ROB3, 'X',  4, A_TossGib,				&States[S_INQ_DIE+14]),
	S_BRIGHT (ROB3, 'Y',  4, A_TossGib,				&States[S_INQ_DIE+15]),
	S_NORMAL (ROB3, 'Z',  4, A_TossGib,				&States[S_INQ_DIE+16]),
	S_NORMAL (ROB3, '[',  4, A_TossGib,				&States[S_INQ_DIE+17]),
	S_NORMAL (ROB3, '\\', 3, A_TossGib,				&States[S_INQ_DIE+18]),
	S_BRIGHT (ROB3, ']',  3, A_ExplodeAndAlert,		&States[S_INQ_DIE+19]),
	S_BRIGHT (RBB3, 'A',  3, A_TossArm,				&States[S_INQ_DIE+20]),
	S_BRIGHT (RBB3, 'B',  3, A_TossGib,				&States[S_INQ_DIE+21]),
	S_NORMAL (RBB3, 'C',  3, A_TossGib,				&States[S_INQ_DIE+22]),
	S_NORMAL (RBB3, 'D',  3, A_TossGib,				&States[S_INQ_DIE+23]),
	S_NORMAL (RBB3, 'E', -1, NULL,					NULL),
	// The Inquisitor called A_BossDeath in Strife, but A_BossDeath doesn't
	// do anything for it, so there's no reason to call it.
};

IMPLEMENT_ACTOR (AInquisitor, Strife, 16, 0)
	PROP_StrifeType (93)
	PROP_SpawnHealth (1000)
	PROP_SpawnState (S_INQ_STND)
	PROP_SeeState (S_INQ_CHASE)
	PROP_MissileState (S_INQ_ATK)
	PROP_DeathState (S_INQ_DIE)
	PROP_SpeedFixed (12)
	PROP_RadiusFixed (40)
	PROP_HeightFixed (110)
	PROP_MassLong(0x7fffffff)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_DROPOFF|MF_NOBLOOD|MF_COUNTKILL)
	PROP_Flags2 (MF2_BOSS|MF2_FLOORCLIP|MF2_PASSMOBJ|MF2_PUSHWALL|MF2_MCROSS)
	PROP_Flags3 (MF3_DONTMORPH|MF3_NORADIUSDMG)
	PROP_MaxDropOffHeight (32)
	PROP_MinMissileChance (150)
	PROP_SeeSound ("inquisitor/sight")
	PROP_DeathSound ("inquisitor/death")
	PROP_ActiveSound ("inquisitor/active")
	PROP_Obituary ("$OB_INQUISITOR")
END_DEFAULTS

// Inquisitor Shot ----------------------------------------------------------

class AInquisitorShot : public AActor
{
	DECLARE_ACTOR (AInquisitorShot, AActor)
public:
	void GetExplodeParms (int &damage, int &dist, bool &hurtSource);
};

FState AInquisitorShot::States[] =
{
	S_NORMAL (UBAM, 'A', 3, A_Countdown,		&States[1]),
	S_NORMAL (UBAM, 'B', 3, A_Countdown,		&States[0]),

	S_BRIGHT (BNG2, 'A', 4, A_ExplodeAndAlert,	&States[3]),
	S_BRIGHT (BNG2, 'B', 4, NULL,				&States[4]),
	S_BRIGHT (BNG2, 'C', 4, NULL,				&States[5]),
	S_BRIGHT (BNG2, 'D', 4, NULL,				&States[6]),
	S_BRIGHT (BNG2, 'E', 4, NULL,				&States[7]),
	S_BRIGHT (BNG2, 'F', 4, NULL,				&States[8]),
	S_BRIGHT (BNG2, 'G', 4, NULL,				&States[9]),
	S_BRIGHT (BNG2, 'H', 4, NULL,				&States[10]),
	S_BRIGHT (BNG2, 'I', 4, NULL,				NULL)
};

IMPLEMENT_ACTOR (AInquisitorShot, Strife, -1, 0)
	PROP_StrifeType (108)
	PROP_SpawnState (0)
	PROP_DeathState (2)
	PROP_ReactionTime (15)
	PROP_SpeedFixed (25)
	PROP_RadiusFixed (13)
	PROP_HeightFixed (13)
	PROP_Mass (15)
	PROP_Flags (MF_NOBLOCKMAP|MF_DROPOFF|MF_MISSILE)
	PROP_Flags2 (MF2_NOTELEPORT)
	PROP_Flags4 (MF4_STRIFEDAMAGE)
	PROP_MaxStepHeight (4)
	PROP_SeeSound ("inquisitor/attack")
	PROP_DeathSound ("inquisitor/atkexplode")
END_DEFAULTS

void AInquisitorShot::GetExplodeParms (int &damage, int &dist, bool &hurtSource)
{
	damage = dist = 192;
	RenderStyle = STYLE_Add;
}

// The Dead Inquisitor's Detached Arm ---------------------------------------

class AInquisitorArm : public AActor
{
	DECLARE_ACTOR (AInquisitorArm, AActor)
};

FState AInquisitorArm::States[] =
{
	S_BRIGHT (RBB3, 'F',  5, NULL,					&States[1]),
	S_BRIGHT (RBB3, 'G',  5, NULL,					&States[2]),
	S_NORMAL (RBB3, 'H', -1, NULL,					NULL)
};

IMPLEMENT_ACTOR (AInquisitorArm, Strife, -1, 0)
	PROP_StrifeType (94)
	PROP_SpawnState (0)
	PROP_SpeedFixed (25)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOCLIP|MF_NOBLOOD)
END_DEFAULTS

void A_InquisitorWalk (AActor *self)
{
	S_Sound (self, CHAN_BODY, "inquisitor/walk", 1, ATTN_NORM);
	A_Chase (self);
}

bool InquisitorCheckDistance (AActor *self)
{
	if (self->reactiontime == 0 && P_CheckSight (self, self->target))
	{
		return P_AproxDistance (self->x - self->target->x, self->y - self->target->y) < 264*FRACUNIT;
	}
	return false;
}

void A_InquisitorDecide (AActor *self)
{
	if (self->target == NULL)
		return;

	A_FaceTarget (self);
	if (!InquisitorCheckDistance (self))
	{
		self->SetState (&AInquisitor::States[S_INQ_ATK2]);
	}
	if (self->target->z != self->z)
	{
		if (self->z + self->height + 54*FRACUNIT < self->ceilingz)
		{
			self->SetState (&AInquisitor::States[S_INQ_BAR]);
		}
	}
}

void A_InquisitorAttack (AActor *self)
{
	AActor *proj;

	if (self->target == NULL)
		return;

	A_FaceTarget (self);

	self->z += 32*FRACBITS;
	self->angle -= ANGLE_45/32;
	proj = P_SpawnMissileZAimed (self, self->z, self->target, RUNTIME_CLASS(AInquisitorShot));
	if (proj != NULL)
	{
		proj->momz += 9*FRACUNIT;
	}
	self->angle += ANGLE_45/16;
	proj = P_SpawnMissileZAimed (self, self->z, self->target, RUNTIME_CLASS(AInquisitorShot));
	if (proj != NULL)
	{
		proj->momz += 16*FRACUNIT;
	}
	self->z -= 32*FRACBITS;
}

void A_InquisitorJump (AActor *self)
{
	fixed_t dist;
	fixed_t speed;
	angle_t an;

	if (self->target == NULL)
		return;

	S_LoopedSound (self, CHAN_ITEM, "inquisitor/jump", 1, ATTN_NORM);
	self->z += 64*FRACUNIT;
	A_FaceTarget (self);
	an = self->angle >> ANGLETOFINESHIFT;
	speed = self->Speed * 2/3;
	self->momx += FixedMul (speed, finecosine[an]);
	self->momy += FixedMul (speed, finesine[an]);
	dist = P_AproxDistance (self->target->x - self->x, self->target->y - self->y);
	dist /= speed;
	if (dist < 1)
	{
		dist = 1;
	}
	self->momz = (self->target->z - self->z) / dist;
	self->reactiontime = 60;
	self->flags |= MF_NOGRAVITY;
}

void A_InquisitorCheckLand (AActor *self)
{
	self->reactiontime--;
	if (self->reactiontime < 0 ||
		self->momx == 0 ||
		self->momy == 0 ||
		self->z <= self->floorz)
	{
		self->SetState (self->SeeState);
		self->reactiontime = 0;
		self->flags &= ~MF_NOGRAVITY;
		A_ShutUp (self);
		return;
	}
	if (!S_IsActorPlayingSomething (self, CHAN_ITEM, -1))
	{
		S_LoopedSound (self, CHAN_ITEM, "inquisitor/jump", 1, ATTN_NORM);
	}

}

void A_TossArm (AActor *self)
{
	AActor *foo = Spawn<AInquisitorArm> (self->x, self->y, self->z + 24*FRACUNIT, ALLOW_REPLACE);
	foo->angle = self->angle - ANGLE_90 + (pr_inq.Random2() << 22);
	foo->momx = FixedMul (foo->Speed, finecosine[foo->angle >> ANGLETOFINESHIFT]) >> 3;
	foo->momy = FixedMul (foo->Speed, finesine[foo->angle >> ANGLETOFINESHIFT]) >> 3;
	foo->momz = pr_inq() << 10;
}

void A_ShutUp (AActor *self)
{
	S_StopSound (self, CHAN_ITEM);
}
