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
	fixedvec3 pos;

	if (!victim->SpecialBlastHandling (Owner, strength))
	{
		return;
	}

	angle = Owner->AngleTo(victim);
	angle >>= ANGLETOFINESHIFT;
	victim->vel.x = FixedMul (speed, finecosine[angle]);
	victim->vel.y = FixedMul (speed, finesine[angle]);

	// Spawn blast puff
	ang = victim->AngleTo(Owner);
	ang >>= ANGLETOFINESHIFT;
	pos = victim->Vec3Offset(
		FixedMul (victim->radius+FRACUNIT, finecosine[ang]),
		FixedMul (victim->radius+FRACUNIT, finesine[ang]),
		-victim->floorclip + (victim->height>>1));
	mo = Spawn (blasteffect, pos, ALLOW_REPLACE);
	if (mo)
	{
		mo->vel.x = victim->vel.x;
		mo->vel.y = victim->vel.y;
	}
	if (victim->flags & MF_MISSILE)
	{
		// [RH] Floor and ceiling huggers should not be blasted vertically.
		if (!(victim->flags3 & (MF3_FLOORHUGGER|MF3_CEILINGHUGGER)))
		{
			victim->vel.z = 8*FRACUNIT;
			mo->vel.z = victim->vel.z;
		}
	}
	else
	{
		victim->vel.z = (1000 / victim->Mass) << FRACBITS;
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
	PARAM_FIXED_OPT	(strength)				{ strength = 255*FRACUNIT; }
	PARAM_FIXED_OPT	(radius)				{ radius = 255*FRACUNIT; }
	PARAM_FIXED_OPT	(speed)					{ speed = 20*FRACUNIT; }
	PARAM_CLASS_OPT	(blasteffect, AActor)	{ blasteffect = PClass::FindActor("BlastEffect"); }
	PARAM_SOUND_OPT	(blastsound)			{ blastsound = "BlastRadius"; }

	AActor *mo;
	TThinkerIterator<AActor> iterator;
	fixed_t dist;

	if (self->player && (blastflags & BF_USEAMMO) && ACTION_CALL_FROM_WEAPON())
	{
		AWeapon *weapon = self->player->ReadyWeapon;
		if (weapon != NULL && !weapon->DepleteAmmo(weapon->bAltFire))
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
		dist = self->AproxDistance (mo);
		if (dist > radius)
		{ // Out of range
			continue;
		}
		if (mo->Sector->PortalGroup != self->Sector->PortalGroup && !P_CheckSight(self, mo))
		{
			// in another region and cannot be seen.
			continue;
		}
		BlastActor (mo, strength, speed, self, blasteffect, !!(blastflags & BF_NOIMPACTDAMAGE));
	}
	return 0;
}
