#include "actor.h"
#include "info.h"
#include "p_local.h"
#include "s_sound.h"
#include "p_enemy.h"
#include "a_action.h"
#include "m_random.h"
#include "a_hexenglobal.h"
#include "i_system.h"
#include "p_acs.h"

//============================================================================
//
//	Sorcerer stuff
//
// Sorcerer Variables
//		special1		Angle of ball 1 (all others relative to that)
//		StopBall		which ball to stop at in stop mode (MT_???)
//		args[0]			Denfense time
//		args[1]			Number of full rotations since stopping mode
//		args[2]			Target orbit speed for acceleration/deceleration
//		args[3]			Movement mode (see SORC_ macros)
//		args[4]			Current ball orbit speed
//	Sorcerer Ball Variables
//		special1		Previous angle of ball (for woosh)
//		special2		Countdown of rapid fire (FX4)
//============================================================================

#define SORCBALL_INITIAL_SPEED 		7
#define SORCBALL_TERMINAL_SPEED		25
#define SORCBALL_SPEED_ROTATIONS 	5
#define SORC_DEFENSE_TIME			255
#define SORC_DEFENSE_HEIGHT			45
#define BOUNCE_TIME_UNIT			(35/2)
#define SORCFX4_RAPIDFIRE_TIME		(6*3)		// 3 seconds
#define SORCFX4_SPREAD_ANGLE		20

#define SORC_DECELERATE		0
#define SORC_ACCELERATE 	1
#define SORC_STOPPING		2
#define SORC_FIRESPELL		3
#define SORC_STOPPED		4
#define SORC_NORMAL			5
#define SORC_FIRING_SPELL	6

#define BALL1_ANGLEOFFSET	0
#define BALL2_ANGLEOFFSET	(ANGLE_MAX/3)
#define BALL3_ANGLEOFFSET	((ANGLE_MAX/3)*2)

void A_SorcBallPop (AActor *actor);
void A_SorcBallOrbit (AActor *actor);
void A_SorcSpinBalls (AActor *actor);
void A_SpeedBalls (AActor *actor);
void A_SlowBalls (AActor *actor);
void A_StopBalls (AActor *actor);
void A_AccelBalls (AActor *actor);
void A_DecelBalls (AActor *actor);
void A_SorcBossAttack (AActor *actor);
void A_SpawnFizzle (AActor *actor);
void A_BounceCheck (AActor *actor);
void A_DoBounceCheck (AActor *actor, const char *sound);
void A_SorcFX1Seek (AActor *actor);
void A_SorcOffense2 (AActor *actor);
void A_SorcFX2Split (AActor *actor);
void A_SorcFX2Orbit (AActor *actor);
void A_SorcererBishopEntry (AActor *actor);
void A_SpawnBishop (AActor *actor);
void A_SorcFX4Check (AActor *actor);

static FRandom pr_heresiarch ("Heresiarch");

// The Heresiarch him/itself ------------------------------------------------

FState AHeresiarch::States[] =
{
#define S_SORC_SPAWN1 0
	S_NORMAL (SORC, 'A',	3, NULL					    , &States[S_SORC_SPAWN1+1]),
	S_NORMAL (SORC, 'A',	2, A_SorcSpinBalls		    , &States[S_SORC_SPAWN1+2]),
	S_NORMAL (SORC, 'A',   10, A_Look				    , &States[S_SORC_SPAWN1+2]),

#define S_SORC_WALK1 (S_SORC_SPAWN1+3)
	S_NORMAL (SORC, 'A',	5, A_Chase				    , &States[S_SORC_WALK1+1]),
	S_NORMAL (SORC, 'B',	5, A_Chase				    , &States[S_SORC_WALK1+2]),
	S_NORMAL (SORC, 'C',	5, A_Chase				    , &States[S_SORC_WALK1+3]),
	S_NORMAL (SORC, 'D',	5, A_Chase				    , &States[S_SORC_WALK1]),

#define S_SORC_PAIN1 (S_SORC_WALK1+4)
	S_NORMAL (SORC, 'G',	8, NULL					    , &States[S_SORC_PAIN1+1]),
	S_NORMAL (SORC, 'G',	8, A_Pain				    , &States[S_SORC_WALK1]),

#define S_SORC_ATK2_1 (S_SORC_PAIN1+2)
	S_BRIGHT (SORC, 'F',	6, A_FaceTarget			    , &States[S_SORC_ATK2_1+1]),
	S_BRIGHT (SORC, 'F',	6, A_SpeedBalls			    , &States[S_SORC_ATK2_1+2]),
	S_BRIGHT (SORC, 'F',	6, A_FaceTarget			    , &States[S_SORC_ATK2_1+2]),

#define S_SORC_ATTACK1 (S_SORC_ATK2_1+3)
	S_BRIGHT (SORC, 'E',	6, NULL					    , &States[S_SORC_ATTACK1+1]),
	S_BRIGHT (SORC, 'E',	6, A_SpawnFizzle		    , &States[S_SORC_ATTACK1+2]),
	S_BRIGHT (SORC, 'E',	5, A_FaceTarget			    , &States[S_SORC_ATTACK1+1]),
	S_BRIGHT (SORC, 'E',	2, NULL					    , &States[S_SORC_ATTACK1+4]),
	S_BRIGHT (SORC, 'E',	2, A_SorcBossAttack		    , &States[S_SORC_WALK1]),

#define S_SORC_DIE1 (S_SORC_ATTACK1+5)
	S_BRIGHT (SORC, 'H',	5, NULL					    , &States[S_SORC_DIE1+1]),
	S_BRIGHT (SORC, 'I',	5, A_FaceTarget			    , &States[S_SORC_DIE1+2]),
	S_BRIGHT (SORC, 'J',	5, A_Scream				    , &States[S_SORC_DIE1+3]),
	S_BRIGHT (SORC, 'K',	5, NULL					    , &States[S_SORC_DIE1+4]),
	S_BRIGHT (SORC, 'L',	5, NULL					    , &States[S_SORC_DIE1+5]),
	S_BRIGHT (SORC, 'M',	5, NULL					    , &States[S_SORC_DIE1+6]),
	S_BRIGHT (SORC, 'N',	5, NULL					    , &States[S_SORC_DIE1+7]),
	S_BRIGHT (SORC, 'O',	5, NULL					    , &States[S_SORC_DIE1+8]),
	S_BRIGHT (SORC, 'P',	5, NULL					    , &States[S_SORC_DIE1+9]),
	S_BRIGHT (SORC, 'Q',	5, NULL					    , &States[S_SORC_DIE1+10]),
	S_BRIGHT (SORC, 'R',	5, NULL					    , &States[S_SORC_DIE1+11]),
	S_BRIGHT (SORC, 'S',	5, NULL					    , &States[S_SORC_DIE1+12]),
	S_BRIGHT (SORC, 'T',	5, NULL					    , &States[S_SORC_DIE1+13]),
	S_BRIGHT (SORC, 'U',	5, A_NoBlocking			    , &States[S_SORC_DIE1+14]),
	S_BRIGHT (SORC, 'V',	5, NULL					    , &States[S_SORC_DIE1+15]),
	S_BRIGHT (SORC, 'W',	5, NULL					    , &States[S_SORC_DIE1+16]),
	S_BRIGHT (SORC, 'X',	5, NULL					    , &States[S_SORC_DIE1+17]),
	S_BRIGHT (SORC, 'Y',	5, NULL					    , &States[S_SORC_DIE1+18]),
	S_BRIGHT (SORC, 'Z',   -1, NULL					    , NULL),
};

IMPLEMENT_ACTOR (AHeresiarch, Hexen, 10080, 0)
	PROP_SpawnHealth (5000)
	PROP_PainChance (10)
	PROP_SpeedFixed (16)
	PROP_RadiusFixed (40)
	PROP_HeightFixed (110)
	PROP_Mass (500)
	PROP_Damage (9)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_NOBLOOD|MF_COUNTKILL)
	PROP_Flags2 (MF2_FLOORCLIP|MF2_PASSMOBJ|MF2_BOSS|MF2_PUSHWALL|MF2_MCROSS)
	PROP_Flags3 (MF3_DONTMORPH|MF3_NOTARGET)
	PROP_Flags4 (MF4_NOICEDEATH|MF4_DEFLECT)

	PROP_SpawnState (S_SORC_SPAWN1)
	PROP_SeeState (S_SORC_WALK1)
	PROP_PainState (S_SORC_PAIN1)
	PROP_MissileState (S_SORC_ATK2_1)
	PROP_DeathState (S_SORC_DIE1)

	PROP_SeeSound ("SorcererSight")
	PROP_PainSound ("SorcererPain")
	PROP_DeathSound ("SorcererDeathScream")
	PROP_ActiveSound ("SorcererActive")
	PROP_Obituary ("$OB_HERESIARCH")
END_DEFAULTS

void AHeresiarch::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << StopBall;
}

void AHeresiarch::Die (AActor *source, AActor *inflictor)
{
	// The heresiarch just executes a script instead of a special upon death
	int script = special;
	special = 0;

	Super::Die (source, inflictor);

	if (script != 0)
	{
		P_StartScript (this, NULL, script, level.mapname, 0, 0, 0, 0, 0, false);
	}
}

// Base class for the balls flying around the Heresiarch's head -------------

class ASorcBall : public AActor
{
	DECLARE_STATELESS_ACTOR (ASorcBall, AActor)
public:
	virtual void DoFireSpell ();
	virtual void SorcUpdateBallAngle ();
	virtual void CastSorcererSpell ();
	angle_t AngleOffset;

	void Serialize (FArchive &arc)
	{
		Super::Serialize (arc);
		arc << AngleOffset;
	}

	void GetExplodeParms (int &damage, int &distance, bool &hurtSource)
	{
		distance = 255;
		damage = 255;
		SeeSound = 0;	// don't play bounce
	}

	bool SpecialBlastHandling (AActor *source, fixed_t strength)
	{ // don't blast sorcerer balls
		return false;
	}
};

IMPLEMENT_STATELESS_ACTOR (ASorcBall, Hexen, -1, 0)
	PROP_SpeedFixed (10)
	PROP_RadiusFixed (5)
	PROP_HeightFixed (5)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY|MF_MISSILE)
	PROP_Flags2 (MF2_HEXENBOUNCE|MF2_NOTELEPORT)
	PROP_Flags3 (MF3_FULLVOLDEATH|MF3_CANBOUNCEWATER|MF3_NOWALLBOUNCESND)

	PROP_SeeSound ("SorcererBallBounce")
	PROP_DeathSound ("SorcererBigBallExplode")
END_DEFAULTS

// First ball (purple) - fires projectiles ----------------------------------

class ASorcBall1 : public ASorcBall
{
	DECLARE_ACTOR (ASorcBall1, ASorcBall)
public:
	void BeginPlay ()
	{
		Super::BeginPlay ();
		AngleOffset = BALL1_ANGLEOFFSET;
	}
	virtual void DoFireSpell ();
	virtual void SorcUpdateBallAngle ();
	virtual void CastSorcererSpell ();
};

FState ASorcBall1::States[] =
{
#define S_SORCBALL1_1 0
	S_NORMAL (SBMP, 'A',	2, A_SorcBallOrbit		    , &States[S_SORCBALL1_1+1]),
	S_NORMAL (SBMP, 'B',	2, A_SorcBallOrbit		    , &States[S_SORCBALL1_1+2]),
	S_NORMAL (SBMP, 'C',	2, A_SorcBallOrbit		    , &States[S_SORCBALL1_1+3]),
	S_NORMAL (SBMP, 'D',	2, A_SorcBallOrbit		    , &States[S_SORCBALL1_1+4]),
	S_NORMAL (SBMP, 'E',	2, A_SorcBallOrbit		    , &States[S_SORCBALL1_1+5]),
	S_NORMAL (SBMP, 'F',	2, A_SorcBallOrbit		    , &States[S_SORCBALL1_1+6]),
	S_NORMAL (SBMP, 'G',	2, A_SorcBallOrbit		    , &States[S_SORCBALL1_1+7]),
	S_NORMAL (SBMP, 'H',	2, A_SorcBallOrbit		    , &States[S_SORCBALL1_1+8]),
	S_NORMAL (SBMP, 'I',	2, A_SorcBallOrbit		    , &States[S_SORCBALL1_1+9]),
	S_NORMAL (SBMP, 'J',	2, A_SorcBallOrbit		    , &States[S_SORCBALL1_1+10]),
	S_NORMAL (SBMP, 'K',	2, A_SorcBallOrbit		    , &States[S_SORCBALL1_1+11]),
	S_NORMAL (SBMP, 'L',	2, A_SorcBallOrbit		    , &States[S_SORCBALL1_1+12]),
	S_NORMAL (SBMP, 'M',	2, A_SorcBallOrbit		    , &States[S_SORCBALL1_1+13]),
	S_NORMAL (SBMP, 'N',	2, A_SorcBallOrbit		    , &States[S_SORCBALL1_1+14]),
	S_NORMAL (SBMP, 'O',	2, A_SorcBallOrbit		    , &States[S_SORCBALL1_1+15]),
	S_NORMAL (SBMP, 'P',	2, A_SorcBallOrbit		    , &States[S_SORCBALL1_1]),

#define S_SORCBALL1_D1 (S_SORCBALL1_1+16)
	S_NORMAL (SBMP, 'A',	5, A_SorcBallPop		    , &States[S_SORCBALL1_D1+1]),
	S_NORMAL (SBMP, 'B',	2, A_BounceCheck		    , &States[S_SORCBALL1_D1+1]),

#define S_SORCBALL1_D5 (S_SORCBALL1_D1+2)
	S_NORMAL (SBS4, 'D',	5, A_Explode			    , &States[S_SORCBALL1_D5+1]),
	S_NORMAL (SBS4, 'E',	5, NULL					    , &States[S_SORCBALL1_D5+2]),
	S_NORMAL (SBS4, 'F',	6, NULL					    , &States[S_SORCBALL1_D5+3]),
	S_NORMAL (SBS4, 'G',	6, NULL					    , &States[S_SORCBALL1_D5+4]),
	S_NORMAL (SBS4, 'H',	6, NULL					    , NULL),
};

IMPLEMENT_ACTOR (ASorcBall1, Hexen, -1, 0)
	PROP_SpawnState (S_SORCBALL1_1)
	PROP_PainState (S_SORCBALL1_D1)
	PROP_DeathState (S_SORCBALL1_D5)
END_DEFAULTS

// Second ball (blue) - generates the shield --------------------------------

class ASorcBall2 : public ASorcBall
{
	DECLARE_ACTOR (ASorcBall2, ASorcBall)
public:
	void BeginPlay ()
	{
		Super::BeginPlay ();
		AngleOffset = BALL2_ANGLEOFFSET;
	}
	virtual void CastSorcererSpell ();
};

FState ASorcBall2::States[] =
{
#define S_SORCBALL2_1 0
	S_NORMAL (SBMB, 'A',	2, A_SorcBallOrbit		    , &States[S_SORCBALL2_1+1]),
	S_NORMAL (SBMB, 'B',	2, A_SorcBallOrbit		    , &States[S_SORCBALL2_1+2]),
	S_NORMAL (SBMB, 'C',	2, A_SorcBallOrbit		    , &States[S_SORCBALL2_1+3]),
	S_NORMAL (SBMB, 'D',	2, A_SorcBallOrbit		    , &States[S_SORCBALL2_1+4]),
	S_NORMAL (SBMB, 'E',	2, A_SorcBallOrbit		    , &States[S_SORCBALL2_1+5]),
	S_NORMAL (SBMB, 'F',	2, A_SorcBallOrbit		    , &States[S_SORCBALL2_1+6]),
	S_NORMAL (SBMB, 'G',	2, A_SorcBallOrbit		    , &States[S_SORCBALL2_1+7]),
	S_NORMAL (SBMB, 'H',	2, A_SorcBallOrbit		    , &States[S_SORCBALL2_1+8]),
	S_NORMAL (SBMB, 'I',	2, A_SorcBallOrbit		    , &States[S_SORCBALL2_1+9]),
	S_NORMAL (SBMB, 'J',	2, A_SorcBallOrbit		    , &States[S_SORCBALL2_1+10]),
	S_NORMAL (SBMB, 'K',	2, A_SorcBallOrbit		    , &States[S_SORCBALL2_1+11]),
	S_NORMAL (SBMB, 'L',	2, A_SorcBallOrbit		    , &States[S_SORCBALL2_1+12]),
	S_NORMAL (SBMB, 'M',	2, A_SorcBallOrbit		    , &States[S_SORCBALL2_1+13]),
	S_NORMAL (SBMB, 'N',	2, A_SorcBallOrbit		    , &States[S_SORCBALL2_1+14]),
	S_NORMAL (SBMB, 'O',	2, A_SorcBallOrbit		    , &States[S_SORCBALL2_1+15]),
	S_NORMAL (SBMB, 'P',	2, A_SorcBallOrbit		    , &States[S_SORCBALL2_1]),

#define S_SORCBALL2_D1 (S_SORCBALL2_1+16)
	S_NORMAL (SBMB, 'A',	5, A_SorcBallPop		    , &States[S_SORCBALL2_D1+1]),
	S_NORMAL (SBMB, 'B',	2, A_BounceCheck		    , &States[S_SORCBALL2_D1+1]),

#define S_SORCBALL2_D5 (S_SORCBALL2_D1+2)
	S_NORMAL (SBS3, 'D',	5, A_Explode			    , &States[S_SORCBALL2_D5+1]),
	S_NORMAL (SBS3, 'E',	5, NULL					    , &States[S_SORCBALL2_D5+2]),
	S_NORMAL (SBS3, 'F',	6, NULL					    , &States[S_SORCBALL2_D5+3]),
	S_NORMAL (SBS3, 'G',	6, NULL					    , &States[S_SORCBALL2_D5+4]),
	S_NORMAL (SBS3, 'H',	6, NULL					    , NULL),
};

IMPLEMENT_ACTOR (ASorcBall2, Hexen, -1, 0)
	PROP_SpawnState (S_SORCBALL2_1)
	PROP_PainState (S_SORCBALL2_D1)
	PROP_DeathState (S_SORCBALL2_D5)
END_DEFAULTS

// Third ball (green) - summons Bishops -------------------------------------

class ASorcBall3 : public ASorcBall
{
	DECLARE_ACTOR (ASorcBall3, ASorcBall)
public:
	void BeginPlay ()
	{
		Super::BeginPlay ();
		AngleOffset = BALL3_ANGLEOFFSET;
	}
	virtual void CastSorcererSpell ();
};

FState ASorcBall3::States[] =
{
#define S_SORCBALL3_1 0
	S_NORMAL (SBMG, 'A',	2, A_SorcBallOrbit		    , &States[S_SORCBALL3_1+1]),
	S_NORMAL (SBMG, 'B',	2, A_SorcBallOrbit		    , &States[S_SORCBALL3_1+2]),
	S_NORMAL (SBMG, 'C',	2, A_SorcBallOrbit		    , &States[S_SORCBALL3_1+3]),
	S_NORMAL (SBMG, 'D',	2, A_SorcBallOrbit		    , &States[S_SORCBALL3_1+4]),
	S_NORMAL (SBMG, 'E',	2, A_SorcBallOrbit		    , &States[S_SORCBALL3_1+5]),
	S_NORMAL (SBMG, 'F',	2, A_SorcBallOrbit		    , &States[S_SORCBALL3_1+6]),
	S_NORMAL (SBMG, 'G',	2, A_SorcBallOrbit		    , &States[S_SORCBALL3_1+7]),
	S_NORMAL (SBMG, 'H',	2, A_SorcBallOrbit		    , &States[S_SORCBALL3_1+8]),
	S_NORMAL (SBMG, 'I',	2, A_SorcBallOrbit		    , &States[S_SORCBALL3_1+9]),
	S_NORMAL (SBMG, 'J',	2, A_SorcBallOrbit		    , &States[S_SORCBALL3_1+10]),
	S_NORMAL (SBMG, 'K',	2, A_SorcBallOrbit		    , &States[S_SORCBALL3_1+11]),
	S_NORMAL (SBMG, 'L',	2, A_SorcBallOrbit		    , &States[S_SORCBALL3_1+12]),
	S_NORMAL (SBMG, 'M',	2, A_SorcBallOrbit		    , &States[S_SORCBALL3_1+13]),
	S_NORMAL (SBMG, 'N',	2, A_SorcBallOrbit		    , &States[S_SORCBALL3_1+14]),
	S_NORMAL (SBMG, 'O',	2, A_SorcBallOrbit		    , &States[S_SORCBALL3_1+15]),
	S_NORMAL (SBMG, 'P',	2, A_SorcBallOrbit		    , &States[S_SORCBALL3_1]),

#define S_SORCBALL3_D1 (S_SORCBALL3_1+16)
	S_NORMAL (SBMG, 'A',	5, A_SorcBallPop		    , &States[S_SORCBALL3_D1+1]),
	S_NORMAL (SBMG, 'B',	2, A_BounceCheck		    , &States[S_SORCBALL3_D1+1]),

#define S_SORCBALL3_D5 (S_SORCBALL3_D1+2)
	S_NORMAL (SBS3, 'D',	5, A_Explode			    , &States[S_SORCBALL3_D5+1]),
	S_NORMAL (SBS3, 'E',	5, NULL					    , &States[S_SORCBALL3_D5+2]),
	S_NORMAL (SBS3, 'F',	6, NULL					    , &States[S_SORCBALL3_D5+3]),
	S_NORMAL (SBS3, 'G',	6, NULL					    , &States[S_SORCBALL3_D5+4]),
	S_NORMAL (SBS3, 'H',	6, NULL					    , NULL),
};

IMPLEMENT_ACTOR (ASorcBall3, Hexen, -1, 0)
	PROP_SpawnState (S_SORCBALL3_1)
	PROP_PainState (S_SORCBALL3_D1)
	PROP_DeathState (S_SORCBALL3_D5)
END_DEFAULTS

// Sorcerer spell 1 (The burning, bouncing head thing) ----------------------

class ASorcFX1 : public AActor
{
	DECLARE_ACTOR (ASorcFX1, AActor)
public:
	void GetExplodeParms (int &damage, int &distance, bool &hurtSource)
	{
		damage = 30;
	}
	bool FloorBounceMissile (secplane_t &plane)
	{
		fixed_t orgmomz = momz;

		if (!Super::FloorBounceMissile (plane))
		{
			momz = -orgmomz;		// no energy absorbed
			return false;
		}
		return true;
	}
};

FState ASorcFX1::States[] =
{
#define S_SORCFX1_1 0
	S_BRIGHT (SBS1, 'A',	2, NULL					    , &States[S_SORCFX1_1+1]),
	S_BRIGHT (SBS1, 'B',	3, A_SorcFX1Seek		    , &States[S_SORCFX1_1+2]),
	S_BRIGHT (SBS1, 'C',	3, A_SorcFX1Seek		    , &States[S_SORCFX1_1+3]),
	S_BRIGHT (SBS1, 'D',	3, A_SorcFX1Seek		    , &States[S_SORCFX1_1]),

#define S_SORCFX1_D1 (S_SORCFX1_1+4)
	S_BRIGHT (FHFX, 'S',	2, A_Explode			    , &States[S_SORCFX1_D1+1]),
	S_BRIGHT (FHFX, 'S',	6, NULL					    , &States[S_SORCFX1_D1+2]),
	S_BRIGHT (FHFX, 'S',	6, NULL					    , NULL),
};

IMPLEMENT_ACTOR (ASorcFX1, Hexen, -1, 0)
	PROP_SpeedFixed (7)
	PROP_RadiusFixed (5)
	PROP_HeightFixed (5)
	PROP_Flags (MF_NOBLOCKMAP|MF_MISSILE)
	PROP_Flags2 (MF2_HEXENBOUNCE|MF2_NOTELEPORT)
	PROP_Flags3 (MF3_FULLVOLDEATH|MF3_CANBOUNCEWATER|MF3_NOWALLBOUNCESND)

	PROP_SpawnState (S_SORCFX1_1)
	PROP_DeathState (S_SORCFX1_D1)
	PROP_XDeathState (S_SORCFX1_D1)

	PROP_SeeSound ("SorcererBallBounce")
	PROP_DeathSound ("SorcererHeadScream")
END_DEFAULTS

// Sorcerer spell 2 (The visible part of the shield) ------------------------

class ASorcFX2 : public AActor
{
	DECLARE_ACTOR (ASorcFX2, AActor)
};

FState ASorcFX2::States[] =
{
#define S_SORCFX2_SPLIT1 0
	S_BRIGHT (SBS2, 'A',	3, A_SorcFX2Split		    , &States[S_SORCFX2_SPLIT1]),

#define S_SORCFX2T1 (S_SORCFX2_SPLIT1+1)
	S_NORMAL (SBS2, 'A',   10, NULL					    , NULL),

#define S_SORCFX2_ORBIT1 (S_SORCFX2T1+1)
	S_BRIGHT (SBS2, 'A',	2, A_SorcFX2Orbit		    , &States[S_SORCFX2_ORBIT1+1]),
	S_BRIGHT (SBS2, 'B',	2, A_SorcFX2Orbit		    , &States[S_SORCFX2_ORBIT1+2]),
	S_BRIGHT (SBS2, 'C',	2, A_SorcFX2Orbit		    , &States[S_SORCFX2_ORBIT1+3]),
	S_BRIGHT (SBS2, 'D',	2, A_SorcFX2Orbit		    , &States[S_SORCFX2_ORBIT1+4]),
	S_BRIGHT (SBS2, 'E',	2, A_SorcFX2Orbit		    , &States[S_SORCFX2_ORBIT1+5]),
	S_BRIGHT (SBS2, 'F',	2, A_SorcFX2Orbit		    , &States[S_SORCFX2_ORBIT1+6]),
	S_BRIGHT (SBS2, 'G',	2, A_SorcFX2Orbit		    , &States[S_SORCFX2_ORBIT1+7]),
	S_BRIGHT (SBS2, 'H',	2, A_SorcFX2Orbit		    , &States[S_SORCFX2_ORBIT1+8]),
	S_BRIGHT (SBS2, 'I',	2, A_SorcFX2Orbit		    , &States[S_SORCFX2_ORBIT1+9]),
	S_BRIGHT (SBS2, 'J',	2, A_SorcFX2Orbit		    , &States[S_SORCFX2_ORBIT1+10]),
	S_BRIGHT (SBS2, 'K',	2, A_SorcFX2Orbit		    , &States[S_SORCFX2_ORBIT1+11]),
	S_BRIGHT (SBS2, 'L',	2, A_SorcFX2Orbit		    , &States[S_SORCFX2_ORBIT1+12]),
	S_BRIGHT (SBS2, 'M',	2, A_SorcFX2Orbit		    , &States[S_SORCFX2_ORBIT1+13]),
	S_BRIGHT (SBS2, 'N',	2, A_SorcFX2Orbit		    , &States[S_SORCFX2_ORBIT1+14]),
	S_BRIGHT (SBS2, 'O',	2, A_SorcFX2Orbit		    , &States[S_SORCFX2_ORBIT1+15]),
	S_BRIGHT (SBS2, 'P',	2, A_SorcFX2Orbit		    , &States[S_SORCFX2_ORBIT1]),
};

IMPLEMENT_ACTOR (ASorcFX2, Hexen, -1, 0)
	PROP_SpeedFixed (15)
	PROP_RadiusFixed (5)
	PROP_HeightFixed (5)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY)
	PROP_Flags2 (MF2_NOTELEPORT)

	PROP_SpawnState (S_SORCFX2_SPLIT1)
	PROP_DeathState (S_SORCFX2T1)
END_DEFAULTS

// The translucent trail behind SorcFX2 -------------------------------------

class ASorcFX2T1 : public ASorcFX2
{
	DECLARE_STATELESS_ACTOR (ASorcFX2T1, ASorcFX2)
};

IMPLEMENT_STATELESS_ACTOR (ASorcFX2T1, Hexen, -1, 0)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY)
	PROP_Flags2 (MF2_NOTELEPORT)
	PROP_RenderStyle (STYLE_Translucent)
	PROP_Alpha (HX_ALTSHADOW)

	PROP_SpawnState (S_SORCFX2T1)
END_DEFAULTS

// Sorcerer spell 3 (The Bishop spawner) ------------------------------------

class ASorcFX3 : public AActor
{
	DECLARE_ACTOR (ASorcFX3, AActor)
};

FState ASorcFX3::States[] =
{
#define S_SORCFX3_1 0
	S_BRIGHT (SBS3, 'A',	2, NULL					    , &States[S_SORCFX3_1+1]),
	S_BRIGHT (SBS3, 'B',	2, NULL					    , &States[S_SORCFX3_1+2]),
	S_BRIGHT (SBS3, 'C',	2, NULL					    , &States[S_SORCFX3_1]),

#define S_BISHMORPH1 (S_SORCFX3_1+3)
	S_BRIGHT (SBS3, 'A',	4, NULL					    , &States[S_BISHMORPH1+1]),
	S_NORMAL (BISH, 'P',	4, A_SorcererBishopEntry    , &States[S_BISHMORPH1+2]),
	S_NORMAL (BISH, 'O',	4, NULL					    , &States[S_BISHMORPH1+3]),
	S_NORMAL (BISH, 'N',	4, NULL					    , &States[S_BISHMORPH1+4]),
	S_NORMAL (BISH, 'M',	3, NULL					    , &States[S_BISHMORPH1+5]),
	S_NORMAL (BISH, 'L',	3, NULL					    , &States[S_BISHMORPH1+6]),
	S_NORMAL (BISH, 'K',	3, NULL					    , &States[S_BISHMORPH1+7]),
	S_NORMAL (BISH, 'J',	3, NULL					    , &States[S_BISHMORPH1+8]),
	S_NORMAL (BISH, 'I',	3, NULL					    , &States[S_BISHMORPH1+9]),
	S_NORMAL (BISH, 'H',	3, NULL					    , &States[S_BISHMORPH1+10]),
	S_NORMAL (BISH, 'G',	3, A_SpawnBishop		    , NULL),
};

IMPLEMENT_ACTOR (ASorcFX3, Hexen, -1, 0)
	PROP_SpeedFixed (15)
	PROP_RadiusFixed (22)
	PROP_HeightFixed (65)
	PROP_Flags (MF_NOBLOCKMAP|MF_MISSILE)
	PROP_Flags2 (MF2_NOTELEPORT)

	PROP_SpawnState (S_SORCFX3_1)
	PROP_DeathState (S_BISHMORPH1)

	PROP_SeeSound ("SorcererBishopSpawn")
END_DEFAULTS

// The Bishop spawner's explosion animation ---------------------------------

class ASorcFX3Explosion : public AActor
{
	DECLARE_ACTOR (ASorcFX3Explosion, AActor)
};

FState ASorcFX3Explosion::States[] =
{
#define S_SORCFX3_EXP1 0
	S_NORMAL (SBS3, 'D',	3, NULL					    , &States[S_SORCFX3_EXP1+1]),
	S_NORMAL (SBS3, 'E',	3, NULL					    , &States[S_SORCFX3_EXP1+2]),
	S_NORMAL (SBS3, 'F',	3, NULL					    , &States[S_SORCFX3_EXP1+3]),
	S_NORMAL (SBS3, 'G',	3, NULL					    , &States[S_SORCFX3_EXP1+4]),
	S_NORMAL (SBS3, 'H',	3, NULL					    , NULL),
};

IMPLEMENT_ACTOR (ASorcFX3Explosion, Hexen, -1, 0)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY)
	PROP_Flags2 (MF2_NOTELEPORT)
	PROP_RenderStyle (STYLE_Translucent)
	PROP_Alpha (HX_ALTSHADOW)

	PROP_SpawnState (S_SORCFX3_EXP1)
END_DEFAULTS


// Sorcerer spell 4 (The purple projectile) ---------------------------------

class ASorcFX4 : public AActor
{
	DECLARE_ACTOR (ASorcFX4, AActor)
public:
	void GetExplodeParms (int &damage, int &distance, bool &hurtSource)
	{
		damage = 20;
	}
};

FState ASorcFX4::States[] =
{
#define S_SORCFX4_1 0
	S_BRIGHT (SBS4, 'A',	2, A_SorcFX4Check		    , &States[S_SORCFX4_1+1]),
	S_BRIGHT (SBS4, 'B',	2, A_SorcFX4Check		    , &States[S_SORCFX4_1+2]),
	S_BRIGHT (SBS4, 'C',	2, A_SorcFX4Check		    , &States[S_SORCFX4_1]),

#define S_SORCFX4_D1 (S_SORCFX4_1+3)
	S_BRIGHT (SBS4, 'D',	2, NULL					    , &States[S_SORCFX4_D1+1]),
	S_BRIGHT (SBS4, 'E',	2, A_Explode			    , &States[S_SORCFX4_D1+2]),
	S_BRIGHT (SBS4, 'F',	2, NULL					    , &States[S_SORCFX4_D1+3]),
	S_BRIGHT (SBS4, 'G',	2, NULL					    , &States[S_SORCFX4_D1+4]),
	S_BRIGHT (SBS4, 'H',	2, NULL					    , NULL),
};

IMPLEMENT_ACTOR (ASorcFX4, Hexen, -1, 0)
	PROP_SpeedFixed (12)
	PROP_RadiusFixed (10)
	PROP_HeightFixed (10)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY|MF_MISSILE)
	PROP_Flags2 (MF2_NOTELEPORT)
	PROP_RenderStyle (STYLE_Add)

	PROP_SpawnState (S_SORCFX4_1)
	PROP_DeathState (S_SORCFX4_D1)

	PROP_DeathSound ("SorcererBallExplode")
END_DEFAULTS

// Spark that appears when shooting SorcFX4 ---------------------------------

class ASorcSpark1 : public AActor
{
	DECLARE_ACTOR (ASorcSpark1, AActor)
};

FState ASorcSpark1::States[] =
{
#define S_SORCSPARK1 0
	S_BRIGHT (SBFX, 'A',	4, NULL					    , &States[S_SORCSPARK1+1]),
	S_BRIGHT (SBFX, 'B',	4, NULL					    , &States[S_SORCSPARK1+2]),
	S_BRIGHT (SBFX, 'C',	4, NULL					    , &States[S_SORCSPARK1+3]),
	S_BRIGHT (SBFX, 'D',	4, NULL					    , &States[S_SORCSPARK1+4]),
	S_BRIGHT (SBFX, 'E',	4, NULL					    , &States[S_SORCSPARK1+5]),
	S_BRIGHT (SBFX, 'F',	4, NULL					    , &States[S_SORCSPARK1+6]),
	S_BRIGHT (SBFX, 'G',	4, NULL					    , NULL),
};

IMPLEMENT_ACTOR (ASorcSpark1, Hexen, -1, 0)
	PROP_RadiusFixed (5)
	PROP_HeightFixed (5)
	PROP_Gravity (FRACUNIT/8)
	PROP_Flags (MF_NOBLOCKMAP|MF_DROPOFF)
	PROP_Flags2 (MF2_NOTELEPORT)
	PROP_RenderStyle (STYLE_Add)

	PROP_SpawnState (S_SORCSPARK1)
END_DEFAULTS

//============================================================================
//
// SorcBall::DoFireSpell
//
//============================================================================

void ASorcBall::DoFireSpell ()
{
	CastSorcererSpell ();
	target->args[3] = SORC_STOPPED;
}

//============================================================================
//
// SorcBall1::DoFireSpell
//
//============================================================================

void ASorcBall1::DoFireSpell ()
{
	if (pr_heresiarch() < 200)
	{
		S_Sound (target, CHAN_VOICE, "SorcererSpellCast", 1, ATTN_NONE);
		special2 = SORCFX4_RAPIDFIRE_TIME;
		args[4] = 128;
		target->args[3] = SORC_FIRING_SPELL;
	}
	else
	{
		Super::DoFireSpell ();
	}
}

//============================================================================
//
// A_SorcSpinBalls
//
// Spawn spinning balls above head - actor is sorcerer
//============================================================================

void A_SorcSpinBalls(AActor *actor)
{
	AActor *mo;
	fixed_t z;

	actor->SpawnState += 2;		// [RH] Don't spawn balls again
	A_SlowBalls(actor);
	actor->args[0] = 0;								// Currently no defense
	actor->args[3] = SORC_NORMAL;
	actor->args[4] = SORCBALL_INITIAL_SPEED;		// Initial orbit speed
	actor->special1 = ANGLE_1;
	z = actor->z - actor->floorclip + actor->height;
	
	mo = Spawn<ASorcBall1> (actor->x, actor->y, z, NO_REPLACE);
	if (mo)
	{
		mo->target = actor;
		mo->special2 = SORCFX4_RAPIDFIRE_TIME;
	}
	mo = Spawn<ASorcBall2> (actor->x, actor->y, z, NO_REPLACE);
	if (mo) mo->target = actor;
	mo = Spawn<ASorcBall3> (actor->x, actor->y, z, NO_REPLACE);
	if (mo) mo->target = actor;
}


//============================================================================
//
// A_SorcBallOrbit
//
//============================================================================

void A_SorcBallOrbit(AActor *ball)
{
	// [RH] If no parent, then die instead of crashing
	if (ball->target == NULL)
	{
		ball->SetState (ball->FindState(NAME_Pain));
		return;
	}

	ASorcBall *actor;
	int x,y;
	angle_t angle, baseangle;
	int mode = ball->target->args[3];
	AHeresiarch *parent = barrier_cast<AHeresiarch *>(ball->target);
	int dist = parent->radius - (ball->radius<<1);
	angle_t prevangle = ball->special1;

	if (!ball->IsKindOf (RUNTIME_CLASS(ASorcBall)))
	{
		I_Error ("Corrupted sorcerer:\nTried to use a %s", RUNTIME_TYPE(ball)->TypeName.GetChars());
	}
	actor = static_cast<ASorcBall *> (ball);

	if (actor->target->health <= 0)
	{
		actor->SetState (actor->FindState(NAME_Pain));
		return;
	}

	baseangle = (angle_t)parent->special1;
	angle = baseangle + actor->AngleOffset;
	actor->angle = angle;
	angle >>= ANGLETOFINESHIFT;

	switch (mode)
	{
	case SORC_NORMAL:			// Balls rotating normally
		actor->SorcUpdateBallAngle ();
		break;

	case SORC_DECELERATE:		// Balls decelerating
		A_DecelBalls(actor);
		actor->SorcUpdateBallAngle ();
		break;

	case SORC_ACCELERATE:		// Balls accelerating
		A_AccelBalls(actor);
		actor->SorcUpdateBallAngle ();
		break;

	case SORC_STOPPING:			// Balls stopping
		if ((parent->StopBall == RUNTIME_TYPE(actor)) &&
			 (parent->args[1] > SORCBALL_SPEED_ROTATIONS) &&
			 (abs(angle - (parent->angle>>ANGLETOFINESHIFT)) < (30<<5)))
		{
			// Can stop now
			actor->target->args[3] = SORC_FIRESPELL;
			actor->target->args[4] = 0;
			// Set angle so ball angle == sorcerer angle
			parent->special1 = (int)(parent->angle - actor->AngleOffset);
		}
		else
		{
			actor->SorcUpdateBallAngle ();
		}
		break;

	case SORC_FIRESPELL:			// Casting spell
		if (parent->StopBall == RUNTIME_TYPE(actor))
		{
			// Put sorcerer into special throw spell anim
			if (parent->health > 0)
				parent->SetStateNF (&AHeresiarch::States[S_SORC_ATTACK1]);

			actor->DoFireSpell ();
		}
		break;

	case SORC_FIRING_SPELL:
		if (parent->StopBall == RUNTIME_TYPE(actor))
		{
			if (actor->special2-- <= 0)
			{
				// Done rapid firing 
				parent->args[3] = SORC_STOPPED;
				// Back to orbit balls
				if (parent->health > 0)
					parent->SetStateNF (&AHeresiarch::States[S_SORC_ATTACK1+3]);
			}
			else
			{
				// Do rapid fire spell
				A_SorcOffense2(actor);
			}
		}
		break;

	case SORC_STOPPED:			// Balls stopped
	default:
		break;
	}

	if ((angle < prevangle) && (parent->args[4]==SORCBALL_TERMINAL_SPEED))
	{
		parent->args[1]++;			// Bump rotation counter
		// Completed full rotation - make woosh sound
		S_Sound (actor, CHAN_BODY, "SorcererBallWoosh", 1, ATTN_NORM);
	}
	actor->special1 = angle;		// Set previous angle
	x = parent->x + FixedMul(dist, finecosine[angle]);
	y = parent->y + FixedMul(dist, finesine[angle]);
	actor->SetOrigin (x, y, parent->z - parent->floorclip + parent->height);
	actor->floorz = parent->floorz;
	actor->ceilingz = parent->ceilingz;
}

//============================================================================
//
// A_SpeedBalls
//
// Set balls to speed mode - actor is sorcerer
//
//============================================================================

void A_SpeedBalls(AActor *actor)
{
	actor->args[3] = SORC_ACCELERATE;				// speed mode
	actor->args[2] = SORCBALL_TERMINAL_SPEED;		// target speed
}


//============================================================================
//
// A_SlowBalls
//
// Set balls to slow mode - actor is sorcerer
//
//============================================================================

void A_SlowBalls(AActor *actor)
{
	actor->args[3] = SORC_DECELERATE;				// slow mode
	actor->args[2] = SORCBALL_INITIAL_SPEED;		// target speed
}

//============================================================================
//
// A_StopBalls
//
// Instant stop when rotation gets to ball in special2
//		actor is sorcerer
//
//============================================================================

void A_StopBalls(AActor *scary)
{
	AHeresiarch *actor = static_cast<AHeresiarch *> (scary);
	int chance = pr_heresiarch();
	actor->args[3] = SORC_STOPPING;				// stopping mode
	actor->args[1] = 0;							// Reset rotation counter

	if ((actor->args[0] <= 0) && (chance < 200))
	{
		actor->StopBall = RUNTIME_CLASS(ASorcBall2);	// Blue
	}
	else if((actor->health < (actor->GetDefault()->health >> 1)) &&
			(chance < 200))
	{
		actor->StopBall = RUNTIME_CLASS(ASorcBall3);	// Green
	}
	else
	{
		actor->StopBall = RUNTIME_CLASS(ASorcBall1);	// Yellow
	}
}

//============================================================================
//
// A_AccelBalls
//
// Increase ball orbit speed - actor is ball
//
//============================================================================

void A_AccelBalls(AActor *actor)
{
	AActor *sorc = actor->target;

	if (sorc->args[4] < sorc->args[2])
	{
		sorc->args[4]++;
	}
	else
	{
		sorc->args[3] = SORC_NORMAL;
		if (sorc->args[4] >= SORCBALL_TERMINAL_SPEED)
		{
			// Reached terminal velocity - stop balls
			A_StopBalls(sorc);
		}
	}
}

//============================================================================
//
// A_DecelBalls
//
// Decrease ball orbit speed - actor is ball
//
//============================================================================

void A_DecelBalls(AActor *actor)
{
	AActor *sorc = actor->target;

	if (sorc->args[4] > sorc->args[2])
	{
		sorc->args[4]--;
	}
	else
	{
		sorc->args[3] = SORC_NORMAL;
	}
}

//============================================================================
//
// ASorcBall1::SorcUpdateBallAngle
//
// Update angle if first ball
//============================================================================

void ASorcBall1::SorcUpdateBallAngle ()
{
	target->special1 += ANGLE_1*target->args[4];
}

//============================================================================
//
// ASorcBall::SorcUpdateBallAngle
//
//============================================================================

void ASorcBall::SorcUpdateBallAngle ()
{
}

//============================================================================
//
// ASorcBall::CastSorcererSpell
//
// Make noise and change the parent sorcerer's animation
//
//============================================================================

void ASorcBall::CastSorcererSpell ()
{
	S_Sound (target, CHAN_VOICE, "SorcererSpellCast", 1, ATTN_NONE);

	// Put sorcerer into throw spell animation
	if (target->health > 0)
		target->SetStateNF (&AHeresiarch::States[S_SORC_ATTACK1+3]);
}

//============================================================================
//
// ASorcBall2::CastSorcererSpell
//
// Defensive
//
//============================================================================

void ASorcBall2::CastSorcererSpell ()
{
	Super::CastSorcererSpell ();

	AActor *parent = target;
	AActor *mo;

	fixed_t z = parent->z - parent->floorclip + SORC_DEFENSE_HEIGHT*FRACUNIT;
	mo = Spawn<ASorcFX2> (x, y, z, ALLOW_REPLACE);
	parent->flags2 |= MF2_REFLECTIVE|MF2_INVULNERABLE;
	parent->args[0] = SORC_DEFENSE_TIME;
	if (mo) mo->target = parent;
}

//============================================================================
//
// ASorcBall3::CastSorcererSpell
//
// Reinforcements
//
//============================================================================

void ASorcBall3::CastSorcererSpell ()
{
	Super::CastSorcererSpell ();

	AActor *mo;
	angle_t ang1, ang2;
	AActor *parent = target;

	ang1 = angle - ANGLE_45;
	ang2 = angle + ANGLE_45;
	if (health < (GetDefault()->health/3))
	{	// Spawn 2 at a time
		mo = P_SpawnMissileAngle(parent, RUNTIME_CLASS(ASorcFX3), ang1, 4*FRACUNIT);
		if (mo) mo->target = parent;
		mo = P_SpawnMissileAngle(parent, RUNTIME_CLASS(ASorcFX3), ang2, 4*FRACUNIT);
		if (mo) mo->target = parent;
	}			
	else
	{
		if (pr_heresiarch() < 128)
			ang1 = ang2;
		mo = P_SpawnMissileAngle(parent, RUNTIME_CLASS(ASorcFX3), ang1, 4*FRACUNIT);
		if (mo) mo->target = parent;
	}
}


/*
void A_SpawnReinforcements(AActor *actor)
{
	AActor *parent = actor->target;
	AActor *mo;
	angle_t ang;

	ang = ANGLE_1 * P_Random();
	mo = P_SpawnMissileAngle(actor, MT_SORCFX3, ang, 5*FRACUNIT);
	if (mo) mo->target = parent;
}
*/

//============================================================================
//
// SorcBall1::CastSorcererSpell
//
// Offensive
//
//============================================================================

void ASorcBall1::CastSorcererSpell ()
{
	Super::CastSorcererSpell ();

	AActor *mo;
	angle_t ang1, ang2;
	AActor *parent = target;

	ang1 = angle + ANGLE_1*70;
	ang2 = angle - ANGLE_1*70;
	mo = P_SpawnMissileAngle (parent, RUNTIME_CLASS(ASorcFX1), ang1, 0);
	if (mo)
	{
		mo->target = parent;
		mo->tracer = parent->target;
		mo->args[4] = BOUNCE_TIME_UNIT;
		mo->args[3] = 15;				// Bounce time in seconds
	}
	mo = P_SpawnMissileAngle (parent, RUNTIME_CLASS(ASorcFX1), ang2, 0);
	if (mo)
	{
		mo->target = parent;
		mo->tracer = parent->target;
		mo->args[4] = BOUNCE_TIME_UNIT;
		mo->args[3] = 15;				// Bounce time in seconds
	}
}

//============================================================================
//
// A_SorcOffense2
//
// Actor is ball
//
//============================================================================

void A_SorcOffense2(AActor *actor)
{
	angle_t ang1;
	AActor *mo;
	int delta, index;
	AActor *parent = actor->target;
	AActor *dest = parent->target;
	int dist;

	// [RH] If no enemy, then don't try to shoot.
	if (dest == NULL)
	{
		return;
	}

	index = actor->args[4] << 5;
	actor->args[4] += 15;
	delta = (finesine[index])*SORCFX4_SPREAD_ANGLE;
	delta = (delta>>FRACBITS)*ANGLE_1;
	ang1 = actor->angle + delta;
	mo = P_SpawnMissileAngle(parent, RUNTIME_CLASS(ASorcFX4), ang1, 0);
	if (mo)
	{
		mo->special2 = 35*5/2;		// 5 seconds
		dist = P_AproxDistance(dest->x - mo->x, dest->y - mo->y);
		dist = dist/mo->Speed;
		if(dist < 1) dist = 1;
		mo->momz = (dest->z-mo->z)/dist;
	}
}

//============================================================================
//
// A_SorcBossAttack
//
// Resume ball spinning
//
//============================================================================

void A_SorcBossAttack(AActor *actor)
{
	actor->args[3] = SORC_ACCELERATE;
	actor->args[2] = SORCBALL_INITIAL_SPEED;
}

//============================================================================
//
// A_SpawnFizzle
//
// spell cast magic fizzle
//
//============================================================================

void A_SpawnFizzle(AActor *actor)
{
	fixed_t x,y,z;
	fixed_t dist = 5*FRACUNIT;
	angle_t angle = actor->angle >> ANGLETOFINESHIFT;
	fixed_t speed = actor->Speed;
	angle_t rangle;
	AActor *mo;
	int ix;

	x = actor->x + FixedMul(dist,finecosine[angle]);
	y = actor->y + FixedMul(dist,finesine[angle]);
	z = actor->z - actor->floorclip + (actor->height>>1);
	for (ix=0; ix<5; ix++)
	{
		mo = Spawn<ASorcSpark1> (x, y, z, ALLOW_REPLACE);
		if (mo)
		{
			rangle = angle + ((pr_heresiarch()%5) << 1);
			mo->momx = FixedMul(pr_heresiarch()%speed,finecosine[rangle]);
			mo->momy = FixedMul(pr_heresiarch()%speed,finesine[rangle]);
			mo->momz = FRACUNIT*2;
		}
	}
}


//============================================================================
//
// A_SorcFX1Seek
//
// Yellow spell - offense
//
//============================================================================

void A_SorcFX1Seek(AActor *actor)
{
	A_DoBounceCheck (actor, "SorcererHeadScream");
	P_SeekerMissile (actor,ANGLE_1*2,ANGLE_1*6);
}


//============================================================================
//
// A_SorcFX2Split
//
// Blue spell - defense
//
//============================================================================
//
// FX2 Variables
//		special1		current angle
//		special2
//		args[0]		0 = CW,  1 = CCW
//		args[1]		
//============================================================================

// Split ball in two
void A_SorcFX2Split(AActor *actor)
{
	AActor *mo;

	mo = Spawn<ASorcFX2> (actor->x, actor->y, actor->z, NO_REPLACE);
	if (mo)
	{
		mo->target = actor->target;
		mo->args[0] = 0;									// CW
		mo->special1 = actor->angle;					// Set angle
		mo->SetStateNF (&ASorcFX2::States[S_SORCFX2_ORBIT1]);
	}
	mo = Spawn<ASorcFX2> (actor->x, actor->y, actor->z, NO_REPLACE);
	if (mo)
	{
		mo->target = actor->target;
		mo->args[0] = 1;									// CCW
		mo->special1 = actor->angle;					// Set angle
		mo->SetStateNF (&ASorcFX2::States[S_SORCFX2_ORBIT1]);
	}
	actor->Destroy ();
}

//============================================================================
//
// A_SorcFX2Orbit
//
// Orbit FX2 about sorcerer
//
//============================================================================

void A_SorcFX2Orbit (AActor *actor)
{
	angle_t angle;
	fixed_t x,y,z;
	AActor *parent = actor->target;

	// [RH] If no parent, then disappear
	if (parent == NULL)
	{
		actor->Destroy();
		return;
	}

	fixed_t dist = parent->radius;

	if ((parent->health <= 0) ||		// Sorcerer is dead
		(!parent->args[0]))				// Time expired
	{
		actor->SetStateNF (actor->FindState(NAME_Death));
		parent->args[0] = 0;
		parent->flags2 &= ~MF2_REFLECTIVE;
		parent->flags2 &= ~MF2_INVULNERABLE;
	}

	if (actor->args[0] && (parent->args[0]-- <= 0))		// Time expired
	{
		actor->SetStateNF (actor->FindState(NAME_Death));
		parent->args[0] = 0;
		parent->flags2 &= ~MF2_REFLECTIVE;
	}

	// Move to new position based on angle
	if (actor->args[0])		// Counter clock-wise
	{
		actor->special1 += ANGLE_1*10;
		angle = ((angle_t)actor->special1) >> ANGLETOFINESHIFT;
		x = parent->x + FixedMul(dist, finecosine[angle]);
		y = parent->y + FixedMul(dist, finesine[angle]);
		z = parent->z - parent->floorclip + SORC_DEFENSE_HEIGHT*FRACUNIT;
		z += FixedMul(15*FRACUNIT,finecosine[angle]);
		// Spawn trailer
		Spawn<ASorcFX2T1> (x, y, z, ALLOW_REPLACE);
	}
	else							// Clock wise
	{
		actor->special1 -= ANGLE_1*10;
		angle = ((angle_t)actor->special1) >> ANGLETOFINESHIFT;
		x = parent->x + FixedMul(dist, finecosine[angle]);
		y = parent->y + FixedMul(dist, finesine[angle]);
		z = parent->z - parent->floorclip + SORC_DEFENSE_HEIGHT*FRACUNIT;
		z += FixedMul(20*FRACUNIT,finesine[angle]);
		// Spawn trailer
		Spawn<ASorcFX2T1> (x, y, z, ALLOW_REPLACE);
	}

	actor->SetOrigin (x, y, z);
	actor->floorz = parent->floorz;
	actor->ceilingz = parent->ceilingz;
}

//============================================================================
//
// A_SpawnBishop
//
// Green spell - spawn bishops
//
//============================================================================

void A_SpawnBishop(AActor *actor)
{
	AActor *mo;
	mo = Spawn("Bishop", actor->x, actor->y, actor->z, ALLOW_REPLACE);
	if (mo)
	{
		if (!P_TestMobjLocation(mo))
		{
			mo->Destroy ();
			level.total_monsters--;
		}
		else if (actor->target != NULL)
		{ // [RH] Make the new bishops inherit the Heriarch's target
			mo->CopyFriendliness (actor->target, true);
			mo->master = actor->target;
		}
	}
	actor->Destroy ();
}

//============================================================================
//
// A_SorcererBishopEntry
//
//============================================================================

void A_SorcererBishopEntry(AActor *actor)
{
	Spawn<ASorcFX3Explosion> (actor->x, actor->y, actor->z, ALLOW_REPLACE);
	S_Sound (actor, CHAN_VOICE, actor->SeeSound, 1, ATTN_NORM);
}

//============================================================================
//
// A_SorcFX4Check
//
// FX4 - rapid fire balls
//
//============================================================================

void A_SorcFX4Check(AActor *actor)
{
	if (actor->special2-- <= 0)
	{
		actor->SetStateNF (actor->FindState(NAME_Death));
	}
}

//============================================================================
//
// A_SorcBallPop
//
// Ball death - bounce away in a random direction
//
//============================================================================

void A_SorcBallPop(AActor *actor)
{
	S_Sound (actor, CHAN_BODY, "SorcererBallPop", 1, ATTN_NONE);
	actor->flags &= ~MF_NOGRAVITY;
	actor->gravity = FRACUNIT/8;
	actor->momx = ((pr_heresiarch()%10)-5) << FRACBITS;
	actor->momy = ((pr_heresiarch()%10)-5) << FRACBITS;
	actor->momz = (2+(pr_heresiarch()%3)) << FRACBITS;
	actor->special2 = 4*FRACUNIT;		// Initial bounce factor
	actor->args[4] = BOUNCE_TIME_UNIT;	// Bounce time unit
	actor->args[3] = 5;					// Bounce time in seconds
}

//============================================================================
//
// A_DoBounceCheck
//
//============================================================================

void A_DoBounceCheck (AActor *actor, const char *sound)
{
	if (actor->args[4]-- <= 0)
	{
		if (actor->args[3]-- <= 0)
		{
			actor->SetState (actor->FindState(NAME_Death));
			S_Sound (actor, CHAN_BODY, sound, 1, ATTN_NONE);
		}
		else
		{
			actor->args[4] = BOUNCE_TIME_UNIT;
		}
	}
}

//============================================================================
//
// A_BounceCheck
//
//============================================================================

void A_BounceCheck (AActor *actor)
{
	A_DoBounceCheck (actor, "SorcererBigBallExplode");
}
