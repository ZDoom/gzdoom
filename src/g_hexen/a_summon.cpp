#include "info.h"
#include "a_pickups.h"
#include "a_artifacts.h"
#include "gstrings.h"
#include "p_local.h"
#include "p_enemy.h"
#include "s_sound.h"
#include "ravenshared.h"

void A_Summon (AActor *);

// Dark Servant Artifact ----------------------------------------------------

BASIC_ARTI (DarkServant, arti_summon, GStrings(TXT_ARTISUMMON))
	AT_GAME_SET_FRIEND (Summon)
private:
	static bool ActivateArti (player_t *player, artitype_t arti);
};

FState AArtiDarkServant::States[] =
{
#define S_ARTI_SUMMON 0
	S_NORMAL (SUMN, 'A',  350, NULL					    , &States[S_ARTI_SUMMON]),
};

IMPLEMENT_ACTOR (AArtiDarkServant, Hexen, 86, 16)
	PROP_Flags (MF_SPECIAL)
	PROP_Flags2 (MF2_FLOATBOB)

	PROP_SpawnState (S_ARTI_SUMMON)
END_DEFAULTS

AT_GAME_SET (Summon)
{
	ArtiDispatch[arti_summon] = AArtiDarkServant::ActivateArti;
	ArtiPics[arti_summon] = "ARTISUMN";
}

// Summoning Doll -----------------------------------------------------------

class ASummoningDoll : public AActor
{
	DECLARE_ACTOR (ASummoningDoll, AActor)
};

FState ASummoningDoll::States[] =
{
#define S_SUMMON_FX1_1 0
	S_NORMAL (SUMN, 'A',	4, NULL					    , &States[S_SUMMON_FX1_1]),

#define S_SUMMON_FX2_1 (S_SUMMON_FX1_1+1)
	S_NORMAL (SUMN, 'A',	4, NULL					    , &States[S_SUMMON_FX2_1+1]),
	S_NORMAL (SUMN, 'A',	4, NULL					    , &States[S_SUMMON_FX2_1+2]),
	S_NORMAL (SUMN, 'A',	4, A_Summon				    , NULL),
};

IMPLEMENT_ACTOR (ASummoningDoll, Hexen, -1, 0)
	PROP_SpeedFixed (20)
	PROP_Flags (MF_NOBLOCKMAP|MF_DROPOFF|MF_MISSILE)
	PROP_Flags2 (MF2_NOTELEPORT)

	PROP_SpawnState (S_SUMMON_FX1_1)
	PROP_DeathState (S_SUMMON_FX2_1)
END_DEFAULTS

// Minotaur Smoke -----------------------------------------------------------

class AMinotaurSmoke : public AActor
{
	DECLARE_ACTOR (AMinotaurSmoke, AActor)
};

FState AMinotaurSmoke::States[] =
{
	S_NORMAL (MNSM, 'A',	3, NULL					    , &States[1]),
	S_NORMAL (MNSM, 'B',	3, NULL					    , &States[2]),
	S_NORMAL (MNSM, 'C',	3, NULL					    , &States[3]),
	S_NORMAL (MNSM, 'D',	3, NULL					    , &States[4]),
	S_NORMAL (MNSM, 'E',	3, NULL					    , &States[5]),
	S_NORMAL (MNSM, 'F',	3, NULL					    , &States[6]),
	S_NORMAL (MNSM, 'G',	3, NULL					    , &States[7]),
	S_NORMAL (MNSM, 'H',	3, NULL					    , &States[8]),
	S_NORMAL (MNSM, 'I',	3, NULL					    , &States[9]),
	S_NORMAL (MNSM, 'J',	3, NULL					    , &States[10]),
	S_NORMAL (MNSM, 'K',	3, NULL					    , &States[11]),
	S_NORMAL (MNSM, 'L',	3, NULL					    , &States[12]),
	S_NORMAL (MNSM, 'M',	3, NULL					    , &States[13]),
	S_NORMAL (MNSM, 'N',	3, NULL					    , &States[14]),
	S_NORMAL (MNSM, 'O',	3, NULL					    , &States[15]),
	S_NORMAL (MNSM, 'P',	3, NULL					    , &States[16]),
	S_NORMAL (MNSM, 'Q',	3, NULL					    , NULL),
};

IMPLEMENT_ACTOR (AMinotaurSmoke, Hexen, -1, 0)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY)
	PROP_Flags2 (MF2_NOTELEPORT)
	PROP_RenderStyle (STYLE_Translucent)
	PROP_Alpha (HX_SHADOW)

	PROP_SpawnState (0)
END_DEFAULTS


//============================================================================
//
// Activate the summoning artifact
//
//============================================================================

bool AArtiDarkServant::ActivateArti (player_t *player, artitype_t arti)
{
	AActor *mo = P_SpawnPlayerMissile (player->mo, RUNTIME_CLASS(ASummoningDoll));
	if (mo)
	{
		mo->target = player->mo;
		mo->tracer = player->mo;
		mo->momz = 5*FRACUNIT;
	}
	return true;
}

//============================================================================
//
// A_Summon
//
//============================================================================

void A_Summon (AActor *actor)
{
	AMinotaur *mo;

	mo = Spawn<AMinotaurFriend> (actor->x, actor->y, actor->z);
	if (mo)
	{
		if (P_TestMobjLocation(mo) == false || !actor->tracer)
		{ // Didn't fit - change back to artifact
			mo->Destroy ();
			AActor *arti = Spawn<AArtiDarkServant> (actor->x, actor->y, actor->z);
			if (arti) arti->flags |= MF_DROPPED;
			return;
		}

		mo->StartTime = level.time;
		if (actor->tracer->flags & MF_CORPSE)
		{	// Master dead
			mo->tracer = NULL;		// No master
		}
		else
		{
			mo->tracer = actor->tracer;		// Pointer to master
			P_GivePower (actor->tracer->player, pw_minotaur);
		}

		// Make smoke puff
		Spawn<AMinotaurSmoke> (actor->x, actor->y, actor->z);
		S_SoundID (actor, CHAN_VOICE, mo->ActiveSound, 1, ATTN_NORM);
	}
}
