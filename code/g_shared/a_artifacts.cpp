#include "info.h"
#include "a_pickups.h"
#include "d_player.h"
#include "p_local.h"
#include "c_dispatch.h"
#include "gi.h"
#include "s_sound.h"
#include "m_random.h"
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
	 0,				// shield
	 0,				// health2
	-SPEEDTICS,		// speed
	 MAULATORTICS	// minotaur
};

static const char *ArtifactNames[NUMARTIFACTS] =
{
	"none",
	"invulnerability",
	"invisibility",
	"health",
	"superhealth",
	"tomeofpower",
	"healingradius",
	"summon",
	"torch",
	"firebomb",
	"egg",
	"fly",
	"blastradius",
	"poisonbag",
	"teleportother",
	"speed",
	"boostmana",
	"boostarmor",
	"teleport",
	// Puzzle artifacts
	"puzzskull",
	"puzzgembig",
	"puzzgemred",
	"puzzgemgreen1",
	"puzzgemgreen2",
	"puzzgemblue1",
	"puzzgemblue2",
	"puzzbook1",
	"puzzbook2",
	"puzzskull2",
	"puzzfweapon",
	"puzzcweapon",
	"puzzmweapon",
	"puzzgear1",
	"puzzgear2",
	"puzzgear3",
	"puzzgear4"
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
	if (PowerTics[power] < 0 && player->powers[power] > BLINKTHRESHOLD)
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
			AddCommandString (cmd);
		}
	}
	else
	{
		if (arti >= arti_firstpuzzitem && multiplayer && !*deathmatch)
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

//===========================================================================
//
// Teleport other stuff
//
//===========================================================================

#define TELEPORT_LIFE 1

void A_TeloSpawnA (AActor *);
void A_TeloSpawnB (AActor *);
void A_TeloSpawnC (AActor *);
void A_TeloSpawnD (AActor *);
void A_CheckTeleRing (AActor *);

// ATelOtherFX? -------------------------------------------------------------

class ATelOtherFX1 : public AActor
{
	DECLARE_ACTOR (ATelOtherFX1, AActor)
};

FState ATelOtherFX1::States[] =
{
#define S_TELO_FX1 0
	S_BRIGHT (TRNG, 'E',    5, NULL                         , &States[S_TELO_FX1+1]),
	S_BRIGHT (TRNG, 'D',    4, NULL                         , &States[S_TELO_FX1+2]),
	S_BRIGHT (TRNG, 'C',    3, A_TeloSpawnC                 , &States[S_TELO_FX1+3]),
	S_BRIGHT (TRNG, 'B',    3, A_TeloSpawnB                 , &States[S_TELO_FX1+4]),
	S_BRIGHT (TRNG, 'A',    3, A_TeloSpawnA                 , &States[S_TELO_FX1+5]),
	S_BRIGHT (TRNG, 'B',    3, A_TeloSpawnB                 , &States[S_TELO_FX1+6]),
	S_BRIGHT (TRNG, 'C',    3, A_TeloSpawnC                 , &States[S_TELO_FX1+7]),
	S_BRIGHT (TRNG, 'D',    3, A_TeloSpawnD                 , &States[S_TELO_FX1+2]),

#define S_TELO_FX_DONE (S_TELO_FX1+8)
	S_BRIGHT (TRNG, 'E',    3, NULL                         , NULL),

#define S_TELO_FX2 (S_TELO_FX_DONE+1)
	S_BRIGHT (TRNG, 'B',    4, NULL                         , &States[S_TELO_FX2+1]),
	S_BRIGHT (TRNG, 'C',    4, NULL                         , &States[S_TELO_FX2+2]),
	S_BRIGHT (TRNG, 'D',    4, NULL                         , &States[S_TELO_FX2+3]),
	S_BRIGHT (TRNG, 'C',    4, NULL                         , &States[S_TELO_FX2+4]),
	S_BRIGHT (TRNG, 'B',    4, NULL                         , &States[S_TELO_FX2+5]),
	S_BRIGHT (TRNG, 'A',    4, A_CheckTeleRing              , &States[S_TELO_FX2+0]),

#define S_TELO_FX3 (S_TELO_FX2+6)
	S_BRIGHT (TRNG, 'C',    4, NULL                         , &States[S_TELO_FX3+1]),
	S_BRIGHT (TRNG, 'D',    4, NULL                         , &States[S_TELO_FX3+2]),
	S_BRIGHT (TRNG, 'C',    4, NULL                         , &States[S_TELO_FX3+3]),
	S_BRIGHT (TRNG, 'B',    4, NULL                         , &States[S_TELO_FX3+4]),
	S_BRIGHT (TRNG, 'A',    4, NULL                         , &States[S_TELO_FX3+5]),
	S_BRIGHT (TRNG, 'B',    4, A_CheckTeleRing              , &States[S_TELO_FX3+0]),

#define S_TELO_FX4 (S_TELO_FX3+6)
	S_BRIGHT (TRNG, 'D',    4, NULL                         , &States[S_TELO_FX4+1]),
	S_BRIGHT (TRNG, 'C',    4, NULL                         , &States[S_TELO_FX4+2]),
	S_BRIGHT (TRNG, 'B',    4, NULL                         , &States[S_TELO_FX4+3]),
	S_BRIGHT (TRNG, 'A',    4, NULL                         , &States[S_TELO_FX4+4]),
	S_BRIGHT (TRNG, 'B',    4, NULL                         , &States[S_TELO_FX4+5]),
	S_BRIGHT (TRNG, 'C',    4, A_CheckTeleRing              , &States[S_TELO_FX4+0]),

#define S_TELO_FX5 (S_TELO_FX4+6)
	S_BRIGHT (TRNG, 'C',    4, NULL                         , &States[S_TELO_FX5+1]),
	S_BRIGHT (TRNG, 'B',    4, NULL                         , &States[S_TELO_FX5+2]),
	S_BRIGHT (TRNG, 'A',    4, NULL                         , &States[S_TELO_FX5+3]),
	S_BRIGHT (TRNG, 'B',    4, NULL                         , &States[S_TELO_FX5+4]),
	S_BRIGHT (TRNG, 'C',    4, NULL                         , &States[S_TELO_FX5+5]),
	S_BRIGHT (TRNG, 'D',    4, A_CheckTeleRing              , &States[S_TELO_FX5+0])
};

IMPLEMENT_ACTOR (ATelOtherFX1, Any, -1, 0)
	PROP_DamageLong (10001)
	PROP_Flags (MF_MISSILE|MF_DROPOFF|MF_NOGRAVITY|MF_NOBLOCKMAP)
	PROP_Flags2 (MF2_NOTELEPORT)

	PROP_SpawnState (S_TELO_FX1)
	PROP_DeathState (S_TELO_FX_DONE)
END_DEFAULTS

class ATelOtherFX2 : public ATelOtherFX1
{
	DECLARE_STATELESS_ACTOR (ATelOtherFX2, ATelOtherFX1)
};

IMPLEMENT_STATELESS_ACTOR (ATelOtherFX2, Any, -1, 0)
	PROP_SpawnState (S_TELO_FX2)
END_DEFAULTS

class ATelOtherFX3 : public ATelOtherFX1
{
	DECLARE_STATELESS_ACTOR (ATelOtherFX3, ATelOtherFX1)
};

IMPLEMENT_STATELESS_ACTOR (ATelOtherFX3, Any, -1, 0)
	PROP_SpawnState (S_TELO_FX3)
END_DEFAULTS

class ATelOtherFX4 : public ATelOtherFX1
{
	DECLARE_STATELESS_ACTOR (ATelOtherFX4, ATelOtherFX1)
};

IMPLEMENT_STATELESS_ACTOR (ATelOtherFX4, Any, -1, 0)
	PROP_SpawnState (S_TELO_FX4)
END_DEFAULTS

class ATelOtherFX5 : public ATelOtherFX1
{
	DECLARE_STATELESS_ACTOR (ATelOtherFX5, ATelOtherFX1)
};

IMPLEMENT_STATELESS_ACTOR (ATelOtherFX5, Any, -1, 0)
	PROP_SpawnState (S_TELO_FX5)
END_DEFAULTS

static void TeloSpawn (AActor *source, const TypeInfo *type)
{
	AActor *fx;

	fx = Spawn (type, source->x, source->y, source->z);
	if (fx)
	{
		fx->special1 = TELEPORT_LIFE;			// Lifetime countdown
		fx->angle = source->angle;
		fx->target = source->target;
		fx->momx = source->momx >> 1;
		fx->momy = source->momy >> 1;
		fx->momz = source->momz >> 1;
	}
}

void A_TeloSpawnA (AActor *actor)
{
	TeloSpawn (actor, RUNTIME_CLASS(ATelOtherFX2));
}

void A_TeloSpawnB (AActor *actor)
{
	TeloSpawn (actor, RUNTIME_CLASS(ATelOtherFX3));
}

void A_TeloSpawnC (AActor *actor)
{
	TeloSpawn (actor, RUNTIME_CLASS(ATelOtherFX4));
}

void A_TeloSpawnD (AActor *actor)
{
	TeloSpawn (actor, RUNTIME_CLASS(ATelOtherFX5));
}

void A_CheckTeleRing (AActor *actor)
{
	if (actor->special1-- <= 0)
	{
		actor->SetState (actor->DeathState);
	}
}

//----------------------------------------------------------------------------
//
// PROC P_ArtiTeleportOther
//
//----------------------------------------------------------------------------

void P_ArtiTeleportOther (player_t *player)
{
	AActor *mo;

	mo = P_SpawnPlayerMissile (player->mo, RUNTIME_CLASS(ATelOtherFX1));
	if (mo)
	{
		mo->target = player->mo;
	}
}


void P_TeleportToPlayerStarts (AActor *victim)
{
	int i,selections=0;
	fixed_t destX,destY;
	angle_t destAngle;

	for (i = 0; i < MAXPLAYERS;i++)
	{
	    if (!playeringame[i]) continue;
		selections++;
	}
	i = P_Random() % selections;
	destX = playerstarts[i].x << FRACBITS;
	destY = playerstarts[i].y << FRACBITS;
	destAngle = ANG45 * (playerstarts[i].angle/45);
	P_Teleport (victim, destX, destY, destAngle, ONFLOORZ, true);
}

void P_TeleportToDeathmatchStarts (AActor *victim)
{
	int i,selections;
	fixed_t destX,destY;
	angle_t destAngle;

	selections = deathmatchstarts.Size ();
	if (selections)
	{
		i = P_Random() % selections;
		destX = deathmatchstarts[i].x << FRACBITS;
		destY = deathmatchstarts[i].y << FRACBITS;
		destAngle = ANG45 * (deathmatchstarts[i].angle/45);
		P_Teleport (victim, destX, destY, destAngle, ONFLOORZ, true);
	}
	else
	{
	 	P_TeleportToPlayerStarts (victim);
	}
}



//----------------------------------------------------------------------------
//
// PROC P_TeleportOther
//
//----------------------------------------------------------------------------
void P_TeleportOther (AActor *victim)
{
	if (victim->player)
	{
		if (*deathmatch)
			P_TeleportToDeathmatchStarts (victim);
		else
			P_TeleportToPlayerStarts (victim);
	}
	else
	{
		// If death action, run it upon teleport
		if (victim->flags & MF_COUNTKILL && victim->special)
		{
			victim->RemoveFromHash ();

			LineSpecials[victim->special] (NULL, victim,
				victim->args[0], victim->args[1], victim->args[2],
				victim->args[3], victim->args[4]);
			victim->special = 0;
		}

		// Send all monsters to deathmatch spots
		P_TeleportToDeathmatchStarts (victim);
	}
}



#define BLAST_RADIUS_DIST	255*FRACUNIT
#define BLAST_SPEED			20*FRACUNIT
#define BLAST_FULLSTRENGTH	255

class ABlastEffect : public AActor
{
	DECLARE_ACTOR (ABlastEffect, AActor)
};

FState ABlastEffect::States[] =
{
	S_NORMAL (RADE, 'A',    4, NULL                         , &States[1]),
	S_NORMAL (RADE, 'B',    4, NULL                         , &States[2]),
	S_NORMAL (RADE, 'C',    4, NULL                         , &States[3]),
	S_NORMAL (RADE, 'D',    4, NULL                         , &States[4]),
	S_NORMAL (RADE, 'E',    4, NULL                         , &States[5]),
	S_NORMAL (RADE, 'F',    4, NULL                         , &States[6]),
	S_NORMAL (RADE, 'G',    4, NULL                         , &States[7]),
	S_NORMAL (RADE, 'H',    4, NULL                         , &States[8]),
	S_NORMAL (RADE, 'I',    4, NULL                         , NULL)
};

IMPLEMENT_ACTOR (ABlastEffect, Any, -1, 0)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY|MF_NOCLIP)
	PROP_Flags2 (MF2_NOTELEPORT)
	PROP_RenderStyle (STYLE_Translucent)
	PROP_Alpha (TRANSLUC66)

	PROP_SpawnState (0)
END_DEFAULTS

void ResetBlasted (AActor *mo)
{
	mo->flags2 &= ~MF2_BLASTED;
	if (!(mo->flags & MF_ICECORPSE))
	{
		mo->flags2 &= ~MF2_SLIDE;
	}
}

void P_BlastMobj (AActor *source, AActor *victim, fixed_t strength)
{
	angle_t angle,ang;
	AActor *mo;
	fixed_t x,y,z;

	angle = R_PointToAngle2 (source->x, source->y, victim->x, victim->y);
	angle >>= ANGLETOFINESHIFT;
	if (strength < BLAST_FULLSTRENGTH)
	{
		victim->momx = FixedMul (strength, finecosine[angle]);
		victim->momy = FixedMul (strength, finesine[angle]);
		if (victim->player)
		{
			// Players handled automatically
		}
		else
		{
			victim->flags2 |= MF2_SLIDE;
			victim->flags2 |= MF2_BLASTED;
		}
	}
	else	// full strength blast from artifact
	{
		if (victim->flags & MF_MISSILE)
		{
#if 0
			if (victim->IsKindOf (RUNTIME_CLASS(ASorcererBall)))
			{ // don't blast sorcerer balls
				return;
			}
			else if (victim->IsKindOf (RUNTIME_CLASS(AMageStaffFX2)))
			{ // Reflect to originator
				victim->special1 = (int)victim->target;	
				victim->target = source;
			}
#endif
		}
#if 0
		if (victim->IsKindOf (RUNTIME_CLASS(AHolyFX)))
		{
			if ((AActor *)(victim->special1) == source)
			{
				victim->special1 = (int)victim->target;	
				victim->target = source;
			}
		}
#endif
		victim->momx = FixedMul (BLAST_SPEED, finecosine[angle]);
		victim->momy = FixedMul (BLAST_SPEED, finesine[angle]);

		// Spawn blast puff
		ang = R_PointToAngle2 (victim->x, victim->y, source->x, source->y);
		ang >>= ANGLETOFINESHIFT;
		x = victim->x + FixedMul (victim->radius+FRACUNIT, finecosine[ang]);
		y = victim->y + FixedMul (victim->radius+FRACUNIT, finesine[ang]);
		z = victim->z - victim->floorclip + (victim->height>>1);
		mo = Spawn<ABlastEffect> (x, y, z);
		if (mo)
		{
			mo->momx = victim->momx;
			mo->momy = victim->momy;
		}

		if (victim->flags & MF_MISSILE)
		{
			victim->momz = 8*FRACUNIT;
			mo->momz = victim->momz;
		}
		else
		{
			victim->momz = (1000 / victim->Mass) << FRACBITS;
		}
		if (victim->player)
		{
			// Players handled automatically
		}
		else
		{
			victim->flags2 |= MF2_SLIDE;
			victim->flags2 |= MF2_BLASTED;
		}
	}
}


// Blast all mobj things away
void P_BlastRadius (player_t *player)
{
	AActor *pmo = player->mo;
	AActor *mo;
	TThinkerIterator<AActor> iterator;
	fixed_t dist;

	S_Sound (pmo, CHAN_AUTO, "SFX_ARTIFACT_BLAST", 1, ATTN_NORM);
	P_NoiseAlert (player->mo, player->mo);

	while ( (mo = iterator.Next ()) )
	{
		if ((mo == pmo) || (mo->flags2 & MF2_BOSS))
		{ // Not a valid monster
			continue;
		}
		if ((mo->flags & MF_ICECORPSE) || (mo->flags3 & MF3_CANBLAST))
		{
			// Let these special cases go
		}
		else if ((mo->flags & MF_COUNTKILL) && (mo->health <= 0))
		{
			continue;
		}
		else if (!(mo->flags & MF_COUNTKILL) &&
			!(mo->player) &&
			!(mo->flags & MF_MISSILE) &&
			!(mo->flags3 & MF3_CANBLAST))
		{	// Must be monster, player, or missile
			continue;
		}
		if (mo->flags2 & MF2_DORMANT)
		{
			continue;		// no dormant creatures
		}
		dist = P_AproxDistance (pmo->x - mo->x, pmo->y - mo->y);
		if (dist > BLAST_RADIUS_DIST)
		{ // Out of range
			continue;
		}
		P_BlastMobj (pmo, mo, BLAST_FULLSTRENGTH);
	}
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
			AddCommandString ("invnext -");
		}
	}
}

//----------------------------------------------------------------------------
//
// PROC P_PlayerUseArtifact
//
//----------------------------------------------------------------------------

void P_PlayerUseArtifact (player_t *player, artitype_t arti)
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
	}
	else if (arti < arti_firstpuzzitem)
	{ // Unable to use artifact, advance pointer
		if (player == &players[consoleplayer])
		{
			AddCommandString ("invnext -");
		}
	}
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

	case arti_blastradius:
		P_BlastRadius (player);
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
