#include "actor.h"
#include "info.h"
#include "p_local.h"
#include "p_spec.h"
#include "p_enemy.h"
#include "a_action.h"

void A_KeenDie (AActor *);

class ACommanderKeen : public AActor
{
	DECLARE_ACTOR (ACommanderKeen, AActor)
};

FState ACommanderKeen::States[] =
{
#define S_KEENSTND 0
	S_NORMAL (KEEN, 'A',   -1, NULL 						, &States[S_KEENSTND]),

#define S_COMMKEEN (S_KEENSTND+1)
	S_NORMAL (KEEN, 'A',	6, NULL 						, &States[S_COMMKEEN+1]),
	S_NORMAL (KEEN, 'B',	6, NULL 						, &States[S_COMMKEEN+2]),
	S_NORMAL (KEEN, 'C',	6, A_Scream 					, &States[S_COMMKEEN+3]),
	S_NORMAL (KEEN, 'D',	6, NULL 						, &States[S_COMMKEEN+4]),
	S_NORMAL (KEEN, 'E',	6, NULL 						, &States[S_COMMKEEN+5]),
	S_NORMAL (KEEN, 'F',	6, NULL 						, &States[S_COMMKEEN+6]),
	S_NORMAL (KEEN, 'G',	6, NULL 						, &States[S_COMMKEEN+7]),
	S_NORMAL (KEEN, 'H',	6, NULL 						, &States[S_COMMKEEN+8]),
	S_NORMAL (KEEN, 'I',	6, NULL 						, &States[S_COMMKEEN+9]),
	S_NORMAL (KEEN, 'J',	6, NULL 						, &States[S_COMMKEEN+10]),
	S_NORMAL (KEEN, 'K',	6, A_KeenDie					, &States[S_COMMKEEN+11]),
	S_NORMAL (KEEN, 'L',   -1, NULL 						, NULL),

#define S_KEENPAIN (S_COMMKEEN+12)
	S_NORMAL (KEEN, 'M',	4, NULL 						, &States[S_KEENPAIN+1]),
	S_NORMAL (KEEN, 'M',	8, A_Pain						, &States[S_KEENSTND])
};

IMPLEMENT_ACTOR (ACommanderKeen, Doom, 72, 0)
	PROP_SpawnHealth (100)
	PROP_RadiusFixed (16)
	PROP_HeightFixed (72)
	PROP_MassLong (10000000)
	PROP_MaxPainChance
	PROP_Flags (MF_SOLID|MF_SPAWNCEILING|MF_NOGRAVITY|MF_SHOOTABLE|MF_COUNTKILL)
	PROP_Flags4 (MF4_NOICEDEATH)

	PROP_SpawnState (S_KEENSTND)
	PROP_PainState (S_KEENPAIN)
	PROP_DeathState (S_COMMKEEN)

	PROP_PainSound ("keen/pain")
	PROP_DeathSound ("keen/death")
END_DEFAULTS

//
// A_KeenDie
// DOOM II special, map 32.
// Uses special tag 666.
//
void A_KeenDie (AActor *self)
{
	A_NoBlocking (self);
	
	// scan the remaining thinkers to see if all Keens are dead
	AActor *other;
	TThinkerIterator<AActor> iterator;
	const PClass *matchClass = self->GetClass ();

	while ( (other = iterator.Next ()) )
	{
		if (other != self && other->health > 0 && other->IsA (matchClass))
		{
			// other Keen not dead
			return;
		}
	}

	EV_DoDoor (DDoor::doorOpen, NULL, NULL, 666, 2*FRACUNIT, 0, 0, 0);
}


