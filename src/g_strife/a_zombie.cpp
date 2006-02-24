#include "actor.h"
#include "m_random.h"
#include "a_action.h"
#include "p_local.h"
#include "p_enemy.h"
#include "a_strifeglobal.h"

void A_20e10 (AActor *);
void A_TossGib (AActor *);
void A_SpawnZombie (AActor *);

// Zombie -------------------------------------------------------------------

class AZombie : public AStrifeHumanoid
{
	DECLARE_ACTOR (AZombie, AStrifeHumanoid)
};

FState AZombie::States[] =
{
#define S_ZOMBIE_STND 0
	S_NORMAL (PEAS, 'A',	5, A_20e10,				&States[S_ZOMBIE_STND]),

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
	PROP_Flags (MF_SOLID|MF_SHOOTABLE)
	PROP_Flags2 (MF2_FLOORCLIP|MF2_PASSMOBJ|MF2_PUSHWALL|MF2_MCROSS)
	PROP_MinMissileChance (150)
	PROP_MaxStepHeight (16)
	PROP_MaxDropOffHeight (32)
	PROP_Translation (TRANSLATION_Standard, 0)
	PROP_StrifeType (28)

	PROP_DeathSound ("zombie/death")
END_DEFAULTS

// Zombie Spawner -----------------------------------------------------------

class AZombieSpawner : public AActor
{
	DECLARE_ACTOR (AZombieSpawner, AActor)
};

FState AZombieSpawner::States[] =
{
	S_NORMAL (TNT1, 'A',  175, A_SpawnZombie,		&States[0])
};

IMPLEMENT_ACTOR (AZombieSpawner, Strife, 170, 0)
	PROP_SpawnHealth (20)
	PROP_Flags (MF_SHOOTABLE|MF_NOSECTOR)
	PROP_SpawnState (0)
	PROP_RenderStyle (STYLE_None)
	PROP_StrifeType (30)
	PROP_ActiveSound ("zombie/spawner")	// Does Strife use this somewhere else?
END_DEFAULTS

//============================================================================
//
// A_SpawnZombie
//
//============================================================================

void A_SpawnZombie (AActor *self)
{
	Spawn<AZombie> (self->x, self->y, self->z);
}
