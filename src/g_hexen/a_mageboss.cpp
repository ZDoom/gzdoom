#include "actor.h"
#include "info.h"
#include "p_local.h"
#include "s_sound.h"
#include "p_enemy.h"
#include "a_action.h"

void A_MageAttack (AActor *);
void A_MageBurnScream (AActor *);

// Mage Boss (Menelkir) -----------------------------------------------------

class AMageBoss : public AActor
{
	DECLARE_ACTOR (AMageBoss, AActor)
};

FState AMageBoss::States[] =
{
#define S_MAGE 0
	S_NORMAL (MAGE, 'A',	2, NULL					    , &States[S_MAGE+1]),
	S_NORMAL (MAGE, 'A',	3, A_ClassBossHealth	    , &States[S_MAGE+2]),
	S_NORMAL (MAGE, 'A',	5, A_Look				    , &States[S_MAGE+2]),

#define S_MAGE_RUN1 (S_MAGE+3)
	S_NORMAL (MAGE, 'A',	4, A_FastChase			    , &States[S_MAGE_RUN1+1]),
	S_NORMAL (MAGE, 'B',	4, A_FastChase			    , &States[S_MAGE_RUN1+2]),
	S_NORMAL (MAGE, 'C',	4, A_FastChase			    , &States[S_MAGE_RUN1+3]),
	S_NORMAL (MAGE, 'D',	4, A_FastChase			    , &States[S_MAGE_RUN1]),

#define S_MAGE_PAIN (S_MAGE_RUN1+4)
	S_NORMAL (MAGE, 'G',	4, NULL					    , &States[S_MAGE_PAIN+1]),
	S_NORMAL (MAGE, 'G',	4, A_Pain				    , &States[S_MAGE_RUN1]),

#define S_MAGE_ATK1 (S_MAGE_PAIN+2)
	S_NORMAL (MAGE, 'E',	8, A_FaceTarget			    , &States[S_MAGE_ATK1+1]),
	S_BRIGHT (MAGE, 'F',	8, A_MageAttack			    , &States[S_MAGE_RUN1]),

#define S_MAGE_DIE1 (S_MAGE_ATK1+2)
	S_NORMAL (MAGE, 'H',	6, NULL					    , &States[S_MAGE_DIE1+1]),
	S_NORMAL (MAGE, 'I',	6, A_Scream				    , &States[S_MAGE_DIE1+2]),
	S_NORMAL (MAGE, 'J',	6, NULL					    , &States[S_MAGE_DIE1+3]),
	S_NORMAL (MAGE, 'K',	6, NULL					    , &States[S_MAGE_DIE1+4]),
	S_NORMAL (MAGE, 'L',	6, A_NoBlocking			    , &States[S_MAGE_DIE1+5]),
	S_NORMAL (MAGE, 'M',	6, NULL					    , &States[S_MAGE_DIE1+6]),
	S_NORMAL (MAGE, 'N',   -1, NULL					    , NULL),

#define S_MAGE_XDIE1 (S_MAGE_DIE1+7)
	S_NORMAL (MAGE, 'O',	5, A_Scream				    , &States[S_MAGE_XDIE1+1]),
	S_NORMAL (MAGE, 'P',	5, NULL					    , &States[S_MAGE_XDIE1+2]),
	S_NORMAL (MAGE, 'R',	5, A_NoBlocking			    , &States[S_MAGE_XDIE1+3]),
	S_NORMAL (MAGE, 'S',	5, NULL					    , &States[S_MAGE_XDIE1+4]),
	S_NORMAL (MAGE, 'T',	5, NULL					    , &States[S_MAGE_XDIE1+5]),
	S_NORMAL (MAGE, 'U',	5, NULL					    , &States[S_MAGE_XDIE1+6]),
	S_NORMAL (MAGE, 'V',	5, NULL					    , &States[S_MAGE_XDIE1+7]),
	S_NORMAL (MAGE, 'W',	5, NULL					    , &States[S_MAGE_XDIE1+8]),
	S_NORMAL (MAGE, 'X',   -1, NULL					    , NULL),

#define S_MAGE_ICE (S_MAGE_XDIE1+9)
	S_NORMAL (MAGE, 'Y',	5, A_FreezeDeath		    , &States[S_MAGE_ICE+1]),
	S_NORMAL (MAGE, 'Y',	1, A_FreezeDeathChunks	    , &States[S_MAGE_ICE+1]),

#define S_MAGE_BURN (S_MAGE_ICE+2)
	S_BRIGHT (FDTH, 'E',	5, A_MageBurnScream		    , &States[S_MAGE_BURN+1]),
	S_BRIGHT (FDTH, 'F',	4, NULL					    , &States[S_MAGE_BURN+2]),
	S_BRIGHT (FDTH, 'G',	5, NULL					    , &States[S_MAGE_BURN+3]),
	S_BRIGHT (FDTH, 'H',	4, A_Scream				    , &States[S_MAGE_BURN+4]),
	S_BRIGHT (FDTH, 'I',	5, NULL					    , &States[S_MAGE_BURN+5]),
	S_BRIGHT (FDTH, 'J',	4, NULL					    , &States[S_MAGE_BURN+6]),
	S_BRIGHT (FDTH, 'K',	5, NULL					    , &States[S_MAGE_BURN+7]),
	S_BRIGHT (FDTH, 'L',	4, NULL					    , &States[S_MAGE_BURN+8]),
	S_BRIGHT (FDTH, 'M',	5, NULL					    , &States[S_MAGE_BURN+9]),
	S_BRIGHT (FDTH, 'N',	4, NULL					    , &States[S_MAGE_BURN+10]),
	S_BRIGHT (FDTH, 'O',	5, NULL					    , &States[S_MAGE_BURN+11]),
	S_BRIGHT (FDTH, 'P',	4, NULL					    , &States[S_MAGE_BURN+12]),
	S_BRIGHT (FDTH, 'Q',	5, NULL					    , &States[S_MAGE_BURN+13]),
	S_BRIGHT (FDTH, 'R',	4, NULL					    , &States[S_MAGE_BURN+14]),
	S_BRIGHT (FDTH, 'S',	5, A_NoBlocking			    , &States[S_MAGE_BURN+15]),
	S_BRIGHT (FDTH, 'T',	4, NULL					    , &States[S_MAGE_BURN+16]),
	S_BRIGHT (FDTH, 'U',	5, NULL					    , &States[S_MAGE_BURN+17]),
	S_BRIGHT (FDTH, 'V',	4, NULL					    , NULL),

};

IMPLEMENT_ACTOR (AMageBoss, Hexen, 10102, 0)
	PROP_SpawnHealth (800)
	PROP_PainChance (50)
	PROP_SpeedFixed (25)
	PROP_RadiusFixed (16)
	PROP_HeightFixed (64)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_COUNTKILL)
	PROP_Flags2 (MF2_FLOORCLIP|MF2_PASSMOBJ|MF2_TELESTOMP|MF2_PUSHWALL|MF2_MCROSS)
	PROP_Flags3 (MF3_DONTMORPH)

	PROP_SpawnState (S_MAGE)
	PROP_SeeState (S_MAGE_RUN1)
	PROP_PainState (S_MAGE_PAIN)
	PROP_MeleeState (S_MAGE_ATK1)
	PROP_MissileState (S_MAGE_ATK1)
	PROP_DeathState (S_MAGE_DIE1)
	PROP_XDeathState (S_MAGE_XDIE1)
	PROP_IDeathState (S_MAGE_ICE)
	PROP_BDeathState (S_MAGE_BURN)

	PROP_PainSound ("PlayerMagePain")
	PROP_DeathSound ("PlayerMageCrazyDeath")
	PROP_Obituary ("$OB_MBOSS")
END_DEFAULTS


//============================================================================
//
// A_MageAttack
//
//============================================================================

void A_MageAttack (AActor *actor)
{
	extern void A_MStaffAttack2 (AActor *actor);

	if (!actor->target) return;
	A_MStaffAttack2 (actor);
}

//============================================================================
//
// A_MageBurnScream
//
//============================================================================

void A_MageBurnScream (AActor *actor)
{
	S_Sound (actor, CHAN_BODY, "PlayerMageBurnDeath", 1, ATTN_NORM);
}
