#include "actor.h"
#include "info.h"
#include "p_local.h"
#include "s_sound.h"
#include "p_enemy.h"
#include "a_action.h"
#include "m_random.h"

void A_ClericAttack (AActor *);
void A_ClericBurnScream (AActor *);

// Cleric Boss (Traductus) --------------------------------------------------

class AClericBoss : public AActor
{
	DECLARE_ACTOR (AClericBoss, AActor)
};

FState AClericBoss::States[] =
{
#define S_CLERIC 0
	S_NORMAL (CLER, 'A',	2, NULL					    , &States[S_CLERIC+1]),
	S_NORMAL (CLER, 'A',	3, A_ClassBossHealth	    , &States[S_CLERIC+2]),
	S_NORMAL (CLER, 'A',	5, A_Look				    , &States[S_CLERIC+2]),

#define S_CLERIC_RUN1 (S_CLERIC+3)
	S_NORMAL (CLER, 'A',	4, A_FastChase			    , &States[S_CLERIC_RUN1+1]),
	S_NORMAL (CLER, 'B',	4, A_FastChase			    , &States[S_CLERIC_RUN1+2]),
	S_NORMAL (CLER, 'C',	4, A_FastChase			    , &States[S_CLERIC_RUN1+3]),
	S_NORMAL (CLER, 'D',	4, A_FastChase			    , &States[S_CLERIC_RUN1]),

#define S_CLERIC_PAIN (S_CLERIC_RUN1+4)
	S_NORMAL (CLER, 'H',	4, NULL					    , &States[S_CLERIC_PAIN+1]),
	S_NORMAL (CLER, 'H',	4, A_Pain				    , &States[S_CLERIC_RUN1]),

#define S_CLERIC_ATK1 (S_CLERIC_PAIN+2)
	S_NORMAL (CLER, 'E',	8, A_FaceTarget			    , &States[S_CLERIC_ATK1+1]),
	S_NORMAL (CLER, 'F',	8, A_FaceTarget			    , &States[S_CLERIC_ATK1+2]),
	S_NORMAL (CLER, 'G',   10, A_ClericAttack		    , &States[S_CLERIC_RUN1]),

#define S_CLERIC_DIE1 (S_CLERIC_ATK1+3)
	S_NORMAL (CLER, 'I',	6, NULL					    , &States[S_CLERIC_DIE1+1]),
	S_NORMAL (CLER, 'K',	6, A_Scream				    , &States[S_CLERIC_DIE1+2]),
	S_NORMAL (CLER, 'L',	6, NULL					    , &States[S_CLERIC_DIE1+3]),
	S_NORMAL (CLER, 'L',	6, NULL					    , &States[S_CLERIC_DIE1+4]),
	S_NORMAL (CLER, 'M',	6, A_NoBlocking			    , &States[S_CLERIC_DIE1+5]),
	S_NORMAL (CLER, 'N',	6, NULL					    , &States[S_CLERIC_DIE1+6]),
	S_NORMAL (CLER, 'O',	6, NULL					    , &States[S_CLERIC_DIE1+7]),
	S_NORMAL (CLER, 'P',	6, NULL					    , &States[S_CLERIC_DIE1+8]),
	S_NORMAL (CLER, 'Q',   -1, NULL					    , NULL),

#define S_CLERIC_XDIE1 (S_CLERIC_DIE1+9)
	S_NORMAL (CLER, 'R',	5, A_Scream				    , &States[S_CLERIC_XDIE1+1]),
	S_NORMAL (CLER, 'S',	5, NULL					    , &States[S_CLERIC_XDIE1+2]),
	S_NORMAL (CLER, 'T',	5, A_NoBlocking			    , &States[S_CLERIC_XDIE1+3]),
	S_NORMAL (CLER, 'U',	5, NULL					    , &States[S_CLERIC_XDIE1+4]),
	S_NORMAL (CLER, 'V',	5, NULL					    , &States[S_CLERIC_XDIE1+5]),
	S_NORMAL (CLER, 'W',	5, NULL					    , &States[S_CLERIC_XDIE1+6]),
	S_NORMAL (CLER, 'X',	5, NULL					    , &States[S_CLERIC_XDIE1+7]),
	S_NORMAL (CLER, 'Y',	5, NULL					    , &States[S_CLERIC_XDIE1+8]),
	S_NORMAL (CLER, 'Z',	5, NULL					    , &States[S_CLERIC_XDIE1+9]),
	S_NORMAL (CLER, '[',   -1, NULL					    , NULL),

#define S_CLERIC_ICE (S_CLERIC_XDIE1+10)
	S_NORMAL (CLER, '\\',	5, A_FreezeDeath		    , &States[S_CLERIC_ICE+1]),
	S_NORMAL (CLER, '\\',	1, A_FreezeDeathChunks	    , &States[S_CLERIC_ICE+1]),

#define S_CLERIC_BURN (S_CLERIC_ICE+2)
	S_BRIGHT (FDTH, 'C',	5, A_ClericBurnScream		, &States[S_CLERIC_BURN+1]),
	S_BRIGHT (FDTH, 'D',	4, NULL					    , &States[S_CLERIC_BURN+2]),
	S_BRIGHT (FDTH, 'G',	5, NULL					    , &States[S_CLERIC_BURN+3]),
	S_BRIGHT (FDTH, 'H',	4, A_Scream					 , &States[S_CLERIC_BURN+4]),
	S_BRIGHT (FDTH, 'I',	5, NULL					    , &States[S_CLERIC_BURN+5]),
	S_BRIGHT (FDTH, 'J',	4, NULL					    , &States[S_CLERIC_BURN+6]),
	S_BRIGHT (FDTH, 'K',	5, NULL					    , &States[S_CLERIC_BURN+7]),
	S_BRIGHT (FDTH, 'L',	4, NULL					    , &States[S_CLERIC_BURN+8]),
	S_BRIGHT (FDTH, 'M',	5, NULL					    , &States[S_CLERIC_BURN+9]),
	S_BRIGHT (FDTH, 'N',	4, NULL					    , &States[S_CLERIC_BURN+10]),
	S_BRIGHT (FDTH, 'O',	5, NULL					    , &States[S_CLERIC_BURN+11]),
	S_BRIGHT (FDTH, 'P',	4, NULL					    , &States[S_CLERIC_BURN+12]),
	S_BRIGHT (FDTH, 'Q',	5, NULL					    , &States[S_CLERIC_BURN+13]),
	S_BRIGHT (FDTH, 'R',	4, NULL					    , &States[S_CLERIC_BURN+14]),
	S_BRIGHT (FDTH, 'S',	5, A_NoBlocking			    , &States[S_CLERIC_BURN+15]),
	S_BRIGHT (FDTH, 'T',	4, NULL					    , &States[S_CLERIC_BURN+16]),
	S_BRIGHT (FDTH, 'U',	5, NULL					    , &States[S_CLERIC_BURN+17]),
	S_BRIGHT (FDTH, 'V',	4, NULL					    , NULL),

};

IMPLEMENT_ACTOR (AClericBoss, Hexen, 10101, 0)
	PROP_SpawnHealth (800)
	PROP_PainChance (50)
	PROP_SpeedFixed (25)
	PROP_RadiusFixed (16)
	PROP_HeightFixed (64)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_COUNTKILL)
	PROP_Flags2 (MF2_FLOORCLIP|MF2_PASSMOBJ|MF2_TELESTOMP|MF2_PUSHWALL|MF2_MCROSS)
	PROP_Flags3 (MF3_DONTMORPH)

	PROP_SpawnState (S_CLERIC)
	PROP_SeeState (S_CLERIC_RUN1)
	PROP_PainState (S_CLERIC_PAIN)
	PROP_MeleeState (S_CLERIC_ATK1)
	PROP_MissileState (S_CLERIC_ATK1)
	PROP_DeathState (S_CLERIC_DIE1)
	PROP_XDeathState (S_CLERIC_XDIE1)
	PROP_BDeathState (S_CLERIC_BURN)
	PROP_IDeathState (S_CLERIC_ICE)

	PROP_PainSound ("PlayerClericPain")
	PROP_DeathSound ("PlayerClericCrazyDeath")
	PROP_Obituary ("$OB_CBOSS")
END_DEFAULTS

//============================================================================
//
// A_ClericAttack
//
//============================================================================

void A_ClericAttack (AActor *actor)
{
	extern void A_CHolyAttack3 (AActor *actor);

	if (!actor->target) return;
	A_CHolyAttack3 (actor);
}

//============================================================================
//
// A_ClericBurnScream
//
//============================================================================

void A_ClericBurnScream (AActor *actor)
{
	S_Sound (actor, CHAN_BODY, "PlayerClericBurnDeath", 1, ATTN_NORM);
}
