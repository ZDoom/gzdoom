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
	S_BRIGHT (PLAY, 'F',	6, NULL 						, &States[S_PLAY_ATK+0]),

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
	S_NORMAL (RGIB, 'H',   -1, A_TossGib	 				, NULL)
};

IMPLEMENT_ACTOR (AStrifePlayer, Strife, -1, 0)
	PROP_SpawnHealth (100)
	PROP_RadiusFixed (18)
	PROP_HeightFixed (56)
	PROP_Mass (100)
	PROP_PainChance (255)
	PROP_SpeedFixed (1)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_DROPOFF|MF_PICKUP|MF_NOTDMATCH)
	PROP_Flags2 (MF2_SLIDE|MF2_PASSMOBJ|MF2_PUSHWALL|MF2_FLOORCLIP)
	PROP_Flags3 (MF3_NOBLOCKMONST)

	PROP_SpawnState (S_PLAY)
	PROP_SeeState (S_PLAY_RUN)
	PROP_PainState (S_PLAY_PAIN)
	PROP_MissileState (S_PLAY_ATK)
	PROP_DeathState (S_PLAY_DIE)
	PROP_XDeathState (S_PLAY_XDIE)
END_DEFAULTS

void AStrifePlayer::GiveDefaultInventory ()
{
	player->health = GetDefault()->health;
	player->weaponowned[wp_fist] = true;
	player->weaponowned[wp_pistol] = true;
	player->ammo[am_clip] = deh.StartBullets;
	if (deh.StartBullets > 0)
	{
		player->readyweapon = player->pendingweapon = wp_pistol;
	}
	else
	{
		player->readyweapon = player->pendingweapon = wp_fist;
	}
}
