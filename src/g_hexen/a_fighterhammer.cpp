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

const fixed_t HAMMER_RANGE = MELEERANGE+MELEERANGE/2;

static FRandom pr_atk ("FHammerAtk");

extern void AdjustPlayerAngle (AActor *pmo);

void A_FHammerAttack (AActor *actor, pspdef_t *psp);
void A_FHammerThrow (AActor *actor, pspdef_t *psp);
void A_HammerSound (AActor *);

// The Fighter's Hammer -----------------------------------------------------

class AFWeapHammer : public AFighterWeapon
{
	DECLARE_ACTOR (AFWeapHammer, AFighterWeapon)
public:
	weapontype_t OldStyleID () const
	{
		return wp_fhammer;
	}

	static FWeaponInfo WeaponInfo;

protected:
	const char *PickupMessage ()
	{
		return GStrings (TXT_WEAPON_F3);
	}
};

FState AFWeapHammer::States[] =
{
#define S_HAMM 0
	S_NORMAL (WFHM, 'A',   -1, NULL					    , NULL),

#define S_FHAMMERREADY (S_HAMM+1)
	S_NORMAL (FHMR, 'A',	1, A_WeaponReady		    , &States[S_FHAMMERREADY]),

#define S_FHAMMERDOWN (S_FHAMMERREADY+1)
	S_NORMAL (FHMR, 'A',	1, A_Lower				    , &States[S_FHAMMERDOWN]),

#define S_FHAMMERUP (S_FHAMMERDOWN+1)
	S_NORMAL (FHMR, 'A',	1, A_Raise				    , &States[S_FHAMMERUP]),

#define S_FHAMMERATK (S_FHAMMERUP+1)
	S_NORMAL2 (FHMR, 'B',	6, NULL					    , &States[S_FHAMMERATK+1], 5, 0),
	S_NORMAL2 (FHMR, 'C',	3, A_FHammerAttack		    , &States[S_FHAMMERATK+2], 5, 0),
	S_NORMAL2 (FHMR, 'D',	3, NULL					    , &States[S_FHAMMERATK+3], 5, 0),
	S_NORMAL2 (FHMR, 'E',	2, NULL					    , &States[S_FHAMMERATK+4], 5, 0),
	S_NORMAL2 (FHMR, 'E',   10, A_FHammerThrow		    , &States[S_FHAMMERATK+5], 5, 150),
	S_NORMAL2 (FHMR, 'A',	1, NULL					    , &States[S_FHAMMERATK+6], 0, 60),
	S_NORMAL2 (FHMR, 'A',	1, NULL					    , &States[S_FHAMMERATK+7], 0, 55),
	S_NORMAL2 (FHMR, 'A',	1, NULL					    , &States[S_FHAMMERATK+8], 0, 50),
	S_NORMAL2 (FHMR, 'A',	1, NULL					    , &States[S_FHAMMERATK+9], 0, 45),
	S_NORMAL2 (FHMR, 'A',	1, NULL					    , &States[S_FHAMMERATK+10], 0, 40),
	S_NORMAL2 (FHMR, 'A',	1, NULL					    , &States[S_FHAMMERATK+11], 0, 35),
	S_NORMAL (FHMR, 'A',	1, NULL					    , &States[S_FHAMMERREADY]),
};

FWeaponInfo AFWeapHammer::WeaponInfo =
{
	0,
	MANA_NONE,
	MANA_2,
	3,
	25,
	&States[S_FHAMMERUP],
	&States[S_FHAMMERDOWN],
	&States[S_FHAMMERREADY],
	&States[S_FHAMMERATK],
	&States[S_FHAMMERATK],
	NULL,
	RUNTIME_CLASS(AFWeapHammer),
	150,
	-10*FRACUNIT,
	NULL,
	NULL,
	RUNTIME_CLASS(AFWeapHammer),
	-1
};

IMPLEMENT_ACTOR (AFWeapHammer, Hexen, 123, 28)
	PROP_Flags (MF_SPECIAL)
	PROP_SpawnState (S_HAMM)
END_DEFAULTS

WEAPON1 (wp_fhammer, AFWeapHammer)

// Hammer Missile -----------------------------------------------------------

class AHammerMissile : public AActor
{
	DECLARE_ACTOR (AHammerMissile, AActor)
public:
	void GetExplodeParms (int &damage, int &dist, bool &hurtSource);
};

FState AHammerMissile::States[] =
{
#define S_HAMMER_MISSILE_1 0
	S_BRIGHT (FHFX, 'A',	2, NULL					    , &States[S_HAMMER_MISSILE_1+1]),
	S_BRIGHT (FHFX, 'B',	2, A_HammerSound		    , &States[S_HAMMER_MISSILE_1+2]),
	S_BRIGHT (FHFX, 'C',	2, NULL					    , &States[S_HAMMER_MISSILE_1+3]),
	S_BRIGHT (FHFX, 'D',	2, NULL					    , &States[S_HAMMER_MISSILE_1+4]),
	S_BRIGHT (FHFX, 'E',	2, NULL					    , &States[S_HAMMER_MISSILE_1+5]),
	S_BRIGHT (FHFX, 'F',	2, NULL					    , &States[S_HAMMER_MISSILE_1+6]),
	S_BRIGHT (FHFX, 'G',	2, NULL					    , &States[S_HAMMER_MISSILE_1+7]),
	S_BRIGHT (FHFX, 'H',	2, NULL					    , &States[S_HAMMER_MISSILE_1]),

#define S_HAMMER_MISSILE_X1 (S_HAMMER_MISSILE_1+8)
	S_BRIGHT (FHFX, 'I',	3, NULL					    , &States[S_HAMMER_MISSILE_X1+1]),
	S_BRIGHT (FHFX, 'J',	3, NULL					    , &States[S_HAMMER_MISSILE_X1+2]),
	S_BRIGHT (FHFX, 'K',	3, A_Explode			    , &States[S_HAMMER_MISSILE_X1+3]),
	S_BRIGHT (FHFX, 'L',	3, NULL					    , &States[S_HAMMER_MISSILE_X1+4]),
	S_BRIGHT (FHFX, 'M',	3, NULL					    , &States[S_HAMMER_MISSILE_X1+5]),
	S_NORMAL (FHFX, 'N',	3, NULL					    , &States[S_HAMMER_MISSILE_X1+6]),
	S_BRIGHT (FHFX, 'O',	3, NULL					    , &States[S_HAMMER_MISSILE_X1+7]),
	S_BRIGHT (FHFX, 'P',	3, NULL					    , &States[S_HAMMER_MISSILE_X1+8]),
	S_BRIGHT (FHFX, 'Q',	3, NULL					    , &States[S_HAMMER_MISSILE_X1+9]),
	S_BRIGHT (FHFX, 'R',	3, NULL					    , NULL),
};

IMPLEMENT_ACTOR (AHammerMissile, Hexen, -1, 0)
	PROP_SpeedFixed (25)
	PROP_RadiusFixed (14)
	PROP_HeightFixed (20)
	PROP_Damage (10)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY|MF_DROPOFF|MF_MISSILE)
	PROP_Flags2 (MF2_NOTELEPORT|MF2_FIREDAMAGE|MF2_IMPACT|MF2_PCROSS)

	PROP_SpawnState (S_HAMMER_MISSILE_1)
	PROP_DeathState (S_HAMMER_MISSILE_X1)

	PROP_DeathSound ("FighterHammerExplode")
END_DEFAULTS

void AHammerMissile::GetExplodeParms (int &damage, int &dist, bool &hurtSource)
{
	damage = 128;
	hurtSource = false;
}

// Hammer Puff (also used by fist) ------------------------------------------

FState AHammerPuff::States[] =
{
	S_NORMAL (FHFX, 'S',	4, NULL 					, &States[1]),
	S_NORMAL (FHFX, 'T',	4, NULL 					, &States[2]),
	S_NORMAL (FHFX, 'U',	4, NULL 					, &States[3]),
	S_NORMAL (FHFX, 'V',	4, NULL 					, &States[4]),
	S_NORMAL (FHFX, 'W',	4, NULL 					, NULL),
};

IMPLEMENT_ACTOR (AHammerPuff, Hexen, -1, 0)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY)
	PROP_Flags3 (MF3_PUFFONACTORS)
	PROP_RenderStyle (STYLE_Translucent)
	PROP_Alpha (HX_SHADOW)

	PROP_SpawnState (0)

	PROP_SeeSound ("FighterHammerHitThing")
	PROP_AttackSound ("FighterHammerHitWall")
	PROP_ActiveSound ("FighterHammerMiss")
END_DEFAULTS

void AHammerPuff::BeginPlay ()
{
	Super::BeginPlay ();
	momz = FRACUNIT*8/10;
}

//============================================================================
//
// A_FHammerAttack
//
//============================================================================

void A_FHammerAttack (AActor *actor, pspdef_t *psp)
{
	angle_t angle;
	int damage;
	fixed_t power;
	int slope;
	int i;
	player_t *player;

	if (NULL == (player = actor->player))
	{
		return;
	}
	AActor *pmo=player->mo;

	damage = 60+(pr_atk()&63);
	power = 10*FRACUNIT;
	PuffType = RUNTIME_CLASS(AHammerPuff);
	for (i = 0; i < 16; i++)
	{
		angle = pmo->angle + i*(ANG45/32);
		slope = P_AimLineAttack (pmo, angle, HAMMER_RANGE);
		if (linetarget)
		{
			P_LineAttack (pmo, angle, HAMMER_RANGE, slope, damage);
			AdjustPlayerAngle(pmo);
			if (linetarget->flags3&MF3_ISMONSTER || linetarget->player)
			{
				P_ThrustMobj (linetarget, angle, power);
			}
			pmo->special1 = false; // Don't throw a hammer
			goto hammerdone;
		}
		angle = pmo->angle-i*(ANG45/32);
		slope = P_AimLineAttack(pmo, angle, HAMMER_RANGE);
		if(linetarget)
		{
			P_LineAttack(pmo, angle, HAMMER_RANGE, slope, damage);
			AdjustPlayerAngle(pmo);
			if (linetarget->flags3&MF3_ISMONSTER || linetarget->player)
			{
				P_ThrustMobj(linetarget, angle, power);
			}
			pmo->special1 = false; // Don't throw a hammer
			goto hammerdone;
		}
	}
	// didn't find any targets in meleerange, so set to throw out a hammer
	PuffSpawned = NULL;
	angle = pmo->angle;
	slope = P_AimLineAttack (pmo, angle, HAMMER_RANGE);
	P_LineAttack (pmo, angle, HAMMER_RANGE, slope, damage);
	if (PuffSpawned)
	{
		pmo->special1 = false;
	}
	else
	{
		pmo->special1 = true;
	}
hammerdone:
	if (player->ammo[MANA_2] < wpnlev1info[player->readyweapon]->ammouse)
	{ // Don't spawn a hammer if the player doesn't have enough mana
		pmo->special1 = false;
	}
	return;		
}

//============================================================================
//
// A_FHammerThrow
//
//============================================================================

void A_FHammerThrow (AActor *actor, pspdef_t *psp)
{
	AActor *mo;
	player_t *player;

	if (NULL == (player = actor->player))
	{
		return;
	}

	if (!player->mo->special1)
	{
		return;
	}
	if (!(dmflags & DF_INFINITE_AMMO))
	{
		player->ammo[MANA_2] -= wpnlev1info[player->readyweapon]->ammouse;
	}
	mo = P_SpawnPlayerMissile (player->mo, RUNTIME_CLASS(AHammerMissile)); 
	if (mo)
	{
		mo->special1 = 0;
	}	
}

//============================================================================
//
// A_HammerSound
//
//============================================================================

void A_HammerSound (AActor *actor)
{
	S_Sound (actor, CHAN_BODY, "FighterHammerContinuous", 1, ATTN_NORM);
}
