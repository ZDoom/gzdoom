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

static FRandom pr_manaradius ("ManaRadius");

// The mage -----------------------------------------------------------------

FState AMagePlayer::States[] =
{
#define S_MPLAY 0
	S_NORMAL (MAGE, 'A',   -1, NULL					    , NULL),

#define S_MPLAY_RUN1 (S_MPLAY+1)
	S_NORMAL (MAGE, 'A',	4, NULL					    , &States[S_MPLAY_RUN1+1]),
	S_NORMAL (MAGE, 'B',	4, NULL					    , &States[S_MPLAY_RUN1+2]),
	S_NORMAL (MAGE, 'C',	4, NULL					    , &States[S_MPLAY_RUN1+3]),
	S_NORMAL (MAGE, 'D',	4, NULL					    , &States[S_MPLAY_RUN1]),

#define S_MPLAY_PAIN (S_MPLAY_RUN1+4)
	S_NORMAL (MAGE, 'G',	4, NULL					    , &States[S_MPLAY_PAIN+1]),
	S_NORMAL (MAGE, 'G',	4, A_Pain				    , &States[S_MPLAY]),

#define S_MPLAY_ATK1 (S_MPLAY_PAIN+2)
	S_NORMAL (MAGE, 'E',	8, NULL					    , &States[S_MPLAY_ATK1+1]),
	S_BRIGHT (MAGE, 'F',	8, NULL					    , &States[S_MPLAY]),

#define S_MPLAY_DIE1 (S_MPLAY_ATK1+2)
	S_NORMAL (MAGE, 'H',	6, NULL					    , &States[S_MPLAY_DIE1+1]),
	S_NORMAL (MAGE, 'I',	6, A_Scream				    , &States[S_MPLAY_DIE1+2]),
	S_NORMAL (MAGE, 'J',	6, NULL					    , &States[S_MPLAY_DIE1+3]),
	S_NORMAL (MAGE, 'K',	6, NULL					    , &States[S_MPLAY_DIE1+4]),
	S_NORMAL (MAGE, 'L',	6, A_NoBlocking			    , &States[S_MPLAY_DIE1+5]),
	S_NORMAL (MAGE, 'M',	6, NULL					    , &States[S_MPLAY_DIE1+6]),
	S_NORMAL (MAGE, 'N',   -1, NULL/*A_AddPlayerCorpse*/, NULL),

#define S_MPLAY_XDIE1 (S_MPLAY_DIE1+7)
	S_NORMAL (MAGE, 'O',	5, A_Scream				    , &States[S_MPLAY_XDIE1+1]),
	S_NORMAL (MAGE, 'P',	5, NULL					    , &States[S_MPLAY_XDIE1+2]),
	S_NORMAL (MAGE, 'R',	5, A_NoBlocking			    , &States[S_MPLAY_XDIE1+3]),
	S_NORMAL (MAGE, 'S',	5, NULL					    , &States[S_MPLAY_XDIE1+4]),
	S_NORMAL (MAGE, 'T',	5, NULL					    , &States[S_MPLAY_XDIE1+5]),
	S_NORMAL (MAGE, 'U',	5, NULL					    , &States[S_MPLAY_XDIE1+6]),
	S_NORMAL (MAGE, 'V',	5, NULL					    , &States[S_MPLAY_XDIE1+7]),
	S_NORMAL (MAGE, 'W',	5, NULL					    , &States[S_MPLAY_XDIE1+8]),
	S_NORMAL (MAGE, 'X',   -1, NULL/*A_AddPlayerCorpse*/, NULL),

#define S_MPLAY_ICE (S_MPLAY_XDIE1+9)
	S_NORMAL (MAGE, 'Y',	5, A_FreezeDeath		    , &States[S_MPLAY_ICE+1]),
	S_NORMAL (MAGE, 'Y',	1, A_FreezeDeathChunks	    , &States[S_MPLAY_ICE+1]),

#define S_PLAY_M_FDTH (S_MPLAY_ICE+2)
	S_BRIGHT (FDTH, 'E',	5, NULL					    , &States[S_PLAY_M_FDTH+1]),
	S_BRIGHT (FDTH, 'F',	4, NULL					    , &States[S_PLAY_M_FDTH+2]),

#define S_PLAY_FDTH (S_PLAY_M_FDTH+2)
	S_BRIGHT (FDTH, 'G',	5, A_FireScream				, &States[S_PLAY_FDTH+1]),
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

IMPLEMENT_ACTOR (AMagePlayer, Hexen, -1, 0)
	PROP_SpawnHealth (100)
	PROP_ReactionTime (0)
	PROP_PainChance (255)
	PROP_RadiusFixed (16)
	PROP_HeightFixed (64)
	PROP_SpeedFixed (1)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_DROPOFF|MF_PICKUP|MF_NOTDMATCH|MF_FRIENDLY)
	PROP_Flags2 (MF2_WINDTHRUST|MF2_FLOORCLIP|MF2_SLIDE|MF2_PASSMOBJ|MF2_TELESTOMP|MF2_PUSHWALL)
	PROP_Flags3 (MF3_NOBLOCKMONST)
	PROP_Flags4 (MF4_NOSKIN)

	PROP_SpawnState (S_MPLAY)
	PROP_SeeState (S_MPLAY_RUN1)
	PROP_PainState (S_MPLAY_PAIN)
	PROP_MissileState (S_MPLAY_ATK1)
	PROP_DeathState (S_MPLAY_DIE1)
	PROP_XDeathState (S_MPLAY_XDIE1)
	PROP_BDeathState (S_PLAY_M_FDTH)
	PROP_IDeathState (S_MPLAY_ICE)

	PROP_PainSound ("PlayerMagePain")
END_DEFAULTS

const char *AMagePlayer::GetSoundClass ()
{
	if (player == NULL || player->userinfo.skin == 0)
	{
		return "mage";
	}
	return Super::GetSoundClass ();
}
void AMagePlayer::PlayAttacking2 ()
{
	SetState (MissileState);
}

void AMagePlayer::GiveDefaultInventory ()
{
	player->health = GetDefault()->health;
	player->ReadyWeapon = player->PendingWeapon = static_cast<AWeapon *>
		(GiveInventoryType (TypeInfo::FindType ("MWeapWand")));
	
	GiveInventoryType (RUNTIME_CLASS(AHexenArmor));
	AHexenArmor *armor = FindInventory<AHexenArmor>();
	armor->Slots[4] = 5*FRACUNIT;
	armor->SlotsIncrement[0] =  5*FRACUNIT;
	armor->SlotsIncrement[1] = 15*FRACUNIT;
	armor->SlotsIncrement[2] = 10*FRACUNIT;
	armor->SlotsIncrement[3] = 25*FRACUNIT;
}

void AMagePlayer::TweakSpeeds (int &forward, int &side)
{
	if ((unsigned int)(forward + 0x31ff) < 0x63ff)
	{
		forward = forward * 0x16 / 0x19;
	}
	else
	{
		forward = forward * 0x2e / 0x32;
	}
	if ((unsigned int)(side + 0x27ff) < 0x4fff)
	{
		side = side * 0x15 / 0x18;
	}
	else
	{
		side = side * 0x25 / 0x28;
	}
	Super::TweakSpeeds (forward, side);
}

fixed_t AMagePlayer::GetJumpZ ()
{
	return FRACUNIT*39/4;	// ~9.75
}

// Radius mana boost
bool AMagePlayer::DoHealingRadius (APlayerPawn *other)
{
	int amount = 50 + (pr_manaradius() % 50);

	if (GiveAmmo (RUNTIME_CLASS(AMana1), amount) ||
		GiveAmmo (RUNTIME_CLASS(AMana2), amount))
	{
		S_Sound (other, CHAN_AUTO, "MysticIncant", 1, ATTN_NORM);
		return true;
	}
	return false;
}

void AMagePlayer::SpecialInvulnerabilityHandling (EInvulState state)
{
	if (state == INVUL_Start)
	{
		flags2 |= MF2_REFLECTIVE;
	}
	else if (state == INVUL_Stop)
	{
		flags2 &= ~MF2_REFLECTIVE;
	}
}

// Mage Weapon Base Class ---------------------------------------------------

IMPLEMENT_ABSTRACT_ACTOR (AMageWeapon)

bool AMageWeapon::TryPickup (AActor *toucher)
{
	// The Doom and Hexen players are not excluded from pickup in case
	// somebody wants to use these weapons with either of those games.
	if (toucher->IsKindOf (RUNTIME_CLASS(AFighterPlayer)) ||
		toucher->IsKindOf (RUNTIME_CLASS(AClericPlayer)))
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
