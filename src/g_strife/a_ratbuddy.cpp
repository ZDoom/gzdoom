#include "actor.h"
#include "p_enemy.h"

// Rat Buddy ----------------------------------------------------------------

class ARatBuddy : public AActor
{
	DECLARE_ACTOR (ARatBuddy, AActor)
};

FState ARatBuddy::States[] =
{
#define S_RAT_STAND 0
	S_NORMAL (RATT, 'A',  10, A_Look,			&States[S_RAT_STAND]),

#define S_RAT_CHASE (S_RAT_STAND+1)
	S_NORMAL (RATT, 'A',   4, A_Chase,			&States[S_RAT_CHASE+1]),
	S_NORMAL (RATT, 'A',   4, A_Chase,			&States[S_RAT_CHASE+2]),
	S_NORMAL (RATT, 'B',   4, A_Chase,			&States[S_RAT_CHASE+3]),
	S_NORMAL (RATT, 'B',   4, A_Chase,			&States[S_RAT_CHASE]),

#define S_RAT_WANDER (S_RAT_CHASE+4)
	S_NORMAL (RATT, 'A',   8, A_Wander,			&States[S_RAT_WANDER+1]),
	S_NORMAL (RATT, 'B',   4, A_Wander,			&States[S_RAT_CHASE]),

#define S_RAT_DIE (S_RAT_WANDER+2)
	S_NORMAL (MEAT, 'Q', 700, NULL,				NULL)
};

IMPLEMENT_ACTOR (ARatBuddy, Strife, 85, 0)
	PROP_StrifeType (202)
	PROP_StrifeTeaserType (196)
	PROP_StrifeTeaserType2 (200)
	PROP_SpawnHealth (5)
	PROP_SpawnState (S_RAT_STAND)
	PROP_SeeState (S_RAT_CHASE)
	PROP_MeleeState (S_RAT_WANDER)
	PROP_DeathState (S_RAT_DIE)
	PROP_SpeedFixed (13)
	PROP_RadiusFixed (10)
	PROP_HeightFixed (16)
	PROP_Flags (MF_NOBLOOD|MF_COUNTKILL)
	PROP_Flags2 (MF2_FLOORCLIP|MF2_PASSMOBJ)
	PROP_Flags4 (MF4_INCOMBAT)
	PROP_MinMissileChance (150)
	PROP_Tag ("rat_buddy")
	PROP_SeeSound ("rat/sight")
	PROP_DeathSound ("rat/death")
	PROP_ActiveSound ("rat/active")
END_DEFAULTS
