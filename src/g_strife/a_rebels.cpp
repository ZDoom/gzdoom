#include "actor.h"
#include "m_random.h"
#include "a_action.h"
#include "p_local.h"
#include "p_enemy.h"
#include "s_sound.h"
#include "a_strifeglobal.h"

static FRandom pr_shootgun ("ShootGun");

void A_ShootGun (AActor *);
void A_TossGib (AActor *);

// Base class for the rebels ------------------------------------------------

class ARebel : public AActor
{
	DECLARE_ACTOR (ARebel, AActor)
};

FState ARebel::States[] =
{
#define S_REBEL_STND 0
	S_NORMAL (HMN1, 'P',	5, A_Look2,				&States[S_REBEL_STND]),
	S_NORMAL (HMN1, 'Q',	8, NULL,				&States[S_REBEL_STND]),
	S_NORMAL (HMN1, 'R',	8, NULL,				&States[S_REBEL_STND]),

#define S_REBEL_WAND (S_REBEL_STND+3)
	S_NORMAL (HMN1, 'A',	6, A_Wander,			&States[S_REBEL_WAND+1]),
	S_NORMAL (HMN1, 'B',	6, A_Wander,			&States[S_REBEL_WAND+2]),
	S_NORMAL (HMN1, 'C',	6, A_Wander,			&States[S_REBEL_WAND+3]),
	S_NORMAL (HMN1, 'D',	6, A_Wander,			&States[S_REBEL_WAND+4]),
	S_NORMAL (HMN1, 'A',	6, A_Wander,			&States[S_REBEL_WAND+5]),
	S_NORMAL (HMN1, 'B',	6, A_Wander,			&States[S_REBEL_WAND+6]),
	S_NORMAL (HMN1, 'C',	6, A_Wander,			&States[S_REBEL_WAND+7]),
	S_NORMAL (HMN1, 'D',	6, A_Wander,			&States[S_REBEL_STND]),

#define S_REBEL_CHASE (S_REBEL_WAND+8)
	S_NORMAL (HMN1, 'A',	3, A_Chase,				&States[S_REBEL_CHASE+1]),
	S_NORMAL (HMN1, 'A',	3, A_Chase,				&States[S_REBEL_CHASE+2]),
	S_NORMAL (HMN1, 'B',	3, A_Chase,				&States[S_REBEL_CHASE+3]),
	S_NORMAL (HMN1, 'B',	3, A_Chase,				&States[S_REBEL_CHASE+4]),
	S_NORMAL (HMN1, 'C',	3, A_Chase,				&States[S_REBEL_CHASE+5]),
	S_NORMAL (HMN1, 'C',	3, A_Chase,				&States[S_REBEL_CHASE+6]),
	S_NORMAL (HMN1, 'D',	3, A_Chase,				&States[S_REBEL_CHASE+7]),
	S_NORMAL (HMN1, 'D',	3, A_Chase,				&States[S_REBEL_CHASE]),

#define S_REBEL_ATK (S_REBEL_CHASE+8)
	S_NORMAL (HMN1, 'E',   10, A_FaceTarget,		&States[S_REBEL_ATK+1]),
	S_BRIGHT (HMN1, 'F',   10, A_ShootGun,			&States[S_REBEL_ATK+2]),
	S_NORMAL (HMN1, 'E',   10, A_ShootGun,			&States[S_REBEL_CHASE]),

#define S_REBEL_PAIN (S_REBEL_ATK+3)
	S_NORMAL (HMN1, 'O',	3, NULL,				&States[S_REBEL_PAIN+1]),
	S_NORMAL (HMN1, 'O',	3, A_Pain,				&States[S_REBEL_CHASE]),

#define S_REBEL_DIE (S_REBEL_PAIN+2)
	S_NORMAL (HMN1, 'G',	5, NULL,				&States[S_REBEL_DIE+1]),
	S_NORMAL (HMN1, 'H',	5, A_Scream,			&States[S_REBEL_DIE+2]),
	S_NORMAL (HMN1, 'I',	3, A_NoBlocking,		&States[S_REBEL_DIE+3]),
	S_NORMAL (HMN1, 'J',	4, NULL,				&States[S_REBEL_DIE+4]),
	S_NORMAL (HMN1, 'K',	3, NULL,				&States[S_REBEL_DIE+5]),
	S_NORMAL (HMN1, 'L',	3, NULL,				&States[S_REBEL_DIE+6]),
	S_NORMAL (HMN1, 'M',	3, NULL,				&States[S_REBEL_DIE+7]),
	S_NORMAL (HMN1, 'N',   -1, NULL,				NULL),

#define S_REBEL_XDIE (S_REBEL_DIE+8)
	S_NORMAL (RGIB, 'A',	4, A_TossGib,			&States[S_REBEL_XDIE+1]),
	S_NORMAL (RGIB, 'B',	4, A_XScream,			&States[S_REBEL_XDIE+2]),
	S_NORMAL (RGIB, 'C',	3, A_NoBlocking,		&States[S_REBEL_XDIE+3]),
	S_NORMAL (RGIB, 'D',	3, A_TossGib,			&States[S_REBEL_XDIE+4]),
	S_NORMAL (RGIB, 'E',	3, A_TossGib,			&States[S_REBEL_XDIE+5]),
	S_NORMAL (RGIB, 'F',	3, A_TossGib,			&States[S_REBEL_XDIE+6]),
	S_NORMAL (RGIB, 'G',	3, NULL,				&States[S_REBEL_XDIE+7]),
	S_NORMAL (RGIB, 'H', 1400, NULL,				NULL)
};

IMPLEMENT_ACTOR (ARebel, Strife, -1, 0)
	PROP_SpawnState (S_REBEL_STND)
	PROP_SeeState (S_REBEL_CHASE)
	PROP_PainState (S_REBEL_PAIN)
	PROP_MissileState (S_REBEL_ATK)
	PROP_DeathState (S_REBEL_DIE)
	PROP_XDeathState (S_REBEL_XDIE)

	PROP_SpawnHealth (60)
	PROP_PainChance (250)
	PROP_SpeedFixed (8)
	PROP_RadiusFixed (20)
	PROP_HeightFixed (56)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_COUNTKILL|MF_STRIFEx4000000)
	PROP_Tag ("Rebel")

	PROP_SeeSound ("rebel/sight")
	PROP_PainSound ("rebel/pain")
	PROP_DeathSound ("rebel/death")
	PROP_ActiveSound ("rebel/active")
END_DEFAULTS

//============================================================================
//
// A_ShootGun
//
//============================================================================

void A_ShootGun (AActor *self)
{
	int pitch;

	if (self->target == NULL)
		return;

	S_Sound (self, CHAN_WEAPON, "monsters/rifle", 1, ATTN_NORM);
	A_FaceTarget (self);
	pitch = P_AimLineAttack (self, self->angle, MISSILERANGE);
	PuffType = RUNTIME_CLASS(AStrifePuff);
	P_LineAttack (self, self->angle + (pr_shootgun.Random2() << 19),
		MISSILERANGE, pitch,
		3*(pr_shootgun() % 5 + 1));
}

// Rebel 1 ------------------------------------------------------------------

class ARebel1 : public ARebel
{
	DECLARE_STATELESS_ACTOR (ARebel1, ARebel)
};

IMPLEMENT_STATELESS_ACTOR (ARebel1, Strife, 9, 0)
END_DEFAULTS

// Rebel 2 ------------------------------------------------------------------

class ARebel2 : public ARebel
{
	DECLARE_STATELESS_ACTOR (ARebel2, ARebel)
};

IMPLEMENT_STATELESS_ACTOR (ARebel2, Strife, 144, 0)
END_DEFAULTS

// Rebel 3 ------------------------------------------------------------------

class ARebel3 : public ARebel
{
	DECLARE_STATELESS_ACTOR (ARebel3, ARebel)
};

IMPLEMENT_STATELESS_ACTOR (ARebel3, Strife, 145, 0)
END_DEFAULTS

// Rebel 4 ------------------------------------------------------------------

class ARebel4 : public ARebel
{
	DECLARE_STATELESS_ACTOR (ARebel4, ARebel)
};

IMPLEMENT_STATELESS_ACTOR (ARebel4, Strife, 149, 0)
END_DEFAULTS

// Rebel 5 ------------------------------------------------------------------

class ARebel5 : public ARebel
{
	DECLARE_STATELESS_ACTOR (ARebel5, ARebel)
};

IMPLEMENT_STATELESS_ACTOR (ARebel5, Strife, 150, 0)
END_DEFAULTS

// Rebel 6 ------------------------------------------------------------------

class ARebel6 : public ARebel
{
	DECLARE_STATELESS_ACTOR (ARebel6, ARebel)
};

IMPLEMENT_STATELESS_ACTOR (ARebel6, Strife, 151, 0)
END_DEFAULTS
