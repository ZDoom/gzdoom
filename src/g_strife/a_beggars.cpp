#include "actor.h"
#include "m_random.h"
#include "a_action.h"
#include "p_local.h"
#include "p_enemy.h"
#include "s_sound.h"

void A_1fa10 (AActor *);
void A_PeasantAttack (AActor *);
void A_TossGib (AActor *);

// Base class for the beggars ---------------------------------------------

class ABeggar : public AActor
{
	DECLARE_ACTOR (ABeggar, AActor);
};

FState ABeggar::States[] =
{
#define S_BEGGAR_STND 0
	S_NORMAL (BEGR, 'A',   10, A_Look,					&States[S_BEGGAR_STND]),

#define S_BEGGAR_RUN (S_BEGGAR_STND+1)
	S_NORMAL (BEGR, 'A',	4, A_Wander,				&States[S_BEGGAR_RUN+1]),
	S_NORMAL (BEGR, 'A',	4, A_Wander,				&States[S_BEGGAR_RUN+2]),
	S_NORMAL (BEGR, 'B',	4, A_Wander,				&States[S_BEGGAR_RUN+3]),
	S_NORMAL (BEGR, 'B',	4, A_Wander,				&States[S_BEGGAR_RUN+4]),
	S_NORMAL (BEGR, 'C',	4, A_Wander,				&States[S_BEGGAR_RUN+5]),
	S_NORMAL (BEGR, 'C',	4, A_Wander,				&States[S_BEGGAR_RUN]),

#define S_BEGGAR_ATTACK (S_BEGGAR_RUN+6)
	S_NORMAL (BEGR, 'D',	8, NULL,					&States[S_BEGGAR_ATTACK+1]),
	S_NORMAL (BEGR, 'E',	8, A_PeasantAttack,			&States[S_BEGGAR_ATTACK+2]),
	S_NORMAL (BEGR, 'E',	1, A_Chase,					&States[S_BEGGAR_ATTACK+3]),
	S_NORMAL (BEGR, 'D',	8, A_1fa10,					&States[S_BEGGAR_ATTACK]),

#define S_BEGGAR_PAIN (S_BEGGAR_ATTACK+4)
	S_NORMAL (BEGR, 'A',	3, A_Pain,					&States[S_BEGGAR_PAIN+1]),
	S_NORMAL (BEGR, 'A',	3, A_Chase,					&States[S_BEGGAR_ATTACK]),

#define S_BEGGAR_DIE (S_BEGGAR_PAIN+2)
	S_NORMAL (BEGR, 'F',	4, NULL,					&States[S_BEGGAR_DIE+1]),
	S_NORMAL (BEGR, 'G',	4, A_Scream,				&States[S_BEGGAR_DIE+2]),
	S_NORMAL (BEGR, 'H',	4, NULL,					&States[S_BEGGAR_DIE+3]),
	S_NORMAL (BEGR, 'I',	4, A_NoBlocking,			&States[S_BEGGAR_DIE+4]),
	S_NORMAL (BEGR, 'J',	4, NULL,					&States[S_BEGGAR_DIE+5]),
	S_NORMAL (BEGR, 'K',	4, NULL,					&States[S_BEGGAR_DIE+6]),
	S_NORMAL (BEGR, 'L',	4, NULL,					&States[S_BEGGAR_DIE+7]),
	S_NORMAL (BEGR, 'M',	4, NULL,					&States[S_BEGGAR_DIE+8]),
	S_NORMAL (BEGR, 'N',   -1, NULL,					NULL),

#define S_BEGGAR_XDIE (S_BEGGAR_DIE+9)
	S_NORMAL (BEGR, 'F',	5, A_TossGib,				&States[S_BEGGAR_XDIE+1]),
	S_NORMAL (GIBS, 'M',	5, A_TossGib,				&States[S_BEGGAR_XDIE+2]),
	S_NORMAL (GIBS, 'N',	5, A_XScream,				&States[S_BEGGAR_XDIE+3]),
	S_NORMAL (GIBS, 'O',	5, A_NoBlocking,			&States[S_BEGGAR_XDIE+4]),
	S_NORMAL (GIBS, 'P',	4, A_TossGib,				&States[S_BEGGAR_XDIE+5]),
	S_NORMAL (GIBS, 'Q',	4, A_TossGib,				&States[S_BEGGAR_XDIE+6]),
	S_NORMAL (GIBS, 'R',	4, A_TossGib,				&States[S_BEGGAR_XDIE+7]),
	S_NORMAL (GIBS, 'S',	4, A_TossGib,				&States[S_BEGGAR_XDIE+8]),
	S_NORMAL (GIBS, 'T',	4, A_TossGib,				&States[S_BEGGAR_XDIE+9]),
	S_NORMAL (GIBS, 'U',	5, NULL,					&States[S_BEGGAR_XDIE+10]),
	S_NORMAL (GIBS, 'V', 1400, NULL,					NULL),
};

IMPLEMENT_ACTOR (ABeggar, Strife, 0, 0)
	PROP_SpawnState (S_BEGGAR_STND)
	PROP_SeeState (S_BEGGAR_RUN)
	PROP_PainState (S_BEGGAR_PAIN)
	PROP_MeleeState (S_BEGGAR_ATTACK)
	PROP_DeathState (S_BEGGAR_DIE)
	PROP_XDeathState (S_BEGGAR_XDIE)

	PROP_SpawnHealth (20)
	PROP_PainChance (250)
	PROP_SpeedFixed (3)
	PROP_RadiusFixed (20)
	PROP_HeightFixed (56)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_JUSTHIT|MF_COUNTKILL)
	PROP_Tag ("Beggar")

	PROP_AttackSound ("beggar/attack")
	PROP_PainSound ("beggar/pain")
	PROP_DeathSound ("beggar/death")
END_DEFAULTS

// Beggar 1 -----------------------------------------------------------------

class ABeggar1 : public ABeggar
{
	DECLARE_STATELESS_ACTOR (ABeggar1, ABeggar)
};

IMPLEMENT_STATELESS_ACTOR (ABeggar1, Strife, 141, 0)
END_DEFAULTS

// Beggar 2 -----------------------------------------------------------------

class ABeggar2 : public ABeggar
{
	DECLARE_STATELESS_ACTOR (ABeggar2, ABeggar)
};

IMPLEMENT_STATELESS_ACTOR (ABeggar2, Strife, 155, 0)
END_DEFAULTS

// Beggar 3 -----------------------------------------------------------------

class ABeggar3 : public ABeggar
{
	DECLARE_STATELESS_ACTOR (ABeggar3, ABeggar)
};

IMPLEMENT_STATELESS_ACTOR (ABeggar3, Strife, 156, 0)
END_DEFAULTS

// Beggar 4 -----------------------------------------------------------------

class ABeggar4 : public ABeggar
{
	DECLARE_STATELESS_ACTOR (ABeggar4, ABeggar)
};

IMPLEMENT_STATELESS_ACTOR (ABeggar4, Strife, 157, 0)
END_DEFAULTS

// Beggar 5 -----------------------------------------------------------------

class ABeggar5 : public ABeggar
{
	DECLARE_STATELESS_ACTOR (ABeggar5, ABeggar)
};

IMPLEMENT_STATELESS_ACTOR (ABeggar5, Strife, 158, 0)
END_DEFAULTS
