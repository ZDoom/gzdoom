#include "info.h"
#include "a_pickups.h"
#include "a_artifacts.h"
#include "gstrings.h"
#include "p_local.h"
#include "p_enemy.h"
#include "s_sound.h"

#define BLAST_RADIUS_DIST	255*FRACUNIT
#define BLAST_SPEED			20*FRACUNIT
#define BLAST_FULLSTRENGTH	255

// Disc of Repulsion --------------------------------------------------------

class AArtiBlastRadius : public AInventory
{
	DECLARE_ACTOR (AArtiBlastRadius, AInventory)
public:
	bool Use (bool pickup);
protected:
	void BlastActor (AActor *victim, fixed_t strength);
};
FState AArtiBlastRadius::States[] =
{
	S_BRIGHT (BLST, 'A',	4, NULL					    , &States[1]),
	S_BRIGHT (BLST, 'B',	4, NULL					    , &States[2]),
	S_BRIGHT (BLST, 'C',	4, NULL					    , &States[3]),
	S_BRIGHT (BLST, 'D',	4, NULL					    , &States[4]),
	S_BRIGHT (BLST, 'E',	4, NULL					    , &States[5]),
	S_BRIGHT (BLST, 'F',	4, NULL					    , &States[6]),
	S_BRIGHT (BLST, 'G',	4, NULL					    , &States[7]),
	S_BRIGHT (BLST, 'H',	4, NULL					    , &States[0]),
};

IMPLEMENT_ACTOR (AArtiBlastRadius, Hexen, 10110, 74)
	PROP_Flags (MF_SPECIAL)
	PROP_Flags2 (MF2_FLOATBOB)
	PROP_SpawnState (0)
	PROP_Inventory_DefMaxAmount
	PROP_Inventory_PickupFlash (1)
	PROP_Inventory_FlagsSet (IF_INVBAR|IF_FANCYPICKUPSOUND)
	PROP_Inventory_Icon ("ARTIBLST")
	PROP_Inventory_PickupSound ("misc/p_pkup")
	PROP_Inventory_PickupMessage("$TXT_ARTIBLASTRADIUS")
END_DEFAULTS

// Blast Effect -------------------------------------------------------------

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

//==========================================================================
//
// AArtiBlastRadius :: Activate
//
// Blast all actors away
//
//==========================================================================

bool AArtiBlastRadius::Use (bool pickup)
{
	AActor *mo;
	TThinkerIterator<AActor> iterator;
	fixed_t dist;

	S_Sound (Owner, CHAN_AUTO, "BlastRadius", 1, ATTN_NORM);
	P_NoiseAlert (Owner, Owner);

	while ( (mo = iterator.Next ()) )
	{
		if ((mo == Owner) || (mo->flags2 & MF2_BOSS))
		{ // Not a valid monster
			continue;
		}
		if ((mo->flags & MF_ICECORPSE) || (mo->flags3 & MF3_CANBLAST))
		{
			// Let these special cases go
		}
		else if ((mo->flags3 & MF3_ISMONSTER) && (mo->health <= 0))
		{
			continue;
		}
		else if (!(mo->flags3 & MF3_ISMONSTER) &&
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
		if (mo->flags3 & MF3_DONTBLAST)
		{ // A few things that would normally be blastable should not be blasted
			continue;
		}
		dist = P_AproxDistance (Owner->x - mo->x, Owner->y - mo->y);
		if (dist > BLAST_RADIUS_DIST)
		{ // Out of range
			continue;
		}
		BlastActor (mo, BLAST_FULLSTRENGTH);
	}
	return true;
}

//==========================================================================
//
// AArtiBlastRadius :: BlastActor
//
//==========================================================================

void AArtiBlastRadius::BlastActor (AActor *victim, fixed_t strength)
{
	angle_t angle,ang;
	AActor *mo;
	fixed_t x,y,z;

	if (!victim->SpecialBlastHandling (Owner, strength))
	{
		return;
	}

	angle = R_PointToAngle2 (Owner->x, Owner->y, victim->x, victim->y);
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
			victim->flags2 |= MF2_BLASTED;
		}
	}
	else	// full strength blast from artifact
	{
		if (victim->flags & MF_MISSILE)
		{
#if 0
			if (victim->IsKindOf (RUNTIME_CLASS(AMageStaffFX2)))
			{ // Reflect to originator
				victim->special1 = (int)victim->target;	
				victim->target = Owner;
			}
#endif
		}
		victim->momx = FixedMul (BLAST_SPEED, finecosine[angle]);
		victim->momy = FixedMul (BLAST_SPEED, finesine[angle]);

		// Spawn blast puff
		ang = R_PointToAngle2 (victim->x, victim->y, Owner->x, Owner->y);
		ang >>= ANGLETOFINESHIFT;
		x = victim->x + FixedMul (victim->radius+FRACUNIT, finecosine[ang]);
		y = victim->y + FixedMul (victim->radius+FRACUNIT, finesine[ang]);
		z = victim->z - victim->floorclip + (victim->height>>1);
		mo = Spawn<ABlastEffect> (x, y, z, ALLOW_REPLACE);
		if (mo)
		{
			mo->momx = victim->momx;
			mo->momy = victim->momy;
		}

		if (victim->flags & MF_MISSILE)
		{
			// [RH] Floor and ceiling huggers should not be blasted vertically.
			if (!(victim->flags3 & (MF3_FLOORHUGGER|MF3_CEILINGHUGGER)))
			{
				victim->momz = 8*FRACUNIT;
				mo->momz = victim->momz;
			}
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
			victim->flags2 |= MF2_BLASTED;
		}
	}
}
