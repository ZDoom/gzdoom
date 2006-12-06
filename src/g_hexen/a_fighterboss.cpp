#include "actor.h"
#include "info.h"
#include "p_local.h"
#include "s_sound.h"
#include "p_enemy.h"
#include "a_action.h"
#include "m_random.h"

void A_FighterAttack (AActor *);
void A_FighterBurnScream (AActor *);

// Fighter Boss (Zedek) -----------------------------------------------------

class AFighterBoss : public AActor
{
	DECLARE_ACTOR (AFighterBoss, AActor)
};

FState AFighterBoss::States[] =
{
#define S_FIGHTER 0
	S_NORMAL (PLAY, 'A',	2, NULL					    , &States[S_FIGHTER+1]),
	S_NORMAL (PLAY, 'A',	3, A_ClassBossHealth	    , &States[S_FIGHTER+2]),
	S_NORMAL (PLAY, 'A',	5, A_Look				    , &States[S_FIGHTER+2]),

#define S_FIGHTER_RUN1 (S_FIGHTER+3)
	S_NORMAL (PLAY, 'A',	4, A_FastChase			    , &States[S_FIGHTER_RUN1+1]),
	S_NORMAL (PLAY, 'B',	4, A_FastChase			    , &States[S_FIGHTER_RUN1+2]),
	S_NORMAL (PLAY, 'C',	4, A_FastChase			    , &States[S_FIGHTER_RUN1+3]),
	S_NORMAL (PLAY, 'D',	4, A_FastChase			    , &States[S_FIGHTER_RUN1]),

#define S_FIGHTER_PAIN (S_FIGHTER_RUN1+4)
	S_NORMAL (PLAY, 'G',	4, NULL					    , &States[S_FIGHTER_PAIN+1]),
	S_NORMAL (PLAY, 'G',	4, A_Pain				    , &States[S_FIGHTER_RUN1]),

#define S_FIGHTER_ATK1 (S_FIGHTER_PAIN+2)
	S_NORMAL (PLAY, 'E',	8, A_FaceTarget			    , &States[S_FIGHTER_ATK1+1]),
	S_NORMAL (PLAY, 'F',	8, A_FighterAttack		    , &States[S_FIGHTER_RUN1]),

#define S_FIGHTER_DIE1 (S_FIGHTER_ATK1+2)
	S_NORMAL (PLAY, 'H',	6, NULL					    , &States[S_FIGHTER_DIE1+1]),
	S_NORMAL (PLAY, 'I',	6, A_Scream				    , &States[S_FIGHTER_DIE1+2]),
	S_NORMAL (PLAY, 'J',	6, NULL					    , &States[S_FIGHTER_DIE1+3]),
	S_NORMAL (PLAY, 'K',	6, NULL					    , &States[S_FIGHTER_DIE1+4]),
	S_NORMAL (PLAY, 'L',	6, A_NoBlocking			    , &States[S_FIGHTER_DIE1+5]),
	S_NORMAL (PLAY, 'M',	6, NULL					    , &States[S_FIGHTER_DIE1+6]),
	S_NORMAL (PLAY, 'N',   -1, NULL					    , NULL),

#define S_FIGHTER_XDIE1 (S_FIGHTER_DIE1+7)
	S_NORMAL (PLAY, 'O',	5, A_Scream				    , &States[S_FIGHTER_XDIE1+1]),
	S_NORMAL (PLAY, 'P',	5, A_SkullPop			    , &States[S_FIGHTER_XDIE1+2]),
	S_NORMAL (PLAY, 'R',	5, A_NoBlocking			    , &States[S_FIGHTER_XDIE1+3]),
	S_NORMAL (PLAY, 'S',	5, NULL					    , &States[S_FIGHTER_XDIE1+4]),
	S_NORMAL (PLAY, 'T',	5, NULL					    , &States[S_FIGHTER_XDIE1+5]),
	S_NORMAL (PLAY, 'U',	5, NULL					    , &States[S_FIGHTER_XDIE1+6]),
	S_NORMAL (PLAY, 'V',	5, NULL					    , &States[S_FIGHTER_XDIE1+7]),
	S_NORMAL (PLAY, 'W',   -1, NULL					    , NULL),

#define S_FIGHTER_ICE (S_FIGHTER_XDIE1+8)
	S_NORMAL (PLAY, 'X',	5, A_FreezeDeath		    , &States[S_FIGHTER_ICE+1]),
	S_NORMAL (PLAY, 'X',	1, A_FreezeDeathChunks	    , &States[S_FIGHTER_ICE+1]),

#define S_FIGHTER_BURN (S_FIGHTER_ICE+2)
	S_BRIGHT (FDTH, 'A',	5, A_FighterBurnScream	    , &States[S_FIGHTER_BURN+1]),
	S_BRIGHT (FDTH, 'B',	4, NULL					    , &States[S_FIGHTER_BURN+2]),
	S_BRIGHT (FDTH, 'G',	5, NULL					    , &States[S_FIGHTER_BURN+3]),
	S_BRIGHT (FDTH, 'H',	4, A_Scream				    , &States[S_FIGHTER_BURN+4]),
	S_BRIGHT (FDTH, 'I',	5, NULL					    , &States[S_FIGHTER_BURN+5]),
	S_BRIGHT (FDTH, 'J',	4, NULL					    , &States[S_FIGHTER_BURN+6]),
	S_BRIGHT (FDTH, 'K',	5, NULL					    , &States[S_FIGHTER_BURN+7]),
	S_BRIGHT (FDTH, 'L',	4, NULL					    , &States[S_FIGHTER_BURN+8]),
	S_BRIGHT (FDTH, 'M',	5, NULL					    , &States[S_FIGHTER_BURN+9]),
	S_BRIGHT (FDTH, 'N',	4, NULL					    , &States[S_FIGHTER_BURN+10]),
	S_BRIGHT (FDTH, 'O',	5, NULL					    , &States[S_FIGHTER_BURN+11]),
	S_BRIGHT (FDTH, 'P',	4, NULL					    , &States[S_FIGHTER_BURN+12]),
	S_BRIGHT (FDTH, 'Q',	5, NULL					    , &States[S_FIGHTER_BURN+13]),
	S_BRIGHT (FDTH, 'R',	4, NULL					    , &States[S_FIGHTER_BURN+14]),
	S_BRIGHT (FDTH, 'S',	5, A_NoBlocking			    , &States[S_FIGHTER_BURN+15]),
	S_BRIGHT (FDTH, 'T',	4, NULL					    , &States[S_FIGHTER_BURN+16]),
	S_BRIGHT (FDTH, 'U',	5, NULL					    , &States[S_FIGHTER_BURN+17]),
	S_BRIGHT (FDTH, 'V',	4, NULL					    , NULL),
};

IMPLEMENT_ACTOR (AFighterBoss, Hexen, 10100, 0)
	PROP_SpawnHealth (800)
	PROP_PainChance (50)
	PROP_SpeedFixed (25)
	PROP_RadiusFixed (16)
	PROP_HeightFixed (64)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_COUNTKILL)
	PROP_Flags2 (MF2_FLOORCLIP|MF2_PASSMOBJ|MF2_TELESTOMP|MF2_PUSHWALL|MF2_MCROSS)
	PROP_Flags3 (MF3_DONTMORPH)

	PROP_SpawnState (S_FIGHTER)
	PROP_SeeState (S_FIGHTER_RUN1)
	PROP_PainState (S_FIGHTER_PAIN)
	PROP_MeleeState (S_FIGHTER_ATK1)
	PROP_MissileState (S_FIGHTER_ATK1)
	PROP_DeathState (S_FIGHTER_DIE1)
	PROP_XDeathState (S_FIGHTER_XDIE1)
	PROP_IDeathState (S_FIGHTER_ICE)
	PROP_BDeathState (S_FIGHTER_BURN)

	PROP_PainSound ("PlayerFighterPain")
	PROP_DeathSound ("PlayerFighterCrazyDeath")
	PROP_Obituary ("$OB_FBOSS")
END_DEFAULTS

void A_FighterAttack (AActor *actor)
{
	extern void A_FSwordAttack2 (AActor *actor);

	if (!actor->target) return;
	A_FSwordAttack2 (actor);
}

void A_FighterBurnScream (AActor *actor)
{
	S_Sound (actor, CHAN_BODY, "PlayerFighterBurnDeath", 1, ATTN_NORM);
}
