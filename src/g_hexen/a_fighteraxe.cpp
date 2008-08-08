#include "actor.h"
#include "gi.h"
#include "m_random.h"
#include "s_sound.h"
#include "d_player.h"
#include "a_action.h"
#include "p_local.h"
#include "p_enemy.h"
#include "a_action.h"
#include "p_pspr.h"
#include "gstrings.h"
#include "a_hexenglobal.h"
#include "p_effect.h"

#define AXERANGE	((fixed_t)(2.25*MELEERANGE))

static FRandom pr_atk ("FAxeAtk");

void A_FAxeCheckReady (AActor *actor);
void A_FAxeCheckUp (AActor *actor);
void A_FAxeCheckAtk (AActor *actor);
void A_FAxeCheckReadyG (AActor *actor);
void A_FAxeCheckUpG (AActor *actor);
void A_FAxeAttack (AActor *actor);

extern void AdjustPlayerAngle (AActor *pmo, AActor *linetarget);

EXTERN_CVAR (Int, cl_bloodtype)

// The Fighter's Axe --------------------------------------------------------

class AFWeapAxe : public AFighterWeapon
{
	DECLARE_CLASS (AFWeapAxe, AFighterWeapon)
public:
	FState *GetUpState ();
	FState *GetDownState ();
	FState *GetReadyState ();
	FState *GetAtkState (bool hold);
};

IMPLEMENT_CLASS (AFWeapAxe)

FState *AFWeapAxe::GetUpState ()
{
	return Ammo1->Amount ? FindState ("SelectGlow") : Super::GetUpState();
}

FState *AFWeapAxe::GetDownState ()
{
	return Ammo1->Amount ? FindState ("DeselectGlow") : Super::GetDownState();
}

FState *AFWeapAxe::GetReadyState ()
{
	return Ammo1->Amount ? FindState ("ReadyGlow") : Super::GetReadyState();
}

FState *AFWeapAxe::GetAtkState (bool hold)
{
	return Ammo1->Amount ? FindState ("FireGlow") :  Super::GetAtkState(hold);
}

//============================================================================
//
// A_FAxeCheckReady
//
//============================================================================

void A_FAxeCheckReady (AActor *actor)
{
	player_t *player;

	if (NULL == (player = actor->player))
	{
		return;
	}
	if (player->ReadyWeapon->Ammo1->Amount)
	{
		P_SetPsprite (player, ps_weapon, player->ReadyWeapon->FindState ("ReadyGlow"));
	}
	else
	{
		A_WeaponReady (actor);
	}
}

//============================================================================
//
// A_FAxeCheckReadyG
//
//============================================================================

void A_FAxeCheckReadyG (AActor *actor)
{
	player_t *player;

	if (NULL == (player = actor->player))
	{
		return;
	}
	if (player->ReadyWeapon->Ammo1->Amount <= 0)
	{
		P_SetPsprite (player, ps_weapon, player->ReadyWeapon->FindState ("Ready"));
	}
	else
	{
		A_WeaponReady (actor);
	}
}

//============================================================================
//
// A_FAxeCheckUp
//
//============================================================================

void A_FAxeCheckUp (AActor *actor)
{
	player_t *player;

	if (NULL == (player = actor->player))
	{
		return;
	}
	if (player->ReadyWeapon->Ammo1->Amount)
	{
		P_SetPsprite (player, ps_weapon, player->ReadyWeapon->FindState ("ReadyGlow"));
	}
	else
	{
		A_Raise (actor);
	}
}

//============================================================================
//
// A_FAxeCheckUpG
//
//============================================================================

void A_FAxeCheckUpG (AActor *actor)
{
	player_t *player;

	if (NULL == (player = actor->player))
	{
		return;
	}
	if (player->ReadyWeapon->Ammo1->Amount <= 0)
	{
		P_SetPsprite (player, ps_weapon, player->ReadyWeapon->FindState ("Select"));
	}
	else
	{
		A_Raise (actor);
	}
}

//============================================================================
//
// A_FAxeCheckAtk
//
//============================================================================

void A_FAxeCheckAtk (AActor *actor)
{
	player_t *player;

	if (NULL == (player = actor->player))
	{
		return;
	}
	if (player->ReadyWeapon->Ammo1->Amount)
	{
		P_SetPsprite (player, ps_weapon, player->ReadyWeapon->FindState ("FireGlow"));
	}
}

//============================================================================
//
// A_FAxeAttack
//
//============================================================================

void A_FAxeAttack (AActor *actor)
{
	angle_t angle;
	fixed_t power;
	int damage;
	int slope;
	int i;
	int useMana;
	player_t *player;
	AWeapon *weapon;
	const PClass *pufftype;
	AActor *linetarget;

	if (NULL == (player = actor->player))
	{
		return;
	}
	AActor *pmo=player->mo;

	damage = 40+(pr_atk()&15);
	damage += pr_atk()&7;
	power = 0;
	weapon = player->ReadyWeapon;
	if (player->ReadyWeapon->Ammo1->Amount > 0)
	{
		damage <<= 1;
		power = 6*FRACUNIT;
		pufftype = PClass::FindClass ("AxePuffGlow");
		useMana = 1;
	}
	else
	{
		pufftype = PClass::FindClass ("AxePuff");
		useMana = 0;
	}
	for (i = 0; i < 16; i++)
	{
		angle = pmo->angle+i*(ANG45/16);
		slope = P_AimLineAttack (pmo, angle, AXERANGE, &linetarget);
		if (linetarget)
		{
			P_LineAttack (pmo, angle, AXERANGE, slope, damage, NAME_Melee, pufftype, true);
			if (linetarget->flags3&MF3_ISMONSTER || linetarget->player)
			{
				P_ThrustMobj (linetarget, angle, power);
			}
			AdjustPlayerAngle (pmo, linetarget);
			useMana++; 
			goto axedone;
		}
		angle = pmo->angle-i*(ANG45/16);
		slope = P_AimLineAttack (pmo, angle, AXERANGE, &linetarget);
		if (linetarget)
		{
			P_LineAttack (pmo, angle, AXERANGE, slope, damage, NAME_Melee, pufftype, true);
			if (linetarget->flags3&MF3_ISMONSTER)
			{
				P_ThrustMobj (linetarget, angle, power);
			}
			AdjustPlayerAngle (pmo, linetarget);
			useMana++; 
			goto axedone;
		}
	}
	// didn't find any creatures, so try to strike any walls
	pmo->special1 = 0;

	angle = pmo->angle;
	slope = P_AimLineAttack (pmo, angle, MELEERANGE, &linetarget);
	P_LineAttack (pmo, angle, MELEERANGE, slope, damage, NAME_Melee, pufftype, true);

axedone:
	if (useMana == 2)
	{
		AWeapon *weapon = player->ReadyWeapon;
		if (weapon != NULL)
		{
			weapon->DepleteAmmo (weapon->bAltFire, false);

			if ((weapon->Ammo1 == NULL || weapon->Ammo1->Amount == 0) &&
				(!(weapon->WeaponFlags & WIF_PRIMARY_USES_BOTH) ||
				  weapon->Ammo2 == NULL || weapon->Ammo2->Amount == 0))
			{
				P_SetPsprite (player, ps_weapon, player->ReadyWeapon->FindState ("Fire") + 5);
			}
		}
	}
	return;		
}

