#include "actor.h"
#include "gi.h"
#include "m_random.h"
#include "s_sound.h"
#include "d_player.h"
#include "a_action.h"
#include "p_local.h"

void A_Pain (AActor *);
void A_PlayerScream (AActor *);
void A_XScream (AActor *);
void A_DoomSkinCheck1 (AActor *);
void A_DoomSkinCheck2 (AActor *);

class ADoomPlayer : public APlayerPawn
{
	DECLARE_ACTOR (ADoomPlayer, APlayerPawn)
public:
	void GiveDefaultInventory ();
	int GetMOD ();
};

FState ADoomPlayer::States[] =
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
	S_NORMAL (PLAY, 'G',	4, NULL 						, &States[S_PLAY_PAIN+1]),
	S_NORMAL (PLAY, 'G',	4, A_Pain						, &States[S_PLAY]),

#define S_PLAY_DIE (S_PLAY_PAIN+2)
	S_NORMAL (PLAY, 'H',   10, A_DoomSkinCheck1				, &States[S_PLAY_DIE+1]),
	S_NORMAL (PLAY, 'I',   10, A_PlayerScream				, &States[S_PLAY_DIE+2]),
	S_NORMAL (PLAY, 'J',   10, A_NoBlocking					, &States[S_PLAY_DIE+3]),
	S_NORMAL (PLAY, 'K',   10, NULL 						, &States[S_PLAY_DIE+4]),
	S_NORMAL (PLAY, 'L',   10, NULL 						, &States[S_PLAY_DIE+5]),
	S_NORMAL (PLAY, 'M',   10, NULL 						, &States[S_PLAY_DIE+6]),
	S_NORMAL (PLAY, 'N',   -1, NULL							, NULL),

#define S_PLAY_XDIE (S_PLAY_DIE+7)
	S_NORMAL (PLAY, 'O',	5, A_DoomSkinCheck2				, &States[S_PLAY_XDIE+1]),
	S_NORMAL (PLAY, 'P',	5, A_XScream					, &States[S_PLAY_XDIE+2]),
	S_NORMAL (PLAY, 'Q',	5, A_NoBlocking					, &States[S_PLAY_XDIE+3]),
	S_NORMAL (PLAY, 'R',	5, NULL 						, &States[S_PLAY_XDIE+4]),
	S_NORMAL (PLAY, 'S',	5, NULL 						, &States[S_PLAY_XDIE+5]),
	S_NORMAL (PLAY, 'T',	5, NULL 						, &States[S_PLAY_XDIE+6]),
	S_NORMAL (PLAY, 'U',	5, NULL 						, &States[S_PLAY_XDIE+7]),
	S_NORMAL (PLAY, 'V',	5, NULL 						, &States[S_PLAY_XDIE+8]),
	S_NORMAL (PLAY, 'W',   -1, NULL							, NULL),

#define S_HTIC_DIE (S_PLAY_XDIE+9)
	S_NORMAL (PLAY, 'H',	6, NULL							, &States[S_HTIC_DIE+1]),
	S_NORMAL (PLAY, 'I',	6, A_PlayerScream				, &States[S_HTIC_DIE+2]),
	S_NORMAL (PLAY, 'J',	6, NULL 						, &States[S_HTIC_DIE+3]),
	S_NORMAL (PLAY, 'K',	6, NULL 						, &States[S_HTIC_DIE+4]),
	S_NORMAL (PLAY, 'L',	6, A_NoBlocking 				, &States[S_HTIC_DIE+5]),
	S_NORMAL (PLAY, 'M',	6, NULL 						, &States[S_HTIC_DIE+6]),
	S_NORMAL (PLAY, 'N',	6, NULL 						, &States[S_HTIC_DIE+7]),
	S_NORMAL (PLAY, 'O',	6, NULL 						, &States[S_HTIC_DIE+8]),
	S_NORMAL (PLAY, 'P',   -1, NULL							, NULL),

#define S_HTIC_XDIE (S_HTIC_DIE+9)
	S_NORMAL (PLAY, 'Q',	5, A_PlayerScream				, &States[S_HTIC_XDIE+1]),
	S_NORMAL (PLAY, 'R',	0, A_NoBlocking					, &States[S_HTIC_XDIE+2]),
	S_NORMAL (PLAY, 'R',	5, A_SkullPop					, &States[S_HTIC_XDIE+3]),
	S_NORMAL (PLAY, 'S',	5, NULL			 				, &States[S_HTIC_XDIE+4]),
	S_NORMAL (PLAY, 'T',	5, NULL 						, &States[S_HTIC_XDIE+5]),
	S_NORMAL (PLAY, 'U',	5, NULL 						, &States[S_HTIC_XDIE+6]),
	S_NORMAL (PLAY, 'V',	5, NULL 						, &States[S_HTIC_XDIE+7]),
	S_NORMAL (PLAY, 'W',	5, NULL 						, &States[S_HTIC_XDIE+8]),
	S_NORMAL (PLAY, 'X',	5, NULL 						, &States[S_HTIC_XDIE+9]),
	S_NORMAL (PLAY, 'Y',   -1, NULL							, NULL),
};

IMPLEMENT_ACTOR (ADoomPlayer, Doom, -1, 0)
	PROP_SpawnHealth (100)
	PROP_RadiusFixed (16)
	PROP_HeightFixed (56)
	PROP_Mass (100)
	PROP_PainChance (255)
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

void ADoomPlayer::GiveDefaultInventory ()
{
	player->health = deh.StartHealth;		// [RH] Used to be MAXHEALTH
	player->weaponowned[wp_fist] = true;
	player->weaponowned[wp_pistol] = true;
	player->ammo[am_clip] = deh.StartBullets; // [RH] Used to be 50
	if (deh.StartBullets > 0)
	{
		player->readyweapon = player->pendingweapon = wp_pistol;
	}
	else
	{
		player->readyweapon = player->pendingweapon = wp_fist;
	}
}

int ADoomPlayer::GetMOD ()
{
	switch (player->readyweapon)
	{
	case wp_fist:			return MOD_FIST;
	case wp_pistol:			return MOD_PISTOL;
	case wp_shotgun:		return MOD_SHOTGUN;
	case wp_chaingun:		return MOD_CHAINGUN;
	case wp_supershotgun:	return MOD_SSHOTGUN;
	case wp_chainsaw:		return MOD_CHAINSAW;
	default:				return MOD_UNKNOWN;
	}
}

void A_PlayerScream (AActor *self)
{
	const char *sound;
	int chan = CHAN_VOICE;
	int gametype;

	if (self->player == NULL || self->player->morphTics != 0)
	{
		S_SoundID (self, CHAN_VOICE, self->DeathSound, 1, ATTN_NORM);
		return;
	}

	gametype = skins[self->player->userinfo.skin].game;

	// Handle the different player death screams
	if ((((level.flags >> 15) | (dmflags)) &
		(DF_FORCE_FALLINGZD | DF_FORCE_FALLINGHX)) &&
		self->momz <= -39*FRACUNIT)
	{
		sound = "*splat";
		chan = CHAN_BODY;
	}
	else if (gametype == GAME_Doom)
	{
		if (self->health < -50)
		{ // IF THE PLAYER DIES LESS THAN -50% WITHOUT GIBBING
			sound = "*xdeath";
		}
		else
		{
			sound = "*death";
		}
	}
	else
	{
		if (gametype == GAME_Heretic && self->special1 < 10)
		{ // Wimpy death sound
			sound = "*wimpydeath";
		}
		else if (self->health > -50)
		{ // Normal death sound
			sound = "*death";
		}
		else if (self->health > -100)
		{ // Crazy death sound
			sound = "*crazydeath";
		}
		else
		{ // Extreme death sound
			sound = "*gibbed";
			chan = CHAN_BODY;
		}
	}
	if (chan != CHAN_VOICE)
	{
		for (int i = 0; i < 8; ++i)
		{ // Stop most playing sounds from this player.
		  // This is mainly to stop *land from messing up *splat.
			if (i != CHAN_WEAPON && i != CHAN_VOICE)
			{
				S_StopSound (self, i);
			}
		}
	}
	S_Sound (self, CHAN_VOICE, sound, 1, ATTN_NORM);
}

//==========================================================================
//
// A_DoomSkinCheck1
//
//==========================================================================

void A_DoomSkinCheck1 (AActor *actor)
{
	if (skins[actor->player->userinfo.skin].game != GAME_Doom)
	{
		actor->SetState (&ADoomPlayer::States[S_HTIC_DIE]);
	}
}

//==========================================================================
//
// A_DoomSkinCheck2
//
//==========================================================================

void A_DoomSkinCheck2 (AActor *actor)
{
	if (skins[actor->player->userinfo.skin].game != GAME_Doom)
	{
		actor->SetState (&ADoomPlayer::States[S_HTIC_XDIE]);
	}
}

// Dead marine -------------------------------------------------------------

class ADeadMarine : public AActor
{
	DECLARE_ACTOR (ADeadMarine, AActor)
};

FState ADeadMarine::States[] =
{
	S_NORMAL (PLAY, 'N',   -1, NULL							, NULL)
};

IMPLEMENT_ACTOR (ADeadMarine, Doom, 15, 0)
	PROP_SpawnState (0)
END_DEFAULTS

// Gibbed marine -----------------------------------------------------------

class AGibbedMarine : public AActor
{
	DECLARE_ACTOR (AGibbedMarine, AActor)
};

FState AGibbedMarine::States[] =
{
	S_NORMAL (PLAY, 'W',   -1, NULL							, NULL)
};

IMPLEMENT_ACTOR (AGibbedMarine, Doom, 10, 145)
	PROP_SpawnState (0)
END_DEFAULTS

// Gibbed marine (extra copy) ----------------------------------------------

class AGibbedMarineExtra : public AGibbedMarine
{
	DECLARE_STATELESS_ACTOR (AGibbedMarineExtra, AGibbedMarine)
};

IMPLEMENT_STATELESS_ACTOR (AGibbedMarineExtra, Doom, 12, 0)
END_DEFAULTS
