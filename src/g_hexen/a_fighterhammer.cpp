/*
#include "actor.h"
#include "gi.h"
#include "m_random.h"
#include "s_sound.h"
#include "d_player.h"
#include "a_action.h"
#include "p_local.h"
#include "a_action.h"
#include "p_pspr.h"
#include "gstrings.h"
#include "a_hexenglobal.h"
#include "thingdef/thingdef.h"
*/

const double HAMMER_RANGE = 1.5 * MELEERANGE;

static FRandom pr_hammeratk ("FHammerAtk");

//============================================================================
//
// A_FHammerAttack
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_FHammerAttack)
{
	PARAM_ACTION_PROLOGUE;

	DAngle angle;
	int damage;
	DAngle slope;
	int i;
	player_t *player;
	FTranslatedLineTarget t;
	PClassActor *hammertime;

	if (NULL == (player = self->player))
	{
		return 0;
	}
	AActor *pmo=player->mo;

	damage = 60+(pr_hammeratk()&63);
	hammertime = PClass::FindActor("HammerPuff");
	for (i = 0; i < 16; i++)
	{
		for (int j = 1; j >= -1; j -= 2)
		{
			angle = pmo->Angles.Yaw + j*i*(45. / 32);
			slope = P_AimLineAttack(pmo, angle, HAMMER_RANGE, &t, 0., ALF_CHECK3D);
			if (t.linetarget != NULL)
			{
				P_LineAttack(pmo, angle, HAMMER_RANGE, slope, damage, NAME_Melee, hammertime, true, &t);
				if (t.linetarget != NULL)
				{
					AdjustPlayerAngle(pmo, &t);
					if (t.linetarget->flags3 & MF3_ISMONSTER || t.linetarget->player)
					{
						t.linetarget->Thrust(t.angleFromSource, 10);
					}
					pmo->weaponspecial = false; // Don't throw a hammer
					goto hammerdone;
				}
			}
		}
	}
	// didn't find any targets in meleerange, so set to throw out a hammer
	angle = pmo->Angles.Yaw;
	slope = P_AimLineAttack (pmo, angle, HAMMER_RANGE, NULL, 0., ALF_CHECK3D);
	if (P_LineAttack (pmo, angle, HAMMER_RANGE, slope, damage, NAME_Melee, hammertime, true) != NULL)
	{
		pmo->weaponspecial = false;
	}
	else
	{
		pmo->weaponspecial = true;
	}
hammerdone:
	// Don't spawn a hammer if the player doesn't have enough mana
	if (player->ReadyWeapon == NULL ||
		!player->ReadyWeapon->CheckAmmo (player->ReadyWeapon->bAltFire ?
			AWeapon::AltFire : AWeapon::PrimaryFire, false, true))
	{ 
		pmo->weaponspecial = false;
	}
	return 0;		
}

//============================================================================
//
// A_FHammerThrow
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_FHammerThrow)
{
	PARAM_ACTION_PROLOGUE;

	AActor *mo;
	player_t *player;

	if (NULL == (player = self->player))
	{
		return 0;
	}

	if (!player->mo->weaponspecial)
	{
		return 0;
	}
	AWeapon *weapon = player->ReadyWeapon;
	if (weapon != NULL)
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire, false))
			return 0;
	}
	mo = P_SpawnPlayerMissile (player->mo, PClass::FindActor("HammerMissile")); 
	if (mo)
	{
		mo->special1 = 0;
	}
	return 0;
}
