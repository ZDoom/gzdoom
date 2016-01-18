/*
#include "actor.h"
#include "gi.h"
#include "m_random.h"
#include "s_sound.h"
#include "d_player.h"
#include "a_action.h"
#include "p_local.h"
#include "a_action.h"
#include "a_hexenglobal.h"
#include "thingdef/thingdef.h"
*/

IMPLEMENT_CLASS (AFighterWeapon)
IMPLEMENT_CLASS (AClericWeapon)
IMPLEMENT_CLASS (AMageWeapon)

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

	angle = pmo->AngleTo(linetarget);
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
// TryPunch
//
// Returns true if an actor was punched, false if not.
//
//============================================================================

static bool TryPunch(APlayerPawn *pmo, angle_t angle, int damage, fixed_t power)
{
	const PClass *pufftype;
	AActor *linetarget;
	int slope;

	slope = P_AimLineAttack (pmo, angle, 2*MELEERANGE, &linetarget);
	if (linetarget != NULL)
	{
		if (++pmo->weaponspecial >= 3)
		{
			damage <<= 1;
			power *= 3;
			pufftype = PClass::FindClass ("HammerPuff");
		}
		else
		{
			pufftype = PClass::FindClass ("PunchPuff");
		}
		P_LineAttack (pmo, angle, 2*MELEERANGE, slope, damage, NAME_Melee, pufftype, true, &linetarget);
		if (linetarget != NULL)
		{
			if (linetarget->player != NULL || 
				(linetarget->Mass != INT_MAX && (linetarget->flags3 & MF3_ISMONSTER)))
			{
				P_ThrustMobj (linetarget, angle, power);
			}
			AdjustPlayerAngle (pmo, linetarget);
			return true;
		}
	}
	return false;
}

//============================================================================
//
// A_FPunchAttack
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_FPunchAttack)
{
	int damage;
	fixed_t power;
	int i;
	player_t *player;

	if (NULL == (player = self->player))
	{
		return;
	}
	APlayerPawn *pmo = player->mo;

	damage = 40+(pr_fpatk()&15);
	power = 2*FRACUNIT;
	for (i = 0; i < 16; i++)
	{
		if (TryPunch(pmo, pmo->angle + i*(ANG45/16), damage, power) ||
			TryPunch(pmo, pmo->angle - i*(ANG45/16), damage, power))
		{ // hit something
			if (pmo->weaponspecial >= 3)
			{
				pmo->weaponspecial = 0;
				P_SetPsprite (player, ps_weapon, player->ReadyWeapon->FindState ("Fire2"));
				S_Sound (pmo, CHAN_VOICE, "*fistgrunt", 1, ATTN_NORM);
			}
			return;
		}
	}
	// didn't find any creatures, so try to strike any walls
	pmo->weaponspecial = 0;

	AActor *linetarget;
	int slope = P_AimLineAttack (pmo, pmo->angle, MELEERANGE, &linetarget);
	P_LineAttack (pmo, pmo->angle, MELEERANGE, slope, damage, NAME_Melee, PClass::FindClass("PunchPuff"), true);
}
