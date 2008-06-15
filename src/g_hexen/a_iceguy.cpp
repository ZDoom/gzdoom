#include "actor.h"
#include "info.h"
#include "p_local.h"
#include "s_sound.h"
#include "p_enemy.h"
#include "a_action.h"
#include "m_random.h"

static FRandom pr_iceguylook ("IceGuyLook");
static FRandom pr_iceguychase ("IceGuyChase");

void A_IceGuyLook (AActor *);
void A_IceGuyChase (AActor *);
void A_IceGuyAttack (AActor *);
void A_IceGuyDie (AActor *);
void A_IceGuyMissilePuff (AActor *);
void A_IceGuyMissileExplode (AActor *);


// Ice Guy ------------------------------------------------------------------

class AIceGuy : public AActor
{
	DECLARE_ACTOR (AIceGuy, AActor)
public:
	void Deactivate (AActor *activator);
};

FState AIceGuy::States[] =
{
#define S_ICEGUY_LOOK 0
	S_NORMAL (ICEY, 'A',   10, A_IceGuyLook			    , &States[S_ICEGUY_LOOK]),

#define S_ICEGUY_WALK1 (S_ICEGUY_LOOK+1)
	S_NORMAL (ICEY, 'A',	4, A_Chase				    , &States[S_ICEGUY_WALK1+1]),
	S_NORMAL (ICEY, 'B',	4, A_IceGuyChase		    , &States[S_ICEGUY_WALK1+2]),
	S_NORMAL (ICEY, 'C',	4, A_Chase				    , &States[S_ICEGUY_WALK1+3]),
	S_NORMAL (ICEY, 'D',	4, A_Chase				    , &States[S_ICEGUY_WALK1]),

#define S_ICEGUY_PAIN1 (S_ICEGUY_WALK1+4)
	S_NORMAL (ICEY, 'A',	1, A_Pain				    , &States[S_ICEGUY_WALK1]),

#define S_ICEGUY_ATK1 (S_ICEGUY_PAIN1+1)
	S_NORMAL (ICEY, 'E',	3, A_FaceTarget			    , &States[S_ICEGUY_ATK1+1]),
	S_NORMAL (ICEY, 'F',	3, A_FaceTarget			    , &States[S_ICEGUY_ATK1+2]),
	S_BRIGHT (ICEY, 'G',	8, A_IceGuyAttack		    , &States[S_ICEGUY_ATK1+3]),
	S_NORMAL (ICEY, 'F',	4, A_FaceTarget			    , &States[S_ICEGUY_WALK1]),

#define S_ICEGUY_DEATH (S_ICEGUY_ATK1+4)
	S_NORMAL (ICEY, 'A',	1, A_IceGuyDie			    , NULL),

#define S_ICEGUY_DORMANT (S_ICEGUY_DEATH+1)
	S_NORMAL (ICEY, 'A',   -1, NULL					    , &States[S_ICEGUY_LOOK]),

};

IMPLEMENT_ACTOR (AIceGuy, Hexen, 8020, 20)
	PROP_SpawnHealth (120)
	PROP_PainChance (144)
	PROP_SpeedFixed (14)
	PROP_RadiusFixed (22)
	PROP_HeightFixed (75)
	PROP_Mass (150)
	PROP_DamageType (NAME_Ice)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_NOBLOOD|MF_COUNTKILL)
	PROP_Flags2 (MF2_PASSMOBJ|MF2_TELESTOMP|MF2_PUSHWALL|MF2_MCROSS)
	PROP_Flags4 (MF4_NOICEDEATH)

	PROP_SpawnState (S_ICEGUY_LOOK)
	PROP_SeeState (S_ICEGUY_WALK1)
	PROP_PainState (S_ICEGUY_PAIN1)
	PROP_MissileState (S_ICEGUY_ATK1)
	PROP_DeathState (S_ICEGUY_DEATH)

	PROP_SeeSound ("IceGuySight")
	PROP_AttackSound ("IceGuyAttack")
	PROP_ActiveSound ("IceGuyActive")
	PROP_Obituary("$OB_ICEGUY")
END_DEFAULTS

void AIceGuy::Deactivate (AActor *activator)
{
	Super::Deactivate (activator);
	SetState (&States[S_ICEGUY_DORMANT]);
}

// Ice Guy Projectile -------------------------------------------------------

class AIceGuyFX : public AActor
{
	DECLARE_ACTOR (AIceGuyFX, AActor)
};

FState AIceGuyFX::States[] =
{
#define S_ICEGUY_FX1 0
	S_BRIGHT (ICPR, 'A',	3, A_IceGuyMissilePuff	    , &States[S_ICEGUY_FX1+1]),
	S_BRIGHT (ICPR, 'B',	3, A_IceGuyMissilePuff	    , &States[S_ICEGUY_FX1+2]),
	S_BRIGHT (ICPR, 'C',	3, A_IceGuyMissilePuff	    , &States[S_ICEGUY_FX1]),

#define S_ICEGUY_FX_X1 (S_ICEGUY_FX1+3)
	S_BRIGHT (ICPR, 'D',	4, NULL					    , &States[S_ICEGUY_FX_X1+1]),
	S_BRIGHT (ICPR, 'E',	4, A_IceGuyMissileExplode   , &States[S_ICEGUY_FX_X1+2]),
	S_BRIGHT (ICPR, 'F',	4, NULL					    , &States[S_ICEGUY_FX_X1+3]),
	S_BRIGHT (ICPR, 'G',	4, NULL					    , &States[S_ICEGUY_FX_X1+4]),
	S_BRIGHT (ICPR, 'H',	3, NULL					    , NULL),
};

IMPLEMENT_ACTOR (AIceGuyFX, Hexen, -1, 0)
	PROP_SpeedFixed (14)
	PROP_RadiusFixed (8)
	PROP_HeightFixed (10)
	PROP_Damage (1)
	PROP_DamageType (NAME_Ice)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY|MF_DROPOFF|MF_MISSILE)
	PROP_Flags2 (MF2_NOTELEPORT)

	PROP_SpawnState (S_ICEGUY_FX1)
	PROP_DeathState (S_ICEGUY_FX_X1)

	PROP_DeathSound ("IceGuyMissileExplode")
END_DEFAULTS

// Ice Guy Projectile's Puff ------------------------------------------------

class AIceFXPuff : public AActor
{
	DECLARE_ACTOR (AIceFXPuff, AActor)
};

FState AIceFXPuff::States[] =
{
	S_NORMAL (ICPR, 'I',	3, NULL					    , &States[1]),
	S_NORMAL (ICPR, 'J',	3, NULL					    , &States[2]),
	S_NORMAL (ICPR, 'K',	3, NULL					    , &States[3]),
	S_NORMAL (ICPR, 'L',	2, NULL					    , &States[4]),
	S_NORMAL (ICPR, 'M',	2, NULL					    , NULL),
};

IMPLEMENT_ACTOR (AIceFXPuff, Hexen, -1, 0)
	PROP_RadiusFixed (1)
	PROP_HeightFixed (1)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY|MF_DROPOFF|MF_SHADOW)
	PROP_Flags2 (MF2_NOTELEPORT)
	PROP_RenderStyle (STYLE_Translucent)
	PROP_Alpha (HX_ALTSHADOW)

	PROP_SpawnState (0)
END_DEFAULTS

// Secondary Ice Guy Projectile (ejected by the primary projectile) ---------

class AIceGuyFX2 : public AActor
{
	DECLARE_ACTOR (AIceGuyFX2, AActor)
public:
	int DoSpecialDamage (AActor *target, int damage);
};

FState AIceGuyFX2::States[] =
{
	S_BRIGHT (ICPR, 'N',	3, NULL					    , &States[1]),
	S_BRIGHT (ICPR, 'O',	3, NULL					    , &States[2]),
	S_BRIGHT (ICPR, 'P',	3, NULL					    , &States[0]),
};

IMPLEMENT_ACTOR (AIceGuyFX2, Hexen, -1, 0)
	PROP_SpeedFixed (10)
	PROP_RadiusFixed (4)
	PROP_HeightFixed (4)
	PROP_Damage (1)
	PROP_DamageType (NAME_Ice)
	PROP_Gravity (FRACUNIT/8)
	PROP_Flags (MF_NOBLOCKMAP|MF_DROPOFF|MF_MISSILE)
	PROP_Flags2 (MF2_NOTELEPORT)

	PROP_SpawnState (0)
END_DEFAULTS

int AIceGuyFX2::DoSpecialDamage (AActor *target, int damage)
{
	return damage >> 1;
}

// Ice Guy Bit --------------------------------------------------------------

class AIceGuyBit : public AActor
{
	DECLARE_ACTOR (AIceGuyBit, AActor)
};

FState AIceGuyBit::States[] =
{
	S_BRIGHT (ICPR, 'Q',   50, NULL					    , NULL),
#define S_ICEGUY_BIT2 (1)
	S_BRIGHT (ICPR, 'R',   50, NULL					    , NULL),
};

IMPLEMENT_ACTOR (AIceGuyBit, Hexen, -1, 0)
	PROP_RadiusFixed (1)
	PROP_HeightFixed (1)
	PROP_Gravity (FRACUNIT/8)
	PROP_Flags (MF_NOBLOCKMAP|MF_DROPOFF)
	PROP_Flags2 (MF2_NOTELEPORT)

	PROP_SpawnState (0)
END_DEFAULTS

// Ice Guy Wisp 1 -----------------------------------------------------------

class AIceGuyWisp1 : public AActor
{
	DECLARE_ACTOR (AIceGuyWisp1, AActor)
};

FState AIceGuyWisp1::States[] =
{
	S_NORMAL (ICWS, 'A',	2, NULL					    , &States[1]),
	S_NORMAL (ICWS, 'B',	2, NULL					    , &States[2]),
	S_NORMAL (ICWS, 'C',	2, NULL					    , &States[3]),
	S_NORMAL (ICWS, 'D',	2, NULL					    , &States[4]),
	S_NORMAL (ICWS, 'E',	2, NULL					    , &States[5]),
	S_NORMAL (ICWS, 'F',	2, NULL					    , &States[6]),
	S_NORMAL (ICWS, 'G',	2, NULL					    , &States[7]),
	S_NORMAL (ICWS, 'H',	2, NULL					    , &States[8]),
	S_NORMAL (ICWS, 'I',	2, NULL					    , NULL),
};

IMPLEMENT_ACTOR (AIceGuyWisp1, Hexen, -1, 0)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY|MF_DROPOFF|MF_MISSILE)
	PROP_Flags2 (MF2_NOTELEPORT)
	PROP_RenderStyle (STYLE_Translucent)
	PROP_Alpha (HX_ALTSHADOW)

	PROP_SpawnState (0)
END_DEFAULTS

// Ice Guy Wisp 2 -----------------------------------------------------------

class AIceGuyWisp2 : public AActor
{
	DECLARE_ACTOR (AIceGuyWisp2, AActor)
};

FState AIceGuyWisp2::States[] =
{
	S_NORMAL (ICWS, 'J',	2, NULL					    , &States[1]),
	S_NORMAL (ICWS, 'K',	2, NULL					    , &States[2]),
	S_NORMAL (ICWS, 'L',	2, NULL					    , &States[3]),
	S_NORMAL (ICWS, 'M',	2, NULL					    , &States[4]),
	S_NORMAL (ICWS, 'N',	2, NULL					    , &States[5]),
	S_NORMAL (ICWS, 'O',	2, NULL					    , &States[6]),
	S_NORMAL (ICWS, 'P',	2, NULL					    , &States[7]),
	S_NORMAL (ICWS, 'Q',	2, NULL					    , &States[8]),
	S_NORMAL (ICWS, 'R',	2, NULL					    , NULL),
};

IMPLEMENT_ACTOR (AIceGuyWisp2, Hexen, -1, 0)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY|MF_DROPOFF|MF_MISSILE)
	PROP_Flags2 (MF2_NOTELEPORT)
	PROP_RenderStyle (STYLE_Translucent)
	PROP_Alpha (HX_ALTSHADOW)

	PROP_SpawnState (0)
END_DEFAULTS

// Wisp types, for randomness below -----------------------------------------

static const PClass *const WispTypes[2] =
{
	RUNTIME_CLASS(AIceGuyWisp1),
	RUNTIME_CLASS(AIceGuyWisp2)
};

//============================================================================
//
// A_IceGuyLook
//
//============================================================================

void A_IceGuyLook (AActor *actor)
{
	fixed_t dist;
	fixed_t an;

	A_Look (actor);
	if (pr_iceguylook() < 64)
	{
		dist = ((pr_iceguylook()-128)*actor->radius)>>7;
		an = (actor->angle+ANG90)>>ANGLETOFINESHIFT;

		Spawn (WispTypes[pr_iceguylook()&1],
			actor->x+FixedMul(dist, finecosine[an]),
			actor->y+FixedMul(dist, finesine[an]),
			actor->z+60*FRACUNIT, ALLOW_REPLACE);
	}
}

//============================================================================
//
// A_IceGuyChase
//
//============================================================================

void A_IceGuyChase (AActor *actor)
{
	fixed_t dist;
	fixed_t an;
	AActor *mo;

	A_Chase (actor);
	if (pr_iceguychase() < 128)
	{
		dist = ((pr_iceguychase()-128)*actor->radius)>>7;
		an = (actor->angle+ANG90)>>ANGLETOFINESHIFT;

		mo = Spawn (WispTypes[pr_iceguychase()&1],
			actor->x+FixedMul(dist, finecosine[an]),
			actor->y+FixedMul(dist, finesine[an]),
			actor->z+60*FRACUNIT, ALLOW_REPLACE);
		if (mo)
		{
			mo->momx = actor->momx;
			mo->momy = actor->momy;
			mo->momz = actor->momz;
			mo->target = actor;
		}
	}
}

//============================================================================
//
// A_IceGuyAttack
//
//============================================================================

void A_IceGuyAttack (AActor *actor)
{
	fixed_t an;

	if(!actor->target) 
	{
		return;
	}
	an = (actor->angle+ANG90)>>ANGLETOFINESHIFT;
	P_SpawnMissileXYZ(actor->x+FixedMul(actor->radius>>1,
		finecosine[an]), actor->y+FixedMul(actor->radius>>1,
		finesine[an]), actor->z+40*FRACUNIT, actor, actor->target,
		RUNTIME_CLASS(AIceGuyFX));
	an = (actor->angle-ANG90)>>ANGLETOFINESHIFT;
	P_SpawnMissileXYZ(actor->x+FixedMul(actor->radius>>1,
		finecosine[an]), actor->y+FixedMul(actor->radius>>1,
		finesine[an]), actor->z+40*FRACUNIT, actor, actor->target,
		RUNTIME_CLASS(AIceGuyFX));
	S_Sound (actor, CHAN_WEAPON, actor->AttackSound, 1, ATTN_NORM);
}

//============================================================================
//
// A_IceGuyMissilePuff
//
//============================================================================

void A_IceGuyMissilePuff (AActor *actor)
{
	AActor *mo;
	mo = Spawn<AIceFXPuff> (actor->x, actor->y, actor->z+2*FRACUNIT, ALLOW_REPLACE);
}

//============================================================================
//
// A_IceGuyDie
//
//============================================================================

void A_IceGuyDie (AActor *actor)
{
	actor->momx = 0;
	actor->momy = 0;
	actor->momz = 0;
	actor->height = actor->GetDefault()->height;
	A_FreezeDeathChunks (actor);
}

//============================================================================
//
// A_IceGuyMissileExplode
//
//============================================================================

void A_IceGuyMissileExplode (AActor *actor)
{
	AActor *mo;
	int i;

	for (i = 0; i < 8; i++)
	{
		mo = P_SpawnMissileAngleZ (actor, actor->z+3*FRACUNIT,
			RUNTIME_CLASS(AIceGuyFX2),
			i*ANG45, (fixed_t)(-0.3*FRACUNIT));
		if (mo)
		{
			mo->target = actor->target;
		}
	}
}

