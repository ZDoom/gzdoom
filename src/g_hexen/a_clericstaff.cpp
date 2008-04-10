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

static FRandom pr_staffcheck ("CStaffCheck");
static FRandom pr_blink ("CStaffBlink");

void A_CStaffInitBlink (AActor *actor);
void A_CStaffCheckBlink (AActor *actor);
void A_CStaffCheck (AActor *actor);
void A_CStaffAttack (AActor *actor);
void A_CStaffMissileSlither (AActor *);

// The Cleric's Serpent Staff -----------------------------------------------

class ACWeapStaff : public AClericWeapon
{
	DECLARE_ACTOR (ACWeapStaff, AClericWeapon)
};

FState ACWeapStaff::States[] =
{
#define S_CSTAFF 0
	S_NORMAL (WCSS, 'A',   -1, NULL					    , NULL),

#define S_CSTAFFREADY (S_CSTAFF+1)
	S_NORMAL (CSSF, 'C',	4, NULL					    , &States[S_CSTAFFREADY+1]),
	S_NORMAL (CSSF, 'B',	3, A_CStaffInitBlink	    , &States[S_CSTAFFREADY+2]),
	S_NORMAL (CSSF, 'A',	1, A_WeaponReady		    , &States[S_CSTAFFREADY+3]),
	S_NORMAL (CSSF, 'A',	1, A_WeaponReady		    , &States[S_CSTAFFREADY+4]),
	S_NORMAL (CSSF, 'A',	1, A_WeaponReady		    , &States[S_CSTAFFREADY+5]),
	S_NORMAL (CSSF, 'A',	1, A_WeaponReady		    , &States[S_CSTAFFREADY+6]),
	S_NORMAL (CSSF, 'A',	1, A_WeaponReady		    , &States[S_CSTAFFREADY+7]),
	S_NORMAL (CSSF, 'A',	1, A_WeaponReady		    , &States[S_CSTAFFREADY+8]),
	S_NORMAL (CSSF, 'A',	1, A_WeaponReady		    , &States[S_CSTAFFREADY+9]),
	S_NORMAL (CSSF, 'A',	1, A_CStaffCheckBlink	    , &States[S_CSTAFFREADY+2]),

#define S_CSTAFFBLINK (S_CSTAFFREADY+10)
	S_NORMAL (CSSF, 'B',	1, A_WeaponReady		    , &States[S_CSTAFFBLINK+1]),
	S_NORMAL (CSSF, 'B',	1, A_WeaponReady		    , &States[S_CSTAFFBLINK+2]),
	S_NORMAL (CSSF, 'B',	1, A_WeaponReady		    , &States[S_CSTAFFBLINK+3]),
	S_NORMAL (CSSF, 'C',	1, A_WeaponReady		    , &States[S_CSTAFFBLINK+4]),
	S_NORMAL (CSSF, 'C',	1, A_WeaponReady		    , &States[S_CSTAFFBLINK+5]),
	S_NORMAL (CSSF, 'C',	1, A_WeaponReady		    , &States[S_CSTAFFBLINK+6]),
	S_NORMAL (CSSF, 'C',	1, A_WeaponReady		    , &States[S_CSTAFFBLINK+7]),
	S_NORMAL (CSSF, 'C',	1, A_WeaponReady		    , &States[S_CSTAFFBLINK+8]),
	S_NORMAL (CSSF, 'B',	1, A_WeaponReady		    , &States[S_CSTAFFBLINK+9]),
	S_NORMAL (CSSF, 'B',	1, A_WeaponReady		    , &States[S_CSTAFFBLINK+10]),
	S_NORMAL (CSSF, 'B',	1, A_WeaponReady		    , &States[S_CSTAFFREADY+2]),

#define S_CSTAFFDOWN (S_CSTAFFBLINK+11)
	S_NORMAL (CSSF, 'B',	3, NULL					    , &States[S_CSTAFFDOWN+1]),
	S_NORMAL (CSSF, 'C',	4, NULL					    , &States[S_CSTAFFDOWN+2]),
	S_NORMAL (CSSF, 'C',	1, A_Lower				    , &States[S_CSTAFFDOWN+2]),

#define S_CSTAFFUP (S_CSTAFFDOWN+3)
	S_NORMAL (CSSF, 'C',	1, A_Raise				    , &States[S_CSTAFFUP]),

#define S_CSTAFFATK (S_CSTAFFUP+1)
	S_NORMAL2 (CSSF, 'A',	1, A_CStaffCheck		    , &States[S_CSTAFFATK+1], 0, 45),
	S_NORMAL2 (CSSF, 'J',	1, A_CStaffAttack		    , &States[S_CSTAFFATK+2], 0, 50),
	S_NORMAL2 (CSSF, 'J',	2, NULL					    , &States[S_CSTAFFATK+3], 0, 50),
	S_NORMAL2 (CSSF, 'J',	2, NULL					    , &States[S_CSTAFFATK+4], 0, 45),
	S_NORMAL2 (CSSF, 'A',	2, NULL					    , &States[S_CSTAFFATK+5], 0, 40),
	S_NORMAL2 (CSSF, 'A',	2, NULL					    , &States[S_CSTAFFREADY+2], 0, 36),

#define S_CSTAFFATK2 (S_CSTAFFATK+6)
	S_NORMAL2 (CSSF, 'K',   10, NULL				    , &States[S_CSTAFFREADY+2], 0, 36),
};

IMPLEMENT_ACTOR (ACWeapStaff, Hexen, 10, 32)
	PROP_Flags (MF_SPECIAL)
	PROP_SpawnState (S_CSTAFF)

	PROP_Weapon_SelectionOrder (1600)
	PROP_Weapon_AmmoUse1 (1)
	PROP_Weapon_AmmoGive1 (25)
	PROP_Weapon_UpState (S_CSTAFFUP)
	PROP_Weapon_DownState (S_CSTAFFDOWN)
	PROP_Weapon_ReadyState (S_CSTAFFREADY)
	PROP_Weapon_AtkState (S_CSTAFFATK)
	PROP_Weapon_Kickback (150)
	PROP_Weapon_YAdjust (10)
	PROP_Weapon_MoveCombatDist (25000000)
	PROP_Weapon_AmmoType1 ("Mana1")
	PROP_Weapon_ProjectileType ("CStaffMissile")
	PROP_Inventory_PickupMessage("$TXT_WEAPON_C2")
END_DEFAULTS

// Serpent Staff Missile ----------------------------------------------------

class ACStaffMissile : public AActor
{
	DECLARE_ACTOR (ACStaffMissile, AActor)
public:
	int DoSpecialDamage (AActor *target, int damage);
};

FState ACStaffMissile::States[] =
{
#define S_CSTAFF_MISSILE1 0
	S_BRIGHT (CSSF, 'D',	1, A_CStaffMissileSlither   , &States[S_CSTAFF_MISSILE1+1]),
	S_BRIGHT (CSSF, 'D',	1, A_CStaffMissileSlither   , &States[S_CSTAFF_MISSILE1+2]),
	S_BRIGHT (CSSF, 'E',	1, A_CStaffMissileSlither   , &States[S_CSTAFF_MISSILE1+3]),
	S_BRIGHT (CSSF, 'E',	1, A_CStaffMissileSlither   , &States[S_CSTAFF_MISSILE1]),

#define S_CSTAFF_MISSILE_X1 (S_CSTAFF_MISSILE1+4)
	S_BRIGHT (CSSF, 'F',	4, NULL					    , &States[S_CSTAFF_MISSILE_X1+1]),
	S_BRIGHT (CSSF, 'G',	4, NULL					    , &States[S_CSTAFF_MISSILE_X1+2]),
	S_BRIGHT (CSSF, 'H',	3, NULL					    , &States[S_CSTAFF_MISSILE_X1+3]),
	S_BRIGHT (CSSF, 'I',	3, NULL					    , NULL),
};

IMPLEMENT_ACTOR (ACStaffMissile, Hexen, -1, 0)
	PROP_SpeedFixed (22)
	PROP_RadiusFixed (12)
	PROP_HeightFixed (10)
	PROP_Damage (5)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY|MF_DROPOFF|MF_MISSILE)
	PROP_Flags2 (MF2_NOTELEPORT|MF2_IMPACT|MF2_PCROSS)
	PROP_RenderStyle (STYLE_Add)

	PROP_SpawnState (S_CSTAFF_MISSILE1)
	PROP_DeathState (S_CSTAFF_MISSILE_X1)

	PROP_DeathSound ("ClericCStaffExplode")
END_DEFAULTS

int ACStaffMissile::DoSpecialDamage (AActor *target, int damage)
{
	// Cleric Serpent Staff does poison damage
	if (target->player)
	{
		P_PoisonPlayer (target->player, this, this->target, 20);
		damage >>= 1;
	}
	return damage;
}

// Serpent Staff Puff -------------------------------------------------------

class ACStaffPuff : public AActor
{
	DECLARE_ACTOR (ACStaffPuff, AActor)
};

FState ACStaffPuff::States[] =
{
	S_NORMAL (FHFX, 'S',	4, NULL					    , &States[1]),
	S_NORMAL (FHFX, 'T',	4, NULL					    , &States[2]),
	S_NORMAL (FHFX, 'U',	4, NULL					    , &States[3]),
	S_NORMAL (FHFX, 'V',	4, NULL					    , &States[4]),
	S_NORMAL (FHFX, 'W',	4, NULL					    , NULL),
};

IMPLEMENT_ACTOR (ACStaffPuff, Hexen, -1, 0)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY)
	PROP_Flags3 (MF3_PUFFONACTORS)
	PROP_RenderStyle (STYLE_Translucent)
	PROP_Alpha (HX_SHADOW)

	PROP_SpawnState (0)

	PROP_SeeSound ("ClericCStaffHitThing")
END_DEFAULTS

//============================================================================
//
// A_CStaffCheck
//
//============================================================================

void A_CStaffCheck (AActor *actor)
{
	AActor *pmo;
	int damage;
	int newLife;
	angle_t angle;
	int slope;
	int i;
	player_t *player;
	AActor *linetarget;

	if (NULL == (player = actor->player))
	{
		return;
	}
	AWeapon *weapon = actor->player->ReadyWeapon;

	pmo = player->mo;
	damage = 20+(pr_staffcheck()&15);
	for (i = 0; i < 3; i++)
	{
		angle = pmo->angle+i*(ANG45/16);
		slope = P_AimLineAttack (pmo, angle, fixed_t(1.5*MELEERANGE), &linetarget);
		if (linetarget)
		{
			P_LineAttack (pmo, angle, fixed_t(1.5*MELEERANGE), slope, damage, NAME_Melee, RUNTIME_CLASS(ACStaffPuff));
			pmo->angle = R_PointToAngle2 (pmo->x, pmo->y, 
				linetarget->x, linetarget->y);
			if ((linetarget->player || linetarget->flags3&MF3_ISMONSTER)
				&& (!(linetarget->flags2&(MF2_DORMANT+MF2_INVULNERABLE))))
			{
				newLife = player->health+(damage>>3);
				newLife = newLife > 100 ? 100 : newLife;
				if (newLife > player->health)
				{
					pmo->health = player->health = newLife;
				}
				P_SetPsprite (player, ps_weapon, &ACWeapStaff::States[S_CSTAFFATK2]);
			}
			if (weapon != NULL)
			{
				weapon->DepleteAmmo (weapon->bAltFire, false);
			}
			break;
		}
		angle = pmo->angle-i*(ANG45/16);
		slope = P_AimLineAttack (player->mo, angle, fixed_t(1.5*MELEERANGE), &linetarget);
		if (linetarget)
		{
			P_LineAttack (pmo, angle, fixed_t(1.5*MELEERANGE), slope, damage, NAME_Melee, RUNTIME_CLASS(ACStaffPuff));
			pmo->angle = R_PointToAngle2 (pmo->x, pmo->y, 
				linetarget->x, linetarget->y);
			if (linetarget->player || linetarget->flags3&MF3_ISMONSTER)
			{
				newLife = player->health+(damage>>4);
				newLife = newLife > 100 ? 100 : newLife;
				pmo->health = player->health = newLife;
				P_SetPsprite (player, ps_weapon, &ACWeapStaff::States[S_CSTAFFATK2]);
			}
			weapon->DepleteAmmo (weapon->bAltFire, false);
			break;
		}
	}
}

//============================================================================
//
// A_CStaffAttack
//
//============================================================================

void A_CStaffAttack (AActor *actor)
{
	AActor *mo;
	player_t *player;

	if (NULL == (player = actor->player))
	{
		return;
	}

	AWeapon *weapon = actor->player->ReadyWeapon;
	if (weapon != NULL)
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire))
			return;
	}
	mo = P_SpawnPlayerMissile (actor, RUNTIME_CLASS(ACStaffMissile), actor->angle-(ANG45/15));
	if (mo)
	{
		mo->special2 = 32;
	}
	mo = P_SpawnPlayerMissile (actor, RUNTIME_CLASS(ACStaffMissile), actor->angle+(ANG45/15));
	if (mo)
	{
		mo->special2 = 0;
	}
	S_Sound (actor, CHAN_WEAPON, "ClericCStaffFire", 1, ATTN_NORM);
}

//============================================================================
//
// A_CStaffMissileSlither
//
//============================================================================

void A_CStaffMissileSlither (AActor *actor)
{
	fixed_t newX, newY;
	int weaveXY;
	int angle;

	weaveXY = actor->special2;
	angle = (actor->angle+ANG90)>>ANGLETOFINESHIFT;
	newX = actor->x-FixedMul(finecosine[angle], FloatBobOffsets[weaveXY]);
	newY = actor->y-FixedMul(finesine[angle], FloatBobOffsets[weaveXY]);
	weaveXY = (weaveXY+3)&63;
	newX += FixedMul(finecosine[angle], FloatBobOffsets[weaveXY]);
	newY += FixedMul(finesine[angle], FloatBobOffsets[weaveXY]);
	P_TryMove (actor, newX, newY, true);
	actor->special2 = weaveXY;
}

//============================================================================
//
// A_CStaffInitBlink
//
//============================================================================

void A_CStaffInitBlink (AActor *actor)
{
	actor->special1 = (pr_blink()>>1)+20;
}

//============================================================================
//
// A_CStaffCheckBlink
//
//============================================================================

void A_CStaffCheckBlink (AActor *actor)
{
	if (!--actor->special1)
	{
		P_SetPsprite (actor->player, ps_weapon, &ACWeapStaff::States[S_CSTAFFBLINK]);
		actor->special1 = (pr_blink()+50)>>2;
	}
	else 
	{
		A_WeaponReady (actor);
	}
}
