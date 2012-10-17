/*
#include "info.h"
#include "a_pickups.h"
#include "a_artifacts.h"
#include "gstrings.h"
#include "p_local.h"
#include "p_enemy.h"
#include "s_sound.h"
*/

/* For reference, the default values:
#define BLAST_RADIUS_DIST	255*FRACUNIT
#define BLAST_SPEED			20*FRACUNIT
#define BLAST_FULLSTRENGTH	255
*/

// Disc of Repulsion --------------------------------------------------------

//==========================================================================
//
// AArtiBlastRadius :: BlastActor
//
//==========================================================================

void BlastActor (AActor *victim, fixed_t strength, fixed_t speed, AActor *Owner, PClassActor *blasteffect, bool dontdamage)
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
	victim->velx = FixedMul (speed, finecosine[angle]);
	victim->vely = FixedMul (speed, finesine[angle]);

	// Spawn blast puff
	ang = R_PointToAngle2 (victim->x, victim->y, Owner->x, Owner->y);
	ang >>= ANGLETOFINESHIFT;
	x = victim->x + FixedMul (victim->radius+FRACUNIT, finecosine[ang]);
	y = victim->y + FixedMul (victim->radius+FRACUNIT, finesine[ang]);
	z = victim->z - victim->floorclip + (victim->height>>1);
	mo = Spawn (blasteffect, x, y, z, ALLOW_REPLACE);
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
	else if (!dontdamage)
	{
		victim->flags2 |= MF2_BLASTED;
	}
	if (victim->flags6 & MF6_TOUCHY)
	{ // Touchy objects die when blasted
		victim->flags6 &= ~MF6_ARMED; // Disarm
		P_DamageMobj(victim, Owner, Owner, victim->health, NAME_Melee, DMG_FORCED);
	}
}

enum
{
	BF_USEAMMO = 1,
	BF_DONTWARN = 2,
	BF_AFFECTBOSSES = 4,
	BF_NOIMPACTDAMAGE = 8,
};

//==========================================================================
//
// AArtiBlastRadius :: Activate
//
// Blast all actors away
//
//==========================================================================

DEFINE_ACTION_FUNCTION_PARAMS (AActor, A_Blast)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_INT_OPT	(blastflags)			{ blastflags = 0; }
	PARAM_INT_OPT	(strength)				{ strength = 255; }
	PARAM_INT_OPT	(radius)				{ radius = 255; }
	PARAM_FIXED_OPT	(speed)					{ speed = 20; }
	PARAM_CLASS_OPT	(blasteffect, AActor)	{ blasteffect = PClass::FindActor("BlastEffect"); }
	PARAM_SOUND_OPT	(blastsound)			{ blastsound = "BlastRadius"; }

	AActor *mo;
	TThinkerIterator<AActor> iterator;
	fixed_t dist;

	if (self->player && (blastflags & BF_USEAMMO))
	{
		AWeapon *weapon = self->player->ReadyWeapon;
		if (!weapon->DepleteAmmo(weapon->bAltFire))
		{
			return 0;
		}
	}

	S_Sound (self, CHAN_AUTO, blastsound, 1, ATTN_NORM);

	if (!(blastflags & BF_DONTWARN))
	{
		P_NoiseAlert (self, self);
	}
	while ( (mo = iterator.Next ()) )
	{
		if ((mo == self) || ((mo->flags2 & MF2_BOSS) && !(blastflags & BF_AFFECTBOSSES))
			|| (mo->flags2 & MF2_DORMANT) || (mo->flags3 & MF3_DONTBLAST))
		{ // Not a valid monster: originator, boss, dormant, or otherwise protected
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
		else if (!(mo->player) &&
			!(mo->flags & MF_MISSILE) &&
			!(mo->flags3 & (MF3_ISMONSTER|MF3_CANBLAST)) &&
			!(mo->flags6 & (MF6_TOUCHY|MF6_VULNERABLE)))
		{	// Must be monster, player, missile, touchy or vulnerable
			continue;
		}
		dist = P_AproxDistance (self->x - mo->x, self->y - mo->y);
		if (dist > radius)
		{ // Out of range
			continue;
		}
		BlastActor (mo, strength, speed, self, blasteffect, !!(blastflags & BF_NOIMPACTDAMAGE));
	}
	return 0;
}
