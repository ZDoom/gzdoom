#include "info.h"
#include "a_pickups.h"
#include "d_player.h"
#include "p_local.h"
#include "c_dispatch.h"
#include "gi.h"
#include "s_sound.h"
#include "p_local.h"
#include "p_spec.h"
#include "p_lnspec.h"
#include "p_enemy.h"
#include "p_effect.h"
#include "a_artifacts.h"
#include "sbar.h"

#define	INVULNTICS (30*TICRATE)
#define	INVISTICS (60*TICRATE)
#define	INFRATICS (120*TICRATE)
#define	IRONTICS (60*TICRATE)
#define WPNLEV2TICS (40*TICRATE)
#define FLIGHTTICS (60*TICRATE)
#define SPEEDTICS (45*TICRATE)
#define MAULATORTICS (25*TICRATE)

EXTERN_CVAR (Bool, r_drawfuzz);

bool (*ArtiDispatch[NUMARTIFACTS]) (player_t *, artitype_t);
const char *ArtiPics[NUMARTIFACTS];

IMPLEMENT_ABSTRACT_ACTOR (APowerup)

static int PowerTics[NUMPOWERS] =
{
	-INVULNTICS,	// invulnerability
	 1,				// strength
	-INVISTICS,		// invisibility
	 IRONTICS,		// ironfeet
	 0,				// allmap
	-INFRATICS,		// infrared
	-WPNLEV2TICS,	// weaponlevel2
	-FLIGHTTICS,	// flight
	 0,				// shield		// [RH] shield and health2 don't appear to be
	 0,				// health2		// used for anything.
	-SPEEDTICS,		// speed
	 MAULATORTICS	// minotaur
};

static const char *ArtifactNames[NUMARTIFACTS] =
{
	"none",
	"ArtiInvulnerability",
	"ArtiInvisibility",
	"ArtiHealth",
	"ArtiSuperHealth",
	"ArtiTomeOfPower",
	"ArtiHealingRadius",
	"ArtiDarkServant",
	"ArtiTorch",
	"ArtiTimeBomb",
	"ArtiEgg",
	"ArtiPork",
	"ArtiFly",
	"ArtiBlastRadius",
	"ArtiPoisonBag1",
	"ArtiPoisonBag2",
	"ArtiPoisonBag3",
	"ArtiTeleportOther",
	"ArtiSpeedBoots",
	"ArtiBoostMana",
	"ArtiBoostArmor",
	"ArtiTeleport",
	// Puzzle artifacts
	"PuzzSkull",
	"PuzzGemBig",
	"PuzzGemRed",
	"PuzzGemGreen1",
	"PuzzGemGreen2",
	"PuzzGemBlue1",
	"PuzzGemBlue2",
	"PuzzBook1",
	"PuzzBook2",
	"PuzzSkull2",
	"PuzzFWeapon",
	"PuzzCWeapon",
	"PuzzMWeapon",
	"PuzzGear1",
	"PuzzGear2",
	"PuzzGear3",
	"PuzzGear4"
};

//---------------------------------------------------------------------------
//
// FUNC P_GivePower
//
// Returns true if power accepted.
//
//---------------------------------------------------------------------------

bool P_GivePower (player_t *player, powertype_t power)
{
	if (power >= NUMPOWERS)
	{
		return false;
	}
	if (PowerTics[power] == 0)
	{
		if (player->powers[power])
		{
			return false; // already got it
		}
		player->powers[power] = 1;
		return true;
	}
	if (gameinfo.gametype != GAME_Doom && PowerTics[power] < 0 && player->powers[power] > BLINKTHRESHOLD)
	{ // Already have it
		return false;
	}
	player->powers[power] = abs (PowerTics[power]);
	if (power == pw_flight)
	{
		player->mo->flags2 |= MF2_FLY;
		player->mo->flags |= MF_NOGRAVITY;
		if (player->mo->z <= player->mo->floorz)
		{
			player->mo->momz = 4*FRACUNIT; // thrust the player in the air a bit
		}
	}
	else if (power == pw_strength)
	{
		P_GiveBody (player, 100);
	}
	else if (power == pw_invisibility)
	{
		player->mo->flags |= MF_SHADOW;
		if (gameinfo.gametype == GAME_Heretic)
		{
			player->mo->flags3 |= MF3_GHOST;
			player->mo->alpha = HR_SHADOW;
			player->mo->RenderStyle = STYLE_Translucent;
		}
		else if (gameinfo.gametype == GAME_Doom)
		{
			player->mo->alpha = FRACUNIT/5;
			player->mo->RenderStyle = STYLE_OptFuzzy;
		}
	}
	else if (power == pw_invulnerability)
	{
		player->mo->effects &= ~FX_RESPAWNINVUL;
	}
	return true;
}

//---------------------------------------------------------------------------
//
// FUNC P_GiveArtifact
//
// Returns true if artifact accepted.
//
//---------------------------------------------------------------------------

bool P_GiveArtifact (player_t *player, artitype_t arti)
{
	if (player->inventory[arti] == 0)
	{
		player->inventory[arti] = 1;
		if (player->artifactCount == 0 && player == &players[consoleplayer])
		{
			char cmd[32];

			sprintf (cmd, "select %s", ArtifactNames[arti]);
			C_DoCommand (cmd);
		}
	}
	else
	{
		if (arti >= arti_firstpuzzitem && multiplayer && !deathmatch)
		{ // Can't carry more than 1 puzzle item in coop netplay
			return false;
		}
		if (player->inventory[arti] >= 
			(gameinfo.gametype == GAME_Heretic ? 16 : 25))
		{ // Player already has max (16 or 25) of this item
			return false;
		}
		player->inventory[arti]++;
	}
	player->artifactCount++;
	return true;
}

//---------------------------------------------------------------------------
//
// FUNC P_NextInventory
//
//---------------------------------------------------------------------------

artitype_t P_NextInventory (player_t *player, artitype_t arti)
{
	int i;

	for (i = arti + 1; i != arti; i++)
	{
		if (i == NUMINVENTORYSLOTS)
		{
			if (arti == arti_none)
				return arti_none;
			else
				i = 0;
		}
		if (player->inventory[i] > 0)
			return (artitype_t)i;
	}
	return arti;
}

//---------------------------------------------------------------------------
//
// FUNC P_PrevInventory
//
//---------------------------------------------------------------------------

artitype_t P_PrevInventory (player_t *player, artitype_t arti)
{
	int i;

	for (i = arti - 1; i != arti; i--)
	{
		if (i <= 0)
			i = NUMINVENTORYSLOTS - 1;
		if (player->inventory[i] > 0)
			return (artitype_t)i;
	}
	return arti;
}

//---------------------------------------------------------------------------
//
// FUNC P_FindNamedInventory
//
//---------------------------------------------------------------------------

artitype_t P_FindNamedInventory (const char *name)
{
	int i;

	for (i = 0; i < NUMARTIFACTS; i++)
	{
		if (stricmp (name, ArtifactNames[i]) == 0)
		{
			return (artitype_t)i;
		}
	}
	return arti_none;
}


#define HEAL_RADIUS_DIST	255*FRACUNIT

// Do class specific effect for everyone in radius
bool P_HealRadius (player_t *player)
{
	AActor *pmo = player->mo;
	TThinkerIterator<APlayerPawn> iterator;
	APlayerPawn *pawn;
	bool effective = false;

	while ( (pawn = iterator.Next ()) )
	{
		if (!pawn->player) continue;
		if (pawn->health <= 0) continue;
		if (HEAL_RADIUS_DIST <=
			P_AproxDistance (pmo->x - pawn->x, pmo->y - pawn->y))
		{ // Out of range
			continue;
		}

		if (player->mo->HealOther (pawn->player))
		{
			effective = true;
			S_Sound (pawn, CHAN_AUTO, "SFX_MYSTICINCANT", 1, ATTN_NORM);
		}
	}
#if 0
	int amount;
		switch (player->class)
		{
			case PCLASS_FIGHTER:		// Radius armor boost
				if ((P_GiveArmor(mo->player, ARMOR_ARMOR, 1)) ||
					(P_GiveArmor(mo->player, ARMOR_SHIELD, 1)) ||
					(P_GiveArmor(mo->player, ARMOR_HELMET, 1)) ||
					(P_GiveArmor(mo->player, ARMOR_AMULET, 1)))
				{
					effective=true;
					S_StartSound(mo, SFX_MYSTICINCANT);
				}
				break;
			case PCLASS_CLERIC:			// Radius heal
				amount = 50 + (P_Random()%50);
				if (P_GiveBody(mo->player, amount))
				{
					effective=true;
					S_StartSound(mo, SFX_MYSTICINCANT);
				}
				break;
			case PCLASS_MAGE:			// Radius mana boost
				amount = 50 + (P_Random()%50);
				if ((P_GiveMana(mo->player, MANA_1, amount)) ||
					(P_GiveMana(mo->player, MANA_2, amount)))
				{
					effective=true;
					S_StartSound(mo, SFX_MYSTICINCANT);
				}
				break;
			case PCLASS_PIG:
			default:
				break;
		}
	}
#endif
	return effective;
}

//----------------------------------------------------------------------------
//
// PROC P_PlayerRemoveArtifact
//
//----------------------------------------------------------------------------

void P_PlayerRemoveArtifact (player_t *player, int slot)
{
	player->artifactCount--;
	if (!(--player->inventory[slot]))
	{ // Used last of a type
		if (player == &players[consoleplayer])
		{
			C_DoCommand ("invnext -");
		}
	}
}

//----------------------------------------------------------------------------
//
// PROC P_PlayerUseArtifact
//
//----------------------------------------------------------------------------

bool P_PlayerUseArtifact (player_t *player, artitype_t arti)
{
	if (P_UseArtifact (player, arti))
	{ // Artifact was used - remove it from inventory
		P_PlayerRemoveArtifact (player, arti);
		if (player == &players[consoleplayer])
		{
			S_Sound (player->mo, CHAN_ITEM,
				arti < arti_firstpuzzitem ? "misc/invuse" : "PuzzleSuccess",
				1, ATTN_NORM);
			StatusBar->FlashArtifact (arti);
		}
		return true;
	}
	else if (arti < arti_firstpuzzitem)
	{ // Unable to use artifact, advance pointer
		if (player == &players[consoleplayer])
		{
			C_DoCommand ("invnext -");
		}
	}
	return false;
}

//----------------------------------------------------------------------------
//
// FUNC P_UseArtifact
//
// Returns true if artifact was used.
//
//----------------------------------------------------------------------------

bool P_UseArtifact (player_t *player, artitype_t arti)
{
	if (player->health <= 0)
	{
		return false;
	}
	if (player->inventory[arti] == 0)
	{
		return false;
	}
	if ((unsigned)arti < NUMARTIFACTS)
	{
		return ArtiDispatch[arti] (player, arti);
	}
	return false;
#if 0
	switch (arti)
	{
	case arti_summon:
		mo = P_SpawnPlayerMissile (player->mo, RUNTIME_CLASS(ASummonFX));
		if (mo)
		{
			mo->target = player->mo;
			mo->special1 = (long)(player->mo);
			mo->momz = 5*FRACUNIT;
		}
		break;

	case arti_teleportother:
		P_ArtiTeleportOther (player);
		break;

	case arti_poisonbag:
		player->mo->ThrowPoisonBag ();
		break;

	case arti_speed:
		if (!P_GivePower (player, pw_speed))
		{
			return false;
		}
		break;

	case arti_boostmana:
		{
			int i;
			bool success = false;
			for (i = 0; i < NUMAMMO; i++)
			{
				success |= P_GiveAmmo (player, (ammotype_t)i, player->maxammo[i]);
			}
			return success;
		}
		break;

	case arti_boostarmor:
		if (gameinfo.gametype == GAME_Hexen)
		{
			int count = 0;
			int i;

			for (i = 0; i < NUMARMOR; i++)
			{
				count += P_GiveArmor (player, (armortype_t)i, 1); // 1 point per armor
			}
			if (!count)
			{
				return false;
			}
		}
		else
		{
			return false;
		}
		break;

	default:
		if (arti >= arti_firstpuzzitem && arti < NUMARTIFACTS)
		{
			if (P_UsePuzzleItem (player, arti - arti_firstpuzzitem))
			{
				return true;
			}
			else
			{
				//P_SetYellowMessage (player, TXT_USEPUZZLEFAILED, false);
				return false;
			}
			break;
		}
		return false;
	}
#endif
}
