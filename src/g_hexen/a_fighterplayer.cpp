#include "actor.h"
#include "gi.h"
#include "m_random.h"
#include "s_sound.h"
#include "d_player.h"
#include "a_action.h"
#include "p_local.h"
#include "p_enemy.h"
#include "a_action.h"
#include "a_hexenglobal.h"
#include "thingdef/thingdef.h"


// Fighter Weapon Base Class ------------------------------------------------

IMPLEMENT_CLASS (AFighterWeapon)
IMPLEMENT_CLASS (AClericWeapon)
IMPLEMENT_CLASS (AMageWeapon)

bool AFighterWeapon::TryPickup (AActor *&toucher)
{
	// The Doom and Hexen players are not excluded from pickup in case
	// somebody wants to use these weapons with either of those games.
	if (toucher->IsKindOf (PClass::FindClass(NAME_ClericPlayer)) ||
		toucher->IsKindOf (PClass::FindClass(NAME_MagePlayer)))
	{ // Wrong class, but try to pick up for mana
		if (ShouldStay())
		{ // Can't pick up weapons for other classes in coop netplay
			return false;
		}

		bool gaveSome = (NULL != AddAmmo (toucher, AmmoType1, AmmoGive1));
		gaveSome |= (NULL != AddAmmo (toucher, AmmoType2, AmmoGive2));
		if (gaveSome)
		{
			GoAwayAndDie ();
		}
		return gaveSome;
	}
	return Super::TryPickup (toucher);
}

// Cleric Weapon Base Class -------------------------------------------------

bool AClericWeapon::TryPickup (AActor *&toucher)
{
	// The Doom and Hexen players are not excluded from pickup in case
	// somebody wants to use these weapons with either of those games.
	if (toucher->IsKindOf (PClass::FindClass(NAME_FighterPlayer)) ||
		toucher->IsKindOf (PClass::FindClass(NAME_MagePlayer)))
	{ // Wrong class, but try to pick up for mana
		if (ShouldStay())
		{ // Can't pick up weapons for other classes in coop netplay
			return false;
		}

		bool gaveSome = (NULL != AddAmmo (toucher, AmmoType1, AmmoGive1));
		gaveSome |= (NULL != AddAmmo (toucher, AmmoType2, AmmoGive2));
		if (gaveSome)
		{
			GoAwayAndDie ();
		}
		return gaveSome;
	}
	return Super::TryPickup (toucher);
}

// Mage Weapon Base Class ---------------------------------------------------

bool AMageWeapon::TryPickup (AActor *&toucher)
{
	// The Doom and Hexen players are not excluded from pickup in case
	// somebody wants to use these weapons with either of those games.
	if (toucher->IsKindOf (PClass::FindClass(NAME_FighterPlayer)) ||
		toucher->IsKindOf (PClass::FindClass(NAME_ClericPlayer)))
	{ // Wrong class, but try to pick up for mana
		if (ShouldStay())
		{ // Can't pick up weapons for other classes in coop netplay
			return false;
		}

		bool gaveSome = (NULL != AddAmmo (toucher, AmmoType1, AmmoGive1));
		gaveSome |= (NULL != AddAmmo (toucher, AmmoType2, AmmoGive2));
		if (gaveSome)
		{
			GoAwayAndDie ();
		}
		return gaveSome;
	}
	return Super::TryPickup (toucher);
}



static FRandom pr_fpatk ("FPunchAttack");

//============================================================================
//
//	AdjustPlayerAngle
//
//============================================================================

#define MAX_ANGLE_ADJUST (5*ANGLE_1)

void AdjustPlayerAngle (AActor *pmo, AActor *linetarget)
{
	angle_t angle;
	int difference;

	angle = R_PointToAngle2 (pmo->x, pmo->y, linetarget->x, linetarget->y);
	difference = (int)angle - (int)pmo->angle;
	if (abs(difference) > MAX_ANGLE_ADJUST)
	{
		if (difference > 0)
		{
			pmo->angle += MAX_ANGLE_ADJUST;
		}
		else
		{
			pmo->angle -= MAX_ANGLE_ADJUST;
		}
	}
	else
	{
		pmo->angle = angle;
	}
}

//============================================================================
//
// A_FPunchAttack
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_FPunchAttack)
{
	angle_t angle;
	int damage;
	int slope;
	fixed_t power;
	int i;
	player_t *player;
	const PClass *pufftype;
	AActor *linetarget;

	if (NULL == (player = self->player))
	{
		return;
	}
	APlayerPawn *pmo = player->mo;

	damage = 40+(pr_fpatk()&15);
	power = 2*FRACUNIT;
	pufftype = PClass::FindClass ("PunchPuff");
	for (i = 0; i < 16; i++)
	{
		angle = pmo->angle + i*(ANG45/16);
		slope = P_AimLineAttack (pmo, angle, 2*MELEERANGE, &linetarget);
		if (linetarget)
		{
			pmo->special1++;
			if (pmo->special1 == 3)
			{
				damage <<= 1;
				power = 6*FRACUNIT;
				pufftype = PClass::FindClass ("HammerPuff");
			}
			P_LineAttack (pmo, angle, 2*MELEERANGE, slope, damage, NAME_Melee, pufftype, true);
			if (linetarget->flags3&MF3_ISMONSTER || linetarget->player)
			{
				P_ThrustMobj (linetarget, angle, power);
			}
			AdjustPlayerAngle (pmo, linetarget);
			goto punchdone;
		}
		angle = pmo->angle-i * (ANG45/16);
		slope = P_AimLineAttack (pmo, angle, 2*MELEERANGE, &linetarget);
		if (linetarget)
		{
			pmo->special1++;
			if (pmo->special1 == 3)
			{
				damage <<= 1;
				power = 6*FRACUNIT;
				pufftype = PClass::FindClass ("HammerPuff");
			}
			P_LineAttack (pmo, angle, 2*MELEERANGE, slope, damage, NAME_Melee, pufftype, true);
			if (linetarget->flags3&MF3_ISMONSTER || linetarget->player)
			{
				P_ThrustMobj (linetarget, angle, power);
			}
			AdjustPlayerAngle (pmo, linetarget);
			goto punchdone;
		}
	}
	// didn't find any creatures, so try to strike any walls
	pmo->special1 = 0;

	angle = pmo->angle;
	slope = P_AimLineAttack (pmo, angle, MELEERANGE, &linetarget);
	P_LineAttack (pmo, angle, MELEERANGE, slope, damage, NAME_Melee, pufftype, true);

punchdone:
	if (pmo->special1 == 3)
	{
		pmo->special1 = 0;
		P_SetPsprite (player, ps_weapon, player->ReadyWeapon->FindState ("Fire2"));
		S_Sound (pmo, CHAN_VOICE, "*fistgrunt", 1, ATTN_NORM);
	}
	return;		
}

