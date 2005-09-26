#include "actor.h"
#include "info.h"
#include "m_random.h"
#include "p_local.h"
#include "p_enemy.h"
#include "gstrings.h"
#include "a_action.h"

static FRandom pr_sargattack ("SargAttack");

void A_SargAttack (AActor *);

class ADemon : public AActor
{
	DECLARE_ACTOR (ADemon, AActor)
public:
	const char *GetHitObituary () { return GStrings("OB_DEMONHIT"); }
};

FState ADemon::States[] =
{
#define S_SARG_STND 0
	S_NORMAL (SARG, 'A',   10, A_Look						, &States[S_SARG_STND+1]),
	S_NORMAL (SARG, 'B',   10, A_Look						, &States[S_SARG_STND]),

#define S_SARG_RUN (S_SARG_STND+2)
	S_NORMAL (SARG, 'A',	2, A_Chase						, &States[S_SARG_RUN+1]),
	S_NORMAL (SARG, 'A',	2, A_Chase						, &States[S_SARG_RUN+2]),
	S_NORMAL (SARG, 'B',	2, A_Chase						, &States[S_SARG_RUN+3]),
	S_NORMAL (SARG, 'B',	2, A_Chase						, &States[S_SARG_RUN+4]),
	S_NORMAL (SARG, 'C',	2, A_Chase						, &States[S_SARG_RUN+5]),
	S_NORMAL (SARG, 'C',	2, A_Chase						, &States[S_SARG_RUN+6]),
	S_NORMAL (SARG, 'D',	2, A_Chase						, &States[S_SARG_RUN+7]),
	S_NORMAL (SARG, 'D',	2, A_Chase						, &States[S_SARG_RUN+0]),

#define S_SARG_ATK (S_SARG_RUN+8)
	S_NORMAL (SARG, 'E',	8, A_FaceTarget 				, &States[S_SARG_ATK+1]),
	S_NORMAL (SARG, 'F',	8, A_FaceTarget 				, &States[S_SARG_ATK+2]),
	S_NORMAL (SARG, 'G',	8, A_SargAttack 				, &States[S_SARG_RUN+0]),

#define S_SARG_PAIN (S_SARG_ATK+3)
	S_NORMAL (SARG, 'H',	2, NULL 						, &States[S_SARG_PAIN+1]),
	S_NORMAL (SARG, 'H',	2, A_Pain						, &States[S_SARG_RUN+0]),

#define S_SARG_DIE (S_SARG_PAIN+2)
	S_NORMAL (SARG, 'I',	8, NULL 						, &States[S_SARG_DIE+1]),
	S_NORMAL (SARG, 'J',	8, A_Scream 					, &States[S_SARG_DIE+2]),
	S_NORMAL (SARG, 'K',	4, NULL 						, &States[S_SARG_DIE+3]),
	S_NORMAL (SARG, 'L',	4, A_NoBlocking					, &States[S_SARG_DIE+4]),
	S_NORMAL (SARG, 'M',	4, NULL 						, &States[S_SARG_DIE+5]),
	S_NORMAL (SARG, 'N',   -1, NULL 						, NULL),

#define S_SARG_RAISE (S_SARG_DIE+6)
	S_NORMAL (SARG, 'N',	5, NULL 						, &States[S_SARG_RAISE+1]),
	S_NORMAL (SARG, 'M',	5, NULL 						, &States[S_SARG_RAISE+2]),
	S_NORMAL (SARG, 'L',	5, NULL 						, &States[S_SARG_RAISE+3]),
	S_NORMAL (SARG, 'K',	5, NULL 						, &States[S_SARG_RAISE+4]),
	S_NORMAL (SARG, 'J',	5, NULL 						, &States[S_SARG_RAISE+5]),
	S_NORMAL (SARG, 'I',	5, NULL 						, &States[S_SARG_RUN+0])
};

IMPLEMENT_ACTOR (ADemon, Doom, 3002, 8)
	PROP_SpawnHealth (150)
	PROP_PainChance (180)
	PROP_SpeedFixed (10)
	PROP_RadiusFixed (30)
	PROP_HeightFixed (56)
	PROP_Mass (400)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_COUNTKILL)
	PROP_Flags2 (MF2_MCROSS|MF2_PASSMOBJ|MF2_PUSHWALL|MF2_FLOORCLIP)

	PROP_SpawnState (S_SARG_STND)
	PROP_SeeState (S_SARG_RUN)
	PROP_PainState (S_SARG_PAIN)
	PROP_MeleeState (S_SARG_ATK)
	PROP_DeathState (S_SARG_DIE)
	PROP_RaiseState (S_SARG_RAISE)

	PROP_SeeSound ("demon/sight")
	PROP_AttackSound ("demon/melee")
	PROP_PainSound ("demon/pain")
	PROP_DeathSound ("demon/death")
	PROP_ActiveSound ("demon/active")
END_DEFAULTS

static void SetTics (FState *state, int count)
{
	state->Tics = (count+1) & 255;
	state->Misc1 = state->GetMisc1() | ((count+1)>>8);
	state->Frame = (state->Frame & ~SF_BIGTIC) | (count > 254 ? SF_BIGTIC : 0);
}

AT_SPEED_SET (Demon, speednow)
{
	static bool isFast = false;
	int i;

	if (speednow == SPEED_Fast && !isFast)
	{
		isFast = true;
		for (i = S_SARG_RUN; i < S_SARG_PAIN; i++)
			SetTics (&ADemon::States[i], ADemon::States[i].GetTics() >> 1);
	}
	else if (speednow == SPEED_Normal && isFast)
	{
		isFast = false;
		for (i = S_SARG_RUN; i < S_SARG_PAIN; i++)
			SetTics (&ADemon::States[i], ADemon::States[i].GetTics() << 1);
	}
}

class AStealthDemon : public ADemon
{
	DECLARE_STATELESS_ACTOR (AStealthDemon, ADemon)
public:
	const char *GetObituary () { return GStrings("OB_STEALTHDEMON"); }
	const char *GetHitObituary () { return GStrings("OB_STEALTHDEMON"); }
};

IMPLEMENT_STATELESS_ACTOR (AStealthDemon, Doom, 9055, 121)
	PROP_FlagsSet (MF_STEALTH)
	PROP_Alpha (0)
	PROP_RenderStyle (STYLE_Translucent)
END_DEFAULTS

class ASpectre : public ADemon
{
	DECLARE_STATELESS_ACTOR (ASpectre, ADemon)
public:
	const char *GetHitObituary () { return GStrings("OB_SPECTREHIT"); }
};

IMPLEMENT_STATELESS_ACTOR (ASpectre, Doom, 58, 9)
	PROP_FlagsSet (MF_SHADOW)
	PROP_RenderStyle (STYLE_OptFuzzy)
	PROP_Alpha (FRACUNIT/5)

	PROP_SeeSound ("spectre/sight")
	PROP_AttackSound ("spectre/melee")
	PROP_PainSound ("spectre/pain")
	PROP_DeathSound ("spectre/death")
	PROP_ActiveSound ("spectre/active")
END_DEFAULTS

void A_SargAttack (AActor *self)
{
	if (!self->target)
		return;
				
	A_FaceTarget (self);
	if (self->CheckMeleeRange ())
	{
		int damage = ((pr_sargattack()%10)+1)*4;
		P_DamageMobj (self->target, self, self, damage, MOD_HIT);
		P_TraceBleed (damage, self->target, self);
	}
}

// Dead demon --------------------------------------------------------------

class ADeadDemon : public ADemon
{
	DECLARE_STATELESS_ACTOR (ADeadDemon, ADemon)
};

IMPLEMENT_STATELESS_ACTOR (ADeadDemon, Doom, 21, 0)
	PROP_SpawnState (S_SARG_DIE+5)

	// Undo all the changes to default Actor properties that ADemon made
	PROP_SpawnHealth (1000)
	PROP_RadiusFixed (20)
	PROP_HeightFixed (16)
	PROP_Mass (100)
	PROP_SpeedFixed (0)
	PROP_PainChance (0)
	PROP_Flags (0)
	PROP_Flags2 (0)
	PROP_SeeState (255)
	PROP_PainState (255)
	PROP_MissileState (255)
	PROP_DeathState (255)
	PROP_RaiseState (255)
	PROP_SeeSound ("")
	PROP_PainSound ("")
	PROP_DeathSound ("")
	PROP_ActiveSound ("")
	PROP_AttackSound ("")
END_DEFAULTS
