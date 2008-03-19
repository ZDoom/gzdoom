#include "actor.h"
#include "m_random.h"
#include "a_action.h"
#include "p_local.h"
#include "p_enemy.h"
#include "s_sound.h"
#include "a_strifeglobal.h"

void A_TossGib (AActor *);

// Crusader -----------------------------------------------------------------

void A_CrusaderChoose (AActor *);
void A_CrusaderSweepLeft (AActor *);
void A_CrusaderSweepRight (AActor *);
void A_CrusaderRefire (AActor *);
void A_CrusaderDeath (AActor *);

class ACrusader : public AActor
{
	DECLARE_ACTOR (ACrusader, AActor)
public:
	void GetExplodeParms (int &damage, int &dist, bool &hurtSource)
	{
		damage = dist = 64;
	}
	void NoBlockingSet ()
	{
		P_DropItem (this, "EnergyPod", 20, 256);
	}
};

FState ACrusader::States[] =
{
#define S_CRUSADER_STND 0
	S_NORMAL (ROB2, 'Q',   10, A_Look,					&States[S_CRUSADER_STND]),

#define S_CRUSADER_RUN (S_CRUSADER_STND+1)
	S_NORMAL (ROB2, 'A',	3, A_Chase,					&States[S_CRUSADER_RUN+1]),
	S_NORMAL (ROB2, 'A',	3, A_Chase,					&States[S_CRUSADER_RUN+2]),
	S_NORMAL (ROB2, 'B',	3, A_Chase,					&States[S_CRUSADER_RUN+3]),
	S_NORMAL (ROB2, 'B',	3, A_Chase,					&States[S_CRUSADER_RUN+4]),
	S_NORMAL (ROB2, 'C',	3, A_Chase,					&States[S_CRUSADER_RUN+5]),
	S_NORMAL (ROB2, 'C',	3, A_Chase,					&States[S_CRUSADER_RUN+6]),
	S_NORMAL (ROB2, 'D',	3, A_Chase,					&States[S_CRUSADER_RUN+7]),
	S_NORMAL (ROB2, 'D',	3, A_Chase,					&States[S_CRUSADER_RUN]),

#define S_CRUSADER_ATTACK (S_CRUSADER_RUN+8)
	S_NORMAL (ROB2, 'E',	3, A_FaceTarget,			&States[S_CRUSADER_ATTACK+1]),
	S_BRIGHT (ROB2, 'F',	2, A_CrusaderChoose,		&States[S_CRUSADER_ATTACK+2]),
	S_BRIGHT (ROB2, 'E',	2, A_CrusaderSweepLeft,		&States[S_CRUSADER_ATTACK+3]),
	S_BRIGHT (ROB2, 'F',	3, A_CrusaderSweepLeft,		&States[S_CRUSADER_ATTACK+4]),
	S_BRIGHT (ROB2, 'E',	2, A_CrusaderSweepLeft,		&States[S_CRUSADER_ATTACK+5]),
	S_BRIGHT (ROB2, 'F',	2, A_CrusaderSweepLeft,		&States[S_CRUSADER_ATTACK+6]),
	S_BRIGHT (ROB2, 'E',	2, A_CrusaderSweepRight,	&States[S_CRUSADER_ATTACK+7]),
	S_BRIGHT (ROB2, 'F',	2, A_CrusaderSweepRight,	&States[S_CRUSADER_ATTACK+8]),
	S_BRIGHT (ROB2, 'E',	2, A_CrusaderSweepRight,	&States[S_CRUSADER_ATTACK+9]),
	S_BRIGHT (ROB2, 'F',	2, A_CrusaderRefire,		&States[S_CRUSADER_ATTACK]),

#define S_CRUSADER_PAIN (S_CRUSADER_ATTACK+10)
	S_NORMAL (ROB2, 'D',	1, A_Pain,					&States[S_CRUSADER_RUN]),

#define S_CRUSADER_DEATH (S_CRUSADER_PAIN+1)
	S_NORMAL (ROB2, 'G',	3, A_Scream,				&States[S_CRUSADER_DEATH+1]),
	S_NORMAL (ROB2, 'H',	5, A_TossGib,				&States[S_CRUSADER_DEATH+2]),
	S_BRIGHT (ROB2, 'I',	4, A_TossGib,				&States[S_CRUSADER_DEATH+3]),
	S_BRIGHT (ROB2, 'J',	4, A_ExplodeAndAlert,		&States[S_CRUSADER_DEATH+4]),
	S_BRIGHT (ROB2, 'K',	4, A_NoBlocking,			&States[S_CRUSADER_DEATH+5]),
	S_NORMAL (ROB2, 'L',	4, A_ExplodeAndAlert,		&States[S_CRUSADER_DEATH+6]),
	S_NORMAL (ROB2, 'M',	4, A_TossGib,				&States[S_CRUSADER_DEATH+7]),
	S_NORMAL (ROB2, 'N',	4, A_TossGib,				&States[S_CRUSADER_DEATH+8]),
	S_NORMAL (ROB2, 'O',	4, A_ExplodeAndAlert,		&States[S_CRUSADER_DEATH+9]),
	S_NORMAL (ROB2, 'P',   -1, A_CrusaderDeath,			NULL)
};

IMPLEMENT_ACTOR (ACrusader, Strife, 3005, 0)
	PROP_SpawnState (S_CRUSADER_STND)
	PROP_SeeState (S_CRUSADER_RUN)
	PROP_MissileState (S_CRUSADER_ATTACK)
	PROP_PainState (S_CRUSADER_PAIN)
	PROP_DeathState (S_CRUSADER_DEATH)
	PROP_SpeedFixed (8)
	PROP_RadiusFixed (40)
	PROP_HeightFixed (56)
	PROP_Mass (400)
	PROP_SpawnHealth (400)
	PROP_PainChance (128)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_NOBLOOD|MF_COUNTKILL)
	PROP_Flags2 (MF2_FLOORCLIP|MF2_PASSMOBJ|MF2_PUSHWALL|MF2_MCROSS)
	PROP_Flags3 (MF3_DONTMORPH)
	PROP_Flags4 (MF4_MISSILEMORE|MF4_INCOMBAT|MF4_NOICEDEATH)
	PROP_MinMissileChance (120)
	PROP_StrifeType (63)
	PROP_MaxDropOffHeight (32)
	PROP_SeeSound ("crusader/sight")
	PROP_PainSound ("crusader/pain")
	PROP_DeathSound ("crusader/death")
	PROP_ActiveSound ("crusader/active")
	PROP_Obituary ("$OB_CRUSADER")
END_DEFAULTS

// Fast Flame Projectile (used by Crusader) ---------------------------------

class AFastFlameMissile : public AFlameMissile
{
	DECLARE_STATELESS_ACTOR (AFastFlameMissile, AFlameMissile)
};

IMPLEMENT_STATELESS_ACTOR (AFastFlameMissile, Strife, -1, 0)
	PROP_Mass (50)
	PROP_Damage (1)
	PROP_SpeedFixed (35)
END_DEFAULTS

// Crusader Missile ---------------------------------------------------------
// This is just like the mini missile the player shoots, except it doesn't
// explode when it dies, and it does slightly less damage for a direct hit.

void A_RocketInFlight (AActor *);
void A_RocketDead (AActor *);

class ACrusaderMissile : public AActor
{
	DECLARE_ACTOR (ACrusaderMissile, AActor)
};

FState ACrusaderMissile::States[] =
{
	S_BRIGHT (MICR, 'A', 6, A_RocketInFlight,	&States[0]),

	S_BRIGHT (SMIS, 'A', 5, A_RocketDead,		&States[2]),
	S_BRIGHT (SMIS, 'B', 5, NULL,				&States[3]),
	S_BRIGHT (SMIS, 'C', 4, NULL,				&States[4]),
	S_BRIGHT (SMIS, 'D', 2, NULL,				&States[5]),
	S_BRIGHT (SMIS, 'E', 2, NULL,				&States[6]),
	S_BRIGHT (SMIS, 'F', 2, NULL,				&States[7]),
	S_BRIGHT (SMIS, 'G', 2, NULL,				NULL),
};

IMPLEMENT_ACTOR (ACrusaderMissile, Strife, -1, 0)
	PROP_SpawnState (0)
	PROP_DeathState (1)
	PROP_SpeedFixed (20)
	PROP_RadiusFixed (10)
	PROP_HeightFixed (14)
	PROP_Damage (7)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY|MF_DROPOFF|MF_MISSILE)
	PROP_Flags2 (MF2_NOTELEPORT|MF2_PCROSS|MF2_IMPACT)
	PROP_Flags4 (MF4_STRIFEDAMAGE)
	PROP_MaxStepHeight (4)
	PROP_SeeSound ("crusader/misl")
	PROP_DeathSound ("crusader/mislx")
END_DEFAULTS

bool Sys_1ed64 (AActor *self)
{
	if (P_CheckSight (self, self->target) && self->reactiontime == 0)
	{
		return P_AproxDistance (self->x - self->target->x, self->y - self->target->y) < 264*FRACUNIT;
	}
	return false;
}

void A_CrusaderChoose (AActor *self)
{
	if (self->target == NULL)
		return;

	if (Sys_1ed64 (self))
	{
		A_FaceTarget (self);
		self->angle -= ANGLE_180/16;
		P_SpawnMissileZAimed (self, self->z + 40*FRACUNIT, self->target, RUNTIME_CLASS(AFastFlameMissile));
	}
	else
	{
		if (P_CheckMissileRange (self))
		{
			A_FaceTarget (self);
			P_SpawnMissileZAimed (self, self->z + 56*FRACUNIT, self->target, RUNTIME_CLASS(ACrusaderMissile));
			self->angle -= ANGLE_45/32;
			P_SpawnMissileZAimed (self, self->z + 40*FRACUNIT, self->target, RUNTIME_CLASS(ACrusaderMissile));
			self->angle += ANGLE_45/16;
			P_SpawnMissileZAimed (self, self->z + 40*FRACUNIT, self->target, RUNTIME_CLASS(ACrusaderMissile));
			self->angle -= ANGLE_45/16;
			self->reactiontime += 15;
		}
		self->SetState (self->SeeState);
	}
}

void A_CrusaderSweepLeft (AActor *self)
{
	self->angle += ANGLE_90/16;
	AActor *misl = P_SpawnMissileZAimed (self, self->z + 48*FRACUNIT, self->target, RUNTIME_CLASS(AFastFlameMissile));
	if (misl != NULL)
	{
		misl->momz += FRACUNIT;
	}
}

void A_CrusaderSweepRight (AActor *self)
{
	self->angle -= ANGLE_90/16;
	AActor *misl = P_SpawnMissileZAimed (self, self->z + 48*FRACUNIT, self->target, RUNTIME_CLASS(AFastFlameMissile));
	if (misl != NULL)
	{
		misl->momz += FRACUNIT;
	}
}

void A_CrusaderRefire (AActor *self)
{
	if (self->target == NULL ||
		self->target->health <= 0 ||
		!P_CheckSight (self, self->target))
	{
		self->SetState (self->SeeState);
	}
}

void A_CrusaderDeath (AActor *self)
{
	if (CheckBossDeath (self))
	{
		EV_DoFloor (DFloor::floorLowerToLowest, NULL, 667, FRACUNIT, 0, 0, 0, false);
	}
}

void A_RocketDead (AActor *self)
{
	self->RenderStyle = STYLE_Add;
	S_StopSound (self, CHAN_VOICE);
}

// Dead Crusader ------------------------------------------------------------

class ADeadCrusader : public AActor
{
	DECLARE_ACTOR (ADeadCrusader, AActor)
};

FState ADeadCrusader::States[] =
{
	S_NORMAL (ROB2, 'N', 4, A_TossGib,			&States[1]),
	S_NORMAL (ROB2, 'O', 4, A_ExplodeAndAlert,	&States[2]),
	S_NORMAL (ROB2, 'P',-1, NULL,				NULL)
};

IMPLEMENT_ACTOR (ADeadCrusader, Strife, 22, 0)
	PROP_SpawnState (0)
	PROP_StrifeType (230)
END_DEFAULTS
