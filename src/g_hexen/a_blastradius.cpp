#include "info.h"
#include "a_pickups.h"
#include "a_artifacts.h"
#include "gstrings.h"
#include "p_local.h"
#include "p_enemy.h"
#include "s_sound.h"

void P_BlastRadius (player_t *player);

#define BLAST_RADIUS_DIST	255*FRACUNIT
#define BLAST_SPEED			20*FRACUNIT
#define BLAST_FULLSTRENGTH	255

// Disc of Repulsion --------------------------------------------------------

BASIC_ARTI (BlastRadius, arti_blastradius, GStrings(TXT_ARTIBLASTRADIUS))
	AT_GAME_SET_FRIEND (BlastRadius)
private:
	static bool ActivateArti (player_t *player, artitype_t arti)
	{
		P_BlastRadius (player);
		return true;
	}
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
END_DEFAULTS

AT_GAME_SET (BlastRadius)
{
	ArtiDispatch[arti_blastradius] = AArtiBlastRadius::ActivateArti;
	ArtiPics[arti_blastradius] = "ARTIBLST";
}

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
// ResetBlasted
//
//==========================================================================

void ResetBlasted (AActor *mo)
{
	mo->flags2 &= ~MF2_BLASTED;
	if (!(mo->flags & MF_ICECORPSE))
	{
		mo->flags2 &= ~MF2_SLIDE;
	}
}

//==========================================================================
//
// P_BlastMobj
//
//==========================================================================

void P_BlastMobj (AActor *source, AActor *victim, fixed_t strength)
{
	angle_t angle,ang;
	AActor *mo;
	fixed_t x,y,z;

	if (!victim->SpecialBlastHandling (source, strength))
	{
		return;
	}

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
			victim->flags2 |= MF2_SLIDE | MF2_BLASTED;
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
				victim->target = source;
			}
#endif
		}
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
			victim->flags2 |= MF2_SLIDE | MF2_BLASTED;
		}
	}
}

//==========================================================================
//
// P_BlastRadius
//
// Blast all actors away
//
//==========================================================================

void P_BlastRadius (player_t *player)
{
	AActor *pmo = player->mo;
	AActor *mo;
	TThinkerIterator<AActor> iterator;
	fixed_t dist;

	S_Sound (pmo, CHAN_AUTO, "BlastRadius", 1, ATTN_NORM);
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
		dist = P_AproxDistance (pmo->x - mo->x, pmo->y - mo->y);
		if (dist > BLAST_RADIUS_DIST)
		{ // Out of range
			continue;
		}
		P_BlastMobj (pmo, mo, BLAST_FULLSTRENGTH);
	}
}
