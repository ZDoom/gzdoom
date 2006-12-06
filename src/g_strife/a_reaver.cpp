#include "actor.h"
#include "p_enemy.h"
#include "a_action.h"
#include "s_sound.h"
#include "m_random.h"
#include "p_local.h"
#include "a_strifeglobal.h"

void A_TossGib (AActor *);
void A_XXScream (AActor *);

static FRandom pr_reaverattack ("ReaverAttack");

// Reaver -------------------------------------------------------------------

void A_ReaverMelee (AActor *self);
void A_ReaverRanged (AActor *self);

void A_21230 (AActor *) {}

class AReaver : public AActor
{
	DECLARE_ACTOR (AReaver, AActor)
public:
	void GetExplodeParms (int &damage, int &dist, bool &hurtSource)
	{
		damage = dist = 32;
	}
};

FState AReaver::States[] =
{
#define S_REAVER_STND (0)
	S_NORMAL (ROB1, 'A',   10, A_Look,				&States[S_REAVER_STND]),
	// Needless duplication of the previous state removed

#define S_REAVER_RUN (S_REAVER_STND+1)
	S_NORMAL (ROB1, 'B',	3, A_Chase,				&States[S_REAVER_RUN+1]),
	S_NORMAL (ROB1, 'B',	3, A_Chase,				&States[S_REAVER_RUN+2]),
	S_NORMAL (ROB1, 'C',	3, A_Chase,				&States[S_REAVER_RUN+3]),
	S_NORMAL (ROB1, 'C',	3, A_Chase,				&States[S_REAVER_RUN+4]),
	S_NORMAL (ROB1, 'D',	3, A_Chase,				&States[S_REAVER_RUN+5]),
	S_NORMAL (ROB1, 'D',	3, A_Chase,				&States[S_REAVER_RUN+6]),
	S_NORMAL (ROB1, 'E',	3, A_Chase,				&States[S_REAVER_RUN+7]),
	S_NORMAL (ROB1, 'E',	3, A_Chase,				&States[S_REAVER_RUN]),

#define S_REAVER_MELEE (S_REAVER_RUN+8)
	S_NORMAL (ROB1, 'H',	6, A_FaceTarget,		&States[S_REAVER_MELEE+1]),
	S_NORMAL (ROB1, 'I',	8, A_ReaverMelee,		&States[S_REAVER_MELEE+2]),
	S_NORMAL (ROB1, 'H',	6, NULL,				&States[S_REAVER_RUN]),

#define S_REAVER_MISSILE (S_REAVER_MELEE+3)
	S_NORMAL (ROB1, 'F',	8, A_FaceTarget,		&States[S_REAVER_MISSILE+1]),
	S_BRIGHT (ROB1, 'G',   11, A_ReaverRanged,		&States[S_REAVER_RUN]),

#define S_REAVER_PAIN (S_REAVER_MISSILE+2)
	S_NORMAL (ROB1, 'A',	2, NULL,				&States[S_REAVER_PAIN+1]),
	S_NORMAL (ROB1, 'A',	2, A_Pain,				&States[S_REAVER_RUN]),

#define S_REAVER_DEATH (S_REAVER_PAIN+2)
	S_BRIGHT (ROB1, 'J',	6, NULL,				&States[S_REAVER_DEATH+1]),
	S_BRIGHT (ROB1, 'K',	6, A_Scream,			&States[S_REAVER_DEATH+2]),
	S_BRIGHT (ROB1, 'L',	5, NULL,				&States[S_REAVER_DEATH+3]),
	S_BRIGHT (ROB1, 'M',	5, A_NoBlocking,		&States[S_REAVER_DEATH+4]),
	S_BRIGHT (ROB1, 'N',	5, NULL,				&States[S_REAVER_DEATH+5]),
	S_BRIGHT (ROB1, 'O',	5, NULL,				&States[S_REAVER_DEATH+6]),
	S_BRIGHT (ROB1, 'P',	5, NULL,				&States[S_REAVER_DEATH+7]),
	S_BRIGHT (ROB1, 'Q',	6, A_ExplodeAndAlert,	&States[S_REAVER_DEATH+8]),
	S_NORMAL (ROB1, 'R',   -1, NULL,				NULL),

#define S_REAVER_XDEATH (S_REAVER_DEATH+9)
	S_BRIGHT (ROB1, 'L',	5, A_TossGib,			&States[S_REAVER_XDEATH+1]),
	S_BRIGHT (ROB1, 'M',	5, A_XXScream,			&States[S_REAVER_XDEATH+2]),
	S_BRIGHT (ROB1, 'N',	5, A_TossGib,			&States[S_REAVER_XDEATH+3]),
	S_BRIGHT (ROB1, 'O',	5, A_NoBlocking,		&States[S_REAVER_XDEATH+4]),
	S_BRIGHT (ROB1, 'P',	5, A_TossGib,			&States[S_REAVER_XDEATH+5]),
	S_BRIGHT (ROB1, 'Q',	6, A_ExplodeAndAlert,	&States[S_REAVER_XDEATH+6]),
	S_NORMAL (ROB1, 'R',   -1, NULL,				NULL),
};

IMPLEMENT_ACTOR (AReaver, Strife, 3001, 0)
	PROP_SpawnHealth (150)
	PROP_PainChance (128)
	PROP_SpeedFixed (12)
	PROP_RadiusFixed (20)
	PROP_HeightFixed (60)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_NOBLOOD|MF_COUNTKILL)
	PROP_Flags2 (MF2_MCROSS|MF2_PASSMOBJ|MF2_PUSHWALL|MF2_FLOORCLIP)
	PROP_Flags4 (MF4_INCOMBAT)
	PROP_MinMissileChance (150)
	PROP_MaxDropOffHeight (32)

	PROP_Mass (500)
	PROP_SpawnState (S_REAVER_STND)
	PROP_SeeState (S_REAVER_RUN)
	PROP_PainState (S_REAVER_PAIN)
	PROP_MeleeState (S_REAVER_MELEE)
	PROP_MissileState (S_REAVER_MISSILE)
	PROP_DeathState (S_REAVER_DEATH)
	PROP_XDeathState (S_REAVER_XDEATH)
	PROP_StrifeType (52)

	PROP_SeeSound ("reaver/sight")
	PROP_PainSound ("reaver/pain")
	PROP_DeathSound ("reaver/death")
	PROP_ActiveSound ("reaver/active")
	PROP_HitObituary ("$OB_REAVERHIT")
	PROP_Obituary ("$OB_REAVER")
END_DEFAULTS

void A_ReaverMelee (AActor *self)
{
	if (self->target != NULL)
	{
		A_FaceTarget (self);

		if (self->CheckMeleeRange ())
		{
			int damage;

			S_Sound (self, CHAN_WEAPON, "reaver/blade", 1, ATTN_NORM);
			damage = ((pr_reaverattack() & 7) + 1) * 3;
			P_DamageMobj (self->target, self, self, damage, NAME_Melee);
			P_TraceBleed (damage, self->target, self);
		}
	}
}

void A_ReaverRanged (AActor *self)
{
	if (self->target != NULL)
	{
		angle_t bangle;
		int pitch;

		A_FaceTarget (self);
		S_Sound (self, CHAN_WEAPON, "reaver/attack", 1, ATTN_NORM);
		bangle = self->angle;
		pitch = P_AimLineAttack (self, bangle, MISSILERANGE);

		for (int i = 0; i < 3; ++i)
		{
			angle_t angle = bangle + (pr_reaverattack.Random2() << 20);
			int damage = ((pr_reaverattack() & 7) + 1) * 3;
			P_LineAttack (self, angle, MISSILERANGE, pitch, damage, NAME_None, RUNTIME_CLASS(AStrifePuff));
		}
	}
}
