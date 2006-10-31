#include "actor.h"
#include "info.h"
#include "a_hereticglobal.h"
#include "p_local.h"
#include "p_enemy.h"
#include "a_action.h"
#include "s_sound.h"
#include "m_random.h"
#include "a_sharedglobal.h"
#include "gstrings.h"

static FRandom pr_s2fx1 ("S2FX1");
static FRandom pr_scrc1atk ("Srcr1Attack");
static FRandom pr_dst ("D'SparilTele");
static FRandom pr_s2d ("Srcr2Decide");
static FRandom pr_s2a ("Srcr2Attack");
static FRandom pr_bluespark ("BlueSpark");

void A_Sor1Chase (AActor *);
void A_Sor1Pain (AActor *);
void A_Srcr1Attack (AActor *);
void A_SorZap (AActor *);
void A_SorcererRise (AActor *);
void A_SorRise (AActor *);
void A_SorSightSnd (AActor *);
void A_Srcr2Decide (AActor *);
void A_Srcr2Attack (AActor *);
void A_Sor2DthInit (AActor *);
void A_SorDSph (AActor *);
void A_Sor2DthLoop (AActor *);
void A_SorDExp (AActor *);
void A_SorDBon (AActor *);
void A_BlueSpark (AActor *);
void A_GenWizard (AActor *);

// Boss spot ----------------------------------------------------------------

class ABossSpot : public AActor
{
	DECLARE_STATELESS_ACTOR (ABossSpot, AActor)
public:
	ABossSpot *NextSpot;
	void Serialize (FArchive &arc);
	void BeginPlay ();
};

void ABossSpot::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << NextSpot;
}

IMPLEMENT_STATELESS_ACTOR (ABossSpot, Heretic, 56, 141)
	PROP_RenderFlags (RF_INVISIBLE)
END_DEFAULTS

void ABossSpot::BeginPlay ()
{
	Super::BeginPlay ();
	NextSpot = NULL;
}

// Sorcerer (D'Sparil on his serpent) ---------------------------------------

class ASorcerer1 : public AActor
{
	DECLARE_ACTOR (ASorcerer1, AActor)
};

FState ASorcerer1::States[] =
{
#define S_SRCR1_LOOK 0
	S_NORMAL (SRCR, 'A',   10, A_Look					, &States[S_SRCR1_LOOK+1]),
	S_NORMAL (SRCR, 'B',   10, A_Look					, &States[S_SRCR1_LOOK+0]),

#define S_SRCR1_WALK (S_SRCR1_LOOK+2)
	S_NORMAL (SRCR, 'A',	5, A_Sor1Chase				, &States[S_SRCR1_WALK+1]),
	S_NORMAL (SRCR, 'B',	5, A_Sor1Chase				, &States[S_SRCR1_WALK+2]),
	S_NORMAL (SRCR, 'C',	5, A_Sor1Chase				, &States[S_SRCR1_WALK+3]),
	S_NORMAL (SRCR, 'D',	5, A_Sor1Chase				, &States[S_SRCR1_WALK+0]),

#define S_SRCR1_PAIN (S_SRCR1_WALK+4)
	S_NORMAL (SRCR, 'Q',	6, A_Sor1Pain				, &States[S_SRCR1_WALK+0]),

#define S_SRCR1_ATK (S_SRCR1_PAIN+1)
	S_NORMAL (SRCR, 'Q',	7, A_FaceTarget 			, &States[S_SRCR1_ATK+1]),
	S_NORMAL (SRCR, 'R',	6, A_FaceTarget 			, &States[S_SRCR1_ATK+2]),
	S_NORMAL (SRCR, 'S',   10, A_Srcr1Attack			, &States[S_SRCR1_WALK+0]),
	S_NORMAL (SRCR, 'S',   10, A_FaceTarget 			, &States[S_SRCR1_ATK+4]),
	S_NORMAL (SRCR, 'Q',	7, A_FaceTarget 			, &States[S_SRCR1_ATK+5]),
	S_NORMAL (SRCR, 'R',	6, A_FaceTarget 			, &States[S_SRCR1_ATK+6]),
	S_NORMAL (SRCR, 'S',   10, A_Srcr1Attack			, &States[S_SRCR1_WALK+0]),

#define S_SRCR1_DIE (S_SRCR1_ATK+7)
	S_NORMAL (SRCR, 'E',	7, NULL 					, &States[S_SRCR1_DIE+1]),
	S_NORMAL (SRCR, 'F',	7, A_Scream 				, &States[S_SRCR1_DIE+2]),
	S_NORMAL (SRCR, 'G',	7, NULL 					, &States[S_SRCR1_DIE+3]),
	S_NORMAL (SRCR, 'H',	6, NULL 					, &States[S_SRCR1_DIE+4]),
	S_NORMAL (SRCR, 'I',	6, NULL 					, &States[S_SRCR1_DIE+5]),
	S_NORMAL (SRCR, 'J',	6, NULL 					, &States[S_SRCR1_DIE+6]),
	S_NORMAL (SRCR, 'K',	6, NULL 					, &States[S_SRCR1_DIE+7]),
	S_NORMAL (SRCR, 'L',   25, A_SorZap 				, &States[S_SRCR1_DIE+8]),
	S_NORMAL (SRCR, 'M',	5, NULL 					, &States[S_SRCR1_DIE+9]),
	S_NORMAL (SRCR, 'N',	5, NULL 					, &States[S_SRCR1_DIE+10]),
	S_NORMAL (SRCR, 'O',	4, NULL 					, &States[S_SRCR1_DIE+11]),
	S_NORMAL (SRCR, 'L',   20, A_SorZap 				, &States[S_SRCR1_DIE+12]),
	S_NORMAL (SRCR, 'M',	5, NULL 					, &States[S_SRCR1_DIE+13]),
	S_NORMAL (SRCR, 'N',	5, NULL 					, &States[S_SRCR1_DIE+14]),
	S_NORMAL (SRCR, 'O',	4, NULL 					, &States[S_SRCR1_DIE+15]),
	S_NORMAL (SRCR, 'L',   12, NULL 					, &States[S_SRCR1_DIE+16]),
	S_NORMAL (SRCR, 'P',   -1, A_SorcererRise			, NULL)
};

IMPLEMENT_ACTOR (ASorcerer1, Heretic, 7, 142)
	PROP_SpawnHealth (2000)
	PROP_RadiusFixed (28)
	PROP_HeightFixed (100)
	PROP_Mass (800)
	PROP_SpeedFixed (16)
	PROP_PainChance (56)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_COUNTKILL)
	PROP_Flags2 (MF2_MCROSS|MF2_FLOORCLIP|MF2_PASSMOBJ|MF2_PUSHWALL|MF2_BOSS)
	PROP_Flags3 (MF3_DONTMORPH|MF3_NORADIUSDMG|MF3_NOTARGET)
	PROP_Flags4 (MF4_NOICEDEATH)

	PROP_SpawnState (S_SRCR1_LOOK)
	PROP_SeeState (S_SRCR1_WALK)
	PROP_PainState (S_SRCR1_PAIN)
	PROP_MissileState (S_SRCR1_ATK)
	PROP_DeathState (S_SRCR1_DIE)

	PROP_SeeSound ("dsparilserpent/sight")
	PROP_AttackSound ("dsparilserpent/attack")
	PROP_PainSound ("dsparilserpent/pain")
	PROP_DeathSound ("dsparilserpent/death")
	PROP_ActiveSound ("dsparilserpent/active")
	PROP_Obituary("$OB_DSPARIL1")
	PROP_HitObituary("$OB_DSPARIL1HIT")
END_DEFAULTS

// Sorcerer FX 1 ------------------------------------------------------------

class ASorcererFX1 : public AActor
{
	DECLARE_ACTOR (ASorcererFX1, AActor)
};

FState ASorcererFX1::States[] =
{
#define S_SRCRFX1 0
	S_BRIGHT (FX14, 'A',	6, NULL 					, &States[S_SRCRFX1+1]),
	S_BRIGHT (FX14, 'B',	6, NULL 					, &States[S_SRCRFX1+2]),
	S_BRIGHT (FX14, 'C',	6, NULL 					, &States[S_SRCRFX1+0]),

#define S_SRCRFXI1 (S_SRCRFX1+3)
	S_BRIGHT (FX14, 'D',	5, NULL 					, &States[S_SRCRFXI1+1]),
	S_BRIGHT (FX14, 'E',	5, NULL 					, &States[S_SRCRFXI1+2]),
	S_BRIGHT (FX14, 'F',	5, NULL 					, &States[S_SRCRFXI1+3]),
	S_BRIGHT (FX14, 'G',	5, NULL 					, &States[S_SRCRFXI1+4]),
	S_BRIGHT (FX14, 'H',	5, NULL 					, NULL)
};

IMPLEMENT_ACTOR (ASorcererFX1, Heretic, -1, 144)
	PROP_RadiusFixed (10)
	PROP_HeightFixed (10)
	PROP_SpeedFixed (20)
	PROP_Damage (10)
	PROP_DamageType (NAME_Fire)
	PROP_Flags (MF_NOBLOCKMAP|MF_MISSILE|MF_DROPOFF|MF_NOGRAVITY)
	PROP_Flags2 (MF2_NOTELEPORT)
	PROP_RenderStyle (STYLE_Add)

	PROP_SpawnState (S_SRCRFX1)
	PROP_DeathState (S_SRCRFXI1)
END_DEFAULTS

AT_SPEED_SET (SorcererFX1, speed)
{
	SimpleSpeedSetter (ASorcererFX1, 20*FRACUNIT, 28*FRACUNIT, speed);
}

// Sorcerer 2 (D'Sparil without his serpent) --------------------------------

FState ASorcerer2::States[] =
{
#define S_SOR2_LOOK 0
	S_NORMAL (SOR2, 'M',   10, A_Look					, &States[S_SOR2_LOOK+1]),
	S_NORMAL (SOR2, 'N',   10, A_Look					, &States[S_SOR2_LOOK+0]),

#define S_SOR2_WALK (S_SOR2_LOOK+2)
	S_NORMAL (SOR2, 'M',	4, A_Chase					, &States[S_SOR2_WALK+1]),
	S_NORMAL (SOR2, 'N',	4, A_Chase					, &States[S_SOR2_WALK+2]),
	S_NORMAL (SOR2, 'O',	4, A_Chase					, &States[S_SOR2_WALK+3]),
	S_NORMAL (SOR2, 'P',	4, A_Chase					, &States[S_SOR2_WALK+0]),

#define S_SOR2_RISE (S_SOR2_WALK+4)
	S_NORMAL (SOR2, 'A',	4, NULL 					, &States[S_SOR2_RISE+1]),
	S_NORMAL (SOR2, 'B',	4, NULL 					, &States[S_SOR2_RISE+2]),
	S_NORMAL (SOR2, 'C',	4, A_SorRise				, &States[S_SOR2_RISE+3]),
	S_NORMAL (SOR2, 'D',	4, NULL 					, &States[S_SOR2_RISE+4]),
	S_NORMAL (SOR2, 'E',	4, NULL 					, &States[S_SOR2_RISE+5]),
	S_NORMAL (SOR2, 'F',	4, NULL 					, &States[S_SOR2_RISE+6]),
	S_NORMAL (SOR2, 'G',   12, A_SorSightSnd			, &States[S_SOR2_WALK+0]),

#define S_SOR2_PAIN (S_SOR2_RISE+7)
	S_NORMAL (SOR2, 'Q',	3, NULL 					, &States[S_SOR2_PAIN+1]),
	S_NORMAL (SOR2, 'Q',	6, A_Pain					, &States[S_SOR2_WALK+0]),

#define S_SOR2_ATK (S_SOR2_PAIN+2)
	S_NORMAL (SOR2, 'R',	9, A_Srcr2Decide			, &States[S_SOR2_ATK+1]),
	S_NORMAL (SOR2, 'S',	9, A_FaceTarget 			, &States[S_SOR2_ATK+2]),
	S_NORMAL (SOR2, 'T',   20, A_Srcr2Attack			, &States[S_SOR2_WALK+0]),

#define S_SOR2_TELE (S_SOR2_ATK+3)
	S_NORMAL (SOR2, 'L',	6, NULL 					, &States[S_SOR2_TELE+1]),
	S_NORMAL (SOR2, 'K',	6, NULL 					, &States[S_SOR2_TELE+2]),
	S_NORMAL (SOR2, 'J',	6, NULL 					, &States[S_SOR2_TELE+3]),
	S_NORMAL (SOR2, 'I',	6, NULL 					, &States[S_SOR2_TELE+4]),
	S_NORMAL (SOR2, 'H',	6, NULL 					, &States[S_SOR2_TELE+5]),
	S_NORMAL (SOR2, 'G',	6, NULL 					, &States[S_SOR2_WALK+0]),

#define S_SOR2_DIE (S_SOR2_TELE+6)
	S_NORMAL (SDTH, 'A',	8, A_Sor2DthInit			, &States[S_SOR2_DIE+1]),
	S_NORMAL (SDTH, 'B',	8, NULL 					, &States[S_SOR2_DIE+2]),
	S_NORMAL (SDTH, 'C',	8, A_SorDSph				, &States[S_SOR2_DIE+3]),
	S_NORMAL (SDTH, 'D',	7, NULL 					, &States[S_SOR2_DIE+4]),
	S_NORMAL (SDTH, 'E',	7, NULL 					, &States[S_SOR2_DIE+5]),
	S_NORMAL (SDTH, 'F',	7, A_Sor2DthLoop			, &States[S_SOR2_DIE+6]),
	S_NORMAL (SDTH, 'G',	6, A_SorDExp				, &States[S_SOR2_DIE+7]),
	S_NORMAL (SDTH, 'H',	6, NULL 					, &States[S_SOR2_DIE+8]),
	S_NORMAL (SDTH, 'I',   18, NULL 					, &States[S_SOR2_DIE+9]),
	S_NORMAL (SDTH, 'J',	6, A_NoBlocking 			, &States[S_SOR2_DIE+10]),
	S_NORMAL (SDTH, 'K',	6, A_SorDBon				, &States[S_SOR2_DIE+11]),
	S_NORMAL (SDTH, 'L',	6, NULL 					, &States[S_SOR2_DIE+12]),
	S_NORMAL (SDTH, 'M',	6, NULL 					, &States[S_SOR2_DIE+13]),
	S_NORMAL (SDTH, 'N',	6, NULL 					, &States[S_SOR2_DIE+14]),
	S_NORMAL (SDTH, 'O',   -1, A_BossDeath				, NULL)
};

IMPLEMENT_ACTOR (ASorcerer2, Heretic, -1, 143)
	PROP_SpawnHealth (3500)
	PROP_RadiusFixed (16)
	PROP_HeightFixed (70)
	PROP_Mass (300)
	PROP_SpeedFixed (14)
	PROP_PainChance (32)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_COUNTKILL|MF_DROPOFF)
	PROP_Flags2 (MF2_MCROSS|MF2_FLOORCLIP|MF2_PASSMOBJ|MF2_PUSHWALL|MF2_BOSS)
	PROP_Flags3 (MF3_DONTMORPH|MF3_FULLVOLACTIVE|MF3_NORADIUSDMG|MF3_NOTARGET)
	PROP_Flags4 (MF4_NOICEDEATH)

	PROP_SpawnState (S_SOR2_LOOK)
	PROP_SeeState (S_SOR2_WALK)
	PROP_PainState (S_SOR2_PAIN)
	PROP_MissileState (S_SOR2_ATK)
	PROP_DeathState (S_SOR2_DIE)

	PROP_SeeSound ("dsparil/sight")
	PROP_AttackSound ("dsparil/attack")
	PROP_PainSound ("dsparil/pain")
	PROP_ActiveSound ("dsparil/active")
	PROP_Obituary("$OB_DSPARIL2")
	PROP_HitObituary("$OB_DSPARIL2HIT")
END_DEFAULTS

void ASorcerer2::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << NumBossSpots << FirstBossSpot;
}

void ASorcerer2::BeginPlay ()
{
	TThinkerIterator<ABossSpot> iterator;
	ABossSpot *spot;

	Super::BeginPlay ();
	NumBossSpots = 0;
	spot = iterator.Next ();
	FirstBossSpot = static_cast<ABossSpot *> (spot);
	while (spot)
	{
		NumBossSpots++;
		spot->NextSpot = iterator.Next ();
		spot = spot->NextSpot;
	}
}

// Sorcerer 2 FX 1 ----------------------------------------------------------

class ASorcerer2FX1 : public AActor
{
	DECLARE_ACTOR (ASorcerer2FX1, AActor)
public:
	void GetExplodeParms (int &damage, int &distance, bool &hurtSource);
};

FState ASorcerer2FX1::States[] =
{
#define S_SOR2FX1 0
	S_BRIGHT (FX16, 'A',	3, A_BlueSpark				, &States[S_SOR2FX1+1]),
	S_BRIGHT (FX16, 'B',	3, A_BlueSpark				, &States[S_SOR2FX1+2]),
	S_BRIGHT (FX16, 'C',	3, A_BlueSpark				, &States[S_SOR2FX1+0]),

#define S_SOR2FXI1 (S_SOR2FX1+3)
	S_BRIGHT (FX16, 'G',	5, A_Explode				, &States[S_SOR2FXI1+1]),
	S_BRIGHT (FX16, 'H',	5, NULL 					, &States[S_SOR2FXI1+2]),
	S_BRIGHT (FX16, 'I',	5, NULL 					, &States[S_SOR2FXI1+3]),
	S_BRIGHT (FX16, 'J',	5, NULL 					, &States[S_SOR2FXI1+4]),
	S_BRIGHT (FX16, 'K',	5, NULL 					, &States[S_SOR2FXI1+5]),
	S_BRIGHT (FX16, 'L',	5, NULL 					, NULL)
};

IMPLEMENT_ACTOR (ASorcerer2FX1, Heretic, -1, 145)
	PROP_RadiusFixed (10)
	PROP_HeightFixed (6)
	PROP_SpeedFixed (20)
	PROP_Damage (1)
	PROP_Flags (MF_NOBLOCKMAP|MF_MISSILE|MF_DROPOFF|MF_NOGRAVITY)
	PROP_Flags2 (MF2_NOTELEPORT)
	PROP_RenderStyle (STYLE_Add)

	PROP_SpawnState (S_SOR2FX1)
	PROP_DeathState (S_SOR2FXI1)
END_DEFAULTS

AT_SPEED_SET (Sorcerer2FX1, speed)
{
	SimpleSpeedSetter (ASorcerer2FX1, 20*FRACUNIT, 28*FRACUNIT, speed);
}

void ASorcerer2FX1::GetExplodeParms (int &damage, int &distance, bool &hurtSource)
{
	damage = 80 + (pr_s2fx1() & 31);
}

// Sorcerer 2 FX Spark ------------------------------------------------------

class ASorcerer2FXSpark : public AActor
{
	DECLARE_ACTOR (ASorcerer2FXSpark, AActor)
};

FState ASorcerer2FXSpark::States[] =
{
#define S_SOR2FXSPARK 0
	S_BRIGHT (FX16, 'D',   12, NULL 					, &States[S_SOR2FXSPARK+1]),
	S_BRIGHT (FX16, 'E',   12, NULL 					, &States[S_SOR2FXSPARK+2]),
	S_BRIGHT (FX16, 'F',   12, NULL 					, NULL)
};

IMPLEMENT_ACTOR (ASorcerer2FXSpark, Heretic, -1, 0)
	PROP_RadiusFixed (20)
	PROP_HeightFixed (16)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY)
	PROP_Flags2 (MF2_NOTELEPORT|MF2_CANNOTPUSH)
	PROP_RenderStyle (STYLE_Add)

	PROP_SpawnState (S_SOR2FXSPARK)
END_DEFAULTS

// Sorcerer 2 FX 2 ----------------------------------------------------------

class ASorcerer2FX2 : public AActor
{
	DECLARE_ACTOR (ASorcerer2FX2, AActor)
};

FState ASorcerer2FX2::States[] =
{
#define S_SOR2FX2 0
	S_BRIGHT (FX11, 'A',   35, NULL 					, &States[S_SOR2FX2+1]),
	S_BRIGHT (FX11, 'A',	5, A_GenWizard				, &States[S_SOR2FX2+2]),
	S_BRIGHT (FX11, 'B',	5, NULL 					, &States[S_SOR2FX2+1]),

#define S_SOR2FXI2 (S_SOR2FX2+3)
	S_BRIGHT (FX11, 'C',	5, NULL 					, &States[S_SOR2FXI2+1]),
	S_BRIGHT (FX11, 'D',	5, NULL 					, &States[S_SOR2FXI2+2]),
	S_BRIGHT (FX11, 'E',	5, NULL 					, &States[S_SOR2FXI2+3]),
	S_BRIGHT (FX11, 'F',	5, NULL 					, &States[S_SOR2FXI2+4]),
	S_BRIGHT (FX11, 'G',	5, NULL 					, NULL)
};

IMPLEMENT_ACTOR (ASorcerer2FX2, Heretic, -1, 146)
	PROP_RadiusFixed (10)
	PROP_HeightFixed (6)
	PROP_SpeedFixed (6)
	PROP_Damage (10)
	PROP_Flags (MF_NOBLOCKMAP|MF_MISSILE|MF_DROPOFF|MF_NOGRAVITY)
	PROP_Flags2 (MF2_NOTELEPORT)
	PROP_RenderStyle (STYLE_Add)

	PROP_SpawnState (S_SOR2FX2)
	PROP_DeathState (S_SOR2FXI2)
END_DEFAULTS

// Sorcerer 2 Telefade ------------------------------------------------------

class ASorcerer2Telefade : public AActor
{
	DECLARE_ACTOR (ASorcerer2Telefade, AActor)
};

FState ASorcerer2Telefade::States[] =
{
#define S_SOR2TELEFADE 0
	S_NORMAL (SOR2, 'G',	8, NULL 					, &States[S_SOR2TELEFADE+1]),
	S_NORMAL (SOR2, 'H',	6, NULL 					, &States[S_SOR2TELEFADE+2]),
	S_NORMAL (SOR2, 'I',	6, NULL 					, &States[S_SOR2TELEFADE+3]),
	S_NORMAL (SOR2, 'J',	6, NULL 					, &States[S_SOR2TELEFADE+4]),
	S_NORMAL (SOR2, 'K',	6, NULL 					, &States[S_SOR2TELEFADE+5]),
	S_NORMAL (SOR2, 'L',	6, NULL 					, NULL)
};

IMPLEMENT_ACTOR (ASorcerer2Telefade, Heretic, -1, 0)
	PROP_Flags (MF_NOBLOCKMAP)
	PROP_SpawnState (S_SOR2TELEFADE)
END_DEFAULTS

//----------------------------------------------------------------------------
//
// PROC A_Sor1Pain
//
//----------------------------------------------------------------------------

void A_Sor1Pain (AActor *actor)
{
	actor->special1 = 20; // Number of steps to walk fast
	A_Pain (actor);
}

//----------------------------------------------------------------------------
//
// PROC A_Sor1Chase
//
//----------------------------------------------------------------------------

void A_Sor1Chase (AActor *actor)
{
	if (actor->special1)
	{
		actor->special1--;
		actor->tics -= 3;
	}
	A_Chase(actor);
}

//----------------------------------------------------------------------------
//
// PROC A_Srcr1Attack
//
// Sorcerer demon attack.
//
//----------------------------------------------------------------------------

void A_Srcr1Attack (AActor *actor)
{
	AActor *mo;
	fixed_t momz;
	angle_t angle;

	if (!actor->target)
	{
		return;
	}
	S_SoundID (actor, CHAN_BODY, actor->AttackSound, 1, ATTN_NORM);
	if (actor->CheckMeleeRange ())
	{
		int damage = pr_scrc1atk.HitDice (8);
		P_DamageMobj (actor->target, actor, actor, damage, NAME_Melee);
		P_TraceBleed (damage, actor->target, actor);
		return;
	}
	if (actor->health > (actor->GetDefault()->health/3)*2)
	{ // Spit one fireball
		P_SpawnMissileZ (actor, actor->z + 48*FRACUNIT, actor->target, RUNTIME_CLASS(ASorcererFX1));
	}
	else
	{ // Spit three fireballs
		mo = P_SpawnMissileZ (actor, actor->z + 48*FRACUNIT, actor->target, RUNTIME_CLASS(ASorcererFX1));
		if (mo != NULL)
		{
			momz = mo->momz;
			angle = mo->angle;
			P_SpawnMissileAngleZ (actor, actor->z + 48*FRACUNIT, RUNTIME_CLASS(ASorcererFX1), angle-ANGLE_1*3, momz);
			P_SpawnMissileAngleZ (actor, actor->z + 48*FRACUNIT, RUNTIME_CLASS(ASorcererFX1), angle+ANGLE_1*3, momz);
		}
		if (actor->health < actor->GetDefault()->health/3)
		{ // Maybe attack again
			if (actor->special1)
			{ // Just attacked, so don't attack again
				actor->special1 = 0;
			}
			else
			{ // Set state to attack again
				actor->special1 = 1;
				actor->SetState (&ASorcerer1::States[S_SRCR1_ATK+3]);
			}
		}
	}
}

//----------------------------------------------------------------------------
//
// PROC A_SorcererRise
//
//----------------------------------------------------------------------------

void A_SorcererRise (AActor *actor)
{
	AActor *mo;

	actor->flags &= ~MF_SOLID;
	mo = Spawn<ASorcerer2> (actor->x, actor->y, actor->z, ALLOW_REPLACE);
	mo->SetState (&ASorcerer2::States[S_SOR2_RISE]);
	mo->angle = actor->angle;
	mo->CopyFriendliness (actor, true);
}

//----------------------------------------------------------------------------
//
// PROC P_DSparilTeleport
//
//----------------------------------------------------------------------------

void P_DSparilTeleport (AActor *actor)
{
	int i;
	fixed_t prevX;
	fixed_t prevY;
	fixed_t prevZ;
	AActor *mo;
	ASorcerer2 *self = static_cast<ASorcerer2 *> (actor);
	ABossSpot *spot;
	ABossSpot *initial;

	if (!self->NumBossSpots)
	{ // No spots
		return;
	}
	i = pr_dst () % self->NumBossSpots;
	spot = static_cast<ABossSpot *> (self->FirstBossSpot);
	while (i-- > 0)
	{
		spot = spot->NextSpot;
	}
	initial = spot;
	while (P_AproxDistance (actor->x - spot->x, actor->y - spot->y) < 128*FRACUNIT)
	{
		spot = spot->NextSpot;
		if (spot == NULL)
		{
			spot = static_cast<ABossSpot *> (self->FirstBossSpot);
		}
		if (spot == initial)
		{
			// [RH] Don't inifinite loop if no spots further than 128*FRACUNIT
			return;
		}
	}
	prevX = actor->x;
	prevY = actor->y;
	prevZ = actor->z;
	if (P_TeleportMove (actor, spot->x, spot->y, spot->z, false))
	{
		mo = Spawn<ASorcerer2Telefade> (prevX, prevY, prevZ, ALLOW_REPLACE);
		S_Sound (mo, CHAN_BODY, "misc/teleport", 1, ATTN_NORM);
		actor->SetState (&ASorcerer2::States[S_SOR2_TELE]);
		S_Sound (actor, CHAN_BODY, "misc/teleport", 1, ATTN_NORM);
		actor->z = actor->floorz;
		actor->angle = spot->angle;
		actor->momx = actor->momy = actor->momz = 0;
	}
}

//----------------------------------------------------------------------------
//
// PROC A_Srcr2Decide
//
//----------------------------------------------------------------------------

void A_Srcr2Decide (AActor *actor)
{

	static const int chance[] =
	{
		192, 120, 120, 120, 64, 64, 32, 16, 0
	};

	unsigned int chanceindex = actor->health / (actor->GetDefault()->health/8);
	if (chanceindex >= countof(chance))
	{
		chanceindex = countof(chance) - 1;
	}

	if (pr_s2d() < chance[chanceindex])
	{
		P_DSparilTeleport (actor);
	}
}

//----------------------------------------------------------------------------
//
// PROC A_Srcr2Attack
//
//----------------------------------------------------------------------------

void A_Srcr2Attack (AActor *actor)
{
	int chance;

	if (!actor->target)
	{
		return;
	}
	S_SoundID (actor, CHAN_BODY, actor->AttackSound, 1, ATTN_NONE);
	if (actor->CheckMeleeRange())
	{
		int damage = pr_s2a.HitDice (20);
		P_DamageMobj (actor->target, actor, actor, damage, NAME_Melee);
		P_TraceBleed (damage, actor->target, actor);
		return;
	}
	chance = actor->health < actor->GetDefault()->health/2 ? 96 : 48;
	if (pr_s2a() < chance)
	{ // Wizard spawners
		P_SpawnMissileAngle (actor, RUNTIME_CLASS(ASorcerer2FX2),
			actor->angle-ANG45, FRACUNIT/2);
		P_SpawnMissileAngle (actor, RUNTIME_CLASS(ASorcerer2FX2),
			actor->angle+ANG45, FRACUNIT/2);
	}
	else
	{ // Blue bolt
		P_SpawnMissile (actor, actor->target, RUNTIME_CLASS(ASorcerer2FX1));
	}
}

//----------------------------------------------------------------------------
//
// PROC A_BlueSpark
//
//----------------------------------------------------------------------------

void A_BlueSpark (AActor *actor)
{
	int i;
	AActor *mo;

	for (i = 0; i < 2; i++)
	{
		mo = Spawn<ASorcerer2FXSpark> (actor->x, actor->y, actor->z, ALLOW_REPLACE);
		mo->momx = pr_bluespark.Random2() << 9;
		mo->momy = pr_bluespark.Random2() << 9;
		mo->momz = FRACUNIT + (pr_bluespark()<<8);
	}
}

//----------------------------------------------------------------------------
//
// PROC A_GenWizard
//
//----------------------------------------------------------------------------

void A_GenWizard (AActor *actor)
{
	AActor *mo;

	mo = Spawn<AWizard> (actor->x, actor->y, actor->z - GetDefault<AWizard>()->height/2, ALLOW_REPLACE);
	if (mo != NULL)
	{
		if (!P_TestMobjLocation (mo))
		{ // Didn't fit
			mo->Destroy ();
			level.total_monsters--;
		}
		else
		{ // [RH] Make the new wizards inherit D'Sparil's target
			mo->CopyFriendliness (actor->target, true);

			actor->momx = actor->momy = actor->momz = 0;
			actor->SetState (actor->FindState(NAME_Death));
			actor->flags &= ~MF_MISSILE;
			mo->master = actor->target;
			// Heretic did not offset it by TELEFOGHEIGHT, so I won't either.
			Spawn<ATeleportFog> (actor->x, actor->y, actor->z, ALLOW_REPLACE);
		}
	}
}

//----------------------------------------------------------------------------
//
// PROC A_Sor2DthInit
//
//----------------------------------------------------------------------------

void A_Sor2DthInit (AActor *actor)
{
	actor->special1 = 7; // Animation loop counter
	P_Massacre (); // Kill monsters early
}

//----------------------------------------------------------------------------
//
// PROC A_Sor2DthLoop
//
//----------------------------------------------------------------------------

void A_Sor2DthLoop (AActor *actor)
{
	if (--actor->special1)
	{ // Need to loop
		actor->SetState (&ASorcerer2::States[S_SOR2_DIE+3]);
	}
}

//----------------------------------------------------------------------------
//
// D'Sparil Sound Routines
//
//----------------------------------------------------------------------------

void A_SorZap (AActor *actor) {S_Sound (actor, CHAN_BODY, "dsparil/zap", 1, ATTN_NONE);}
void A_SorRise (AActor *actor) {S_Sound (actor, CHAN_BODY, "dsparil/rise", 1, ATTN_NONE);}
void A_SorDSph (AActor *actor) {S_Sound (actor, CHAN_BODY, "dsparil/scream", 1, ATTN_NONE);}
void A_SorDExp (AActor *actor) {S_Sound (actor, CHAN_BODY, "dsparil/explode", 1, ATTN_NONE);}
void A_SorDBon (AActor *actor) {S_Sound (actor, CHAN_BODY, "dsparil/bones", 1, ATTN_NONE);}
void A_SorSightSnd (AActor *actor) {S_Sound (actor, CHAN_BODY, "dsparil/sight", 1, ATTN_NONE);}
