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
	S_Sound (actor, CHAN_BODY, actor->AttackSound, 1, ATTN_NORM);
	if (actor->flags & MF_SHADOW || pr_knightatk () < 40)
	{ // Red axe
		P_SpawnMissileZ (actor, actor->z + 36*FRACUNIT, actor->target, PClass::FindClass("RedAxe"));
		return;
	}
	// Green axe
	P_SpawnMissileZ (actor, actor->z + 36*FRACUNIT, actor->target, PClass::FindClass("KnightAxe"));
}

