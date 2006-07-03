#include "actor.h"
#include "gi.h"
#include "m_random.h"
#include "s_sound.h"
#include "d_player.h"
#include "a_action.h"
#include "p_local.h"
#include "p_enemy.h"
#include "a_action.h"
#include "a_hexenglobal.h"
#include "templates.h"

// The cleric ---------------------------------------------------------------

FState AClericPlayer::States[] =
{
#define S_CPLAY 0
	S_NORMAL (CLER, 'A',   -1, NULL					    , NULL),

#define S_CPLAY_RUN1 (S_CPLAY+1)
	S_NORMAL (CLER, 'A',	4, NULL					    , &States[S_CPLAY_RUN1+1]),
	S_NORMAL (CLER, 'B',	4, NULL					    , &States[S_CPLAY_RUN1+2]),
	S_NORMAL (CLER, 'C',	4, NULL					    , &States[S_CPLAY_RUN1+3]),
	S_NORMAL (CLER, 'D',	4, NULL					    , &States[S_CPLAY_RUN1]),

#define S_CPLAY_PAIN (S_CPLAY_RUN1+4)
	S_NORMAL (CLER, 'H',	4, NULL					    , &States[S_CPLAY_PAIN+1]),
	S_NORMAL (CLER, 'H',	4, A_Pain				    , &States[S_CPLAY]),

#define S_CPLAY_ATK1 (S_CPLAY_PAIN+2)
	S_NORMAL (CLER, 'E',	6, NULL					    , &States[S_CPLAY_ATK1+1]),
	S_NORMAL (CLER, 'F',	6, NULL					    , &States[S_CPLAY_ATK1+2]),
	S_NORMAL (CLER, 'G',	6, NULL					    , &States[S_CPLAY]),

#define S_CPLAY_DIE1 (S_CPLAY_ATK1+3)
	S_NORMAL (CLER, 'I',	6, NULL					    , &States[S_CPLAY_DIE1+1]),
	S_NORMAL (CLER, 'K',	6, A_PlayerScream		    , &States[S_CPLAY_DIE1+2]),
	S_NORMAL (CLER, 'L',	6, NULL					    , &States[S_CPLAY_DIE1+3]),
	S_NORMAL (CLER, 'L',	6, NULL					    , &States[S_CPLAY_DIE1+4]),
	S_NORMAL (CLER, 'M',	6, A_NoBlocking			    , &States[S_CPLAY_DIE1+5]),
	S_NORMAL (CLER, 'N',	6, NULL					    , &States[S_CPLAY_DIE1+6]),
	S_NORMAL (CLER, 'O',	6, NULL					    , &States[S_CPLAY_DIE1+7]),
	S_NORMAL (CLER, 'P',	6, NULL					    , &States[S_CPLAY_DIE1+8]),
	S_NORMAL (CLER, 'Q',   -1, NULL/*A_AddPlayerCorpse*/, NULL),

#define S_CPLAY_XDIE1 (S_CPLAY_DIE1+9)
	S_NORMAL (CLER, 'R',	5, A_PlayerScream		    , &States[S_CPLAY_XDIE1+1]),
	S_NORMAL (CLER, 'S',	5, NULL					    , &States[S_CPLAY_XDIE1+2]),
	S_NORMAL (CLER, 'T',	5, A_NoBlocking			    , &States[S_CPLAY_XDIE1+3]),
	S_NORMAL (CLER, 'U',	5, NULL					    , &States[S_CPLAY_XDIE1+4]),
	S_NORMAL (CLER, 'V',	5, NULL					    , &States[S_CPLAY_XDIE1+5]),
	S_NORMAL (CLER, 'W',	5, NULL					    , &States[S_CPLAY_XDIE1+6]),
	S_NORMAL (CLER, 'X',	5, NULL					    , &States[S_CPLAY_XDIE1+7]),
	S_NORMAL (CLER, 'Y',	5, NULL					    , &States[S_CPLAY_XDIE1+8]),
	S_NORMAL (CLER, 'Z',	5, NULL					    , &States[S_CPLAY_XDIE1+9]),
	S_NORMAL (CLER, '[',   -1, NULL/*A_AddPlayerCorpse*/, NULL),

#define S_CPLAY_ICE (S_CPLAY_XDIE1+10)
	S_NORMAL (CLER, '\\',	5, A_FreezeDeath		    , &States[S_CPLAY_ICE+1]),
	S_NORMAL (CLER, '\\',	1, A_FreezeDeathChunks	    , &States[S_CPLAY_ICE+1]),

#define S_PLAY_C_FDTH (S_CPLAY_ICE+2)
	S_BRIGHT (FDTH, 'C',	5, A_FireScream			    , &States[S_PLAY_C_FDTH+1]),
	S_BRIGHT (FDTH, 'D',	4, NULL					    , &States[S_PLAY_C_FDTH+2]),

#define S_PLAY_FDTH (S_PLAY_C_FDTH+2)
	S_BRIGHT (FDTH, 'G',	5, NULL						, &States[S_PLAY_FDTH+1]),
	S_BRIGHT (FDTH, 'H',	4, A_PlayerScream 			, &States[S_PLAY_FDTH+2]),
	S_BRIGHT (FDTH, 'I',	5, NULL 					, &States[S_PLAY_FDTH+3]),
	S_BRIGHT (FDTH, 'J',	4, NULL 					, &States[S_PLAY_FDTH+4]),
	S_BRIGHT (FDTH, 'K',	5, NULL 					, &States[S_PLAY_FDTH+5]),
	S_BRIGHT (FDTH, 'L',	4, NULL 					, &States[S_PLAY_FDTH+6]),
	S_BRIGHT (FDTH, 'M',	5, NULL 					, &States[S_PLAY_FDTH+7]),
	S_BRIGHT (FDTH, 'N',	4, NULL 					, &States[S_PLAY_FDTH+8]),
	S_BRIGHT (FDTH, 'O',	5, NULL 					, &States[S_PLAY_FDTH+9]),
	S_BRIGHT (FDTH, 'P',	4, NULL 					, &States[S_PLAY_FDTH+10]),
	S_BRIGHT (FDTH, 'Q',	5, NULL 					, &States[S_PLAY_FDTH+11]),
	S_BRIGHT (FDTH, 'R',	4, NULL 					, &States[S_PLAY_FDTH+12]),
	S_BRIGHT (FDTH, 'S',	5, A_NoBlocking 			, &States[S_PLAY_FDTH+13]),
	S_BRIGHT (FDTH, 'T',	4, NULL 					, &States[S_PLAY_FDTH+14]),
	S_BRIGHT (FDTH, 'U',	5, NULL 					, &States[S_PLAY_FDTH+15]),
	S_BRIGHT (FDTH, 'V',	4, NULL 					, &States[S_PLAY_FDTH+16]),
	S_NORMAL (ACLO, 'E',   35, A_CheckBurnGone			, &States[S_PLAY_FDTH+16]),
	S_NORMAL (ACLO, 'E',	8, NULL 					, NULL),
};

IMPLEMENT_ACTOR (AClericPlayer, Hexen, -1, 0)
	PROP_SpawnHealth (100)
	PROP_ReactionTime (0)
	PROP_PainChance (255)
	PROP_RadiusFixed (16)
	PROP_HeightFixed (64)
	PROP_SpeedFixed (1)
	PROP_RadiusdamageFactor(FRACUNIT/4)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_DROPOFF|MF_PICKUP|MF_NOTDMATCH|MF_FRIENDLY)
	PROP_Flags2 (MF2_WINDTHRUST|MF2_FLOORCLIP|MF2_SLIDE|MF2_PASSMOBJ|MF2_TELESTOMP|MF2_PUSHWALL)
	PROP_Flags3 (MF3_NOBLOCKMONST)
	PROP_Flags4 (MF4_NOSKIN)

	PROP_SpawnState (S_CPLAY)
	PROP_SeeState (S_CPLAY_RUN1)
	PROP_PainState (S_CPLAY_PAIN)
	PROP_MissileState (S_CPLAY_ATK1)
	PROP_DeathState (S_CPLAY_DIE1)
	PROP_XDeathState (S_CPLAY_XDIE1)
	PROP_BDeathState (S_PLAY_C_FDTH)
	PROP_IDeathState (S_CPLAY_ICE)

	PROP_PainSound ("PlayerClericPain")
END_DEFAULTS

const char *AClericPlayer::GetSoundClass ()
{
	if (player == NULL || player->userinfo.skin == 0)
	{
		return "cleric";
	}
	return Super::GetSoundClass ();
}
void AClericPlayer::PlayAttacking2 ()
{
	SetState (MissileState);
}

void AClericPlayer::GiveDefaultInventory ()
{
	player->health = GetDefault()->health;
	player->ReadyWeapon = player->PendingWeapon = static_cast<AWeapon *>
		(GiveInventoryType (PClass::FindClass ("CWeapMace")));

	GiveInventoryType (RUNTIME_CLASS(AHexenArmor));
	AHexenArmor *armor = FindInventory<AHexenArmor>();
	armor->Slots[4] = 10*FRACUNIT;
	armor->SlotsIncrement[0] = 10*FRACUNIT;
	armor->SlotsIncrement[1] = 25*FRACUNIT;
	armor->SlotsIncrement[2] =  5*FRACUNIT;
	armor->SlotsIncrement[3] = 20*FRACUNIT;
}

fixed_t AClericPlayer::GetJumpZ ()
{
	return FRACUNIT*39/4;	// ~9.75
}

void AClericPlayer::SpecialInvulnerabilityHandling (EInvulState state, fixed_t * pAlpha)
{
	if (state == INVUL_Active)
	{
		RenderStyle = STYLE_Translucent;
		if (!(level.time & 7) && alpha > 0 && alpha < OPAQUE)
		{
			if (alpha == HX_SHADOW)
			{
				alpha = HX_ALTSHADOW;
			}
			else
			{
				alpha = 0;
				flags2 |= MF2_NONSHOOTABLE;
			}
		}
		if (!(level.time & 31))
		{
			if (alpha == 0)
			{
				flags2 &= ~MF2_NONSHOOTABLE;
				alpha = HX_ALTSHADOW;
			}
			else
			{
				alpha = HX_SHADOW;
			}
		}
	}
	else if (state == INVUL_Stop)
	{
		flags2 &= ~MF2_NONSHOOTABLE;
		RenderStyle = STYLE_Normal;
		alpha = OPAQUE;
	}
	else if (state == INVUL_GetAlpha)
	{
		if (pAlpha != NULL) *pAlpha = MIN<fixed_t>(FRACUNIT/4 + alpha*3/4, FRACUNIT);
	}
}

// Cleric Weapon Base Class -------------------------------------------------

IMPLEMENT_STATELESS_ACTOR (AClericWeapon, Hexen, -1, 0)
	PROP_Weapon_Kickback (150)
END_DEFAULTS

bool AClericWeapon::TryPickup (AActor *toucher)
{
	// The Doom and Hexen players are not excluded from pickup in case
	// somebody wants to use these weapons with either of those games.
	if (toucher->IsKindOf (RUNTIME_CLASS(AFighterPlayer)) ||
		toucher->IsKindOf (RUNTIME_CLASS(AMagePlayer)))
	{ // Wrong class, but try to pick up for mana
		if (ShouldStay())
		{ // Can't pick up weapons for other classes in coop netplay
			return false;
		}

		bool gaveSome = (NULL != AddAmmo (toucher, AmmoType1, AmmoGive1));
		gaveSome |= (NULL != AddAmmo (toucher, AmmoType2, AmmoGive2));
		if (gaveSome)
		{
			GoAwayAndDie ();
		}
		return gaveSome;
	}
	return Super::TryPickup (toucher);
}
