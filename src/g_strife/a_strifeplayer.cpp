#include "actor.h"
#include "gi.h"
#include "m_random.h"
#include "s_sound.h"
#include "d_player.h"
#include "a_action.h"
#include "p_local.h"
#include "a_doomglobal.h"

void A_Pain (AActor *);
void A_PlayerScream (AActor *);
void A_XXScream (AActor *);
void A_TossGib (AActor *);

void A_ItBurnsItBurns (AActor *);
void A_DropFire (AActor *);
void A_CrispyPlayer (AActor *);
void A_Wander (AActor *);

// The player ---------------------------------------------------------------

class AStrifePlayer : public APlayerPawn
{
	DECLARE_ACTOR (AStrifePlayer, APlayerPawn)
public:
	void GiveDefaultInventory ();
};

FState AStrifePlayer::States[] =
{
#define S_PLAY 0
	S_NORMAL (PLAY, 'A',   -1, NULL 						, NULL),

#define S_PLAY_RUN (S_PLAY+1)
	S_NORMAL (PLAY, 'A',	4, NULL 						, &States[S_PLAY_RUN+1]),
	S_NORMAL (PLAY, 'B',	4, NULL 						, &States[S_PLAY_RUN+2]),
	S_NORMAL (PLAY, 'C',	4, NULL 						, &States[S_PLAY_RUN+3]),
	S_NORMAL (PLAY, 'D',	4, NULL 						, &States[S_PLAY_RUN+0]),

#define S_PLAY_ATK (S_PLAY_RUN+4)
	S_NORMAL (PLAY, 'E',   12, NULL 						, &States[S_PLAY]),
	S_NORMAL (PLAY, 'F',	6, NULL 						, &States[S_PLAY_ATK+0]),

#define S_PLAY_PAIN (S_PLAY_ATK+2)
	S_NORMAL (PLAY, 'Q',	4, NULL 						, &States[S_PLAY_PAIN+1]),
	S_NORMAL (PLAY, 'Q',	4, A_Pain						, &States[S_PLAY]),

#define S_PLAY_DIE (S_PLAY_PAIN+2)
	S_NORMAL (PLAY, 'H',	3, NULL							, &States[S_PLAY_DIE+1]),
	S_NORMAL (PLAY, 'I',	3, A_PlayerScream				, &States[S_PLAY_DIE+2]),
	S_NORMAL (PLAY, 'J',	3, A_NoBlocking					, &States[S_PLAY_DIE+3]),
	S_NORMAL (PLAY, 'K',	4, NULL 						, &States[S_PLAY_DIE+4]),
	S_NORMAL (PLAY, 'L',	4, NULL 						, &States[S_PLAY_DIE+5]),
	S_NORMAL (PLAY, 'M',	4, NULL 						, &States[S_PLAY_DIE+6]),
	S_NORMAL (PLAY, 'N',	4, NULL							, &States[S_PLAY_DIE+7]),
	S_NORMAL (PLAY, 'O',	4, NULL							, &States[S_PLAY_DIE+8]),
	S_NORMAL (PLAY, 'P',  700, NULL							, &States[S_PLAY_DIE+9+7]),

#define S_PLAY_XDIE (S_PLAY_DIE+9)
	S_NORMAL (RGIB, 'A',	5, A_TossGib					, &States[S_PLAY_XDIE+1]),
	S_NORMAL (RGIB, 'B',	5, A_XXScream					, &States[S_PLAY_XDIE+2]),
	S_NORMAL (RGIB, 'C',	5, A_NoBlocking					, &States[S_PLAY_XDIE+3]),
	S_NORMAL (RGIB, 'D',	5, A_TossGib					, &States[S_PLAY_XDIE+4]),
	S_NORMAL (RGIB, 'E',	5, A_TossGib					, &States[S_PLAY_XDIE+5]),
	S_NORMAL (RGIB, 'F',	5, A_TossGib					, &States[S_PLAY_XDIE+6]),
	S_NORMAL (RGIB, 'G',	5, A_TossGib					, &States[S_PLAY_XDIE+7]),
	S_NORMAL (RGIB, 'H',   -1, A_TossGib	 				, NULL),

	// [RH] These weren't bright in Strife, but I think they should be.
	// (After all, they are now a light source.)
#define S_PLAY_BURNDEATH (S_PLAY_XDIE+8)
	S_BRIGHT (BURN, 'A', 3, A_ItBurnsItBurns,	&States[S_PLAY_BURNDEATH+1]),
	S_BRIGHT (BURN, 'B', 3, A_DropFire,			&States[S_PLAY_BURNDEATH+2]),
	S_BRIGHT (BURN, 'C', 3, A_Wander,			&States[S_PLAY_BURNDEATH+3]),
	S_BRIGHT (BURN, 'D', 3, A_NoBlocking,		&States[S_PLAY_BURNDEATH+4]),
	S_BRIGHT (BURN, 'E', 5, A_DropFire,			&States[S_PLAY_BURNDEATH+5]),
	S_BRIGHT (BURN, 'F', 5, A_Wander,			&States[S_PLAY_BURNDEATH+6]),
	S_BRIGHT (BURN, 'G', 5, A_Wander,			&States[S_PLAY_BURNDEATH+7]),
	S_BRIGHT (BURN, 'H', 5, A_Wander,			&States[S_PLAY_BURNDEATH+8]),
	S_BRIGHT (BURN, 'I', 5, A_DropFire,			&States[S_PLAY_BURNDEATH+9]),
	S_BRIGHT (BURN, 'J', 5, A_Wander,			&States[S_PLAY_BURNDEATH+10]),
	S_BRIGHT (BURN, 'K', 5, A_Wander,			&States[S_PLAY_BURNDEATH+11]),
	S_BRIGHT (BURN, 'L', 5, A_Wander,			&States[S_PLAY_BURNDEATH+12]),
	S_BRIGHT (BURN, 'M', 3, A_DropFire,			&States[S_PLAY_BURNDEATH+13]),
	S_BRIGHT (BURN, 'N', 3, A_CrispyPlayer,		&States[S_PLAY_BURNDEATH+14]),
	S_BRIGHT (BURN, 'O', 5, NULL,				&States[S_PLAY_BURNDEATH+15]),
	S_BRIGHT (BURN, 'P', 5, NULL,				&States[S_PLAY_BURNDEATH+16]),
	S_BRIGHT (BURN, 'Q', 5, NULL,				&States[S_PLAY_BURNDEATH+17]),
	S_BRIGHT (BURN, 'P', 5, NULL,				&States[S_PLAY_BURNDEATH+18]),
	S_BRIGHT (BURN, 'Q', 5, NULL,				&States[S_PLAY_BURNDEATH+19]),
	S_BRIGHT (BURN, 'R', 7, NULL,				&States[S_PLAY_BURNDEATH+20]),
	S_BRIGHT (BURN, 'S', 7, NULL,				&States[S_PLAY_BURNDEATH+21]),
	S_BRIGHT (BURN, 'T', 7, NULL,				&States[S_PLAY_BURNDEATH+22]),
	S_BRIGHT (BURN, 'U', 7, NULL,				&States[S_PLAY_BURNDEATH+23]),
	S_NORMAL (BURN, 'V',-1, NULL,				NULL),

#define S_PLAY_ZAPDEATH (S_PLAY_BURNDEATH+24)
	S_NORMAL (DISR, 'A', 5, NULL,				&States[S_PLAY_ZAPDEATH+1]),
	S_NORMAL (DISR, 'B', 5, NULL,				&States[S_PLAY_ZAPDEATH+2]),
	S_NORMAL (DISR, 'C', 5, NULL,				&States[S_PLAY_ZAPDEATH+3]),
	S_NORMAL (DISR, 'D', 5, A_NoBlocking,		&States[S_PLAY_ZAPDEATH+4]),
	S_NORMAL (DISR, 'E', 5, NULL,				&States[S_PLAY_ZAPDEATH+5]),
	S_NORMAL (DISR, 'F', 5, NULL,				&States[S_PLAY_ZAPDEATH+6]),
	S_NORMAL (DISR, 'G', 4, NULL,				&States[S_PLAY_ZAPDEATH+7]),
	S_NORMAL (DISR, 'H', 4, NULL,				&States[S_PLAY_ZAPDEATH+8]),
	S_NORMAL (DISR, 'I', 4, NULL,				&States[S_PLAY_ZAPDEATH+9]),
	S_NORMAL (DISR, 'J', 4, NULL,				&States[S_PLAY_ZAPDEATH+10]),
	S_NORMAL (MEAT, 'D',-1, NULL,				NULL)
};

IMPLEMENT_ACTOR (AStrifePlayer, Strife, -1, 0)
	PROP_SpawnHealth (100)
	PROP_RadiusFixed (18)
	PROP_HeightFixed (56)
	PROP_Mass (100)
	PROP_PainChance (255)
	PROP_SpeedFixed (1)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_DROPOFF|MF_PICKUP|MF_NOTDMATCH|MF_FRIENDLY)
	PROP_Flags2 (MF2_SLIDE|MF2_PASSMOBJ|MF2_PUSHWALL|MF2_FLOORCLIP)
	PROP_Flags3 (MF3_NOBLOCKMONST)
	PROP_MaxStepHeight (16)

	PROP_SpawnState (S_PLAY)
	PROP_SeeState (S_PLAY_RUN)
	PROP_PainState (S_PLAY_PAIN)
	PROP_MissileState (S_PLAY_ATK)
	PROP_DeathState (S_PLAY_DIE)
	PROP_XDeathState (S_PLAY_XDIE)
	PROP_BDeathState (S_PLAY_BURNDEATH)
	PROP_EDeathState (S_PLAY_ZAPDEATH)
END_DEFAULTS

void AStrifePlayer::GiveDefaultInventory ()
{
	AWeapon *weapon;

	player->health = GetDefault()->health;
	weapon = static_cast<AWeapon *>(player->mo->GiveInventoryType (TypeInfo::FindType ("PunchDagger")));
	player->ReadyWeapon = player->PendingWeapon = weapon;
}
