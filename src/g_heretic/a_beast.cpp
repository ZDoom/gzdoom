#include "actor.h"
#include "info.h"
#include "m_random.h"
#include "s_sound.h"
#include "p_local.h"
#include "p_enemy.h"
#include "a_action.h"
#include "gstrings.h"

static FRandom pr_beastatk ("BeastAttack");
static FRandom pr_beastpuff ("BeastPuff");

void A_BeastAttack (AActor *);
void A_BeastPuff (AActor *);

// Beast --------------------------------------------------------------------

class ABeast : public AActor
{
	DECLARE_ACTOR (ABeast, AActor)
public:
	void NoBlockingSet ();
	const char *GetObituary ();
};

FState ABeast::States[] =
{
#define S_BEAST_LOOK 0
	S_NORMAL (BEAS, 'A',   10, A_Look					, &States[S_BEAST_LOOK+1]),
	S_NORMAL (BEAS, 'B',   10, A_Look					, &States[S_BEAST_LOOK+0]),

#define S_BEAST_WALK (S_BEAST_LOOK+2)
	S_NORMAL (BEAS, 'A',	3, A_Chase					, &States[S_BEAST_WALK+1]),
	S_NORMAL (BEAS, 'B',	3, A_Chase					, &States[S_BEAST_WALK+2]),
	S_NORMAL (BEAS, 'C',	3, A_Chase					, &States[S_BEAST_WALK+3]),
	S_NORMAL (BEAS, 'D',	3, A_Chase					, &States[S_BEAST_WALK+4]),
	S_NORMAL (BEAS, 'E',	3, A_Chase					, &States[S_BEAST_WALK+5]),
	S_NORMAL (BEAS, 'F',	3, A_Chase					, &States[S_BEAST_WALK+0]),

#define S_BEAST_ATK (S_BEAST_WALK+6)
	S_NORMAL (BEAS, 'H',   10, A_FaceTarget 			, &States[S_BEAST_ATK+1]),
	S_NORMAL (BEAS, 'I',   10, A_BeastAttack			, &States[S_BEAST_WALK+0]),

#define S_BEAST_PAIN (S_BEAST_ATK+2)
	S_NORMAL (BEAS, 'G',	3, NULL 					, &States[S_BEAST_PAIN+1]),
	S_NORMAL (BEAS, 'G',	3, A_Pain					, &States[S_BEAST_WALK+0]),

#define S_BEAST_DIE (S_BEAST_PAIN+2)
	S_NORMAL (BEAS, 'R',	6, NULL 					, &States[S_BEAST_DIE+1]),
	S_NORMAL (BEAS, 'S',	6, A_Scream 				, &States[S_BEAST_DIE+2]),
	S_NORMAL (BEAS, 'T',	6, NULL 					, &States[S_BEAST_DIE+3]),
	S_NORMAL (BEAS, 'U',	6, NULL 					, &States[S_BEAST_DIE+4]),
	S_NORMAL (BEAS, 'V',	6, NULL 					, &States[S_BEAST_DIE+5]),
	S_NORMAL (BEAS, 'W',	6, A_NoBlocking 			, &States[S_BEAST_DIE+6]),
	S_NORMAL (BEAS, 'X',	6, NULL 					, &States[S_BEAST_DIE+7]),
	S_NORMAL (BEAS, 'Y',	6, NULL 					, &States[S_BEAST_DIE+8]),
	S_NORMAL (BEAS, 'Z',   -1, NULL 					, NULL),

#define S_BEAST_XDIE (S_BEAST_DIE+9)
	S_NORMAL (BEAS, 'J',	5, NULL 					, &States[S_BEAST_XDIE+1]),
	S_NORMAL (BEAS, 'K',	6, A_Scream 				, &States[S_BEAST_XDIE+2]),
	S_NORMAL (BEAS, 'L',	5, NULL 					, &States[S_BEAST_XDIE+3]),
	S_NORMAL (BEAS, 'M',	6, NULL 					, &States[S_BEAST_XDIE+4]),
	S_NORMAL (BEAS, 'N',	5, NULL 					, &States[S_BEAST_XDIE+5]),
	S_NORMAL (BEAS, 'O',	6, A_NoBlocking 			, &States[S_BEAST_XDIE+6]),
	S_NORMAL (BEAS, 'P',	5, NULL 					, &States[S_BEAST_XDIE+7]),
	S_NORMAL (BEAS, 'Q',   -1, NULL 					, NULL)
};

IMPLEMENT_ACTOR (ABeast, Heretic, 70, 3)
	PROP_SpawnHealth (220)
	PROP_RadiusFixed (32)
	PROP_HeightFixed (74)
	PROP_Mass (200)
	PROP_SpeedFixed (14)
	PROP_PainChance (100)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_COUNTKILL)
	PROP_Flags2 (MF2_FLOORCLIP|MF2_PASSMOBJ|MF2_PUSHWALL)

	PROP_SpawnState (S_BEAST_LOOK)
	PROP_SeeState (S_BEAST_WALK)
	PROP_PainState (S_BEAST_PAIN)
	PROP_MissileState (S_BEAST_ATK)
	PROP_DeathState (S_BEAST_DIE)
	PROP_XDeathState (S_BEAST_XDIE)

	PROP_SeeSound ("beast/sight")
	PROP_AttackSound ("beast/attack")
	PROP_PainSound ("beast/pain")
	PROP_DeathSound ("beast/death")
	PROP_ActiveSound ("beast/active")
END_DEFAULTS

void ABeast::NoBlockingSet ()
{
	P_DropItem (this, "CrossbowAmmo", 10, 84);
}

const char *ABeast::GetObituary ()
{
	return GStrings(OB_BEAST);
}

// Beast ball ---------------------------------------------------------------

// Heretic also had a MT_BURNBALL and MT_BURNBALLFB based on the beast ball,
// but it didn't use them anywhere.

class ABeastBall : public AActor
{
	DECLARE_ACTOR (ABeastBall, AActor)
};

FState ABeastBall::States[] =
{
#define S_BEASTBALL 0
	S_NORMAL (FRB1, 'A',	2, A_BeastPuff				, &States[S_BEASTBALL+1]),
	S_NORMAL (FRB1, 'A',	2, A_BeastPuff				, &States[S_BEASTBALL+2]),
	S_NORMAL (FRB1, 'B',	2, A_BeastPuff				, &States[S_BEASTBALL+3]),
	S_NORMAL (FRB1, 'B',	2, A_BeastPuff				, &States[S_BEASTBALL+4]),
	S_NORMAL (FRB1, 'C',	2, A_BeastPuff				, &States[S_BEASTBALL+5]),
	S_NORMAL (FRB1, 'C',	2, A_BeastPuff				, &States[S_BEASTBALL+0]),

#define S_BEASTBALLX (S_BEASTBALL+6)
	S_NORMAL (FRB1, 'D',	4, NULL 					, &States[S_BEASTBALLX+1]),
	S_NORMAL (FRB1, 'E',	4, NULL 					, &States[S_BEASTBALLX+2]),
	S_NORMAL (FRB1, 'F',	4, NULL 					, &States[S_BEASTBALLX+3]),
	S_NORMAL (FRB1, 'G',	4, NULL 					, &States[S_BEASTBALLX+4]),
	S_NORMAL (FRB1, 'H',	4, NULL 					, NULL)
};

IMPLEMENT_ACTOR (ABeastBall, Heretic, -1, 120)
	PROP_RadiusFixed (9)
	PROP_HeightFixed (8)
	PROP_SpeedFixed (12)
	PROP_Damage (4)
	PROP_Flags (MF_NOBLOCKMAP|MF_MISSILE|MF_DROPOFF|MF_NOGRAVITY)
	PROP_Flags2 (MF2_WINDTHRUST|MF2_NOTELEPORT)
	PROP_RenderStyle (STYLE_Add)

	PROP_SpawnState (S_BEASTBALL)
	PROP_DeathState (S_BEASTBALLX)
END_DEFAULTS

AT_SPEED_SET (BeastBall, speed)
{
	SimpleSpeedSetter (ABeastBall, 12*FRACUNIT, 20*FRACUNIT, speed);
}

// Puffy --------------------------------------------------------------------

class APuffy : public AActor
{
	DECLARE_ACTOR (APuffy, AActor)
};

FState APuffy::States[] =
{
	S_NORMAL (FRB1, 'D',	4, NULL 					, &States[1]),
	S_NORMAL (FRB1, 'E',	4, NULL 					, &States[2]),
	S_NORMAL (FRB1, 'F',	4, NULL 					, &States[3]),
	S_NORMAL (FRB1, 'G',	4, NULL 					, &States[4]),
	S_NORMAL (FRB1, 'H',	4, NULL 					, NULL)
};

IMPLEMENT_ACTOR (APuffy, Heretic, -1, 0)
	PROP_RadiusFixed (6)
	PROP_HeightFixed (8)
	PROP_SpeedFixed (10)
	PROP_Damage (2)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY|MF_MISSILE)
	PROP_Flags2 (MF2_NOTELEPORT)
	PROP_Flags3 (MF3_DONTSPLASH)

	// It's a tough call between Add and Translucent.
	PROP_RenderStyle (STYLE_Add)

	PROP_SpawnState (0)
END_DEFAULTS

//----------------------------------------------------------------------------
//
// PROC A_BeastAttack
//
//----------------------------------------------------------------------------

void A_BeastAttack (AActor *actor)
{
	if (!actor->target)
	{
		return;
	}
	S_SoundID (actor, CHAN_BODY, actor->AttackSound, 1, ATTN_NORM);
	if (P_CheckMeleeRange(actor))
	{
		int damage = pr_beastatk.HitDice (3);
		P_DamageMobj (actor->target, actor, actor, damage, MOD_HIT);
		P_TraceBleed (damage, actor->target, actor);
		return;
	}
	P_SpawnMissile (actor, actor->target, RUNTIME_CLASS(ABeastBall));
}

//----------------------------------------------------------------------------
//
// PROC A_BeastPuff
//
//----------------------------------------------------------------------------

void A_BeastPuff (AActor *actor)
{
	if (pr_beastpuff() > 64)
	{
		fixed_t x, y, z;

		x = actor->x + (pr_beastpuff.Random2 () << 10);
		y = actor->y + (pr_beastpuff.Random2 () << 10);
		z = actor->z + (pr_beastpuff.Random2 () << 10);
		Spawn<APuffy> (x, y, z);
	}
}
