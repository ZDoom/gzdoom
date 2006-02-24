#include "actor.h"
#include "info.h"
#include "m_random.h"
#include "s_sound.h"
#include "p_local.h"
#include "p_enemy.h"
#include "a_action.h"
#include "gstrings.h"

static FRandom pr_clinkattack ("ClinkAttack");

void A_ClinkAttack (AActor *);

// Clink --------------------------------------------------------------------

class AClink : public AActor
{
	DECLARE_ACTOR (AClink, AActor)
public:
	void NoBlockingSet ();
	const char *GetObituary ();
};

FState AClink::States[] =
{
#define S_CLINK_LOOK 0
	S_NORMAL (CLNK, 'A',   10, A_Look					, &States[S_CLINK_LOOK+1]),
	S_NORMAL (CLNK, 'B',   10, A_Look					, &States[S_CLINK_LOOK+0]),

#define S_CLINK_WALK (S_CLINK_LOOK+2)
	S_NORMAL (CLNK, 'A',	3, A_Chase					, &States[S_CLINK_WALK+1]),
	S_NORMAL (CLNK, 'B',	3, A_Chase					, &States[S_CLINK_WALK+2]),
	S_NORMAL (CLNK, 'C',	3, A_Chase					, &States[S_CLINK_WALK+3]),
	S_NORMAL (CLNK, 'D',	3, A_Chase					, &States[S_CLINK_WALK+0]),

#define S_CLINK_ATK (S_CLINK_WALK+4)
	S_NORMAL (CLNK, 'E',	5, A_FaceTarget 			, &States[S_CLINK_ATK+1]),
	S_NORMAL (CLNK, 'F',	4, A_FaceTarget 			, &States[S_CLINK_ATK+2]),
	S_NORMAL (CLNK, 'G',	7, A_ClinkAttack			, &States[S_CLINK_WALK+0]),

#define S_CLINK_PAIN (S_CLINK_ATK+3)
	S_NORMAL (CLNK, 'H',	3, NULL 					, &States[S_CLINK_PAIN+1]),
	S_NORMAL (CLNK, 'H',	3, A_Pain					, &States[S_CLINK_WALK+0]),

#define S_CLINK_DIE (S_CLINK_PAIN+2)
	S_NORMAL (CLNK, 'I',	6, NULL 					, &States[S_CLINK_DIE+1]),
	S_NORMAL (CLNK, 'J',	6, NULL 					, &States[S_CLINK_DIE+2]),
	S_NORMAL (CLNK, 'K',	5, A_Scream 				, &States[S_CLINK_DIE+3]),
	S_NORMAL (CLNK, 'L',	5, A_NoBlocking 			, &States[S_CLINK_DIE+4]),
	S_NORMAL (CLNK, 'M',	5, NULL 					, &States[S_CLINK_DIE+5]),
	S_NORMAL (CLNK, 'N',	5, NULL 					, &States[S_CLINK_DIE+6]),
	S_NORMAL (CLNK, 'O',   -1, NULL 					, NULL)
};

IMPLEMENT_ACTOR (AClink, Heretic, 90, 1)
	PROP_SpawnHealth (150)
	PROP_RadiusFixed (20)
	PROP_HeightFixed (64)
	PROP_Mass (75)
	PROP_SpeedFixed (14)
	PROP_PainChance (32)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_COUNTKILL|MF_NOBLOOD)
	PROP_Flags2 (MF2_FLOORCLIP|MF2_PASSMOBJ|MF2_PUSHWALL)

	PROP_SpawnState (S_CLINK_LOOK)
	PROP_SeeState (S_CLINK_WALK)
	PROP_PainState (S_CLINK_PAIN)
	PROP_MeleeState (S_CLINK_ATK)
	PROP_DeathState (S_CLINK_DIE)

	PROP_SeeSound ("clink/sight")
	PROP_AttackSound ("clink/attack")
	PROP_PainSound ("clink/pain")
	PROP_DeathSound ("clink/death")
	PROP_ActiveSound ("clink/active")
END_DEFAULTS

void AClink::NoBlockingSet ()
{
	P_DropItem (this, "SkullRodAmmo", 20, 84);
}

const char *AClink::GetObituary ()
{
	return GStrings("OB_CLINK");
}

//----------------------------------------------------------------------------
//
// PROC A_ClinkAttack
//
//----------------------------------------------------------------------------

void A_ClinkAttack (AActor *actor)
{
	int damage;

	if (!actor->target)
	{
		return;
	}
	S_SoundID (actor, CHAN_BODY, actor->AttackSound, 1, ATTN_NORM);
	if (actor->CheckMeleeRange ())
	{
		damage = ((pr_clinkattack()%7)+3);
		P_DamageMobj (actor->target, actor, actor, damage, MOD_HIT);
		P_TraceBleed (damage, actor->target, actor);
	}
}
