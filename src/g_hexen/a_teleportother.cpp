#include "info.h"
#include "a_pickups.h"
#include "a_artifacts.h"
#include "gstrings.h"
#include "p_local.h"
#include "p_enemy.h"
#include "s_sound.h"
#include "p_lnspec.h"
#include "m_random.h"

#define TELEPORT_LIFE 1

static FRandom pr_telestarts ("TeleStarts");
static FRandom pr_teledm ("TeleDM");

void A_TeloSpawnA (AActor *);
void A_TeloSpawnB (AActor *);
void A_TeloSpawnC (AActor *);
void A_TeloSpawnD (AActor *);
void A_CheckTeleRing (AActor *);
void P_TeleportToPlayerStarts (AActor *victim);
void P_TeleportToDeathmatchStarts (AActor *victim);

// Teleport Other Artifact --------------------------------------------------

class AArtiTeleportOther : public AInventory
{
	DECLARE_ACTOR (AArtiTeleportOther, AInventory)
public:
	bool Use (bool pickup);
};

FState AArtiTeleportOther::States[] =
{
#define S_ARTI_TELOTHER1 0
	S_NORMAL (TELO, 'A',	5, NULL					    , &States[S_ARTI_TELOTHER1+1]),
	S_NORMAL (TELO, 'B',	5, NULL					    , &States[S_ARTI_TELOTHER1+2]),
	S_NORMAL (TELO, 'C',	5, NULL					    , &States[S_ARTI_TELOTHER1+3]),
	S_NORMAL (TELO, 'D',	5, NULL					    , &States[S_ARTI_TELOTHER1]),
};

IMPLEMENT_ACTOR (AArtiTeleportOther, Hexen, 10040, 17)
	PROP_Flags (MF_SPECIAL|MF_COUNTITEM)
	PROP_Flags2 (MF2_FLOATBOB)
	PROP_SpawnState (S_ARTI_TELOTHER1)
	PROP_Inventory_DefMaxAmount
	PROP_Inventory_PickupFlash (1)
	PROP_Inventory_FlagsSet (IF_INVBAR|IF_FANCYPICKUPSOUND)
	PROP_Inventory_Icon ("ARTITELO")
	PROP_Inventory_PickupSound ("misc/p_pkup")
	PROP_Inventory_PickupMessage("$TXT_ARTITELEPORTOTHER")
END_DEFAULTS

// Teleport Other FX --------------------------------------------------------

class ATelOtherFX1 : public AActor
{
	DECLARE_ACTOR (ATelOtherFX1, AActor)
public:
	int DoSpecialDamage (AActor *target, int damage);
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
	PROP_Flags3 (MF3_BLOODLESSIMPACT)
	PROP_RadiusFixed (16)
	PROP_HeightFixed (16)
	PROP_SpeedFixed (20)

	PROP_SpawnState (S_TELO_FX1)
	PROP_DeathState (S_TELO_FX_DONE)
END_DEFAULTS

class ATelOtherFX2 : public ATelOtherFX1
{
	DECLARE_STATELESS_ACTOR (ATelOtherFX2, ATelOtherFX1)
};

IMPLEMENT_STATELESS_ACTOR (ATelOtherFX2, Any, -1, 0)
	PROP_SpeedFixed (16)
	PROP_SpawnState (S_TELO_FX2)
END_DEFAULTS

class ATelOtherFX3 : public ATelOtherFX1
{
	DECLARE_STATELESS_ACTOR (ATelOtherFX3, ATelOtherFX1)
};

IMPLEMENT_STATELESS_ACTOR (ATelOtherFX3, Any, -1, 0)
	PROP_SpeedFixed (16)
	PROP_SpawnState (S_TELO_FX3)
END_DEFAULTS

class ATelOtherFX4 : public ATelOtherFX1
{
	DECLARE_STATELESS_ACTOR (ATelOtherFX4, ATelOtherFX1)
};

IMPLEMENT_STATELESS_ACTOR (ATelOtherFX4, Any, -1, 0)
	PROP_SpeedFixed (16)
	PROP_SpawnState (S_TELO_FX4)
END_DEFAULTS

class ATelOtherFX5 : public ATelOtherFX1
{
	DECLARE_STATELESS_ACTOR (ATelOtherFX5, ATelOtherFX1)
};

IMPLEMENT_STATELESS_ACTOR (ATelOtherFX5, Any, -1, 0)
	PROP_SpeedFixed (16)
	PROP_SpawnState (S_TELO_FX5)
END_DEFAULTS

static void TeloSpawn (AActor *source, const PClass *type)
{
	AActor *fx;

	fx = Spawn (type, source->x, source->y, source->z, ALLOW_REPLACE);
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
		actor->SetState (actor->FindState(NAME_Death));
	}
}

//===========================================================================
//
// Activate Teleport Other
//
//===========================================================================

bool AArtiTeleportOther::Use (bool pickup)
{
	AActor *mo;

	mo = P_SpawnPlayerMissile (Owner, RUNTIME_CLASS(ATelOtherFX1));
	if (mo)
	{
		mo->target = Owner;
	}
	return true;
}

//===========================================================================
//
// Perform Teleport Other
//
//===========================================================================

int ATelOtherFX1::DoSpecialDamage (AActor *target, int damage)
{
	if ((target->flags3 & MF3_ISMONSTER || target->player != NULL) &&
		!(target->flags2 & MF2_BOSS) &&
		!(target->flags3 & MF3_NOTELEOTHER))
	{
		if (target->player)
		{
			if (deathmatch)
				P_TeleportToDeathmatchStarts (target);
			else
				P_TeleportToPlayerStarts (target);
		}
		else
		{
			// If death action, run it upon teleport
			if (target->flags3 & MF3_ISMONSTER && target->special)
			{
				target->RemoveFromHash ();
				LineSpecials[target->special] (NULL, level.flags & LEVEL_ACTOWNSPECIAL
					? target : this->target, false, target->args[0], target->args[1],
					target->args[2], target->args[3], target->args[4]);
				target->special = 0;
			}

			// Send all monsters to deathmatch spots
			P_TeleportToDeathmatchStarts (target);
		}
	}
	return -1;
}

//===========================================================================
//
// P_TeleportToPlayerStarts
//
//===========================================================================

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
	i = pr_telestarts() % selections;
	destX = playerstarts[i].x << FRACBITS;
	destY = playerstarts[i].y << FRACBITS;
	destAngle = ANG45 * (playerstarts[i].angle/45);
	P_Teleport (victim, destX, destY, ONFLOORZ, destAngle, true, true, false);
}

//===========================================================================
//
// P_TeleportToDeathmatchStarts
//
//===========================================================================

void P_TeleportToDeathmatchStarts (AActor *victim)
{
	unsigned int i, selections;
	fixed_t destX,destY;
	angle_t destAngle;

	selections = deathmatchstarts.Size ();
	if (selections > 0)
	{
		i = pr_teledm() % selections;
		destX = deathmatchstarts[i].x << FRACBITS;
		destY = deathmatchstarts[i].y << FRACBITS;
		destAngle = ANG45 * (deathmatchstarts[i].angle/45);
		P_Teleport (victim, destX, destY, ONFLOORZ, destAngle, true, true, false);
	}
	else
	{
	 	P_TeleportToPlayerStarts (victim);
	}
}
