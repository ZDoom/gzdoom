#include "actor.h"
#include "m_random.h"
#include "a_action.h"
#include "p_local.h"
#include "p_enemy.h"

extern AActor *soundtarget;

void A_20960 (AActor *);
void A_214c0 (AActor *);

void A_TossGib (AActor *);

// Becoming Acolyte ---------------------------------------------------------

class ABecomingAcolyte : public AActor
{
	DECLARE_ACTOR (ABecomingAcolyte, AActor)
};

FState ABecomingAcolyte::States[] =
{
#define S_BECOMING_STND 0
	S_NORMAL (ARMR, 'A',   -1, NULL,			NULL),

#define S_BECOMING_PAIN (S_BECOMING_STND+1)
	S_NORMAL (ARMR, 'A',   -1, A_20960,			NULL),

#define S_BECOMING_DIE (S_BECOMING_PAIN+1)
	S_NORMAL (GIBS, 'A',	5, A_NoBlocking,	&States[S_BECOMING_DIE+1]),
	S_NORMAL (GIBS, 'B',	5, A_TossGib,		&States[S_BECOMING_DIE+2]),
	S_NORMAL (GIBS, 'C',	5, A_TossGib,		&States[S_BECOMING_DIE+3]),
	S_NORMAL (GIBS, 'D',	4, A_TossGib,		&States[S_BECOMING_DIE+4]),
	S_NORMAL (GIBS, 'E',	4, A_XScream,		&States[S_BECOMING_DIE+5]),
	S_NORMAL (GIBS, 'F',	4, A_TossGib,		&States[S_BECOMING_DIE+6]),
	S_NORMAL (GIBS, 'G',	4, NULL,			&States[S_BECOMING_DIE+7]),
	S_NORMAL (GIBS, 'H',	4, NULL,			&States[S_BECOMING_DIE+8]),
	S_NORMAL (GIBS, 'I',	5, NULL,			&States[S_BECOMING_DIE+9]),
	S_NORMAL (GIBS, 'J',	5, A_214c0,			&States[S_BECOMING_DIE+10]),
	S_NORMAL (GIBS, 'K',	5, NULL,			&States[S_BECOMING_DIE+11]),
	S_NORMAL (GIBS, 'L', 1400, NULL,			NULL),
};

IMPLEMENT_ACTOR (ABecomingAcolyte, Strife, 201, 0)
	PROP_SpawnState (S_BECOMING_STND)
	PROP_PainState (S_BECOMING_PAIN)
	PROP_DeathState (S_BECOMING_DIE)

	PROP_SpawnHealth (61)
	PROP_PainChance (255)
	PROP_RadiusFixed (20)
	PROP_HeightFixed (56)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_COUNTKILL)

	PROP_DeathSound ("becoming/death")
END_DEFAULTS

void A_20960 (AActor *self)
{
	EV_DoDoor (DDoor::doorClose, NULL, self, 999, 8*FRACUNIT, 0, NoKey);
	if (self->target != NULL && self->target->player != NULL)
	{
		validcount++;
		soundtarget = self->target;
		P_RecursiveSound (self->Sector, 0);
	}
}

void A_214c0 (AActor *self)
{
	// Only needed by mobjtype 60, so will delay implementing it
}
