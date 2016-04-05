/*
#include "info.h"
#include "a_pickups.h"
#include "a_artifacts.h"
#include "gstrings.h"
#include "p_local.h"
#include "s_sound.h"
#include "p_lnspec.h"
#include "m_random.h"
#include "thingdef/thingdef.h"
#include "g_level.h"
#include "doomstat.h"
*/

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
	DECLARE_CLASS (AArtiTeleportOther, AInventory)
public:
	bool Use (bool pickup);
};

IMPLEMENT_CLASS (AArtiTeleportOther)

// Teleport Other FX --------------------------------------------------------

class ATelOtherFX1 : public AActor
{
	DECLARE_CLASS (ATelOtherFX1, AActor)
public:
	int DoSpecialDamage (AActor *target, int damage, FName damagetype);
};

IMPLEMENT_CLASS (ATelOtherFX1)

static void TeloSpawn (AActor *source, const char *type)
{
	AActor *fx;

	fx = Spawn (type, source->Pos(), ALLOW_REPLACE);
	if (fx)
	{
		fx->special1 = TELEPORT_LIFE;			// Lifetime countdown
		fx->Angles.Yaw = source->Angles.Yaw;
		fx->target = source->target;
		fx->Vel = source->Vel / 2;
	}
}

DEFINE_ACTION_FUNCTION(AActor, A_TeloSpawnA)
{
	PARAM_ACTION_PROLOGUE;
	TeloSpawn (self, "TelOtherFX2");
	return 0;
}

DEFINE_ACTION_FUNCTION(AActor, A_TeloSpawnB)
{
	PARAM_ACTION_PROLOGUE;
	TeloSpawn (self, "TelOtherFX3");
	return 0;
}

DEFINE_ACTION_FUNCTION(AActor, A_TeloSpawnC)
{
	PARAM_ACTION_PROLOGUE;
	TeloSpawn (self, "TelOtherFX4");
	return 0;
}

DEFINE_ACTION_FUNCTION(AActor, A_TeloSpawnD)
{
	PARAM_ACTION_PROLOGUE;
	TeloSpawn (self, "TelOtherFX5");
	return 0;
}

DEFINE_ACTION_FUNCTION(AActor, A_CheckTeleRing)
{
	PARAM_ACTION_PROLOGUE;

	if (self->special1-- <= 0)
	{
		self->SetState (self->FindState(NAME_Death));
	}
	return 0;
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

int ATelOtherFX1::DoSpecialDamage (AActor *target, int damage, FName damagetype)
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
				P_ExecuteSpecial(target->special, NULL, level.flags & LEVEL_ACTOWNSPECIAL
					? target : (AActor *)(this->target), false, target->args[0], target->args[1],
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
	DVector3 dest;

	FPlayerStart *start = G_PickPlayerStart(0, PPS_FORCERANDOM | PPS_NOBLOCKINGCHECK);
	dest = start->pos;
	dest.Z = ONFLOORZ;
	P_Teleport (victim, dest, (double)start->angle, TELF_SOURCEFOG | TELF_DESTFOG);
}

//===========================================================================
//
// P_TeleportToDeathmatchStarts
//
//===========================================================================

void P_TeleportToDeathmatchStarts (AActor *victim)
{
	unsigned int i, selections;
	DVector3 dest;

	selections = deathmatchstarts.Size ();
	if (selections > 0)
	{
		i = pr_teledm() % selections;
		dest = deathmatchstarts[i].pos;
		dest.Z = ONFLOORZ;
		P_Teleport (victim, dest, (double)deathmatchstarts[i].angle, TELF_SOURCEFOG | TELF_DESTFOG);
	}
	else
	{
	 	P_TeleportToPlayerStarts (victim);
	}
}
