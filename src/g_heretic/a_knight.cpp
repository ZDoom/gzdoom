/*
#include "actor.h"
#include "info.h"
#include "m_random.h"
#include "s_sound.h"
#include "p_local.h"
#include "a_action.h"
#include "a_sharedglobal.h"
#include "gstrings.h"
#include "thingdef/thingdef.h"
*/

static FRandom pr_dripblood ("DripBlood");
static FRandom pr_knightatk ("KnightAttack");

//----------------------------------------------------------------------------
//
// PROC A_DripBlood
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_DripBlood)
{
	AActor *mo;

	fixed_t xo = (pr_dripblood.Random2() << 11);
	fixed_t yo = (pr_dripblood.Random2() << 11);
	mo = Spawn ("Blood", self->Vec3Offset(xo, yo, 0), ALLOW_REPLACE);
	mo->velx = pr_dripblood.Random2 () << 10;
	mo->vely = pr_dripblood.Random2 () << 10;
	mo->gravity = FRACUNIT/8;
}

//----------------------------------------------------------------------------
//
// PROC A_KnightAttack
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_KnightAttack)
{
	if (!self->target)
	{
		return;
	}
	if (self->CheckMeleeRange ())
	{
		int damage = pr_knightatk.HitDice (3);
		int newdam = P_DamageMobj (self->target, self, self, damage, NAME_Melee);
		P_TraceBleed (newdam > 0 ? newdam : damage, self->target, self);
		S_Sound (self, CHAN_BODY, "hknight/melee", 1, ATTN_NORM);
		return;
	}
	// Throw axe
	S_Sound (self, CHAN_BODY, self->AttackSound, 1, ATTN_NORM);
	if (self->flags & MF_SHADOW || pr_knightatk () < 40)
	{ // Red axe
		P_SpawnMissileZ (self, self->Z() + 36*FRACUNIT, self->target, PClass::FindClass("RedAxe"));
		return;
	}
	// Green axe
	P_SpawnMissileZ (self, self->Z() + 36*FRACUNIT, self->target, PClass::FindClass("KnightAxe"));
}

