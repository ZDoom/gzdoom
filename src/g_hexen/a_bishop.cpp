#include "actor.h"
#include "info.h"
#include "p_local.h"
#include "s_sound.h"
#include "p_enemy.h"
#include "a_action.h"
#include "m_random.h"

void A_BishopDecide (AActor *);
void A_BishopDoBlur (AActor *);
void A_BishopSpawnBlur (AActor *);
void A_BishopPainBlur (AActor *);
void A_BishopChase (AActor *);
void A_BishopAttack (AActor *);
void A_BishopAttack2 (AActor *);
void A_BishopPuff (AActor *);
void A_SetAltShadow (AActor *);
void A_BishopMissileWeave (AActor *);
void A_BishopMissileSeek (AActor *);

// Bishop -------------------------------------------------------------------

class ABishop : public AActor
{
	DECLARE_ACTOR (ABishop, AActor)
public:
	void GetExplodeParms (int &damage, int &distance, bool &hurtSource);
	bool NewTarget (AActor *other);
};

FState ABishop::States[] =
{
#define S_BISHOP_LOOK 0
	S_NORMAL (BISH, 'A',   10, A_Look					, &States[S_BISHOP_LOOK+0]),

#define S_BISHOP_BLUR (S_BISHOP_LOOK+1)
	S_NORMAL (BISH, 'A',	2, A_BishopDoBlur			, &States[S_BISHOP_BLUR+1]),
	S_NORMAL (BISH, 'A',	4, A_BishopSpawnBlur		, &States[S_BISHOP_BLUR+1]),

#define S_BISHOP_WALK (S_BISHOP_BLUR+2)
	S_NORMAL (BISH, 'A',	2, A_Chase					, &States[S_BISHOP_WALK+1]),
	S_NORMAL (BISH, 'A',	2, A_BishopChase			, &States[S_BISHOP_WALK+2]),
	S_NORMAL (BISH, 'A',	2, NULL 					, &States[S_BISHOP_WALK+3]),
	S_NORMAL (BISH, 'B',	2, A_BishopChase			, &States[S_BISHOP_WALK+4]),
	S_NORMAL (BISH, 'B',	2, A_Chase					, &States[S_BISHOP_WALK+5]),
	S_NORMAL (BISH, 'B',	2, A_BishopChase			, &States[S_BISHOP_WALK+6]),	// S_BISHOP_DECIDE

#define S_BISHOP_DECIDE (S_BISHOP_WALK+6)
	S_NORMAL (BISH, 'A',	1, A_BishopDecide			, &States[S_BISHOP_WALK+0]),

#define S_BISHOP_ATK (S_BISHOP_DECIDE+1)
	S_NORMAL (BISH, 'A',	3, A_FaceTarget 			, &States[S_BISHOP_ATK+1]),
	S_BRIGHT (BISH, 'D',	3, A_FaceTarget 			, &States[S_BISHOP_ATK+2]),
	S_BRIGHT (BISH, 'E',	3, A_FaceTarget 			, &States[S_BISHOP_ATK+3]),
	S_BRIGHT (BISH, 'F',	3, A_BishopAttack			, &States[S_BISHOP_ATK+4]),
	S_BRIGHT (BISH, 'F',	5, A_BishopAttack2			, &States[S_BISHOP_ATK+4]),

#define S_BISHOP_PAIN (S_BISHOP_ATK+5)
	S_NORMAL (BISH, 'C',	6, A_Pain					, &States[S_BISHOP_PAIN+1]),
	S_NORMAL (BISH, 'C',	6, A_BishopPainBlur 		, &States[S_BISHOP_PAIN+2]),
	S_NORMAL (BISH, 'C',	6, A_BishopPainBlur 		, &States[S_BISHOP_PAIN+3]),
	S_NORMAL (BISH, 'C',	6, A_BishopPainBlur 		, &States[S_BISHOP_PAIN+4]),
	S_NORMAL (BISH, 'C',	0, NULL 					, &States[S_BISHOP_WALK+0]),

#define S_BISHOP_DEATH (S_BISHOP_PAIN+5)
	S_NORMAL (BISH, 'G',	6, NULL 					, &States[S_BISHOP_DEATH+1]),
	S_BRIGHT (BISH, 'H',	6, A_Scream 				, &States[S_BISHOP_DEATH+2]),
	S_BRIGHT (BISH, 'I',	5, A_NoBlocking 			, &States[S_BISHOP_DEATH+3]),
	S_BRIGHT (BISH, 'J',	5, A_Explode				, &States[S_BISHOP_DEATH+4]),
	S_BRIGHT (BISH, 'K',	5, NULL 					, &States[S_BISHOP_DEATH+5]),
	S_BRIGHT (BISH, 'L',	4, NULL 					, &States[S_BISHOP_DEATH+6]),
	S_BRIGHT (BISH, 'M',	4, NULL 					, &States[S_BISHOP_DEATH+7]),
	S_NORMAL (BISH, 'N',	4, A_BishopPuff 			, &States[S_BISHOP_DEATH+8]),
	S_NORMAL (BISH, 'O',	4, A_QueueCorpse			, &States[S_BISHOP_DEATH+9]),
	S_NORMAL (BISH, 'P',   -1, NULL 					, NULL),

#define S_BISHOP_ICE (S_BISHOP_DEATH+10)
	S_NORMAL (BISH, 'X',	5, A_FreezeDeath			, &States[S_BISHOP_ICE+1]),
	S_NORMAL (BISH, 'X',	1, A_FreezeDeathChunks		, &States[S_BISHOP_ICE+1])
};

IMPLEMENT_ACTOR (ABishop, Hexen, 114, 19)
	PROP_SpawnHealth (130)
	PROP_RadiusFixed (22)
	PROP_HeightFixed (65)
	PROP_SpeedFixed (10)
	PROP_PainChance (110)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_COUNTKILL|MF_FLOAT|MF_NOGRAVITY|MF_NOBLOOD)
	PROP_Flags2 (MF2_PASSMOBJ|MF2_PUSHWALL|MF2_TELESTOMP)
	PROP_Flags3 (MF3_SEEISALSOACTIVE|MF3_DONTOVERLAP)

	PROP_SpawnState (S_BISHOP_LOOK)
	PROP_SeeState (S_BISHOP_WALK)
	PROP_PainState (S_BISHOP_PAIN)
	PROP_MissileState (S_BISHOP_ATK)
	PROP_DeathState (S_BISHOP_DEATH)
	PROP_IDeathState (S_BISHOP_ICE)

	PROP_SeeSound ("BishopSight")
	PROP_AttackSound ("BishopAttack")
	PROP_PainSound ("BishopPain")
	PROP_DeathSound ("BishopDeath")
	PROP_ActiveSound ("BishopActive")
END_DEFAULTS

void ABishop::GetExplodeParms (int &damage, int &distance, bool &hurtSource)
{
	damage = 25 + (P_Random() & 15);
}

bool ABishop::NewTarget (AActor *other)
{ // Bishops never change their target until they've killed their current one.
	return false;
}

// Bishop puff --------------------------------------------------------------

class ABishopPuff : public AActor
{
	DECLARE_ACTOR (ABishopPuff, AActor)
};

FState ABishopPuff::States[] =
{
	S_NORMAL (BISH, 'Q',	5, NULL 					, &States[1]),
	S_NORMAL (BISH, 'R',	5, NULL 					, &States[2]),
	S_NORMAL (BISH, 'S',	5, NULL 					, &States[3]),
	S_NORMAL (BISH, 'T',	5, NULL 					, &States[4]),
	S_NORMAL (BISH, 'U',	6, NULL 					, &States[5]),
	S_NORMAL (BISH, 'V',	6, NULL 					, &States[6]),
	S_NORMAL (BISH, 'W',	5, NULL 					, NULL)
};

IMPLEMENT_ACTOR (ABishopPuff, Hexen, -1, 0)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY)
	PROP_RenderStyle (STYLE_Translucent)
	PROP_Alpha (HX_SHADOW)

	PROP_SpawnState (0)
END_DEFAULTS

// Bishop blur --------------------------------------------------------------

class ABishopBlur : public AActor
{
	DECLARE_ACTOR (ABishopBlur, AActor)
};

FState ABishopBlur::States[] =
{
	S_NORMAL (BISH, 'A',   16, NULL 					, &States[1]),
	S_NORMAL (BISH, 'A',	8, A_SetAltShadow			, NULL)
};

IMPLEMENT_ACTOR (ABishopBlur, Hexen, -1, 0)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY)
	PROP_RenderStyle (STYLE_Translucent)
	PROP_Alpha (HX_SHADOW)

	PROP_SpawnState (0)
END_DEFAULTS

// Bishop pain blur ---------------------------------------------------------

class ABishopPainBlur : public AActor
{
	DECLARE_ACTOR (ABishopPainBlur, AActor)
};

FState ABishopPainBlur::States[] =
{
	S_NORMAL (BISH, 'C',	8, NULL 					, NULL)
};

IMPLEMENT_ACTOR (ABishopPainBlur, Hexen, -1, 0)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY)
	PROP_RenderStyle (STYLE_Translucent)
	PROP_Alpha (HX_SHADOW)

	PROP_SpawnState (0)
END_DEFAULTS

// Bishop FX ----------------------------------------------------------------

class ABishopFX : public AActor
{
	DECLARE_ACTOR (ABishopFX, AActor)
public:
	int DoSpecialDamage (AActor *target, int damage);
};

FState ABishopFX::States[] =
{
#define S_BISHFX1 0
	S_BRIGHT (BPFX, 'A',	1, A_BishopMissileWeave 	, &States[S_BISHFX1+1]),
	S_BRIGHT (BPFX, 'B',	1, A_BishopMissileWeave 	, &States[S_BISHFX1+2]),
	S_BRIGHT (BPFX, 'A',	1, A_BishopMissileWeave 	, &States[S_BISHFX1+3]),
	S_BRIGHT (BPFX, 'B',	1, A_BishopMissileWeave 	, &States[S_BISHFX1+4]),
	S_BRIGHT (BPFX, 'B',	0, A_BishopMissileSeek		, &States[S_BISHFX1+0]),

#define S_BISHFXI1 (S_BISHFX1+5)
	S_BRIGHT (BPFX, 'C',	4, NULL 					, &States[S_BISHFXI1+1]),
	S_BRIGHT (BPFX, 'D',	4, NULL 					, &States[S_BISHFXI1+2]),
	S_BRIGHT (BPFX, 'E',	4, NULL 					, &States[S_BISHFXI1+3]),
	S_BRIGHT (BPFX, 'F',	4, NULL 					, &States[S_BISHFXI1+4]),
	S_BRIGHT (BPFX, 'G',	3, NULL 					, &States[S_BISHFXI1+5]),
	S_BRIGHT (BPFX, 'H',	3, NULL 					, NULL)
};

IMPLEMENT_ACTOR (ABishopFX, Hexen, -1, 0)
	PROP_RadiusFixed (10)
	PROP_HeightFixed (6)
	PROP_SpeedFixed (10)
	PROP_Damage (1)
	PROP_Flags (MF_NOBLOCKMAP|MF_MISSILE|MF_DROPOFF|MF_NOGRAVITY)
	PROP_Flags2 (MF2_NOTELEPORT|MF2_SEEKERMISSILE)

	PROP_SpawnState (S_BISHFX1)
	PROP_DeathState (S_BISHFXI1)

	PROP_DeathSound ("BishopMissileExplode")
END_DEFAULTS

int ABishopFX::DoSpecialDamage (AActor *target, int damage)
{ // Bishops are just too nasty
	return damage >> 1;
}

//============================================================================
//
// A_BishopAttack
//
//============================================================================

void A_BishopAttack (AActor *actor)
{
	if (!actor->target)
	{
		return;
	}
	S_SoundID (actor, CHAN_BODY, actor->AttackSound, 1, ATTN_NORM);
	if (P_CheckMeleeRange(actor))
	{
		P_DamageMobj (actor->target, actor, actor, HITDICE(4));
		return;
	}
	actor->special1 = (P_Random() & 3) + 5;
}

//============================================================================
//
// A_BishopAttack2
//
//		Spawns one of a string of bishop missiles
//============================================================================

void A_BishopAttack2 (AActor *actor)
{
	AActor *mo;

	if (!actor->target || !actor->special1)
	{
		actor->special1 = 0;
		actor->SetState (actor->SeeState);
		return;
	}
	mo = P_SpawnMissile (actor, actor->target, RUNTIME_CLASS(ABishopFX));
	if (mo != NULL)
	{
		mo->tracer = actor->target;
		mo->special2 = 16; // High word == x/y, Low word == z
	}
	actor->special1--;
}

//============================================================================
//
// A_BishopMissileWeave
//
//============================================================================

void A_BishopMissileWeave (AActor *actor)
{
	fixed_t newX, newY;
	int weaveXY, weaveZ;
	int angle;

	weaveXY = actor->special2 >> 16;
	weaveZ = actor->special2 & 0xFFFF;
	angle = (actor->angle + ANG90) >> ANGLETOFINESHIFT;
	newX = actor->x - FixedMul (finecosine[angle], FloatBobOffsets[weaveXY]<<1);
	newY = actor->y - FixedMul (finesine[angle], FloatBobOffsets[weaveXY]<<1);
	weaveXY = (weaveXY + 2) & 63;
	newX += FixedMul (finecosine[angle], FloatBobOffsets[weaveXY]<<1);
	newY += FixedMul (finesine[angle], FloatBobOffsets[weaveXY]<<1);
	P_TryMove (actor, newX, newY, true);
	actor->z -= FloatBobOffsets[weaveZ];
	weaveZ = (weaveZ + 2) & 63;
	actor->z += FloatBobOffsets[weaveZ];	
	actor->special2 = weaveZ + (weaveXY<<16);
}

//============================================================================
//
// A_BishopMissileSeek
//
//============================================================================

void A_BishopMissileSeek (AActor *actor)
{
	P_SeekerMissile (actor, ANGLE_1*2, ANGLE_1*3);
}

//============================================================================
//
// A_BishopDecide
//
//============================================================================

void A_BishopDecide (AActor *actor)
{
	if (P_Random() < 220)
	{
		return;
	}
	else
	{
		actor->SetState (&ABishop::States[S_BISHOP_BLUR]);
	}		
}

//============================================================================
//
// A_BishopDoBlur
//
//============================================================================

void A_BishopDoBlur (AActor *actor)
{
	actor->special1 = (P_Random() & 3) + 3; // Random number of blurs
	if (P_Random() < 120)
	{
		P_ThrustMobj (actor, actor->angle + ANG90, 11*FRACUNIT);
	}
	else if (P_Random() > 125)
	{
		P_ThrustMobj (actor, actor->angle - ANG90, 11*FRACUNIT);
	}
	else
	{ // Thrust forward
		P_ThrustMobj (actor, actor->angle, 11*FRACUNIT);
	}
	S_Sound (actor, CHAN_BODY, "BishopBlur", 1, ATTN_NORM);
}

//============================================================================
//
// A_BishopSpawnBlur
//
//============================================================================

void A_BishopSpawnBlur (AActor *actor)
{
	AActor *mo;

	if (!--actor->special1)
	{
		actor->momx = 0;
		actor->momy = 0;
		if (P_Random() > 96)
		{
			actor->SetState (actor->SeeState);
		}
		else
		{
			actor->SetState (actor->MissileState);
		}
	}
	mo = Spawn<ABishopBlur> (actor->x, actor->y, actor->z);
	if (mo)
	{
		mo->angle = actor->angle;
	}
}

//============================================================================
//
// A_BishopChase
//
//============================================================================

void A_BishopChase (AActor *actor)
{
	actor->z -= FloatBobOffsets[actor->special2] >> 1;
	actor->special2 = (actor->special2 + 4) & 63;
	actor->z += FloatBobOffsets[actor->special2] >> 1;
}

//============================================================================
//
// A_BishopPuff
//
//============================================================================

void A_BishopPuff (AActor *actor)
{
	AActor *mo;

	mo = Spawn<ABishopPuff> (actor->x, actor->y, actor->z + 40*FRACUNIT);
	if (mo)
	{
		mo->momz = FRACUNIT/2;
	}
}

//============================================================================
//
// A_BishopPainBlur
//
//============================================================================

void A_BishopPainBlur (AActor *actor)
{
	AActor *mo;

	if (P_Random() < 64)
	{
		actor->SetState (&ABishop::States[S_BISHOP_BLUR]);
		return;
	}
	mo = Spawn<ABishopPainBlur> (actor->x + (PS_Random()<<12), actor->y
		+ (PS_Random()<<12), actor->z + (PS_Random()<<11));
	if (mo)
	{
		mo->angle = actor->angle;
	}
}

//==========================================================================
//
// A_SetAltShadow
//
//==========================================================================

void A_SetAltShadow (AActor *actor)
{
	actor->alpha = HX_ALTSHADOW;
	actor->RenderStyle = STYLE_Translucent;
}
