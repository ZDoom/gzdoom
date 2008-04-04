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


// Fighter Weapon Base Class ------------------------------------------------

IMPLEMENT_STATELESS_ACTOR (AFighterWeapon, Hexen, -1, 0)
	PROP_Weapon_Kickback (150)
END_DEFAULTS

bool AFighterWeapon::TryPickup (AActor *toucher)
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

IMPLEMENT_STATELESS_ACTOR (AClericWeapon, Hexen, -1, 0)
	PROP_Weapon_Kickback (150)
END_DEFAULTS

bool AClericWeapon::TryPickup (AActor *toucher)
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

IMPLEMENT_STATELESS_ACTOR (AMageWeapon, Hexen, -1, 0)
	PROP_Weapon_Kickback (150)
END_DEFAULTS

bool AMageWeapon::TryPickup (AActor *toucher)
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

void AdjustPlayerAngle (AActor *pmo)
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

// Fist (first weapon) ------------------------------------------------------

void A_FPunchAttack (AActor *actor);

class AFWeapFist : public AFighterWeapon
{
	DECLARE_ACTOR (AFWeapFist, AFighterWeapon)
};

FState AFWeapFist::States[] =
{
#define S_PUNCHREADY 0
	S_NORMAL (FPCH, 'A',	1, A_WeaponReady			, &States[S_PUNCHREADY]),

#define S_PUNCHDOWN (S_PUNCHREADY+1)
	S_NORMAL (FPCH, 'A',	1, A_Lower					, &States[S_PUNCHDOWN]),

#define S_PUNCHUP (S_PUNCHDOWN+1)
	S_NORMAL (FPCH, 'A',	1, A_Raise					, &States[S_PUNCHUP]),

#define S_PUNCHATK1 (S_PUNCHUP+1)
	S_NORMAL2(FPCH, 'B',	5, NULL 					, &States[S_PUNCHATK1+1], 5, 40),
	S_NORMAL2(FPCH, 'C',	4, NULL 					, &States[S_PUNCHATK1+2], 5, 40),
	S_NORMAL2(FPCH, 'D',	4, A_FPunchAttack			, &States[S_PUNCHATK1+3], 5, 40),
	S_NORMAL2(FPCH, 'C',	4, NULL 					, &States[S_PUNCHATK1+4], 5, 40),
	S_NORMAL2(FPCH, 'B',	5, A_ReFire 				, &States[S_PUNCHREADY], 5, 40),

#define S_PUNCHATK2 (S_PUNCHATK1+5)
	S_NORMAL2(FPCH, 'D',	4, NULL 					, &States[S_PUNCHATK2+1], 5, 40),
	S_NORMAL2(FPCH, 'E',	4, NULL 					, &States[S_PUNCHATK2+2], 5, 40),
	S_NORMAL2(FPCH, 'E',	1, NULL 					, &States[S_PUNCHATK2+3], 15, 50),
	S_NORMAL2(FPCH, 'E',	1, NULL 					, &States[S_PUNCHATK2+4], 25, 60),
	S_NORMAL2(FPCH, 'E',	1, NULL 					, &States[S_PUNCHATK2+5], 35, 70),
	S_NORMAL2(FPCH, 'E',	1, NULL 					, &States[S_PUNCHATK2+6], 45, 80),
	S_NORMAL2(FPCH, 'E',	1, NULL 					, &States[S_PUNCHATK2+7], 55, 90),
	S_NORMAL2(FPCH, 'E',	1, NULL 					, &States[S_PUNCHATK2+8], 65, 100),
	S_NORMAL2(FPCH, 'E',   10, NULL 					, &States[S_PUNCHREADY], 0, 150)
};

IMPLEMENT_ACTOR (AFWeapFist, Hexen, -1, 0)
	PROP_Flags5 (MF5_BLOODSPLATTER)
	PROP_Weapon_SelectionOrder (3400)
	PROP_Weapon_Flags (WIF_BOT_MELEE)
	PROP_Weapon_UpState (S_PUNCHUP)
	PROP_Weapon_DownState (S_PUNCHDOWN)
	PROP_Weapon_ReadyState (S_PUNCHREADY)
	PROP_Weapon_AtkState (S_PUNCHATK1)
	PROP_Weapon_Kickback (150)
END_DEFAULTS

// Punch puff ---------------------------------------------------------------

class APunchPuff : public AActor
{
	DECLARE_ACTOR (APunchPuff, AActor)
public:
	void BeginPlay ();
};

FState APunchPuff::States[] =
{
	S_NORMAL (FHFX, 'S',	4, NULL 					, &States[1]),
	S_NORMAL (FHFX, 'T',	4, NULL 					, &States[2]),
	S_NORMAL (FHFX, 'U',	4, NULL 					, &States[3]),
	S_NORMAL (FHFX, 'V',	4, NULL 					, &States[4]),
	S_NORMAL (FHFX, 'W',	4, NULL 					, NULL)
};

IMPLEMENT_ACTOR (APunchPuff, Hexen, -1, 0)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY)
	PROP_Flags3 (MF3_PUFFONACTORS)
	PROP_RenderStyle (STYLE_Translucent)
	PROP_Alpha (HX_SHADOW)

	PROP_SpawnState (0)

	PROP_SeeSound ("FighterPunchHitThing")
	PROP_AttackSound ("FighterPunchHitWall")
	PROP_ActiveSound ("FighterPunchMiss")
END_DEFAULTS

void APunchPuff::BeginPlay ()
{
	Super::BeginPlay ();
	momz = FRACUNIT;
}

//============================================================================
//
// A_FPunchAttack
//
//============================================================================

void A_FPunchAttack (AActor *actor)
{
	angle_t angle;
	int damage;
	int slope;
	fixed_t power;
	int i;
	player_t *player;
	const PClass *pufftype;

	if (NULL == (player = actor->player))
	{
		return;
	}
	APlayerPawn *pmo = player->mo;

	damage = 40+(pr_fpatk()&15);
	power = 2*FRACUNIT;
	pufftype = RUNTIME_CLASS(APunchPuff);
	for (i = 0; i < 16; i++)
	{
		angle = pmo->angle + i*(ANG45/16);
		slope = P_AimLineAttack (pmo, angle, 2*MELEERANGE);
		if (linetarget)
		{
			pmo->special1++;
			if (pmo->special1 == 3)
			{
				damage <<= 1;
				power = 6*FRACUNIT;
				pufftype = RUNTIME_CLASS(AHammerPuff);
			}
			P_LineAttack (pmo, angle, 2*MELEERANGE, slope, damage, NAME_Melee, pufftype, true);
			if (linetarget->flags3&MF3_ISMONSTER || linetarget->player)
			{
				P_ThrustMobj (linetarget, angle, power);
			}
			AdjustPlayerAngle (pmo);
			goto punchdone;
		}
		angle = pmo->angle-i * (ANG45/16);
		slope = P_AimLineAttack (pmo, angle, 2*MELEERANGE);
		if (linetarget)
		{
			pmo->special1++;
			if (pmo->special1 == 3)
			{
				damage <<= 1;
				power = 6*FRACUNIT;
				pufftype = RUNTIME_CLASS(AHammerPuff);
			}
			P_LineAttack (pmo, angle, 2*MELEERANGE, slope, damage, NAME_Melee, pufftype, true);
			if (linetarget->flags3&MF3_ISMONSTER || linetarget->player)
			{
				P_ThrustMobj (linetarget, angle, power);
			}
			AdjustPlayerAngle (pmo);
			goto punchdone;
		}
	}
	// didn't find any creatures, so try to strike any walls
	pmo->special1 = 0;

	angle = pmo->angle;
	slope = P_AimLineAttack (pmo, angle, MELEERANGE);
	P_LineAttack (pmo, angle, MELEERANGE, slope, damage, NAME_Melee, pufftype, true);

punchdone:
	if (pmo->special1 == 3)
	{
		pmo->special1 = 0;
		P_SetPsprite (player, ps_weapon, &AFWeapFist::States[S_PUNCHATK2]);
		S_Sound (pmo, CHAN_VOICE, "*fistgrunt", 1, ATTN_NORM);
	}
	return;		
}

