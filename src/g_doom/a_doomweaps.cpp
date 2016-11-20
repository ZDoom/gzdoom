/*
#include "actor.h"
#include "info.h"
#include "s_sound.h"
#include "m_random.h"
#include "a_pickups.h"
#include "d_player.h"
#include "p_pspr.h"
#include "p_local.h"
#include "gstrings.h"
#include "p_effect.h"
#include "gi.h"
#include "templates.h"
#include "vm.h"
#include "doomstat.h"
*/

void P_SetSafeFlash(AWeapon *weapon, player_t *player, FState *flashstate, int index);
static FRandom pr_firerail ("FireRail");
static FRandom pr_bfgspray ("BFGSpray");
static FRandom pr_oldbfg ("OldBFG");


//
// [RH] A_FireRailgun
//
static void FireRailgun(AActor *self, int offset_xy, bool fromweapon)
{
	int damage;
	player_t *player;

	if (NULL == (player = self->player))
	{
		return;
	}

	AWeapon *weapon = self->player->ReadyWeapon;
	if (weapon != NULL && fromweapon)
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire, true, 1))
			return;

		FState *flash = weapon->FindState(NAME_Flash);
		if (flash != NULL)
		{
			P_SetSafeFlash(weapon, player, flash, (pr_firerail()&1));
		}
	}

	damage = deathmatch ? 100 : 150;

	FRailParams p;
	p.source = self;
	p.damage = damage;
	p.offset_xy = offset_xy;
	P_RailAttack (&p);
}


DEFINE_ACTION_FUNCTION(AActor, A_FireRailgun)
{
	PARAM_ACTION_PROLOGUE(AActor);
	FireRailgun(self, 0, ACTION_CALL_FROM_PSPRITE());
	return 0;
}

DEFINE_ACTION_FUNCTION(AActor, A_FireRailgunRight)
{
	PARAM_ACTION_PROLOGUE(AActor);
	FireRailgun(self, 10, ACTION_CALL_FROM_PSPRITE());
	return 0;
}

DEFINE_ACTION_FUNCTION(AActor, A_FireRailgunLeft)
{
	PARAM_ACTION_PROLOGUE(AActor);
	FireRailgun(self, -10, ACTION_CALL_FROM_PSPRITE());
	return 0;
}

//
// A_FireBFG
//

DEFINE_ACTION_FUNCTION(AActor, A_FireBFG)
{
	PARAM_ACTION_PROLOGUE(AActor);

	player_t *player;

	if (NULL == (player = self->player))
	{
		return 0;
	}

	AWeapon *weapon = self->player->ReadyWeapon;
	if (weapon != NULL && ACTION_CALL_FROM_PSPRITE())
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire, true, deh.BFGCells))
			return 0;
	}

	P_SpawnPlayerMissile (self,  0, 0, 0, PClass::FindActor("BFGBall"), self->Angles.Yaw, NULL, NULL, !!(dmflags2 & DF2_NO_FREEAIMBFG));
	return 0;
}


//
// A_BFGSpray
// Spawn a BFG explosion on every monster in view
//
enum BFG_Flags
{
	BFGF_HURTSOURCE = 1,
	BFGF_MISSILEORIGIN = 2,
};

DEFINE_ACTION_FUNCTION(AActor, A_BFGSpray)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_CLASS_DEF	(spraytype, AActor)
	PARAM_INT_DEF	(numrays)			
	PARAM_INT_DEF	(damagecnt)			
	PARAM_ANGLE_DEF	(angle)				
	PARAM_FLOAT_DEF	(distance)			
	PARAM_ANGLE_DEF	(vrange)			
	PARAM_INT_DEF	(defdamage)			
	PARAM_INT_DEF	(flags)				

	int 				i;
	int 				j;
	int 				damage;
	DAngle 				an;
	FTranslatedLineTarget t;
	AActor				*originator;

	if (spraytype == NULL) spraytype = PClass::FindActor("BFGExtra");
	if (numrays <= 0) numrays = 40;
	if (damagecnt <= 0) damagecnt = 15;
	if (angle == 0) angle = 90.;
	if (distance <= 0) distance = 16 * 64;
	if (vrange == 0) vrange = 32.;

	// [RH] Don't crash if no target
	if (!self->target)
		return 0;

	// [XA] Set the originator of the rays to the projectile (self) if
	//      the new flag is set, else set it to the player (self->target)
	originator = (flags & BFGF_MISSILEORIGIN) ? self : (AActor *)(self->target);

	// offset angles from its attack angle
	for (i = 0; i < numrays; i++)
	{
		an = self->Angles.Yaw - angle / 2 + angle / numrays*i;

		P_AimLineAttack(originator, an, distance, &t, vrange);

		if (t.linetarget != NULL)
		{
			AActor *spray = Spawn(spraytype, t.linetarget->PosPlusZ(t.linetarget->Height / 4), ALLOW_REPLACE);

			int dmgFlags = 0;
			FName dmgType = NAME_BFGSplash;

			if (spray != NULL)
			{
				if ((spray->flags6 & MF6_MTHRUSPECIES && self->target->GetSpecies() == t.linetarget->GetSpecies()) || 
					(!(flags & BFGF_HURTSOURCE) && self->target == t.linetarget)) // [XA] Don't hit oneself unless we say so.
				{
					spray->Destroy(); // [MC] Remove it because technically, the spray isn't trying to "hit" them.
					continue;
				}
				if (spray->flags5 & MF5_PUFFGETSOWNER) spray->target = self->target;
				if (spray->flags3 & MF3_FOILINVUL) dmgFlags |= DMG_FOILINVUL;
				if (spray->flags7 & MF7_FOILBUDDHA) dmgFlags |= DMG_FOILBUDDHA;
				dmgType = spray->DamageType;
			}

			if (defdamage == 0)
			{
				damage = 0;
				for (j = 0; j < damagecnt; ++j)
					damage += (pr_bfgspray() & 7) + 1;
			}
			else
			{
				// if this is used, damagecnt will be ignored
				damage = defdamage;
			}

			int newdam = P_DamageMobj(t.linetarget, originator, self->target, damage, dmgType, dmgFlags|DMG_USEANGLE, t.angleFromSource.Degrees);
			P_TraceBleed(newdam > 0 ? newdam : damage, &t, self);
		}
	}
	return 0;
}

//
// A_FireOldBFG
//
// This function emulates Doom's Pre-Beta BFG
// By Lee Killough 6/6/98, 7/11/98, 7/19/98, 8/20/98
//
// This code may not be used in other mods without appropriate credit given.
// Code leeches will be telefragged.

DEFINE_ACTION_FUNCTION(AActor, A_FireOldBFG)
{
	PARAM_ACTION_PROLOGUE(AActor);
	PClassActor *plasma[] = { PClass::FindActor("PlasmaBall1"), PClass::FindActor("PlasmaBall2") };
	AActor * mo = NULL;

	player_t *player;
	bool doesautoaim = false;

	if (NULL == (player = self->player))
	{
		return 0;
	}

	AWeapon *weapon = self->player->ReadyWeapon;
	if (!ACTION_CALL_FROM_PSPRITE()) weapon = NULL;
	if (weapon != NULL)
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire, true, 1))
			return 0;

		doesautoaim = !(weapon->WeaponFlags & WIF_NOAUTOAIM);
		weapon->WeaponFlags |= WIF_NOAUTOAIM; // No autoaiming that gun
	}
	self->player->extralight = 2;

	// Save values temporarily
	DAngle SavedPlayerAngle = self->Angles.Yaw;
	DAngle SavedPlayerPitch = self->Angles.Pitch;
	for (int i = 0; i < 2; i++) // Spawn two plasma balls in sequence
    {
		self->Angles.Yaw += ((pr_oldbfg()&127) - 64) * (90./768);
		self->Angles.Pitch += ((pr_oldbfg()&127) - 64) * (90./640);
		mo = P_SpawnPlayerMissile (self, plasma[i]);
		// Restore saved values
		self->Angles.Yaw = SavedPlayerAngle;
		self->Angles.Pitch = SavedPlayerPitch;
    }
	if (doesautoaim && weapon != NULL)
	{ // Restore autoaim setting
		weapon->WeaponFlags &= ~WIF_NOAUTOAIM;
	}
	return 0;
}
