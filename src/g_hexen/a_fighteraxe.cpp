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

#define AXERANGE	((fixed_t)(2.25*MELEERANGE))

static FRandom pr_atk ("FAxeAtk");
static FRandom pr_splat ("FAxeSplatter");

void A_FAxeCheckReady (AActor *actor, pspdef_t *psp);
void A_FAxeCheckUp (AActor *actor, pspdef_t *psp);
void A_FAxeCheckAtk (AActor *actor, pspdef_t *psp);
void A_FAxeCheckReadyG (AActor *actor, pspdef_t *psp);
void A_FAxeCheckUpG (AActor *actor, pspdef_t *psp);
void A_FAxeAttack (AActor *actor, pspdef_t *psp);

extern void AdjustPlayerAngle (AActor *pmo);

// The Fighter's Axe --------------------------------------------------------

class AFWeapAxe : public AFighterWeapon
{
	DECLARE_ACTOR (AFWeapAxe, AFighterWeapon)
public:
	weapontype_t OldStyleID () const
	{
		return wp_faxe;
	}
	static FWeaponInfo WeaponInfo;

protected:
	const char *PickupMessage ()
	{
		return GStrings (TXT_WEAPON_F2);
	}
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

FWeaponInfo AFWeapAxe::WeaponInfo =
{
	WIF_AXEBLOOD,
	MANA_NONE,
	MANA_1,
	2,
	25,
	&States[S_FAXEUP],
	&States[S_FAXEDOWN],
	&States[S_FAXEREADY],
	&States[S_FAXEATK],
	&States[S_FAXEATK],
	NULL,
	RUNTIME_CLASS(AFWeapAxe),
	150,
	-12*FRACUNIT,
	NULL,
	NULL,
	RUNTIME_CLASS(AFWeapAxe),
	-1
};

IMPLEMENT_ACTOR (AFWeapAxe, Hexen, 8010, 27)
	PROP_Flags (MF_SPECIAL)
	PROP_SpawnState (S_AXE)
END_DEFAULTS

WEAPON1 (wp_faxe, AFWeapAxe)

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

// Axe Blood ----------------------------------------------------------------

class AAxeBlood : public AActor
{
	DECLARE_ACTOR (AAxeBlood, AActor)
};

FState AAxeBlood::States[] =
{
	S_NORMAL (FAXE, 'F',	3, NULL					    , &States[1]),
	S_NORMAL (FAXE, 'G',	3, NULL					    , &States[2]),
	S_NORMAL (FAXE, 'H',	3, NULL					    , &States[3]),
	S_NORMAL (FAXE, 'I',	3, NULL					    , &States[4]),
	S_NORMAL (FAXE, 'J',	3, NULL					    , &States[5]),
	S_NORMAL (FAXE, 'K',	3, NULL					    , NULL),
};

IMPLEMENT_ACTOR (AAxeBlood, Hexen, -1, 0)
	PROP_RadiusFixed (2)
	PROP_HeightFixed (4)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY|MF_DROPOFF)
	PROP_Flags2 (MF2_NOTELEPORT|MF2_CANNOTPUSH)

	PROP_SpawnState (0)
	PROP_DeathState (5)
END_DEFAULTS

//============================================================================
//
// A_FAxeCheckReady
//
//============================================================================

void A_FAxeCheckReady (AActor *actor, pspdef_t *psp)
{
	player_t *player;

	if (NULL == (player = actor->player))
	{
		return;
	}
	if (player->ammo[MANA_1])
	{
		P_SetPsprite (player, ps_weapon, &AFWeapAxe::States[S_FAXEREADY_G]);
	}
	else
	{
		A_WeaponReady (actor, psp);
	}
}

//============================================================================
//
// A_FAxeCheckReadyG
//
//============================================================================

void A_FAxeCheckReadyG (AActor *actor, pspdef_t *psp)
{
	player_t *player;

	if (NULL == (player = actor->player))
	{
		return;
	}
	if (player->ammo[MANA_1] <= 0)
	{
		P_SetPsprite (player, ps_weapon, &AFWeapAxe::States[S_FAXEREADY]);
	}
	else
	{
		A_WeaponReady (actor, psp);
	}
}

//============================================================================
//
// A_FAxeCheckUp
//
//============================================================================

void A_FAxeCheckUp (AActor *actor, pspdef_t *psp)
{
	player_t *player;

	if (NULL == (player = actor->player))
	{
		return;
	}
	if (player->ammo[MANA_1])
	{
		P_SetPsprite (player, ps_weapon, &AFWeapAxe::States[S_FAXEUP_G]);
	}
	else
	{
		A_Raise (actor, psp);
	}
}

//============================================================================
//
// A_FAxeCheckUpG
//
//============================================================================

void A_FAxeCheckUpG (AActor *actor, pspdef_t *psp)
{
	player_t *player;

	if (NULL == (player = actor->player))
	{
		return;
	}
	if (player->ammo[MANA_1] <= 0)
	{
		P_SetPsprite (player, ps_weapon, &AFWeapAxe::States[S_FAXEUP]);
	}
	else
	{
		A_Raise (actor, psp);
	}
}

//============================================================================
//
// A_FAxeCheckAtk
//
//============================================================================

void A_FAxeCheckAtk (AActor *actor, pspdef_t *psp)
{
	player_t *player;

	if (NULL == (player = actor->player))
	{
		return;
	}
	if (player->ammo[MANA_1])
	{
		P_SetPsprite (player, ps_weapon, &AFWeapAxe::States[S_FAXEATK_G]);
	}
}

//============================================================================
//
// A_FAxeAttack
//
//============================================================================

void A_FAxeAttack (AActor *actor, pspdef_t *psp)
{
	angle_t angle;
	fixed_t power;
	int damage;
	int slope;
	int i;
	int useMana;
	player_t *player;

	if (NULL == (player = actor->player))
	{
		return;
	}
	AActor *pmo=player->mo;

	damage = 40+(pr_atk()&15);
	damage += pr_atk()&7;
	power = 0;
	if (player->ammo[MANA_1] > 0)
	{
		damage <<= 1;
		power = 6*FRACUNIT;
		PuffType = RUNTIME_CLASS(AAxePuffGlow);
		useMana = 1;
	}
	else
	{
		PuffType = RUNTIME_CLASS(AAxePuff);
		useMana = 0;
	}
	for (i = 0; i < 16; i++)
	{
		angle = pmo->angle+i*(ANG45/16);
		slope = P_AimLineAttack (pmo, angle, AXERANGE);
		if (linetarget)
		{
			P_LineAttack (pmo, angle, AXERANGE, slope, damage);
			if (linetarget->flags3&MF3_ISMONSTER || linetarget->player)
			{
				P_ThrustMobj (linetarget, angle, power);
			}
			AdjustPlayerAngle (pmo);
			useMana++; 
			goto axedone;
		}
		angle = pmo->angle-i*(ANG45/16);
		slope = P_AimLineAttack (pmo, angle, AXERANGE);
		if (linetarget)
		{
			P_LineAttack (pmo, angle, AXERANGE, slope, damage);
			if (linetarget->flags3&MF3_ISMONSTER)
			{
				P_ThrustMobj (linetarget, angle, power);
			}
			AdjustPlayerAngle (pmo);
			useMana++; 
			goto axedone;
		}
	}
	// didn't find any creatures, so try to strike any walls
	pmo->special1 = 0;

	angle = pmo->angle;
	slope = P_AimLineAttack (pmo, angle, MELEERANGE);
	P_LineAttack (pmo, angle, MELEERANGE, slope, damage);

axedone:
	if (useMana == 2)
	{
		if (!(dmflags & DF_INFINITE_AMMO))
		{
			player->ammo[MANA_1] -= wpnlev1info[player->readyweapon]->ammouse;
			if (player->ammo[MANA_1] <= 0)
			{
				player->ammo[MANA_1] = 0;
				P_SetPsprite (player, ps_weapon, &AFWeapAxe::States[S_FAXEATK+5]);
			}
		}
	}
	return;		
}

//===========================================================================
//
//  P_BloodSplatter2
//
//===========================================================================

void P_BloodSplatter2 (fixed_t x, fixed_t y, fixed_t z, AActor *originator)
{
	AActor *mo;
	
	x += ((pr_splat()-128)<<11);
	y += ((pr_splat()-128)<<11);

	mo = Spawn<AAxeBlood> (x, y, z);
	mo->target = originator;
}
