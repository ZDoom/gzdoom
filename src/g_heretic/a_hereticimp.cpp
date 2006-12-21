#include "templates.h"
#include "actor.h"
#include "info.h"
#include "m_random.h"
#include "s_sound.h"
#include "p_local.h"
#include "p_enemy.h"
#include "gstrings.h"

static FRandom pr_imp ("ImpExplode");
static FRandom pr_impmeatk ("ImpMeAttack");
static FRandom pr_impmsatk ("ImpMsAttack");
static FRandom pr_impmsatk2 ("ImpMsAttack2");

void A_ImpExplode (AActor *);
void A_ImpMeAttack (AActor *);
void A_ImpMsAttack (AActor *);
void A_ImpMsAttack2 (AActor *);
void A_ImpDeath (AActor *);
void A_ImpXDeath1 (AActor *);
void A_ImpXDeath2 (AActor *);

// Heretic imp (as opposed to the Doom variety) -----------------------------

class AHereticImp : public AActor
{
	DECLARE_ACTOR (AHereticImp, AActor)
};

FState AHereticImp::States[] =
{
#define S_IMP_LOOK 0
	S_NORMAL (IMPX, 'A',   10, A_Look					, &States[S_IMP_LOOK+1]),
	S_NORMAL (IMPX, 'B',   10, A_Look					, &States[S_IMP_LOOK+2]),
	S_NORMAL (IMPX, 'C',   10, A_Look					, &States[S_IMP_LOOK+3]),
	S_NORMAL (IMPX, 'B',   10, A_Look					, &States[S_IMP_LOOK+0]),

#define S_IMP_FLY (S_IMP_LOOK+4)
	S_NORMAL (IMPX, 'A',	3, A_Chase					, &States[S_IMP_FLY+1]),
	S_NORMAL (IMPX, 'A',	3, A_Chase					, &States[S_IMP_FLY+2]),
	S_NORMAL (IMPX, 'B',	3, A_Chase					, &States[S_IMP_FLY+3]),
	S_NORMAL (IMPX, 'B',	3, A_Chase					, &States[S_IMP_FLY+4]),
	S_NORMAL (IMPX, 'C',	3, A_Chase					, &States[S_IMP_FLY+5]),
	S_NORMAL (IMPX, 'C',	3, A_Chase					, &States[S_IMP_FLY+6]),
	S_NORMAL (IMPX, 'B',	3, A_Chase					, &States[S_IMP_FLY+7]),
	S_NORMAL (IMPX, 'B',	3, A_Chase					, &States[S_IMP_FLY+0]),

#define S_IMP_MEATK (S_IMP_FLY+8)
	S_NORMAL (IMPX, 'D',	6, A_FaceTarget 			, &States[S_IMP_MEATK+1]),
	S_NORMAL (IMPX, 'E',	6, A_FaceTarget 			, &States[S_IMP_MEATK+2]),
	S_NORMAL (IMPX, 'F',	6, A_ImpMeAttack			, &States[S_IMP_FLY+0]),

#define S_IMP_MSATK1 (S_IMP_MEATK+3)
	S_NORMAL (IMPX, 'A',   10, A_FaceTarget 			, &States[S_IMP_MSATK1+1]),
	S_NORMAL (IMPX, 'B',	6, A_ImpMsAttack			, &States[S_IMP_MSATK1+2]),
	S_NORMAL (IMPX, 'C',	6, NULL 					, &States[S_IMP_MSATK1+3]),
	S_NORMAL (IMPX, 'B',	6, NULL 					, &States[S_IMP_MSATK1+4]),
	S_NORMAL (IMPX, 'A',	6, NULL 					, &States[S_IMP_MSATK1+5]),
	S_NORMAL (IMPX, 'B',	6, NULL 					, &States[S_IMP_MSATK1+2]),

#define S_IMP_PAIN (S_IMP_MSATK1+6)
	S_NORMAL (IMPX, 'G',	3, NULL 					, &States[S_IMP_PAIN+1]),
	S_NORMAL (IMPX, 'G',	3, A_Pain					, &States[S_IMP_FLY+0]),

#define S_IMP_DIE (S_IMP_PAIN+2)
	S_NORMAL (IMPX, 'G',	4, A_ImpDeath				, &States[S_IMP_DIE+1]),
	S_NORMAL (IMPX, 'H',	5, NULL 					, &States[S_IMP_DIE+1]),

#define S_IMP_XDIE (S_IMP_DIE+2)
	S_NORMAL (IMPX, 'S',	5, A_ImpXDeath1 			, &States[S_IMP_XDIE+1]),
	S_NORMAL (IMPX, 'T',	5, NULL 					, &States[S_IMP_XDIE+2]),
	S_NORMAL (IMPX, 'U',	5, NULL 					, &States[S_IMP_XDIE+3]),
	S_NORMAL (IMPX, 'V',	5, A_ImpXDeath2 			, &States[S_IMP_XDIE+4]),
	S_NORMAL (IMPX, 'W',	5, NULL 					, &States[S_IMP_XDIE+4]),

#define S_IMP_CRASH (S_IMP_XDIE+5)
	S_NORMAL (IMPX, 'I',	7, A_ImpExplode 			, &States[S_IMP_CRASH+1]),
	S_NORMAL (IMPX, 'J',	7, A_Scream 				, &States[S_IMP_CRASH+2]),
	S_NORMAL (IMPX, 'K',	7, NULL 					, &States[S_IMP_CRASH+3]),
	S_NORMAL (IMPX, 'L',   -1, NULL 					, NULL),

#define S_IMP_XCRASH (S_IMP_CRASH+4)
	S_NORMAL (IMPX, 'X',	7, NULL 					, &States[S_IMP_XCRASH+1]),
	S_NORMAL (IMPX, 'Y',	7, NULL 					, &States[S_IMP_XCRASH+2]),
	S_NORMAL (IMPX, 'Z',   -1, NULL 					, NULL)
};

IMPLEMENT_ACTOR (AHereticImp, Heretic, 66, 5)
	PROP_SpawnHealth (40)
	PROP_RadiusFixed (16)
	PROP_HeightFixed (36)
	PROP_Mass (50)
	PROP_SpeedFixed (10)
	PROP_PainChance (200)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_FLOAT|MF_NOGRAVITY|MF_COUNTKILL)
	PROP_Flags2 (MF2_MCROSS|MF2_SPAWNFLOAT|MF2_PASSMOBJ|MF2_PUSHWALL)
	PROP_Flags3 (MF3_DONTOVERLAP)
	PROP_Flags4 (MF4_MISSILEMORE)

	PROP_SpawnState (S_IMP_LOOK)
	PROP_SeeState (S_IMP_FLY)
	PROP_PainState (S_IMP_PAIN)
	PROP_MeleeState (S_IMP_MEATK)
	PROP_MissileState (S_IMP_MSATK1)
	PROP_CrashState (S_IMP_CRASH)
	PROP_DeathState (S_IMP_DIE)
	PROP_XDeathState (S_IMP_XDIE)

	PROP_SeeSound ("himp/sight")
	PROP_AttackSound ("himp/attack")
	PROP_PainSound ("himp/pain")
	PROP_DeathSound ("himp/death")
	PROP_ActiveSound ("himp/active")
	PROP_Obituary("$OB_HERETICIMP")
	PROP_HitObituary("$OB_HERETICIMPHIT")
END_DEFAULTS

// Heretic imp leader -------------------------------------------------------

class AHereticImpLeader : public AHereticImp
{
	DECLARE_ACTOR (AHereticImpLeader, AHereticImp)
};

FState AHereticImpLeader::States[] =
{
#define S_IMP_MSATK2 0
	S_NORMAL (IMPX, 'D',	6, A_FaceTarget 			, &States[S_IMP_MSATK2+1]),
	S_NORMAL (IMPX, 'E',	6, A_FaceTarget 			, &States[S_IMP_MSATK2+2]),
	S_NORMAL (IMPX, 'F',	6, A_ImpMsAttack2			, &AHereticImp::States[S_IMP_FLY]),
};

IMPLEMENT_ACTOR (AHereticImpLeader, Heretic, 5, 7)
	PROP_SpawnHealth (80)
	
	PROP_MeleeState (PROP_CLEAR_STATE)
	PROP_MissileState (S_IMP_MSATK2)
	PROP_Flags4Clear(MF4_MISSILEMORE)	// The imp leader does have a 'normal' missile range!

	PROP_AttackSound ("himp/leaderattack")
END_DEFAULTS

// Heretic imp chunk 1 ------------------------------------------------------

class AHereticImpChunk1 : public AActor
{
	DECLARE_ACTOR (AHereticImpChunk1, AActor)
};

FState AHereticImpChunk1::States[] =
{
	S_NORMAL (IMPX, 'M',	5, NULL 					, &States[1]),
	S_NORMAL (IMPX, 'N',  700, NULL 					, &States[2]),
	S_NORMAL (IMPX, 'O',  700, NULL 					, NULL)
};

IMPLEMENT_ACTOR (AHereticImpChunk1, Heretic, -1, 0)
	PROP_Mass (5)
	PROP_Radius (4)

	PROP_SpawnState (0)
END_DEFAULTS

// Heretic imp chunk 2 ------------------------------------------------------

class AHereticImpChunk2 : public AActor
{
	DECLARE_ACTOR (AHereticImpChunk2, AActor)
};

FState AHereticImpChunk2::States[] =
{
	S_NORMAL (IMPX, 'P',	5, NULL 					, &States[1]),
	S_NORMAL (IMPX, 'Q',  700, NULL 					, &States[2]),
	S_NORMAL (IMPX, 'R',  700, NULL 					, NULL)
};

IMPLEMENT_ACTOR (AHereticImpChunk2, Heretic, -1, 0)
	PROP_Mass (5)
	PROP_Radius (4)

	PROP_SpawnState (0)
END_DEFAULTS

// Heretic imp ball ---------------------------------------------------------

class AHereticImpBall : public AActor
{
	DECLARE_ACTOR (AHereticImpBall, AActor)
};

FState AHereticImpBall::States[] =
{
#define S_IMPFX 0
	S_BRIGHT (FX10, 'A',	6, NULL 					, &States[S_IMPFX+1]),
	S_BRIGHT (FX10, 'B',	6, NULL 					, &States[S_IMPFX+2]),
	S_BRIGHT (FX10, 'C',	6, NULL 					, &States[S_IMPFX+0]),

#define S_IMPFXI (S_IMPFX+3)
	S_BRIGHT (FX10, 'D',	5, NULL 					, &States[S_IMPFXI+1]),
	S_BRIGHT (FX10, 'E',	5, NULL 					, &States[S_IMPFXI+2]),
	S_BRIGHT (FX10, 'F',	5, NULL 					, &States[S_IMPFXI+3]),
	S_BRIGHT (FX10, 'G',	5, NULL 					, NULL)
};

IMPLEMENT_ACTOR (AHereticImpBall, Heretic, -1, 10)
	PROP_RadiusFixed (8)
	PROP_HeightFixed (8)
	PROP_SpeedFixed (10)
	PROP_Damage (1)
	PROP_Flags (MF_MISSILE|MF_DROPOFF|MF_NOGRAVITY)
	PROP_Flags2 (MF2_WINDTHRUST|MF2_NOTELEPORT)
	PROP_RenderStyle (STYLE_Add)

	PROP_SpawnState (S_IMPFX)
	PROP_DeathState (S_IMPFXI)
END_DEFAULTS

AT_SPEED_SET (HereticImpBall, speed)
{
	SimpleSpeedSetter (AHereticImpBall, 10*FRACUNIT, 20*FRACUNIT, speed);
}

//----------------------------------------------------------------------------
//
// PROC A_ImpExplode
//
//----------------------------------------------------------------------------

void A_ImpExplode (AActor *self)
{
	AActor *chunk;

	self->flags &= ~MF_NOGRAVITY;

	chunk = Spawn<AHereticImpChunk1> (self->x, self->y, self->z, ALLOW_REPLACE);
	chunk->momx = pr_imp.Random2 () << 10;
	chunk->momy = pr_imp.Random2 () << 10;
	chunk->momz = 9*FRACUNIT;

	chunk = Spawn<AHereticImpChunk2> (self->x, self->y, self->z, ALLOW_REPLACE);
	chunk->momx = pr_imp.Random2 () << 10;
	chunk->momy = pr_imp.Random2 () << 10;
	chunk->momz = 9*FRACUNIT;
	if (self->special1 == 666)
	{ // Extreme death crash
		self->SetState (&AHereticImp::States[S_IMP_XCRASH]);
	}
}

//----------------------------------------------------------------------------
//
// PROC A_ImpMeAttack
//
//----------------------------------------------------------------------------

void A_ImpMeAttack (AActor *self)
{
	if (!self->target)
	{
		return;
	}
	S_SoundID (self, CHAN_WEAPON, self->AttackSound, 1, ATTN_NORM);
	if (self->CheckMeleeRange ())
	{
		int damage = 5+(pr_impmeatk()&7);
		P_DamageMobj (self->target, self, self, damage, NAME_Melee);
		P_TraceBleed (damage, self->target, self);
	}
}

//----------------------------------------------------------------------------
//
// PROC A_ImpMsAttack
//
//----------------------------------------------------------------------------

void A_ImpMsAttack (AActor *self)
{
	AActor *dest;
	angle_t an;
	int dist;

	if (!self->target || pr_impmsatk() > 64)
	{
		self->SetState (self->SeeState);
		return;
	}
	dest = self->target;
	self->flags |= MF_SKULLFLY;
	S_SoundID (self, CHAN_WEAPON, self->AttackSound, 1, ATTN_NORM);
	A_FaceTarget (self);
	an = self->angle >> ANGLETOFINESHIFT;
	self->momx = FixedMul (12*FRACUNIT, finecosine[an]);
	self->momy = FixedMul (12*FRACUNIT, finesine[an]);
	dist = P_AproxDistance (dest->x - self->x, dest->y - self->y);
	dist = dist/(12*FRACUNIT);
	if (dist < 1)
	{
		dist = 1;
	}
	self->momz = (dest->z + (dest->height>>1) - self->z)/dist;
}

//----------------------------------------------------------------------------
//
// PROC A_ImpMsAttack2
//
// Fireball attack of the imp leader.
//
//----------------------------------------------------------------------------

void A_ImpMsAttack2 (AActor *self)
{
	if (!self->target)
	{
		return;
	}
	S_SoundID (self, CHAN_WEAPON, self->AttackSound, 1, ATTN_NORM);
	if (self->CheckMeleeRange ())
	{
		int damage = 5+(pr_impmsatk2()&7);
		P_DamageMobj (self->target, self, self, damage, NAME_Melee);
		P_TraceBleed (damage, self->target, self);
		return;
	}
	P_SpawnMissile (self, self->target, RUNTIME_CLASS(AHereticImpBall));
}

//----------------------------------------------------------------------------
//
// PROC A_ImpDeath
//
//----------------------------------------------------------------------------

void A_ImpDeath (AActor *self)
{
	self->flags &= ~MF_SOLID;
	self->flags2 |= MF2_FLOORCLIP;
	if (self->z <= self->floorz)
	{
		//self->SetState (&AHereticImp::States[S_IMP_CRASH]);
	}
}

//----------------------------------------------------------------------------
//
// PROC A_ImpXDeath1
//
//----------------------------------------------------------------------------

void A_ImpXDeath1 (AActor *self)
{
	self->flags &= ~MF_SOLID;
	self->flags |= MF_NOGRAVITY;
	self->flags2 |= MF2_FLOORCLIP;
	self->special1 = 666; // Flag the crash routine
}

//----------------------------------------------------------------------------
//
// PROC A_ImpXDeath2
//
//----------------------------------------------------------------------------

void A_ImpXDeath2 (AActor *self)
{
	self->flags &= ~MF_NOGRAVITY;
	if (self->z <= self->floorz)
	{
		self->SetState (&AHereticImp::States[S_IMP_CRASH]);
	}
}

