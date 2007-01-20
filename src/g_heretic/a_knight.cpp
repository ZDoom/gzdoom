#include "actor.h"
#include "info.h"
#include "m_random.h"
#include "s_sound.h"
#include "p_local.h"
#include "p_enemy.h"
#include "a_action.h"
#include "a_sharedglobal.h"
#include "gstrings.h"

static FRandom pr_dripblood ("DripBlood");
static FRandom pr_knightatk ("KnightAttack");

void A_KnightAttack (AActor *);
void A_DripBlood (AActor *);
void A_AxeSound (AActor *);

// Knight -------------------------------------------------------------------

class AKnight : public AActor
{
	DECLARE_ACTOR (AKnight, AActor)
public:
	void NoBlockingSet ();
};

FState AKnight::States[] =
{
#define S_KNIGHT_STND 0
	S_NORMAL (KNIG, 'A',   10, A_Look					, &States[S_KNIGHT_STND+1]),
	S_NORMAL (KNIG, 'B',   10, A_Look					, &States[S_KNIGHT_STND+0]),

#define S_KNIGHT_WALK (S_KNIGHT_STND+2)
	S_NORMAL (KNIG, 'A',	4, A_Chase					, &States[S_KNIGHT_WALK+1]),
	S_NORMAL (KNIG, 'B',	4, A_Chase					, &States[S_KNIGHT_WALK+2]),
	S_NORMAL (KNIG, 'C',	4, A_Chase					, &States[S_KNIGHT_WALK+3]),
	S_NORMAL (KNIG, 'D',	4, A_Chase					, &States[S_KNIGHT_WALK+0]),

#define S_KNIGHT_ATK (S_KNIGHT_WALK+4)
	S_NORMAL (KNIG, 'E',   10, A_FaceTarget 			, &States[S_KNIGHT_ATK+1]),
	S_NORMAL (KNIG, 'F',	8, A_FaceTarget 			, &States[S_KNIGHT_ATK+2]),
	S_NORMAL (KNIG, 'G',	8, A_KnightAttack			, &States[S_KNIGHT_ATK+3]),
	S_NORMAL (KNIG, 'E',   10, A_FaceTarget 			, &States[S_KNIGHT_ATK+4]),
	S_NORMAL (KNIG, 'F',	8, A_FaceTarget 			, &States[S_KNIGHT_ATK+5]),
	S_NORMAL (KNIG, 'G',	8, A_KnightAttack			, &States[S_KNIGHT_WALK+0]),

#define S_KNIGHT_PAIN (S_KNIGHT_ATK+6)
	S_NORMAL (KNIG, 'H',	3, NULL 					, &States[S_KNIGHT_PAIN+1]),
	S_NORMAL (KNIG, 'H',	3, A_Pain					, &States[S_KNIGHT_WALK+0]),

#define S_KNIGHT_DIE (S_KNIGHT_PAIN+2)
	S_NORMAL (KNIG, 'I',	6, NULL 					, &States[S_KNIGHT_DIE+1]),
	S_NORMAL (KNIG, 'J',	6, A_Scream 				, &States[S_KNIGHT_DIE+2]),
	S_NORMAL (KNIG, 'K',	6, NULL 					, &States[S_KNIGHT_DIE+3]),
	S_NORMAL (KNIG, 'L',	6, A_NoBlocking 			, &States[S_KNIGHT_DIE+4]),
	S_NORMAL (KNIG, 'M',	6, NULL 					, &States[S_KNIGHT_DIE+5]),
	S_NORMAL (KNIG, 'N',	6, NULL 					, &States[S_KNIGHT_DIE+6]),
	S_NORMAL (KNIG, 'O',   -1, NULL 					, NULL)
};

IMPLEMENT_ACTOR (AKnight, Heretic, 64, 6)
	PROP_SpawnHealth (200)
	PROP_RadiusFixed (24)
	PROP_HeightFixed (78)
	PROP_Mass (150)
	PROP_SpeedFixed (12)
	PROP_PainChance (100)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_COUNTKILL)
	PROP_Flags2 (MF2_MCROSS|MF2_FLOORCLIP|MF2_PASSMOBJ|MF2_PUSHWALL)

	PROP_SpawnState (S_KNIGHT_STND)
	PROP_SeeState (S_KNIGHT_WALK)
	PROP_PainState (S_KNIGHT_PAIN)
	PROP_MeleeState (S_KNIGHT_ATK)
	PROP_MissileState (S_KNIGHT_ATK)
	PROP_DeathState (S_KNIGHT_DIE)

	PROP_SeeSound ("hknight/sight")
	PROP_AttackSound ("hknight/attack")
	PROP_PainSound ("hknight/pain")
	PROP_DeathSound ("hknight/death")
	PROP_ActiveSound ("hknight/active")
	PROP_Obituary("$OB_BONEKNIGHT")
	PROP_HitObituary("$OB_BONEKNIGHTHIT")
END_DEFAULTS

void AKnight::NoBlockingSet ()
{
	P_DropItem (this, "CrossbowAmmo", 5, 84);
}

// Knight ghost -------------------------------------------------------------

class AKnightGhost : public AKnight
{
	DECLARE_STATELESS_ACTOR (AKnightGhost, AKnight)
};

IMPLEMENT_STATELESS_ACTOR (AKnightGhost, Heretic, 65, 129)
	PROP_FlagsSet (MF_SHADOW)
	PROP_Flags3 (MF3_GHOST)
	PROP_RenderStyle (STYLE_Translucent)
	PROP_Alpha (HR_SHADOW)
END_DEFAULTS

// Knight axe ---------------------------------------------------------------

class AKnightAxe : public AActor
{
	DECLARE_ACTOR (AKnightAxe, AActor)
};

FState AKnightAxe::States[] =
{
#define S_SPINAXE 0
	S_BRIGHT (SPAX, 'A',	3, A_AxeSound				, &States[S_SPINAXE+1]),
	S_BRIGHT (SPAX, 'B',	3, NULL 					, &States[S_SPINAXE+2]),
	S_BRIGHT (SPAX, 'C',	3, NULL 					, &States[S_SPINAXE+0]),

#define S_SPINAXEX (S_SPINAXE+3)
	S_BRIGHT (SPAX, 'D',	6, NULL 					, &States[S_SPINAXEX+1]),
	S_BRIGHT (SPAX, 'E',	6, NULL 					, &States[S_SPINAXEX+2]),
	S_BRIGHT (SPAX, 'F',	6, NULL 					, NULL)
};

IMPLEMENT_ACTOR (AKnightAxe, Heretic, -1, 127)
	PROP_RadiusFixed (10)
	PROP_HeightFixed (8)
	PROP_SpeedFixed (9)
	PROP_Damage (2)
	PROP_Flags (MF_MISSILE|MF_DROPOFF|MF_NOGRAVITY)
	PROP_Flags2 (MF2_WINDTHRUST|MF2_NOTELEPORT|MF2_THRUGHOST)

	PROP_SpawnState (S_SPINAXE)
	PROP_DeathState (S_SPINAXEX)

	PROP_DeathSound ("hknight/hit")
END_DEFAULTS

AT_SPEED_SET (KnightAxe, speed)
{
	SimpleSpeedSetter (AKnightAxe, 9*FRACUNIT, 18*FRACUNIT, speed);
}

// Red axe ------------------------------------------------------------------

class ARedAxe : public AKnightAxe
{
	DECLARE_ACTOR (ARedAxe, AKnightAxe)
};

FState ARedAxe::States[] =
{
#define S_REDAXE 0
	S_BRIGHT (RAXE, 'A',	5, A_DripBlood				, &States[S_REDAXE+1]),
	S_BRIGHT (RAXE, 'B',	5, A_DripBlood				, &States[S_REDAXE+0]),

#define S_REDAXEX (S_REDAXE+2)
	S_BRIGHT (RAXE, 'C',	6, NULL 					, &States[S_REDAXEX+1]),
	S_BRIGHT (RAXE, 'D',	6, NULL 					, &States[S_REDAXEX+2]),
	S_BRIGHT (RAXE, 'E',	6, NULL 					, NULL)
};

IMPLEMENT_ACTOR (ARedAxe, Heretic, -1, 128)
	PROP_Damage (7)
	PROP_FlagsSet (MF_NOBLOCKMAP)
	PROP_Flags2Clear (MF2_WINDTHRUST)

	PROP_SpawnState (S_REDAXE)
	PROP_DeathState (S_REDAXEX)
END_DEFAULTS

AT_SPEED_SET (RedAxe, speed)
{
	SimpleSpeedSetter (ARedAxe, 9*FRACUNIT, 18*FRACUNIT, speed);
}

//----------------------------------------------------------------------------
//
// PROC A_DripBlood
//
//----------------------------------------------------------------------------

void A_DripBlood (AActor *actor)
{
	AActor *mo;
	fixed_t x, y;

	x = actor->x + (pr_dripblood.Random2 () << 11);
	y = actor->y + (pr_dripblood.Random2 () << 11);
	mo = Spawn ("Blood", x, y, actor->z, ALLOW_REPLACE);
	mo->momx = pr_dripblood.Random2 () << 10;
	mo->momy = pr_dripblood.Random2 () << 10;
	mo->gravity = FRACUNIT/8;
}

//----------------------------------------------------------------------------
//
// PROC A_KnightAttack
//
//----------------------------------------------------------------------------

void A_KnightAttack (AActor *actor)
{
	if (!actor->target)
	{
		return;
	}
	if (actor->CheckMeleeRange ())
	{
		int damage = pr_knightatk.HitDice (3);
		P_DamageMobj (actor->target, actor, actor, damage, NAME_Melee);
		P_TraceBleed (damage, actor->target, actor);
		S_Sound (actor, CHAN_BODY, "hknight/melee", 1, ATTN_NORM);
		return;
	}
	// Throw axe
	S_SoundID (actor, CHAN_BODY, actor->AttackSound, 1, ATTN_NORM);
	if (actor->flags & MF_SHADOW || pr_knightatk () < 40)
	{ // Red axe
		P_SpawnMissileZ (actor, actor->z + 36*FRACUNIT, actor->target, RUNTIME_CLASS(ARedAxe));
		return;
	}
	// Green axe
	P_SpawnMissileZ (actor, actor->z + 36*FRACUNIT, actor->target, RUNTIME_CLASS(AKnightAxe));
}

//---------------------------------------------------------------------------
//
// PROC A_AxeSound
//
//---------------------------------------------------------------------------

void A_AxeSound (AActor *actor)
{
	S_Sound (actor, CHAN_BODY, "hknight/axewhoosh", 1, ATTN_NORM);
}
