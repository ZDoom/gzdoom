#include "actor.h"
#include "m_random.h"
#include "a_action.h"
#include "p_local.h"
#include "p_enemy.h"

void A_20e10 (AActor *);
void A_TossGib (AActor *);

// Zombie -------------------------------------------------------------------

class AZombie : public AActor
{
	DECLARE_ACTOR (AZombie, AActor)
};

FState AZombie::States[] =
{
#define S_ZOMBIE_STND 0
	S_NORMAL (PEAS, 'A',	5, A_20e10,				&States[S_ZOMBIE_STND]),

// Since it never enters the pain state, why does it have one?
#define S_ZOMBIE_PAIN (S_ZOMBIE_STND+1)
	S_NORMAL (AGRD, 'A',	5, A_20e10,				&States[S_ZOMBIE_PAIN]),

#define S_ZOMBIE_DIE (S_ZOMBIE_PAIN+1)
	S_NORMAL (GIBS, 'M',	5, A_TossGib,			&States[S_ZOMBIE_DIE+1]),
	S_NORMAL (GIBS, 'N',	5, A_XScream,			&States[S_ZOMBIE_DIE+2]),
	S_NORMAL (GIBS, 'O',	5, A_NoBlocking,		&States[S_ZOMBIE_DIE+3]),
	S_NORMAL (GIBS, 'P',	4, A_TossGib,			&States[S_ZOMBIE_DIE+4]),
	S_NORMAL (GIBS, 'Q',	4, A_TossGib,			&States[S_ZOMBIE_DIE+5]),
	S_NORMAL (GIBS, 'R',	4, A_TossGib,			&States[S_ZOMBIE_DIE+6]),
	S_NORMAL (GIBS, 'S',	4, A_TossGib,			&States[S_ZOMBIE_DIE+7]),
	S_NORMAL (GIBS, 'T',	4, A_TossGib,			&States[S_ZOMBIE_DIE+8]),
	S_NORMAL (GIBS, 'U',	5, NULL,				&States[S_ZOMBIE_DIE+9]),
	S_NORMAL (GIBS, 'V', 1400, NULL,				NULL),
};

IMPLEMENT_ACTOR (AZombie, Strife, 169, 0)
	PROP_SpawnState (S_ZOMBIE_STND)
	PROP_PainState (S_ZOMBIE_PAIN)
	PROP_DeathState (S_ZOMBIE_DIE)

	PROP_SpawnHealth (31)
	PROP_RadiusFixed (20)
	PROP_HeightFixed (56)
	PROP_PainChance (0)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_COUNTKILL)
	PROP_Translation (TRANSLATION_Standard, 0)

	PROP_DeathSound ("zombie/death")
END_DEFAULTS
