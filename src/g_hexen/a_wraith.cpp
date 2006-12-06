#include "actor.h"
#include "info.h"
#include "p_local.h"
#include "s_sound.h"
#include "p_enemy.h"
#include "a_action.h"
#include "m_random.h"
#include "a_sharedglobal.h"

void A_WraithInit (AActor *);
void A_WraithLook (AActor *);
void A_WraithRaiseInit (AActor *);
void A_WraithRaise (AActor *);
void A_WraithChase (AActor *);
void A_WraithMelee (AActor *);
void A_WraithMissile (AActor *);
void A_WraithFX2 (AActor *);
void A_WraithFX3 (AActor *);
void A_WraithChase (AActor *);

static FRandom pr_stealhealth ("StealHealth");
static FRandom pr_wraithfx2 ("WraithFX2");
static FRandom pr_wraithfx3 ("WraithFX3");
static FRandom pr_wraithfx4 ("WraithFX4");

// Wraith -------------------------------------------------------------------

class AWraith : public AActor
{
	DECLARE_ACTOR (AWraith, AActor)
};

FState AWraith::States[] =
{
#define S_WRAITH_LOOK1 0
	S_NORMAL (WRTH, 'A',   15, A_WraithLook			    , &States[S_WRAITH_LOOK1+1]),
	S_NORMAL (WRTH, 'B',   15, A_WraithLook			    , &States[S_WRAITH_LOOK1]),

#define S_WRAITH_RAISE1 (S_WRAITH_LOOK1+2)
	S_NORMAL (WRTH, 'A',	2, A_WraithRaiseInit	    , &States[S_WRAITH_RAISE1+1]),
	S_NORMAL (WRTH, 'A',	2, A_WraithRaise		    , &States[S_WRAITH_RAISE1+2]),
	S_NORMAL (WRTH, 'A',	2, A_FaceTarget			    , &States[S_WRAITH_RAISE1+3]),
	S_NORMAL (WRTH, 'B',	2, A_WraithRaise		    , &States[S_WRAITH_RAISE1+4]),
	S_NORMAL (WRTH, 'B',	2, A_WraithRaise		    , &States[S_WRAITH_RAISE1+1]),

#define S_WRAITH_ICE (S_WRAITH_RAISE1+5)
	S_NORMAL (WRT2, 'I',	5, A_FreezeDeath		    , &States[S_WRAITH_ICE+1]),
	S_NORMAL (WRT2, 'I',	1, A_FreezeDeathChunks	    , &States[S_WRAITH_ICE+1]),

#define S_WRAITH_INIT1 (S_WRAITH_ICE+2)
	S_NORMAL (WRTH, 'A',   10, NULL					    , &States[S_WRAITH_INIT1+1]),
	S_NORMAL (WRTH, 'B',	5, A_WraithInit			    , &States[S_WRAITH_LOOK1]),

#define S_WRAITH_CHASE1 (S_WRAITH_INIT1+2)
	S_NORMAL (WRTH, 'A',	4, A_WraithChase			, &States[S_WRAITH_CHASE1+1]),
	S_NORMAL (WRTH, 'B',	4, A_WraithChase			, &States[S_WRAITH_CHASE1+2]),
	S_NORMAL (WRTH, 'C',	4, A_WraithChase			, &States[S_WRAITH_CHASE1+3]),
	S_NORMAL (WRTH, 'D',	4, A_WraithChase			, &States[S_WRAITH_CHASE1+0]),

#define S_WRAITH_PAIN1 (S_WRAITH_CHASE1+4)
	S_NORMAL (WRTH, 'A',	2, NULL					    , &States[S_WRAITH_PAIN1+1]),
	S_NORMAL (WRTH, 'H',	6, A_Pain				    , &States[S_WRAITH_CHASE1]),

#define S_WRAITH_ATK1_1 (S_WRAITH_PAIN1+2)
	S_NORMAL (WRTH, 'E',	6, A_FaceTarget			    , &States[S_WRAITH_ATK1_1+1]),
	S_NORMAL (WRTH, 'F',	6, A_WraithFX3			    , &States[S_WRAITH_ATK1_1+2]),
	S_NORMAL (WRTH, 'G',	6, A_WraithMelee		    , &States[S_WRAITH_CHASE1]),

#define S_WRAITH_ATK2_1 (S_WRAITH_ATK1_1+3)
	S_NORMAL (WRTH, 'E',	6, A_FaceTarget			    , &States[S_WRAITH_ATK2_1+1]),
	S_NORMAL (WRTH, 'F',	6, NULL					    , &States[S_WRAITH_ATK2_1+2]),
	S_NORMAL (WRTH, 'G',	6, A_WraithMissile		    , &States[S_WRAITH_CHASE1]),

#define S_WRAITH_DEATH1_1 (S_WRAITH_ATK2_1+3)
	S_NORMAL (WRTH, 'I',	4, NULL					    , &States[S_WRAITH_DEATH1_1+1]),
	S_NORMAL (WRTH, 'J',	4, A_Scream				    , &States[S_WRAITH_DEATH1_1+2]),
	S_NORMAL (WRTH, 'K',	4, NULL					    , &States[S_WRAITH_DEATH1_1+3]),
	S_NORMAL (WRTH, 'L',	4, NULL					    , &States[S_WRAITH_DEATH1_1+4]),
	S_NORMAL (WRTH, 'M',	4, A_NoBlocking			    , &States[S_WRAITH_DEATH1_1+5]),
	S_NORMAL (WRTH, 'N',	4, A_QueueCorpse		    , &States[S_WRAITH_DEATH1_1+6]),
	S_NORMAL (WRTH, 'O',	4, NULL					    , &States[S_WRAITH_DEATH1_1+7]),
	S_NORMAL (WRTH, 'P',	5, NULL					    , &States[S_WRAITH_DEATH1_1+8]),
	S_NORMAL (WRTH, 'Q',	5, NULL					    , &States[S_WRAITH_DEATH1_1+9]),
	S_NORMAL (WRTH, 'R',   -1, NULL					    , NULL),

#define S_WRAITH_DEATH2_1 (S_WRAITH_DEATH1_1+10)
	S_NORMAL (WRT2, 'A',	5, NULL					    , &States[S_WRAITH_DEATH2_1+1]),
	S_NORMAL (WRT2, 'B',	5, A_Scream				    , &States[S_WRAITH_DEATH2_1+2]),
	S_NORMAL (WRT2, 'C',	5, NULL					    , &States[S_WRAITH_DEATH2_1+3]),
	S_NORMAL (WRT2, 'D',	5, NULL					    , &States[S_WRAITH_DEATH2_1+4]),
	S_NORMAL (WRT2, 'E',	5, A_NoBlocking			    , &States[S_WRAITH_DEATH2_1+5]),
	S_NORMAL (WRT2, 'F',	5, A_QueueCorpse		    , &States[S_WRAITH_DEATH2_1+6]),
	S_NORMAL (WRT2, 'G',	5, NULL					    , &States[S_WRAITH_DEATH2_1+7]),
	S_NORMAL (WRT2, 'H',   -1, NULL					    , NULL),
};

IMPLEMENT_ACTOR (AWraith, Hexen, 34, 8)
	PROP_SpawnHealth (150)
	PROP_PainChance (25)
	PROP_SpeedFixed (11)
	PROP_HeightFixed (55)
	PROP_Mass (75)
	PROP_Damage (10)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_NOGRAVITY|MF_DROPOFF|MF_FLOAT|MF_COUNTKILL)
	PROP_Flags2 (MF2_FLOORCLIP|MF2_PASSMOBJ|MF2_TELESTOMP|MF2_PUSHWALL)

	PROP_SpawnState (S_WRAITH_INIT1)
	PROP_SeeState (S_WRAITH_CHASE1)
	PROP_PainState (S_WRAITH_PAIN1)
	PROP_MeleeState (S_WRAITH_ATK1_1)
	PROP_MissileState (S_WRAITH_ATK2_1)
	PROP_DeathState (S_WRAITH_DEATH1_1)
	PROP_XDeathState (S_WRAITH_DEATH2_1)
	PROP_IDeathState (S_WRAITH_ICE)

	PROP_SeeSound ("WraithSight")
	PROP_AttackSound ("WraithAttack")
	PROP_PainSound ("WraithPain")
	PROP_DeathSound ("WraithDeath")
	PROP_ActiveSound ("WraithActive")
	PROP_HitObituary("$OB_WRAITHHIT")
	PROP_Obituary("$OB_WRAITH")
END_DEFAULTS

// Buried wraith ------------------------------------------------------------

class AWraithBuried : public AWraith
{
	DECLARE_STATELESS_ACTOR (AWraithBuried, AWraith)
public:
	fixed_t GetRaiseSpeed ()
	{
		return 2*FRACUNIT;
	}
};

IMPLEMENT_STATELESS_ACTOR (AWraithBuried, Hexen, 10011, 9)
	PROP_HeightFixed (68)
	PROP_Flags (MF_NOGRAVITY|MF_DROPOFF|MF_FLOAT|MF_COUNTKILL)
	PROP_Flags2 (MF2_FLOORCLIP|MF2_PASSMOBJ|MF2_TELESTOMP|MF2_PUSHWALL)
	PROP_Flags3 (MF3_DONTMORPH|MF3_DONTBLAST|MF3_SPECIALFLOORCLIP|MF3_STAYMORPHED)
	PROP_RenderFlags (RF_INVISIBLE)
	PROP_PainChance (0)

	PROP_SpawnState (S_WRAITH_LOOK1)
	PROP_SeeState (S_WRAITH_RAISE1)
END_DEFAULTS

// --------------------------------------------------------------------------

class AWraithFX1 : public AActor
{
	DECLARE_ACTOR (AWraithFX1, AActor)
};

FState AWraithFX1::States[] =
{
#define S_WRTHFX_MOVE1 0
	S_BRIGHT (WRBL, 'A',	3, NULL					    , &States[S_WRTHFX_MOVE1+1]),
	S_BRIGHT (WRBL, 'B',	3, A_WraithFX2			    , &States[S_WRTHFX_MOVE1+2]),
	S_BRIGHT (WRBL, 'C',	3, NULL					    , &States[S_WRTHFX_MOVE1]),

#define S_WRTHFX_BOOM1 (S_WRTHFX_MOVE1+3)
	S_BRIGHT (WRBL, 'D',	4, NULL					    , &States[S_WRTHFX_BOOM1+1]),
	S_BRIGHT (WRBL, 'E',	4, A_WraithFX2			    , &States[S_WRTHFX_BOOM1+2]),
	S_BRIGHT (WRBL, 'F',	4, NULL					    , &States[S_WRTHFX_BOOM1+3]),
	S_BRIGHT (WRBL, 'G',	3, A_WraithFX2			    , &States[S_WRTHFX_BOOM1+4]),
	S_BRIGHT (WRBL, 'H',	3, A_WraithFX2			    , &States[S_WRTHFX_BOOM1+5]),
	S_BRIGHT (WRBL, 'I',	3, NULL					    , NULL),
};

IMPLEMENT_ACTOR (AWraithFX1, Hexen, -1, 0)
	PROP_SpeedFixed (14)
	PROP_RadiusFixed (10)
	PROP_HeightFixed (6)
	PROP_Mass (5)
	PROP_Damage (5)
	PROP_DamageType (NAME_Fire)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY|MF_DROPOFF|MF_MISSILE)
	PROP_Flags2 (MF2_FLOORCLIP|MF2_NOTELEPORT|MF2_IMPACT|MF2_PCROSS)

	PROP_SpawnState (S_WRTHFX_MOVE1)
	PROP_DeathState (S_WRTHFX_BOOM1)

	PROP_SeeSound ("WraithMissileFire")
	PROP_DeathSound ("WraithMissileExplode")
END_DEFAULTS

// --------------------------------------------------------------------------

class AWraithFX2 : public AActor
{
	DECLARE_ACTOR (AWraithFX2, AActor)
};

FState AWraithFX2::States[] =
{
#define S_WRTHFX_SIZZLE1 0
	S_BRIGHT (WRBL, 'J',	4, NULL					    , &States[S_WRTHFX_SIZZLE1+1]),
	S_BRIGHT (WRBL, 'K',	4, NULL					    , &States[S_WRTHFX_SIZZLE1+2]),
	S_BRIGHT (WRBL, 'L',	4, NULL					    , &States[S_WRTHFX_SIZZLE1+3]),
	S_BRIGHT (WRBL, 'M',	4, NULL					    , &States[S_WRTHFX_SIZZLE1+4]),
	S_BRIGHT (WRBL, 'N',	4, NULL					    , &States[S_WRTHFX_SIZZLE1+5]),
	S_BRIGHT (WRBL, 'O',	4, NULL					    , &States[S_WRTHFX_SIZZLE1+6]),
	S_BRIGHT (WRBL, 'P',	4, NULL					    , NULL),
};

IMPLEMENT_ACTOR (AWraithFX2, Hexen, -1, 108)
	PROP_RadiusFixed (2)
	PROP_HeightFixed (5)
	PROP_Mass (5)
	PROP_Flags (MF_NOBLOCKMAP|MF_DROPOFF)
	PROP_Flags2 (MF2_FLOORCLIP|MF2_NOTELEPORT)

	PROP_SpawnState (S_WRTHFX_SIZZLE1)
END_DEFAULTS

// --------------------------------------------------------------------------

class AWraithFX3 : public AActor
{
	DECLARE_ACTOR (AWraithFX3, AActor)
};

FState AWraithFX3::States[] =
{
#define S_WRTHFX_DROP1 0
	S_BRIGHT (WRBL, 'Q',	4, NULL					    , &States[S_WRTHFX_DROP1+1]),
	S_BRIGHT (WRBL, 'R',	4, NULL					    , &States[S_WRTHFX_DROP1+2]),
	S_BRIGHT (WRBL, 'S',	4, NULL					    , &States[S_WRTHFX_DROP1]),

#define S_WRTHFX_DEAD1 (S_WRTHFX_DROP1+3)
	S_BRIGHT (WRBL, 'S',	4, NULL					    , NULL),
};

IMPLEMENT_ACTOR (AWraithFX3, Hexen, -1, 0)
	PROP_RadiusFixed (2)
	PROP_HeightFixed (5)
	PROP_Mass (5)
	PROP_Flags (MF_NOBLOCKMAP|MF_DROPOFF|MF_MISSILE)
	PROP_Flags2 (MF2_FLOORCLIP|MF2_NOTELEPORT)

	PROP_SpawnState (S_WRTHFX_DROP1)
	PROP_DeathState (S_WRTHFX_DEAD1)

	PROP_DeathSound ("Drip")
END_DEFAULTS

// --------------------------------------------------------------------------

class AWraithFX4 : public AActor
{
	DECLARE_ACTOR (AWraithFX4, AActor)
};

FState AWraithFX4::States[] =
{
#define S_WRTHFX_ADROP1 0
	S_NORMAL (WRBL, 'T',	4, NULL					    , &States[S_WRTHFX_ADROP1+1]),
	S_NORMAL (WRBL, 'U',	4, NULL					    , &States[S_WRTHFX_ADROP1+2]),
	S_NORMAL (WRBL, 'V',	4, NULL					    , &States[S_WRTHFX_ADROP1+3]),
	S_NORMAL (WRBL, 'W',	4, NULL					    , &States[S_WRTHFX_ADROP1]),

#define S_WRTHFX_ADEAD1 (S_WRTHFX_ADROP1+4)
	S_NORMAL (WRBL, 'W',   10, NULL					    , NULL),
};

IMPLEMENT_ACTOR (AWraithFX4, Hexen, -1, 106)
	PROP_RadiusFixed (2)
	PROP_HeightFixed (5)
	PROP_Mass (5)
	PROP_Flags (MF_NOBLOCKMAP|MF_DROPOFF|MF_MISSILE)
	PROP_Flags2 (MF2_NOTELEPORT)

	PROP_SpawnState (S_WRTHFX_ADROP1)
	PROP_DeathState (S_WRTHFX_ADEAD1)

	PROP_DeathSound ("Drip")
END_DEFAULTS

// --------------------------------------------------------------------------

class AWraithFX5 : public AActor
{
	DECLARE_ACTOR (AWraithFX5, AActor)
};

FState AWraithFX5::States[] =
{
#define S_WRTHFX_BDROP1 0
	S_NORMAL (WRBL, 'X',	7, NULL					    , &States[S_WRTHFX_BDROP1+1]),
	S_NORMAL (WRBL, 'Y',	7, NULL					    , &States[S_WRTHFX_BDROP1+2]),
	S_NORMAL (WRBL, 'Z',	7, NULL					    , &States[S_WRTHFX_BDROP1]),

#define S_WRTHFX_BDEAD1 (S_WRTHFX_BDROP1+3)
	S_NORMAL (WRBL, 'Z',   35, NULL					    , NULL),
};

IMPLEMENT_ACTOR (AWraithFX5, Hexen, -1, 107)
	PROP_RadiusFixed (2)
	PROP_HeightFixed (5)
	PROP_Mass (5)
	PROP_Flags (MF_NOBLOCKMAP|MF_DROPOFF|MF_MISSILE)
	PROP_Flags2 (MF2_NOTELEPORT)

	PROP_SpawnState (S_WRTHFX_BDROP1)
	PROP_DeathState (S_WRTHFX_BDEAD1)

	PROP_DeathSound ("Drip")
END_DEFAULTS

//============================================================================
// Wraith Variables
//
//	special1				Internal index into floatbob
//============================================================================

//============================================================================
//
// A_WraithInit
//
//============================================================================

void A_WraithInit (AActor *actor)
{
	actor->z += 48<<FRACBITS;

	// [RH] Make sure the wraith didn't go into the ceiling
	if (actor->z + actor->height > actor->ceilingz)
	{
		actor->z = actor->ceilingz - actor->height;
	}

	actor->special1 = 0;			// index into floatbob
}

//============================================================================
//
// A_WraithRaiseInit
//
//============================================================================

void A_WraithRaiseInit (AActor *actor)
{
	actor->renderflags &= ~RF_INVISIBLE;
	actor->flags2 &= ~MF2_NONSHOOTABLE;
	actor->flags3 &= ~MF3_DONTBLAST;
	actor->flags |= MF_SHOOTABLE|MF_SOLID;
	actor->floorclip = actor->height;
}

//============================================================================
//
// A_WraithRaise
//
//============================================================================

void A_WraithRaise (AActor *actor)
{
	if (A_RaiseMobj (actor))
	{
		// Reached it's target height
		// [RH] Once a buried wraith is fully raised, it should be
		// morphable, right?
		actor->flags3 &= ~(MF3_DONTMORPH|MF3_SPECIALFLOORCLIP);
		actor->SetState (&AWraith::States[S_WRAITH_CHASE1]);
		// [RH] Reset PainChance to a normal wraith's.
		actor->PainChance = GetDefault<AWraith>()->PainChance;
	}

	P_SpawnDirt (actor, actor->radius);
}

//============================================================================
//
// A_WraithMelee
//
//============================================================================

void A_WraithMelee (AActor *actor)
{
	int amount;

	// Steal health from target and give to self
	if (actor->CheckMeleeRange() && (pr_stealhealth()<220))
	{
		amount = pr_stealhealth.HitDice (2);
		P_DamageMobj (actor->target, actor, actor, amount, NAME_Melee);
		actor->health += amount;
	}
}

//============================================================================
//
// A_WraithMissile
//
//============================================================================

void A_WraithMissile (AActor *actor)
{
	if (actor->target != NULL)
	{
		P_SpawnMissile (actor, actor->target, RUNTIME_CLASS(AWraithFX1));
	}
}

//============================================================================
//
// A_WraithFX2 - spawns sparkle tail of missile
//
//============================================================================

void A_WraithFX2 (AActor *actor)
{
	AActor *mo;
	angle_t angle;
	int i;

	for (i = 2; i; --i)
	{
		mo = Spawn<AWraithFX2> (actor->x, actor->y, actor->z, ALLOW_REPLACE);
		if(mo)
		{
			if (pr_wraithfx2 ()<128)
			{
				 angle = actor->angle+(pr_wraithfx2()<<22);
			}
			else
			{
				 angle = actor->angle-(pr_wraithfx2()<<22);
			}
			mo->momz = 0;
			mo->momx = FixedMul((pr_wraithfx2()<<7)+FRACUNIT,
				 finecosine[angle>>ANGLETOFINESHIFT]);
			mo->momy = FixedMul((pr_wraithfx2()<<7)+FRACUNIT, 
				 finesine[angle>>ANGLETOFINESHIFT]);
			mo->target = actor;
			mo->floorclip = 10*FRACUNIT;
		}
	}
}

//============================================================================
//
// A_WraithFX3
//
// Spawn an FX3 around the actor during attacks
//
//============================================================================

void A_WraithFX3 (AActor *actor)
{
	AActor *mo;
	int numdropped = pr_wraithfx3()%15;

	while (numdropped-- > 0)
	{
		mo = Spawn<AWraithFX3> (actor->x, actor->y, actor->z, ALLOW_REPLACE);
		if (mo)
		{
			mo->x += (pr_wraithfx3()-128)<<11;
			mo->y += (pr_wraithfx3()-128)<<11;
			mo->z += (pr_wraithfx3()<<10);
			mo->target = actor;
		}
	}
}

//============================================================================
//
// A_WraithFX4
//
// Spawn an FX4 during movement
//
//============================================================================

void A_WraithFX4 (AActor *actor)
{
	AActor *mo;
	int chance = pr_wraithfx4();
	bool spawn4, spawn5;

	if (chance < 10)
	{
		spawn4 = true;
		spawn5 = false;
	}
	else if (chance < 20)
	{
		spawn4 = false;
		spawn5 = true;
	}
	else if (chance < 25)
	{
		spawn4 = true;
		spawn5 = true;
	}
	else
	{
		spawn4 = false;
		spawn5 = false;
	}

	if (spawn4)
	{
		mo = Spawn<AWraithFX4> (actor->x, actor->y, actor->z, ALLOW_REPLACE);
		if (mo)
		{
			mo->x += (pr_wraithfx4()-128)<<12;
			mo->y += (pr_wraithfx4()-128)<<12;
			mo->z += (pr_wraithfx4()<<10);
			mo->target = actor;
		}
	}
	if (spawn5)
	{
		mo = Spawn<AWraithFX5> (actor->x, actor->y, actor->z, ALLOW_REPLACE);
		if (mo)
		{
			mo->x += (pr_wraithfx4()-128)<<11;
			mo->y += (pr_wraithfx4()-128)<<11;
			mo->z += (pr_wraithfx4()<<10);
			mo->target = actor;
		}
	}
}

//============================================================================
//
// A_WraithLook
//
//============================================================================

void A_WraithLook (AActor *actor)
{
//	A_WraithFX4(actor);		// too expensive
	A_Look(actor);
}

//============================================================================
//
// A_WraithChase
//
//============================================================================

void A_WraithChase (AActor *actor)
{
	int weaveindex = actor->special1;
	actor->z += FloatBobOffsets[weaveindex];
	actor->special1 = (weaveindex+2)&63;
//	if (actor->floorclip > 0)
//	{
//		P_SetMobjState(actor, S_WRAITH_RAISE2);
//		return;
//	}
	A_Chase (actor);
	A_WraithFX4 (actor);
}
