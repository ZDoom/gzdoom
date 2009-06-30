/*
#include "info.h"
#include "a_pickups.h"
#include "a_artifacts.h"
#include "gstrings.h"
#include "p_local.h"
#include "p_enemy.h"
#include "s_sound.h"
*/

#define BLAST_RADIUS_DIST	255*FRACUNIT
#define BLAST_SPEED			20*FRACUNIT
#define BLAST_FULLSTRENGTH	255

// Disc of Repulsion --------------------------------------------------------

class AArtiBlastRadius : public AInventory
{
	DECLARE_CLASS (AArtiBlastRadius, AInventory)
public:
	bool Use (bool pickup);
protected:
	void BlastActor (AActor *victim, fixed_t strength);
};

IMPLEMENT_CLASS (AArtiBlastRadius)

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
		victim->velx = FixedMul (strength, finecosine[angle]);
		victim->vely = FixedMul (strength, finesine[angle]);
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
		victim->velx = FixedMul (BLAST_SPEED, finecosine[angle]);
		victim->vely = FixedMul (BLAST_SPEED, finesine[angle]);

		// Spawn blast puff
		ang = R_PointToAngle2 (victim->x, victim->y, Owner->x, Owner->y);
		ang >>= ANGLETOFINESHIFT;
		x = victim->x + FixedMul (victim->radius+FRACUNIT, finecosine[ang]);
		y = victim->y + FixedMul (victim->radius+FRACUNIT, finesine[ang]);
		z = victim->z - victim->floorclip + (victim->height>>1);
		mo = Spawn ("BlastEffect", x, y, z, ALLOW_REPLACE);
		if (mo)
		{
			mo->velx = victim->velx;
			mo->vely = victim->vely;
		}

		if (victim->flags & MF_MISSILE)
		{
			// [RH] Floor and ceiling huggers should not be blasted vertically.
			if (!(victim->flags3 & (MF3_FLOORHUGGER|MF3_CEILINGHUGGER)))
			{
				victim->velz = 8*FRACUNIT;
				mo->velz = victim->velz;
			}
		}
		else
		{
			victim->velz = (1000 / victim->Mass) << FRACBITS;
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
