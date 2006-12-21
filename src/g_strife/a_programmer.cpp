#include "actor.h"
#include "m_random.h"
#include "a_action.h"
#include "p_local.h"
#include "p_enemy.h"
#include "s_sound.h"
#include "a_strifeglobal.h"
#include "f_finale.h"

static FRandom pr_prog ("Programmer");

void A_SentinelBob (AActor *);
void A_TossGib (AActor *);
void A_Countdown (AActor *);

void A_ProgrammerMelee (AActor *);
void A_SpotLightning (AActor *);
void A_SpawnProgrammerBase (AActor *);
void A_ProgrammerDeath (AActor *);

// Programmer ---------------------------------------------------------------

class AProgrammer : public AActor
{
	DECLARE_ACTOR (AProgrammer, AActor)
public:
	void NoBlockingSet ();
};

FState AProgrammer::States[] =
{
#define S_PROG_STND 0
	S_NORMAL (PRGR, 'A',   5, A_Look,				&States[S_PROG_STND+1]),
	S_NORMAL (PRGR, 'A',   1, A_SentinelBob,		&States[S_PROG_STND]),

#define S_PROG_CHASE (S_PROG_STND+2)
	S_NORMAL (PRGR, 'A', 160, A_SentinelBob,		&States[S_PROG_CHASE+1]),
	S_NORMAL (PRGR, 'B',   5, A_SentinelBob,		&States[S_PROG_CHASE+2]),
	S_NORMAL (PRGR, 'C',   5, A_SentinelBob,		&States[S_PROG_CHASE+3]),
	S_NORMAL (PRGR, 'D',   5, A_SentinelBob,		&States[S_PROG_CHASE+4]),
	S_NORMAL (PRGR, 'E',   2, A_SentinelBob,		&States[S_PROG_CHASE+5]),
	S_NORMAL (PRGR, 'F',   2, A_SentinelBob,		&States[S_PROG_CHASE+6]),
	S_NORMAL (PRGR, 'E',   3, A_Chase,				&States[S_PROG_CHASE+7]),
	S_NORMAL (PRGR, 'F',   3, A_Chase,				&States[S_PROG_CHASE+4]),

#define S_PROG_MELEE (S_PROG_CHASE+8)
	S_NORMAL (PRGR, 'E',   2, A_SentinelBob,		&States[S_PROG_MELEE+1]),
	S_NORMAL (PRGR, 'F',   3, A_SentinelBob,		&States[S_PROG_MELEE+2]),
	S_NORMAL (PRGR, 'E',   3, A_FaceTarget,			&States[S_PROG_MELEE+4]),
	S_NORMAL (PRGR, 'F',   4, A_ProgrammerMelee,	&States[S_PROG_CHASE+4]),

#define S_PROG_MISSILE (S_PROG_MELEE+4)
	S_NORMAL (PRGR, 'G',   5, A_FaceTarget,			&States[S_PROG_MISSILE+1]),
	S_NORMAL (PRGR, 'H',   5, A_SentinelBob,		&States[S_PROG_MISSILE+2]),
	S_BRIGHT (PRGR, 'I',   5, A_FaceTarget,			&States[S_PROG_MISSILE+3]),
	S_BRIGHT (PRGR, 'J',   5, A_SpotLightning,		&States[S_PROG_CHASE+4]),

#define S_PROG_PAIN (S_PROG_MISSILE+4)
	S_NORMAL (PRGR, 'K',   5, A_Pain,				&States[S_PROG_PAIN+1]),
	S_NORMAL (PRGR, 'L',   5, A_SentinelBob,		&States[S_PROG_CHASE+4]),

#define S_PROG_DIE (S_PROG_PAIN+2)
	S_BRIGHT (PRGR, 'L',   7, A_TossGib,			&States[S_PROG_DIE+1]),
	S_BRIGHT (PRGR, 'M',   7, A_Scream,				&States[S_PROG_DIE+2]),
	S_BRIGHT (PRGR, 'N',   7, A_TossGib,			&States[S_PROG_DIE+3]),
	S_BRIGHT (PRGR, 'O',   7, A_NoBlocking,			&States[S_PROG_DIE+4]),
	S_BRIGHT (PRGR, 'P',   7, A_TossGib,			&States[S_PROG_DIE+5]),
	S_BRIGHT (PRGR, 'Q',   7, A_SpawnProgrammerBase,&States[S_PROG_DIE+6]),
	S_BRIGHT (PRGR, 'R',   7, NULL,					&States[S_PROG_DIE+7]),
	S_BRIGHT (PRGR, 'S',   6, NULL,					&States[S_PROG_DIE+8]),
	S_BRIGHT (PRGR, 'T',   5, NULL,					&States[S_PROG_DIE+9]),
	S_BRIGHT (PRGR, 'U',   5, NULL,					&States[S_PROG_DIE+10]),
	S_BRIGHT (PRGR, 'V',   5, NULL,					&States[S_PROG_DIE+11]),
	S_BRIGHT (PRGR, 'W',   5, NULL,					&States[S_PROG_DIE+12]),
	S_BRIGHT (PRGR, 'X',  32, NULL,					&States[S_PROG_DIE+13]),
	S_BRIGHT (PRGR, 'X',  -1, A_ProgrammerDeath,	NULL)
};

IMPLEMENT_ACTOR (AProgrammer, Strife, 71, 0)
	PROP_StrifeType (95)
	PROP_SpawnHealth (1100)
	PROP_SpawnState (S_PROG_STND)
	PROP_SeeState (S_PROG_CHASE)
	PROP_PainState (S_PROG_PAIN)
	PROP_PainChance (50)
	PROP_MeleeState (S_PROG_MELEE)
	PROP_MissileState (S_PROG_MISSILE)
	PROP_DeathState (S_PROG_DIE)
	PROP_SpeedFixed (26)
	PROP_FloatSpeed (5)
	PROP_RadiusFixed (45)
	PROP_HeightFixed (60)
	PROP_Mass (800)
	PROP_Damage (4)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_NOGRAVITY|MF_FLOAT|
				MF_NOBLOOD|MF_COUNTKILL|MF_NOTDMATCH)
	PROP_Flags2 (MF2_PASSMOBJ|MF2_PUSHWALL|MF2_MCROSS)
	PROP_Flags3 (MF3_DONTMORPH|MF3_NOBLOCKMONST)
	PROP_Flags4 (MF4_LOOKALLAROUND|MF4_FIRERESIST|MF4_NOICEDEATH|MF4_NOTARGETSWITCH)
	PROP_MinMissileChance (150)
	PROP_AttackSound ("programmer/attack")
	PROP_PainSound ("programmer/pain")
	PROP_DeathSound ("programmer/death")
	PROP_ActiveSound ("programmer/active")
	PROP_Obituary ("$OB_PROGRAMMER")
END_DEFAULTS

//============================================================================
//
// AProgrammer :: NoBlockingSet
//
//============================================================================

void AProgrammer::NoBlockingSet ()
{
	P_DropItem (this, "Sigil1", -1, 256);
}

// The Programmer's base for when he dies -----------------------------------

class AProgrammerBase : public AActor
{
	DECLARE_ACTOR (AProgrammerBase, AActor)
public:
	void GetExplodeParms (int &damage, int &dist, bool &hurtSource);
};

FState AProgrammerBase::States[] =
{
	S_BRIGHT (BASE, 'A',  5, A_ExplodeAndAlert,	&States[1]),
	S_BRIGHT (BASE, 'B',  5, NULL,				&States[2]),
	S_BRIGHT (BASE, 'C',  5, NULL,				&States[3]),
	S_BRIGHT (BASE, 'D',  5, NULL,				&States[4]),
	S_NORMAL (BASE, 'E',  5, NULL,				&States[5]),
	S_NORMAL (BASE, 'F',  5, NULL,				&States[6]),
	S_NORMAL (BASE, 'G',  5, NULL,				&States[7]),
	S_NORMAL (BASE, 'H', -1, NULL,				NULL)
};

IMPLEMENT_ACTOR (AProgrammerBase, Strife, -1, 0)
	PROP_StrifeType (96)
	PROP_SpawnState (0)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOCLIP|MF_NOBLOOD)
END_DEFAULTS

//============================================================================
//
// AProgrammerBase :: GetExplodeParms
//
//============================================================================

void AProgrammerBase::GetExplodeParms (int &damage, int &dist, bool &hurtSource)
{
	damage = dist = 32;
}

// The Programmer level ending thing ----------------------------------------
// [RH] I took some liberties to make this "cooler" than it was in Strife.

class AProgLevelEnder : public AInventory
{
	DECLARE_STATELESS_ACTOR (AProgLevelEnder, AInventory)
public:
	void Tick ();
	PalEntry GetBlend ();
};

IMPLEMENT_STATELESS_ACTOR (AProgLevelEnder, Strife, -1, 0)
	PROP_Inventory_FlagsSet (IF_UNDROPPABLE)
END_DEFAULTS

//============================================================================
//
// AProgLevelEnder :: Tick
//
// Fade to black, end the level, then unfade.
//
//============================================================================

void AProgLevelEnder::Tick ()
{
	if (special2 == 0)
	{ // fade out over .66 second
		special1 += 255 / (TICRATE*2/3);
		if (++special1 >= 255)
		{
			special1 = 255;
			special2 = 1;
			G_ExitLevel (0, false);
		}
	}
	else
	{ // fade in over two seconds
		special1 -= 255 / (TICRATE*2);
		if (special1 <= 0)
		{
			Destroy ();
		}
	}
}

//============================================================================
//
// AProgLevelEnder :: GetBlend
//
//============================================================================

PalEntry AProgLevelEnder::GetBlend ()
{
	return PalEntry ((BYTE)special1, 0, 0, 0);
}

//============================================================================
//
// A_ProgrammerMelee
//
//============================================================================

void A_ProgrammerMelee (AActor *self)
{
	int damage;

	if (self->target == NULL)
		return;

	A_FaceTarget (self);

	if (!self->CheckMeleeRange ())
		return;

	S_Sound (self, CHAN_WEAPON, "programmer/clank", 1, ATTN_NORM);

	damage = ((pr_prog() % 10) + 1) * 6;
	P_DamageMobj (self->target, self, self, damage, NAME_Melee);
	P_TraceBleed (damage, self->target, self);
}

//============================================================================
//
// A_SpotLightning
//
//============================================================================

void A_SpotLightning (AActor *self)
{
	AActor *spot;

	if (self->target == NULL)
		return;

	spot = Spawn<ASpectralLightningSpot> (self->target->x, self->target->y, ONFLOORZ, ALLOW_REPLACE);
	if (spot != NULL)
	{
		spot->threshold = 25;
		spot->target = self;
		spot->health = -2;
		spot->tracer = self->target;
	}
}

//============================================================================
//
// A_SpawnProgrammerBase
//
//============================================================================

void A_SpawnProgrammerBase (AActor *self)
{
	AActor *foo = Spawn<AProgrammerBase> (self->x, self->y, self->z + 24*FRACUNIT, ALLOW_REPLACE);
	if (foo != NULL)
	{
		foo->angle = self->angle + ANGLE_180 + (pr_prog.Random2() << 22);
		foo->momx = FixedMul (foo->Speed, finecosine[foo->angle >> ANGLETOFINESHIFT]);
		foo->momy = FixedMul (foo->Speed, finesine[foo->angle >> ANGLETOFINESHIFT]);
		foo->momz = pr_prog() << 9;
	}
}

//============================================================================
//
// A_ProgrammerDeath
//
//============================================================================

void A_ProgrammerDeath (AActor *self)
{
	if (!CheckBossDeath (self))
		return;

	for (int i = 0; i < MAXPLAYERS; ++i)
	{
		if (playeringame[i] && players[i].health > 0)
		{
			players[i].mo->GiveInventoryType (RUNTIME_CLASS(AProgLevelEnder));
			break;
		}
	}
	// the sky change scripts are now done as special actions in MAPINFO
	A_BossDeath(self);
	//P_StartScript (self, NULL, 250, NULL, 0, 0, 0, 0, false, false);
}
