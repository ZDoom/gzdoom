#include "templates.h"
#include "actor.h"
#include "info.h"
#include "m_random.h"
#include "s_sound.h"
#include "p_local.h"
#include "p_enemy.h"
#include "gstrings.h"
#include "a_action.h"

static FRandom pr_spidey ("SpidMissileRange");
static FRandom pr_spidrefire ("SpidRefire");

void A_SpidRefire (AActor *);
void A_Metal (AActor *);
void A_SPosAttackUseAtkSound (AActor *);

class ASpiderMastermind : public AActor
{
	DECLARE_ACTOR (ASpiderMastermind, AActor)
public:
	bool SuggestMissileAttack (fixed_t dist);
	const char *GetObituary () { return GStrings(OB_SPIDER); }
};

FState ASpiderMastermind::States[] =
{
#define S_SPID_STND 0
	S_NORMAL (SPID, 'A',   10, A_Look						, &States[S_SPID_STND+1]),
	S_NORMAL (SPID, 'B',   10, A_Look						, &States[S_SPID_STND]),

#define S_SPID_RUN (S_SPID_STND+2)
	S_NORMAL (SPID, 'A',	3, A_Metal						, &States[S_SPID_RUN+1]),
	S_NORMAL (SPID, 'A',	3, A_Chase						, &States[S_SPID_RUN+2]),
	S_NORMAL (SPID, 'B',	3, A_Chase						, &States[S_SPID_RUN+3]),
	S_NORMAL (SPID, 'B',	3, A_Chase						, &States[S_SPID_RUN+4]),
	S_NORMAL (SPID, 'C',	3, A_Metal						, &States[S_SPID_RUN+5]),
	S_NORMAL (SPID, 'C',	3, A_Chase						, &States[S_SPID_RUN+6]),
	S_NORMAL (SPID, 'D',	3, A_Chase						, &States[S_SPID_RUN+7]),
	S_NORMAL (SPID, 'D',	3, A_Chase						, &States[S_SPID_RUN+8]),
	S_NORMAL (SPID, 'E',	3, A_Metal						, &States[S_SPID_RUN+9]),
	S_NORMAL (SPID, 'E',	3, A_Chase						, &States[S_SPID_RUN+10]),
	S_NORMAL (SPID, 'F',	3, A_Chase						, &States[S_SPID_RUN+11]),
	S_NORMAL (SPID, 'F',	3, A_Chase						, &States[S_SPID_RUN+0]),

#define S_SPID_ATK (S_SPID_RUN+12)
	S_BRIGHT (SPID, 'A',   20, A_FaceTarget 				, &States[S_SPID_ATK+1]),
	S_BRIGHT (SPID, 'G',	4, A_SPosAttackUseAtkSound		, &States[S_SPID_ATK+2]),
	S_BRIGHT (SPID, 'H',	4, A_SPosAttackUseAtkSound		, &States[S_SPID_ATK+3]),
	S_BRIGHT (SPID, 'H',	1, A_SpidRefire 				, &States[S_SPID_ATK+1]),

#define S_SPID_PAIN (S_SPID_ATK+4)
	S_NORMAL (SPID, 'I',	3, NULL 						, &States[S_SPID_PAIN+1]),
	S_NORMAL (SPID, 'I',	3, A_Pain						, &States[S_SPID_RUN+0]),

#define S_SPID_DIE (S_SPID_PAIN+2)
	S_NORMAL (SPID, 'J',   20, A_Scream 					, &States[S_SPID_DIE+1]),
	S_NORMAL (SPID, 'K',   10, A_NoBlocking					, &States[S_SPID_DIE+2]),
	S_NORMAL (SPID, 'L',   10, NULL 						, &States[S_SPID_DIE+3]),
	S_NORMAL (SPID, 'M',   10, NULL 						, &States[S_SPID_DIE+4]),
	S_NORMAL (SPID, 'N',   10, NULL 						, &States[S_SPID_DIE+5]),
	S_NORMAL (SPID, 'O',   10, NULL 						, &States[S_SPID_DIE+6]),
	S_NORMAL (SPID, 'P',   10, NULL 						, &States[S_SPID_DIE+7]),
	S_NORMAL (SPID, 'Q',   10, NULL 						, &States[S_SPID_DIE+8]),
	S_NORMAL (SPID, 'R',   10, NULL 						, &States[S_SPID_DIE+9]),
	S_NORMAL (SPID, 'S',   30, NULL 						, &States[S_SPID_DIE+10]),
	S_NORMAL (SPID, 'S',   -1, A_BossDeath					, NULL)
};

IMPLEMENT_ACTOR (ASpiderMastermind, Doom, 7, 7)
	PROP_SpawnHealth (3000)
	PROP_RadiusFixed (128)
	PROP_HeightFixed (100)
	PROP_Mass (1000)
	PROP_SpeedFixed (12)
	PROP_PainChance (40)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_COUNTKILL)
	PROP_Flags2 (MF2_MCROSS|MF2_PASSMOBJ|MF2_PUSHWALL|MF2_BOSS|MF2_FLOORCLIP)
	PROP_Flags3 (MF3_NORADIUSDMG)
	PROP_Flags4 (MF4_BOSSDEATH)

	PROP_SpawnState (S_SPID_STND)
	PROP_SeeState (S_SPID_RUN)
	PROP_PainState (S_SPID_PAIN)
	PROP_MissileState (S_SPID_ATK)
	PROP_DeathState (S_SPID_DIE)

	PROP_SeeSound ("spider/sight")
	PROP_AttackSound ("spider/attack")
	PROP_PainSound ("spider/pain")
	PROP_DeathSound ("spider/death")
	PROP_ActiveSound ("spider/active")
END_DEFAULTS

bool ASpiderMastermind::SuggestMissileAttack (fixed_t dist)
{
	return pr_spidey() >= MIN<int> (dist >> (FRACBITS + 1), 200);
}

void A_SpidRefire (AActor *self)
{
	// keep firing unless target got out of sight
	A_FaceTarget (self);

	if (pr_spidrefire() < 10)
		return;

	if (!self->target
		|| self->target->health <= 0
		|| !P_CheckSight (self, self->target, false) )
	{
		self->SetState (self->SeeState);
	}
}

void A_Metal (AActor *self)
{
	S_Sound (self, CHAN_BODY, "spider/walk", 1, ATTN_IDLE);
	A_Chase (self);
}
