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
	DECLARE_ACTOR (AFWeapAxe, AFighterWeapon)
public:
	FState *GetUpState ();
	FState *GetDownState ();
	FState *GetReadyState ();
	FState *GetAtkState (bool hold);
};

FState AFWeapAxe::States[] =
{
#define S_AXE 0
	S_NORMAL (WFAX, 'A',   -1, NULL					    , NULL),

#define S_FAXEREADY (S_AXE+1)
	S_NORMAL (FAXE, 'A',	1, A_FAxeCheckReady			, &States[S_FAXEREADY]),

#define S_FAXEDOWN (S_FAXEREADY+1)
	S_NORMAL (FAXE, 'A',	1, A_Lower				    , &States[S_FAXEDOWN]),

#define S_FAXEUP (S_FAXEDOWN+1)
	S_NORMAL (FAXE, 'A',	1, A_FAxeCheckUp			, &States[S_FAXEUP]),

#define S_FAXEATK (S_FAXEUP+1)
	S_NORMAL2 (FAXE, 'B',	4, A_FAxeCheckAtk		    , &States[S_FAXEATK+1], 15, 32),
	S_NORMAL2 (FAXE, 'C',	3, NULL					    , &States[S_FAXEATK+2], 15, 32),
	S_NORMAL2 (FAXE, 'D',	2, NULL					    , &States[S_FAXEATK+3], 15, 32),
	S_NORMAL2 (FAXE, 'D',	1, A_FAxeAttack			    , &States[S_FAXEATK+4], -5, 70),
	S_NORMAL2 (FAXE, 'D',	2, NULL					    , &States[S_FAXEATK+5], -25, 90),
	S_NORMAL2 (FAXE, 'E',	1, NULL					    , &States[S_FAXEATK+6], 15, 32),
	S_NORMAL2 (FAXE, 'E',	2, NULL					    , &States[S_FAXEATK+7], 10, 54),
	S_NORMAL2 (FAXE, 'E',	7, NULL					    , &States[S_FAXEATK+8], 10, 150),
	S_NORMAL2 (FAXE, 'A',	1, A_ReFire				    , &States[S_FAXEATK+9], 0, 60),
	S_NORMAL2 (FAXE, 'A',	1, NULL					    , &States[S_FAXEATK+10], 0, 52),
	S_NORMAL2 (FAXE, 'A',	1, NULL					    , &States[S_FAXEATK+11], 0, 44),
	S_NORMAL2 (FAXE, 'A',	1, NULL					    , &States[S_FAXEATK+12], 0, 36),
	S_NORMAL  (FAXE, 'A',	1, NULL					    , &States[S_FAXEREADY]),

#define S_FAXEREADY_G (S_FAXEATK+13)
	S_NORMAL (FAXE, 'L',	1, A_FAxeCheckReadyG	    , &States[S_FAXEREADY_G+1]),
	S_NORMAL (FAXE, 'L',	1, A_FAxeCheckReadyG	    , &States[S_FAXEREADY_G+2]),
	S_NORMAL (FAXE, 'L',	1, A_FAxeCheckReadyG	    , &States[S_FAXEREADY_G+3]),
	S_NORMAL (FAXE, 'M',	1, A_FAxeCheckReadyG	    , &States[S_FAXEREADY_G+4]),
	S_NORMAL (FAXE, 'M',	1, A_FAxeCheckReadyG	    , &States[S_FAXEREADY_G+5]),
	S_NORMAL (FAXE, 'M',	1, A_FAxeCheckReadyG	    , &States[S_FAXEREADY_G]),

#define S_FAXEDOWN_G (S_FAXEREADY_G+6)
	S_NORMAL (FAXE, 'L',	1, A_Lower				    , &States[S_FAXEDOWN_G]),

#define S_FAXEUP_G (S_FAXEDOWN_G+1)
	S_NORMAL (FAXE, 'L',	1, A_FAxeCheckUpG			, &States[S_FAXEUP_G]),

#define S_FAXEATK_G (S_FAXEUP_G+1)
	S_NORMAL2 (FAXE, 'N',	4, NULL					    , &States[S_FAXEATK_G+1], 15, 32),
	S_NORMAL2 (FAXE, 'O',	3, NULL					    , &States[S_FAXEATK_G+2], 15, 32),
	S_NORMAL2 (FAXE, 'P',	2, NULL					    , &States[S_FAXEATK_G+3], 15, 32),
	S_NORMAL2 (FAXE, 'P',	1, A_FAxeAttack			    , &States[S_FAXEATK_G+4], -5, 70),
	S_NORMAL2 (FAXE, 'P',	2, NULL					    , &States[S_FAXEATK_G+5], -25, 90),
	S_NORMAL2 (FAXE, 'Q',	1, NULL					    , &States[S_FAXEATK_G+6], 15, 32),
	S_NORMAL2 (FAXE, 'Q',	2, NULL					    , &States[S_FAXEATK_G+7], 10, 54),
	S_NORMAL2 (FAXE, 'Q',	7, NULL					    , &States[S_FAXEATK_G+8], 10, 150),
	S_NORMAL2 (FAXE, 'A',	1, A_ReFire				    , &States[S_FAXEATK_G+9], 0, 60),
	S_NORMAL2 (FAXE, 'A',	1, NULL					    , &States[S_FAXEATK_G+10], 0, 52),
	S_NORMAL2 (FAXE, 'A',	1, NULL					    , &States[S_FAXEATK_G+11], 0, 44),
	S_NORMAL2 (FAXE, 'A',	1, NULL					    , &States[S_FAXEATK_G+12], 0, 36),
	S_NORMAL  (FAXE, 'A',	1, NULL					    , &States[S_FAXEREADY_G]),
};

IMPLEMENT_ACTOR (AFWeapAxe, Hexen, 8010, 27)
	PROP_Flags (MF_SPECIAL)
	PROP_SpawnState (S_AXE)

	PROP_Weapon_SelectionOrder (1500)
	PROP_Weapon_Flags (WIF_AXEBLOOD|WIF_AMMO_OPTIONAL|WIF_BOT_MELEE)
	PROP_Weapon_AmmoUse1 (2)
	PROP_Weapon_AmmoGive1 (25)
	PROP_Weapon_UpState (S_FAXEUP)
	PROP_Weapon_DownState (S_FAXEDOWN)
	PROP_Weapon_ReadyState (S_FAXEREADY)
	PROP_Weapon_AtkState (S_FAXEATK)
	PROP_Weapon_Kickback (150)
	PROP_Weapon_YAdjust (0-12)
	PROP_Weapon_AmmoType1 ("Mana1")
	PROP_Inventory_PickupMessage("$TXT_WEAPON_F2")
END_DEFAULTS

FState *AFWeapAxe::GetUpState ()
{
	return Ammo1->Amount ? &States[S_FAXEUP_G] : Super::GetUpState();
}

FState *AFWeapAxe::GetDownState ()
{
	return Ammo1->Amount ? &States[S_FAXEDOWN_G] : Super::GetDownState();
}

FState *AFWeapAxe::GetReadyState ()
{
	return Ammo1->Amount ? &States[S_FAXEREADY_G] : Super::GetReadyState();
}

FState *AFWeapAxe::GetAtkState (bool hold)
{
	return Ammo1->Amount ? &States[S_FAXEATK_G] :  Super::GetAtkState(hold);
}

// Axe Puff -----------------------------------------------------------------

class AAxePuff : public AActor
{
	DECLARE_ACTOR (AAxePuff, AActor)
};

FState AAxePuff::States[] =
{
	S_NORMAL (FHFX, 'S',	4, NULL					    , &States[1]),
	S_NORMAL (FHFX, 'T',	4, NULL					    , &States[2]),
	S_NORMAL (FHFX, 'U',	4, NULL					    , &States[3]),
	S_NORMAL (FHFX, 'V',	4, NULL					    , &States[4]),
	S_NORMAL (FHFX, 'W',	4, NULL					    , NULL),
};

IMPLEMENT_ACTOR (AAxePuff, Hexen, -1, 0)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY)
	PROP_Flags3 (MF3_PUFFONACTORS)
	PROP_RenderStyle (STYLE_Translucent)
	PROP_Alpha (HX_SHADOW)

	PROP_SpawnState (0)

	PROP_SeeSound ("FighterAxeHitThing")
	PROP_AttackSound ("FighterHammerHitWall")
	PROP_ActiveSound ("FighterHammerMiss")
END_DEFAULTS

// Glowing Axe Puff ---------------------------------------------------------

class AAxePuffGlow : public AAxePuff
{
	DECLARE_ACTOR (AAxePuffGlow, AAxePuff)
};

FState AAxePuffGlow::States[] =
{
	S_BRIGHT (FAXE, 'R',	4, NULL					    , &States[1]),
	S_BRIGHT (FAXE, 'S',	4, NULL					    , &States[2]),
	S_BRIGHT (FAXE, 'T',	4, NULL					    , &States[3]),
	S_BRIGHT (FAXE, 'U',	4, NULL					    , &States[4]),
	S_BRIGHT (FAXE, 'V',	4, NULL					    , &States[5]),
	S_BRIGHT (FAXE, 'W',	4, NULL					    , &States[6]),
	S_BRIGHT (FAXE, 'X',	4, NULL					    , NULL),
};

IMPLEMENT_ACTOR (AAxePuffGlow, Hexen, -1, 0)
	PROP_Flags3 (MF3_PUFFONACTORS)
	PROP_RenderStyle (STYLE_Add)
	PROP_Alpha (OPAQUE)
	PROP_SpawnState (0)
END_DEFAULTS

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
		P_SetPsprite (player, ps_weapon, &AFWeapAxe::States[S_FAXEREADY_G]);
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
		P_SetPsprite (player, ps_weapon, &AFWeapAxe::States[S_FAXEREADY]);
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
		P_SetPsprite (player, ps_weapon, &AFWeapAxe::States[S_FAXEUP_G]);
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
		P_SetPsprite (player, ps_weapon, &AFWeapAxe::States[S_FAXEUP]);
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
		P_SetPsprite (player, ps_weapon, &AFWeapAxe::States[S_FAXEATK_G]);
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
		pufftype = RUNTIME_CLASS(AAxePuffGlow);
		useMana = 1;
	}
	else
	{
		pufftype = RUNTIME_CLASS(AAxePuff);
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
				P_SetPsprite (player, ps_weapon, &AFWeapAxe::States[S_FAXEATK+5]);
			}
		}
	}
	return;		
}

