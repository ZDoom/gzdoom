#include "actor.h"
#include "m_random.h"
#include "a_action.h"
#include "p_local.h"
#include "p_enemy.h"
#include "a_strifeglobal.h"

static FRandom pr_peasatk ("PeasantAttack");

void A_PeasantAttack (AActor *);
void A_TossGib (AActor *);
void A_GetHurt (AActor *);

// Peasant Base Class -------------------------------------------------------

class APeasant : public AStrifeHumanoid
{
	DECLARE_ACTOR (APeasant, AStrifeHumanoid)
};

FState APeasant::States[] =
{
#define S_PEASANT_STND 0
	S_NORMAL (PEAS, 'A',   10, A_Look2,			&States[S_PEASANT_STND]),

#define S_PEASANT_RUN (S_PEASANT_STND+1)
	S_NORMAL (PEAS, 'A',	5, A_Wander,		&States[S_PEASANT_RUN+1]),
	S_NORMAL (PEAS, 'A',	5, A_Wander,		&States[S_PEASANT_RUN+2]),
	S_NORMAL (PEAS, 'B',	5, A_Wander,		&States[S_PEASANT_RUN+3]),
	S_NORMAL (PEAS, 'B',	5, A_Wander,		&States[S_PEASANT_RUN+4]),
	S_NORMAL (PEAS, 'C',	5, A_Wander,		&States[S_PEASANT_RUN+5]),
	S_NORMAL (PEAS, 'C',	5, A_Wander,		&States[S_PEASANT_RUN+6]),
	S_NORMAL (PEAS, 'D',	5, A_Wander,		&States[S_PEASANT_RUN+7]),
	S_NORMAL (PEAS, 'D',	5, A_Wander,		&States[S_PEASANT_STND]),

#define S_PEASANT_MELEE (S_PEASANT_RUN+8)
	S_NORMAL (PEAS, 'E',   10, A_FaceTarget,	&States[S_PEASANT_MELEE+1]),
	S_NORMAL (PEAS, 'F',	8, A_PeasantAttack,	&States[S_PEASANT_MELEE+2]),
	S_NORMAL (PEAS, 'E',	8, NULL,			&States[S_PEASANT_RUN]),

#define S_PEASANT_PAIN (S_PEASANT_MELEE+3)
	S_NORMAL (PEAS, 'O',	3, NULL,			&States[S_PEASANT_PAIN+1]),
	S_NORMAL (PEAS, 'O',	3, A_Pain,			&States[S_PEASANT_MELEE]),

#define S_PEASANT_WOUNDED (S_PEASANT_PAIN+2)
	S_NORMAL (PEAS, 'G',	5, NULL,			&States[S_PEASANT_WOUNDED+1]),
	S_NORMAL (PEAS, 'H',   10, A_GetHurt,		&States[S_PEASANT_WOUNDED+2]),
	S_NORMAL (PEAS, 'I',	6, NULL,			&States[S_PEASANT_WOUNDED+1]),

#define S_PEASANT_DIE (S_PEASANT_WOUNDED+3)
	S_NORMAL (PEAS, 'G',	5, NULL,			&States[S_PEASANT_DIE+1]),
	S_NORMAL (PEAS, 'H',	5, A_Scream,		&States[S_PEASANT_DIE+2]),
	S_NORMAL (PEAS, 'I',	6, NULL,			&States[S_PEASANT_DIE+3]),
	S_NORMAL (PEAS, 'J',	5, A_NoBlocking,	&States[S_PEASANT_DIE+4]),
	S_NORMAL (PEAS, 'K',	5, NULL,			&States[S_PEASANT_DIE+5]),
	S_NORMAL (PEAS, 'L',	6, NULL,			&States[S_PEASANT_DIE+6]),
	S_NORMAL (PEAS, 'M',	8, NULL,			&States[S_PEASANT_DIE+7]),
	S_NORMAL (PEAS, 'N', 1400, NULL,			&States[S_PEASANT_DIE+8]),
	S_NORMAL (GIBS, 'U',	5, NULL,			&States[S_PEASANT_DIE+9]),
	S_NORMAL (GIBS, 'V', 1400, NULL,			NULL),

#define S_PEASANT_XDIE (S_PEASANT_DIE+10)
	S_NORMAL (GIBS, 'M',	5, A_TossGib,		&States[S_PEASANT_XDIE+1]),
	S_NORMAL (GIBS, 'N',	5, A_XScream,		&States[S_PEASANT_XDIE+2]),
	S_NORMAL (GIBS, 'O',	5, A_NoBlocking,	&States[S_PEASANT_XDIE+3]),
	S_NORMAL (GIBS, 'P',	4, A_TossGib,		&States[S_PEASANT_XDIE+4]),
	S_NORMAL (GIBS, 'Q',	4, A_TossGib,		&States[S_PEASANT_XDIE+5]),
	S_NORMAL (GIBS, 'R',	4, A_TossGib,		&States[S_PEASANT_XDIE+6]),
	S_NORMAL (GIBS, 'S',	4, A_TossGib,		&States[S_PEASANT_DIE+8])
};

IMPLEMENT_ACTOR (APeasant, Strife, -1, 0)
	PROP_SpawnState (S_PEASANT_STND)
	PROP_SeeState (S_PEASANT_RUN)
	PROP_PainState (S_PEASANT_PAIN)
	PROP_MeleeState (S_PEASANT_MELEE)
	PROP_WoundState (S_PEASANT_WOUNDED)
	PROP_DeathState (S_PEASANT_DIE)
	PROP_XDeathState (S_PEASANT_XDIE)

	PROP_SpawnHealth (31)
	PROP_PainChance (200)
	PROP_SpeedFixed (8)
	PROP_RadiusFixed (20)
	PROP_HeightFixed (56)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_JUSTHIT|MF_FRIENDLY)
	PROP_Flags2 (MF2_FLOORCLIP|MF2_PASSMOBJ|MF2_PUSHWALL|MF2_MCROSS)
	PROP_Flags3 (MF3_ISMONSTER)
	PROP_Flags4 (MF4_NOSPLASHALERT)
	PROP_MinMissileChance (150)

	PROP_SeeSound ("peasant/sight")
	PROP_AttackSound ("peasant/attack")
	PROP_PainSound ("peasant/pain")
	PROP_DeathSound ("peasant/death")
END_DEFAULTS

// Peasant Variant 1 --------------------------------------------------------

class APeasant1 : public APeasant
{
	DECLARE_STATELESS_ACTOR (APeasant1, APeasant)
};

IMPLEMENT_STATELESS_ACTOR (APeasant1, Strife, 3004, 0)
	PROP_StrifeType (6)
	PROP_StrifeTeaserType (6)
	PROP_SpeedFixed (4)
END_DEFAULTS

// Peasant Variant 2 --------------------------------------------------------

class APeasant2 : public APeasant
{
	DECLARE_STATELESS_ACTOR (APeasant2, APeasant)
};

IMPLEMENT_STATELESS_ACTOR (APeasant2, Strife, 130, 0)
	PROP_StrifeType (7)
	PROP_StrifeTeaserType (7)
	PROP_SpeedFixed (5)
END_DEFAULTS

// Peasant Variant 3 --------------------------------------------------------

class APeasant3 : public APeasant
{
	DECLARE_STATELESS_ACTOR (APeasant3, APeasant)
};

IMPLEMENT_STATELESS_ACTOR (APeasant3, Strife, 131, 0)
	PROP_StrifeType (8)
	PROP_StrifeTeaserType (8)
	PROP_SpeedFixed (5)
END_DEFAULTS

// Peasant Variant 4 --------------------------------------------------------

class APeasant4 : public APeasant
{
	DECLARE_STATELESS_ACTOR (APeasant4, APeasant)
};

IMPLEMENT_STATELESS_ACTOR (APeasant4, Strife, 65, 0)
	PROP_Translation (TRANSLATION_Standard,0)
	PROP_StrifeType (9)
	PROP_StrifeTeaserType (9)
	PROP_SpeedFixed (7)
END_DEFAULTS

// Peasant Variant 5 --------------------------------------------------------

class APeasant5 : public APeasant
{
	DECLARE_STATELESS_ACTOR (APeasant5, APeasant)
};

IMPLEMENT_STATELESS_ACTOR (APeasant5, Strife, 132, 0)
	PROP_Translation (TRANSLATION_Standard,0)
	PROP_StrifeType (10)
	PROP_StrifeTeaserType (10)
	PROP_SpeedFixed (7)
END_DEFAULTS

// Peasant Variant 6 --------------------------------------------------------

class APeasant6 : public APeasant
{
	DECLARE_STATELESS_ACTOR (APeasant6, APeasant)
};

IMPLEMENT_STATELESS_ACTOR (APeasant6, Strife, 133, 0)
	PROP_Translation (TRANSLATION_Standard,0)
	PROP_StrifeType (11)
	PROP_StrifeTeaserType (11)
	PROP_SpeedFixed (7)
END_DEFAULTS

// Peasant Variant 7 --------------------------------------------------------

class APeasant7 : public APeasant
{
	DECLARE_STATELESS_ACTOR (APeasant7, APeasant)
};

IMPLEMENT_STATELESS_ACTOR (APeasant7, Strife, 66, 0)
	PROP_Translation (TRANSLATION_Standard,2)
	PROP_StrifeType (12)
	PROP_StrifeTeaserType (12)
END_DEFAULTS

// Peasant Variant 8 --------------------------------------------------------

class APeasant8 : public APeasant
{
	DECLARE_STATELESS_ACTOR (APeasant8, APeasant)
};

IMPLEMENT_STATELESS_ACTOR (APeasant8, Strife, 134, 0)
	PROP_Translation (TRANSLATION_Standard,2)
	PROP_StrifeType (13)
	PROP_StrifeTeaserType (13)
END_DEFAULTS

// Peasant Variant 9 --------------------------------------------------------

class APeasant9 : public APeasant
{
	DECLARE_STATELESS_ACTOR (APeasant9, APeasant)
};

IMPLEMENT_STATELESS_ACTOR (APeasant9, Strife, 135, 0)
	PROP_Translation (TRANSLATION_Standard,2)
	PROP_StrifeType (14)
	PROP_StrifeTeaserType (14)
END_DEFAULTS

// Peasant Variant 10 --------------------------------------------------------

class APeasant10 : public APeasant
{
	DECLARE_STATELESS_ACTOR (APeasant10, APeasant)
};

IMPLEMENT_STATELESS_ACTOR (APeasant10, Strife, 67, 0)
	PROP_Translation (TRANSLATION_Standard,1)
	PROP_StrifeType (15)
	PROP_StrifeTeaserType (15)
END_DEFAULTS

// Peasant Variant 11 --------------------------------------------------------

class APeasant11 : public APeasant
{
	DECLARE_STATELESS_ACTOR (APeasant11, APeasant)
};

IMPLEMENT_STATELESS_ACTOR (APeasant11, Strife, 136, 0)
	PROP_Translation (TRANSLATION_Standard,1)
	PROP_StrifeType (16)
	PROP_StrifeTeaserType (16)
	PROP_SpeedFixed (7)
END_DEFAULTS

// Peasant Variant 12 --------------------------------------------------------

class APeasant12 : public APeasant
{
	DECLARE_STATELESS_ACTOR (APeasant12, APeasant)
};

IMPLEMENT_STATELESS_ACTOR (APeasant12, Strife, 137, 0)
	PROP_Translation (TRANSLATION_Standard,1)
	PROP_StrifeType (17)
	PROP_StrifeTeaserType (17)
END_DEFAULTS

// Peasant Variant 13 --------------------------------------------------------

class APeasant13 : public APeasant
{
	DECLARE_STATELESS_ACTOR (APeasant13, APeasant)
};

IMPLEMENT_STATELESS_ACTOR (APeasant13, Strife, 172, 0)
	PROP_Translation (TRANSLATION_Standard,3)
	PROP_StrifeType (18)
	PROP_StrifeTeaserType (18)
END_DEFAULTS

// Peasant Variant 14 --------------------------------------------------------

class APeasant14 : public APeasant
{
	DECLARE_STATELESS_ACTOR (APeasant14, APeasant)
};

IMPLEMENT_STATELESS_ACTOR (APeasant14, Strife, 173, 0)
	PROP_Translation (TRANSLATION_Standard,3)
	PROP_StrifeType (19)
	PROP_StrifeTeaserType (19)
END_DEFAULTS

// Peasant Variant 15 --------------------------------------------------------

class APeasant15 : public APeasant
{
	DECLARE_STATELESS_ACTOR (APeasant15, APeasant)
};

IMPLEMENT_STATELESS_ACTOR (APeasant15, Strife, 174, 0)
	PROP_Translation (TRANSLATION_Standard,3)
	PROP_StrifeType (20)
	PROP_StrifeTeaserType (20)
END_DEFAULTS

// Peasant Variant 16 --------------------------------------------------------

class APeasant16 : public APeasant
{
	DECLARE_STATELESS_ACTOR (APeasant16, APeasant)
};

IMPLEMENT_STATELESS_ACTOR (APeasant16, Strife, 175, 0)
	PROP_Translation (TRANSLATION_Standard,5)
	PROP_StrifeType (21)
	PROP_StrifeTeaserType (21)
END_DEFAULTS

// Peasant Variant 17 --------------------------------------------------------

class APeasant17 : public APeasant
{
	DECLARE_STATELESS_ACTOR (APeasant17, APeasant)
};

IMPLEMENT_STATELESS_ACTOR (APeasant17, Strife, 176, 0)
	PROP_Translation (TRANSLATION_Standard,5)
	PROP_StrifeType (22)
	PROP_StrifeTeaserType (22)
END_DEFAULTS

// Peasant Variant 18 --------------------------------------------------------

class APeasant18 : public APeasant
{
	DECLARE_STATELESS_ACTOR (APeasant18, APeasant)
};

IMPLEMENT_STATELESS_ACTOR (APeasant18, Strife, 177, 0)
	PROP_Translation (TRANSLATION_Standard,5)
	PROP_StrifeType (23)
	PROP_StrifeTeaserType (23)
END_DEFAULTS

// Peasant Variant 19 --------------------------------------------------------

class APeasant19 : public APeasant
{
	DECLARE_STATELESS_ACTOR (APeasant19, APeasant)
};

IMPLEMENT_STATELESS_ACTOR (APeasant19, Strife, 178, 0)
	PROP_Translation (TRANSLATION_Standard,4)
	PROP_StrifeType (24)
	PROP_StrifeTeaserType (24)
END_DEFAULTS

// Peasant Variant 20 --------------------------------------------------------

class APeasant20 : public APeasant
{
	DECLARE_STATELESS_ACTOR (APeasant20, APeasant)
};

IMPLEMENT_STATELESS_ACTOR (APeasant20, Strife, 179, 0)
	PROP_Translation (TRANSLATION_Standard,4)
	PROP_StrifeType (25)
	PROP_StrifeTeaserType (25)
END_DEFAULTS

// Peasant Variant 21 --------------------------------------------------------

class APeasant21 : public APeasant
{
	DECLARE_STATELESS_ACTOR (APeasant21, APeasant)
};

IMPLEMENT_STATELESS_ACTOR (APeasant21, Strife, 180, 0)
	PROP_Translation (TRANSLATION_Standard,4)
	PROP_StrifeType (26)
	PROP_StrifeTeaserType (26)
END_DEFAULTS

// Peasant Variant 22 --------------------------------------------------------

class APeasant22 : public APeasant
{
	DECLARE_STATELESS_ACTOR (APeasant22, APeasant)
};

IMPLEMENT_STATELESS_ACTOR (APeasant22, Strife, 181, 0)
	PROP_Translation (TRANSLATION_Standard,6)
	PROP_StrifeType (27)
	PROP_StrifeTeaserType (27)
END_DEFAULTS

//============================================================================
//
// A_PeasantAttack
//
//============================================================================

void A_PeasantAttack (AActor *self)
{
	if (self->target == NULL)
		return;

	A_FaceTarget (self);

	if (self->CheckMeleeRange ())
	{
		P_DamageMobj (self->target, self, self, (pr_peasatk() % 5) * 2 + 2);
	}
}
