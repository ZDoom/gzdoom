#include "templates.h"
#include "actor.h"
#include "info.h"
#include "m_random.h"
#include "p_local.h"
#include "s_sound.h"
#include "p_enemy.h"
#include "a_doomglobal.h"
#include "gstrings.h"
#include "a_action.h"

void A_CyberAttack (AActor *);
void A_Hoof (AActor *);
void A_Metal (AActor *);

class ACyberdemon : public AActor
{
	DECLARE_ACTOR (ACyberdemon, AActor)
public:
	const char *GetObituary () { return GStrings("OB_CYBORG"); }
};

FState ACyberdemon::States[] =
{
#define S_CYBER_STND 0
	S_NORMAL (CYBR, 'A',   10, A_Look						, &States[S_CYBER_STND+1]),
	S_NORMAL (CYBR, 'B',   10, A_Look						, &States[S_CYBER_STND]),

#define S_CYBER_RUN (S_CYBER_STND+2)
	S_NORMAL (CYBR, 'A',	3, A_Hoof						, &States[S_CYBER_RUN+1]),
	S_NORMAL (CYBR, 'A',	3, A_Chase						, &States[S_CYBER_RUN+2]),
	S_NORMAL (CYBR, 'B',	3, A_Chase						, &States[S_CYBER_RUN+3]),
	S_NORMAL (CYBR, 'B',	3, A_Chase						, &States[S_CYBER_RUN+4]),
	S_NORMAL (CYBR, 'C',	3, A_Chase						, &States[S_CYBER_RUN+5]),
	S_NORMAL (CYBR, 'C',	3, A_Chase						, &States[S_CYBER_RUN+6]),
	S_NORMAL (CYBR, 'D',	3, A_Metal						, &States[S_CYBER_RUN+7]),
	S_NORMAL (CYBR, 'D',	3, A_Chase						, &States[S_CYBER_RUN+0]),

#define S_CYBER_ATK (S_CYBER_RUN+8)
	S_NORMAL (CYBR, 'E',	6, A_FaceTarget 				, &States[S_CYBER_ATK+1]),
	S_NORMAL (CYBR, 'F',   12, A_CyberAttack				, &States[S_CYBER_ATK+2]),
	S_NORMAL (CYBR, 'E',   12, A_FaceTarget 				, &States[S_CYBER_ATK+3]),
	S_NORMAL (CYBR, 'F',   12, A_CyberAttack				, &States[S_CYBER_ATK+4]),
	S_NORMAL (CYBR, 'E',   12, A_FaceTarget 				, &States[S_CYBER_ATK+5]),
	S_NORMAL (CYBR, 'F',   12, A_CyberAttack				, &States[S_CYBER_RUN+0]),

#define S_CYBER_PAIN (S_CYBER_ATK+6)
	S_NORMAL (CYBR, 'G',   10, A_Pain						, &States[S_CYBER_RUN+0]),

#define S_CYBER_DIE (S_CYBER_PAIN+1)
	S_NORMAL (CYBR, 'H',   10, NULL 						, &States[S_CYBER_DIE+1]),
	S_NORMAL (CYBR, 'I',   10, A_Scream 					, &States[S_CYBER_DIE+2]),
	S_NORMAL (CYBR, 'J',   10, NULL 						, &States[S_CYBER_DIE+3]),
	S_NORMAL (CYBR, 'K',   10, NULL 						, &States[S_CYBER_DIE+4]),
	S_NORMAL (CYBR, 'L',   10, NULL 						, &States[S_CYBER_DIE+5]),
	S_NORMAL (CYBR, 'M',   10, A_NoBlocking					, &States[S_CYBER_DIE+6]),
	S_NORMAL (CYBR, 'N',   10, NULL 						, &States[S_CYBER_DIE+7]),
	S_NORMAL (CYBR, 'O',   10, NULL 						, &States[S_CYBER_DIE+8]),
	S_NORMAL (CYBR, 'P',   30, NULL 						, &States[S_CYBER_DIE+9]),
	S_NORMAL (CYBR, 'P',   -1, A_BossDeath					, NULL)
};

IMPLEMENT_ACTOR (ACyberdemon, Doom, 16, 114)
	PROP_SpawnHealth (4000)
	PROP_RadiusFixed (40)
	PROP_HeightFixed (110)
	PROP_Mass (1000)
	PROP_SpeedFixed (16)
	PROP_PainChance (20)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_COUNTKILL)
	PROP_Flags2 (MF2_MCROSS|MF2_PASSMOBJ|MF2_PUSHWALL|MF2_BOSS|MF2_FLOORCLIP)
	PROP_Flags3 (MF3_NORADIUSDMG|MF3_DONTMORPH)
	PROP_Flags4 (MF4_BOSSDEATH|MF4_MISSILEMORE)
	PROP_MinMissileChance (160)

	PROP_SpawnState (S_CYBER_STND)
	PROP_SeeState (S_CYBER_RUN)
	PROP_PainState (S_CYBER_PAIN)
	PROP_MissileState (S_CYBER_ATK)
	PROP_DeathState (S_CYBER_DIE)

	PROP_SeeSound ("cyber/sight")
	PROP_PainSound ("cyber/pain")
	PROP_DeathSound ("cyber/death")
	PROP_ActiveSound ("cyber/active")
END_DEFAULTS

void A_CyberAttack (AActor *self)
{
	if (!self->target)
		return;
				
	A_FaceTarget (self);
	P_SpawnMissile (self, self->target, RUNTIME_CLASS(ARocket));
}

void A_Hoof (AActor *self)
{
	S_Sound (self, CHAN_BODY, "cyber/hoof", 1, ATTN_IDLE);
	A_Chase (self);
}
