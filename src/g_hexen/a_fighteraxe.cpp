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

#define AXERANGE	(2.25 * MELEERANGE)

static FRandom pr_axeatk ("FAxeAtk");

void A_FAxeCheckReady (AActor *actor);
void A_FAxeCheckUp (AActor *actor);
void A_FAxeCheckAtk (AActor *actor);
void A_FAxeCheckReadyG (AActor *actor);
void A_FAxeCheckUpG (AActor *actor);
void A_FAxeAttack (AActor *actor);

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

DEFINE_ACTION_FUNCTION(AActor, A_FAxeCheckReady)
{
	PARAM_ACTION_PROLOGUE;

	player_t *player;

	if (nullptr == (player = self->player))
	{
		return 0;
	}
	if (player->ReadyWeapon->Ammo1->Amount)
	{
		P_SetPsprite(player, PSP_WEAPON, player->ReadyWeapon->FindState ("ReadyGlow"));
	}
	else
	{
		DoReadyWeapon(self);
	}
	return 0;
}

//============================================================================
//
// A_FAxeCheckReadyG
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_FAxeCheckReadyG)
{
	PARAM_ACTION_PROLOGUE;

	player_t *player;

	if (nullptr == (player = self->player))
	{
		return 0;
	}
	if (player->ReadyWeapon->Ammo1->Amount <= 0)
	{
		P_SetPsprite(player, PSP_WEAPON, player->ReadyWeapon->FindState ("Ready"));
	}
	else
	{
		DoReadyWeapon(self);
	}
	return 0;
}

//============================================================================
//
// A_FAxeCheckUp
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_FAxeCheckUp)
{
	PARAM_ACTION_PROLOGUE;

	player_t *player;

	if (nullptr == (player = self->player))
	{
		return 0;
	}
	if (player->ReadyWeapon->Ammo1->Amount)
	{
		P_SetPsprite(player, PSP_WEAPON, player->ReadyWeapon->FindState ("SelectGlow"));
	}
	else
	{
		CALL_ACTION(A_Raise, self);
	}
	return 0;
}

//============================================================================
//
// A_FAxeCheckUpG
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_FAxeCheckUpG)
{
	PARAM_ACTION_PROLOGUE;

	player_t *player;

	if (nullptr == (player = self->player))
	{
		return 0;
	}
	if (player->ReadyWeapon->Ammo1->Amount <= 0)
	{
		P_SetPsprite(player, PSP_WEAPON, player->ReadyWeapon->FindState ("Select"));
	}
	else
	{
		CALL_ACTION(A_Raise, self);
	}
	return 0;
}

//============================================================================
//
// A_FAxeCheckAtk
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_FAxeCheckAtk)
{
	PARAM_ACTION_PROLOGUE;

	player_t *player;

	if (nullptr == (player = self->player))
	{
		return 0;
	}
	if (player->ReadyWeapon->Ammo1->Amount)
	{
		P_SetPsprite(player, PSP_WEAPON, player->ReadyWeapon->FindState ("FireGlow"));
	}
	return 0;
}

//============================================================================
//
// A_FAxeAttack
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_FAxeAttack)
{
	PARAM_ACTION_PROLOGUE;

	DAngle angle;
	int power;
	int damage;
	DAngle slope;
	int i;
	int useMana;
	player_t *player;
	AWeapon *weapon;
	PClassActor *pufftype;
	FTranslatedLineTarget t;

	if (nullptr == (player = self->player))
	{
		return 0;
	}
	AActor *pmo=player->mo;

	damage = 40+(pr_axeatk()&15);
	damage += pr_axeatk()&7;
	power = 0;
	weapon = player->ReadyWeapon;
	if (player->ReadyWeapon->Ammo1->Amount > 0)
	{
		damage <<= 1;
		power = 6;
		pufftype = PClass::FindActor ("AxePuffGlow");
		useMana = 1;
	}
	else
	{
		pufftype = PClass::FindActor ("AxePuff");
		useMana = 0;
	}
	for (i = 0; i < 16; i++)
	{
		for (int j = 1; j >= -1; j -= 2)
		{
			angle = pmo->Angles.Yaw + j*i*(45. / 16);
			slope = P_AimLineAttack(pmo, angle, AXERANGE, &t);
			if (t.linetarget)
			{
				P_LineAttack(pmo, angle, AXERANGE, slope, damage, NAME_Melee, pufftype, true, &t);
				if (t.linetarget != nullptr)
				{
					if (t.linetarget->flags3&MF3_ISMONSTER || t.linetarget->player)
					{
						t.linetarget->Thrust(t.angleFromSource, power);
					}
					AdjustPlayerAngle(pmo, &t);
					useMana++;
					goto axedone;
				}
			}
		}
	}
	// didn't find any creatures, so try to strike any walls
	pmo->weaponspecial = 0;

	angle = pmo->Angles.Yaw;
	slope = P_AimLineAttack (pmo, angle, MELEERANGE);
	P_LineAttack (pmo, angle, MELEERANGE, slope, damage, NAME_Melee, pufftype, true);

axedone:
	if (useMana == 2)
	{
		AWeapon *weapon = player->ReadyWeapon;
		if (weapon != nullptr)
		{
			weapon->DepleteAmmo (weapon->bAltFire, false);

			if ((weapon->Ammo1 == nullptr || weapon->Ammo1->Amount == 0) &&
				(!(weapon->WeaponFlags & WIF_PRIMARY_USES_BOTH) ||
				  weapon->Ammo2 == nullptr || weapon->Ammo2->Amount == 0))
			{
				P_SetPsprite(player, PSP_WEAPON, player->ReadyWeapon->FindState ("Fire") + 5);
			}
		}
	}
	return 0;		
}

