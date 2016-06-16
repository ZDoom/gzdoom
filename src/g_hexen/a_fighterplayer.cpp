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

#define MAX_ANGLE_ADJUST (5.)

void AdjustPlayerAngle (AActor *pmo, FTranslatedLineTarget *t)
{
	// normally this will adjust relative to the actual direction to the target,
	// but with arbitrary portals that cannot be calculated so using the actual
	// attack angle is the only option.
	DAngle atkangle = t->unlinked ? t->angleFromSource : pmo->AngleTo(t->linetarget);
	DAngle difference = deltaangle(pmo->Angles.Yaw, atkangle);
	if (fabs(difference) > MAX_ANGLE_ADJUST)
	{
		if (difference > 0)
		{
			pmo->Angles.Yaw += MAX_ANGLE_ADJUST;
		}
		else
		{
			pmo->Angles.Yaw -= MAX_ANGLE_ADJUST;
		}
	}
	else
	{
		pmo->Angles.Yaw = t->angleFromSource;
	}
}

//============================================================================
//
// TryPunch
//
// Returns true if an actor was punched, false if not.
//
//============================================================================

static bool TryPunch(APlayerPawn *pmo, DAngle angle, int damage, int power)
{
	PClassActor *pufftype;
	FTranslatedLineTarget t;
	DAngle slope;

	slope = P_AimLineAttack (pmo, angle, 2*MELEERANGE, &t);
	if (t.linetarget != NULL)
	{
		if (++pmo->weaponspecial >= 3)
		{
			damage <<= 1;
			power *= 3;
			pufftype = PClass::FindActor("HammerPuff");
		}
		else
		{
			pufftype = PClass::FindActor("PunchPuff");
		}
		P_LineAttack (pmo, angle, 2*MELEERANGE, slope, damage, NAME_Melee, pufftype, true, &t);
		if (t.linetarget != NULL)
		{
			if (t.linetarget->player != NULL || 
				(t.linetarget->Mass != INT_MAX && (t.linetarget->flags3 & MF3_ISMONSTER)))
			{
				t.linetarget->Thrust(t.angleFromSource, power);
			}
			AdjustPlayerAngle (pmo, &t);
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
	PARAM_ACTION_PROLOGUE;

	int damage;
	int i;
	player_t *player;

	if (nullptr == (player = self->player))
	{
		return 0;
	}
	APlayerPawn *pmo = player->mo;

	damage = 40+(pr_fpatk()&15);
	for (i = 0; i < 16; i++)
	{
		if (TryPunch(pmo, pmo->Angles.Yaw + i*(45./16), damage, 2) ||
			TryPunch(pmo, pmo->Angles.Yaw - i*(45./16), damage, 2))
		{ // hit something
			if (pmo->weaponspecial >= 3)
			{
				pmo->weaponspecial = 0;
				P_SetPsprite(player, PSP_WEAPON, player->ReadyWeapon->FindState("Fire2"));
				S_Sound (pmo, CHAN_VOICE, "*fistgrunt", 1, ATTN_NORM);
			}
			return 0;
		}
	}
	// didn't find any creatures, so try to strike any walls
	pmo->weaponspecial = 0;

	DAngle slope = P_AimLineAttack (pmo, pmo->Angles.Yaw, MELEERANGE);
	P_LineAttack (pmo, pmo->Angles.Yaw, MELEERANGE, slope, damage, NAME_Melee, PClass::FindActor("PunchPuff"), true);
	return 0;
}
