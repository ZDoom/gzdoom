#include "actor.h"
#include "m_random.h"
#include "a_action.h"
#include "p_local.h"
#include "p_enemy.h"
#include "s_sound.h"
#include "a_strifeglobal.h"

void A_ShootGun (AActor *);
void A_SentinelRefire (AActor *);
void A_SpawnSpectre4 (AActor *);

// Macil (version 1) ---------------------------------------------------------

class AMacil1 : public AStrifeHumanoid
{
	DECLARE_ACTOR (AMacil1, AStrifeHumanoid)
public:
	void NoBlockingSet ();
};

FState AMacil1::States[] =
{
#define S_MACIL_STAND 0
	S_NORMAL (LEDR, 'C',	5, A_Look2,			&States[S_MACIL_STAND]),
	S_NORMAL (LEDR, 'A',	8, NULL,			&States[S_MACIL_STAND]),
	S_NORMAL (LEDR, 'B',	8, NULL,			&States[S_MACIL_STAND]),
	S_NORMAL (LEAD, 'A',	6, A_Wander,		&States[S_MACIL_STAND+4]),
	S_NORMAL (LEAD, 'B',	6, A_Wander,		&States[S_MACIL_STAND+5]),
	S_NORMAL (LEAD, 'C',	6, A_Wander,		&States[S_MACIL_STAND+6]),
	S_NORMAL (LEAD, 'D',	6, A_Wander,		&States[S_MACIL_STAND]),

#define S_MACIL_CHASE (S_MACIL_STAND+7)
	S_NORMAL (LEAD, 'A',	3, A_Chase,			&States[S_MACIL_CHASE+1]),
	S_NORMAL (LEAD, 'A',	3, A_Chase,			&States[S_MACIL_CHASE+2]),
	S_NORMAL (LEAD, 'B',	3, A_Chase,			&States[S_MACIL_CHASE+3]),
	S_NORMAL (LEAD, 'B',	3, A_Chase,			&States[S_MACIL_CHASE+4]),
	S_NORMAL (LEAD, 'C',	3, A_Chase,			&States[S_MACIL_CHASE+5]),
	S_NORMAL (LEAD, 'C',	3, A_Chase,			&States[S_MACIL_CHASE+6]),
	S_NORMAL (LEAD, 'D',	3, A_Chase,			&States[S_MACIL_CHASE+7]),
	S_NORMAL (LEAD, 'D',	3, A_Chase,			&States[S_MACIL_CHASE]),

#define S_MACIL_ATK (S_MACIL_CHASE+8)
	S_NORMAL (LEAD, 'E',	2, A_FaceTarget,	&States[S_MACIL_ATK+1]),
	S_BRIGHT (LEAD, 'F',	2, A_ShootGun,		&States[S_MACIL_ATK+2]),
	S_NORMAL (LEAD, 'E',	2, A_SentinelRefire,&States[S_MACIL_ATK]),

#define S_MACIL_PAIN (S_MACIL_ATK+3)
	S_NORMAL (LEAD, 'Y',	3, NULL,			&States[S_MACIL_PAIN+1]),
	S_NORMAL (LEAD, 'Y',	3, A_Pain,			&States[S_MACIL_CHASE]),

#define S_MACIL_ATK2 (S_MACIL_PAIN+2)
	S_NORMAL (LEAD, 'E',	4, A_FaceTarget,	&States[S_MACIL_ATK2+1]),
	S_BRIGHT (LEAD, 'F',	4, A_ShootGun,		&States[S_MACIL_ATK2+2]),
	S_NORMAL (LEAD, 'E',	2, A_SentinelRefire,&States[S_MACIL_ATK2]),

#define S_MACIL_DIE (S_MACIL_ATK2+3)
	S_NORMAL (LEAD, 'G',	5, NULL,			&States[S_MACIL_DIE+1]),
	S_NORMAL (LEAD, 'H',	5, A_Scream,		&States[S_MACIL_DIE+2]),
	S_NORMAL (LEAD, 'I',	4, NULL,			&States[S_MACIL_DIE+3]),
	S_NORMAL (LEAD, 'J',	4, NULL,			&States[S_MACIL_DIE+4]),
	S_NORMAL (LEAD, 'K',	3, NULL,			&States[S_MACIL_DIE+5]),
	S_NORMAL (LEAD, 'L',	3, A_NoBlocking,	&States[S_MACIL_DIE+6]),
	S_NORMAL (LEAD, 'M',	3, NULL,			&States[S_MACIL_DIE+7]),
	S_NORMAL (LEAD, 'N',	3, NULL,			&States[S_MACIL_DIE+8]),
	S_NORMAL (LEAD, 'O',	3, NULL,			&States[S_MACIL_DIE+9]),
	S_NORMAL (LEAD, 'P',	3, NULL,			&States[S_MACIL_DIE+10]),
	S_NORMAL (LEAD, 'Q',	3, NULL,			&States[S_MACIL_DIE+11]),
	S_NORMAL (LEAD, 'R',	3, NULL,			&States[S_MACIL_DIE+12]),
	S_NORMAL (LEAD, 'S',	3, NULL,			&States[S_MACIL_DIE+13]),
	S_NORMAL (LEAD, 'T',	3, NULL,			&States[S_MACIL_DIE+14]),
	S_NORMAL (LEAD, 'U',	3, NULL,			&States[S_MACIL_DIE+15]),
	S_NORMAL (LEAD, 'V',	3, NULL,			&States[S_MACIL_DIE+16]),
	S_NORMAL (LEAD, 'W',	3, A_SpawnSpectre4,	&States[S_MACIL_DIE+17]),
	S_NORMAL (LEAD, 'X',   -1, NULL,			NULL)
};

IMPLEMENT_ACTOR (AMacil1, Strife, 64, 0)
	PROP_StrifeType (49)
	PROP_StrifeTeaserType (48)
	PROP_StrifeTeaserType2 (49)
	PROP_SpawnState (S_MACIL_STAND)
	PROP_SpawnHealth (95)
	PROP_SeeState (S_MACIL_CHASE)
	PROP_PainState (S_MACIL_PAIN)
	PROP_PainChance (250)
	PROP_MissileState (S_MACIL_ATK)
	PROP_DeathState (S_MACIL_CHASE)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_NOTDMATCH)
	PROP_Flags2 (MF2_FLOORCLIP|MF2_PASSMOBJ|MF2_PUSHWALL|MF2_MCROSS)
	PROP_Flags3 (MF3_ISMONSTER)
	PROP_Flags4 (MF4_FIRERESIST|MF4_NOICEDEATH|MF4_NOSPLASHALERT)
	PROP_Flags5 (MF5_NODAMAGE)
	PROP_MinMissileChance (150)
	PROP_RadiusFixed (20)
	PROP_HeightFixed (56)
	PROP_SpeedFixed (8)
	PROP_SeeSound ("macil/sight")
	PROP_PainSound ("macil/pain")
	PROP_ActiveSound ("macil/active")
	PROP_Tag ("MACIL")
	PROP_Obituary ("$OB_MACIL")
END_DEFAULTS

//============================================================================
//
// AMacil1 :: NoBlockingSet
//
//============================================================================

void AMacil1::NoBlockingSet ()
{
	P_DropItem (this, "BoxOfBullets", -1, 256);
}

// Macil (version 2) ---------------------------------------------------------

class AMacil2 : public AMacil1
{
	DECLARE_STATELESS_ACTOR (AMacil2, AMacil1)
public:
	void NoBlockingSet ();
	int TakeSpecialDamage (AActor *inflictor, AActor *source, int damage, FName damagetype);
};

IMPLEMENT_STATELESS_ACTOR (AMacil2, Strife, 200, 0)
	PROP_StrifeType (50)
	PROP_StrifeTeaserType (49)
	PROP_StrifeTeaserType2 (50)
	PROP_PainChance (200)
	PROP_MissileState (S_MACIL_ATK2)
	PROP_DeathState (S_MACIL_DIE)
	PROP_XDeathState (S_MACIL_DIE)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_COUNTKILL|MF_NOTDMATCH)
	PROP_Flags2 (MF2_FLOORCLIP|MF2_PASSMOBJ|MF2_PUSHWALL|MF2_MCROSS)
	PROP_Flags4Clear (MF4_FIRERESIST)
	PROP_Flags4Set (MF4_SPECTRAL)
	PROP_Flags5Clear (MF5_NODAMAGE)
	PROP_MinMissileChance (150)
	PROP_Tag ("MACIL")
	PROP_DeathSound ("macil/slop")
END_DEFAULTS


//============================================================================
//
// AMacil2 :: NoBlockingSet
//
// The second version of Macil spawns a specter instead of dropping something.
//
//============================================================================

void AMacil2::NoBlockingSet ()
{
}

//============================================================================
//
// AMacil2 :: TakeSpecialDamage
//
// Macil is invulnerable to the first stage Sigil.
//
//============================================================================

int AMacil2::TakeSpecialDamage (AActor *inflictor, AActor *source, int damage, FName damagetype)
{
	if (inflictor != NULL && inflictor->IsKindOf (RUNTIME_CLASS(ASpectralLightningV1)))
		return -1;

	return Super::TakeSpecialDamage(inflictor, source, damage, damagetype);
}
