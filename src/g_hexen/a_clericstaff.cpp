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

// Serpent Staff Missile ----------------------------------------------------

class ACStaffMissile : public AActor
{
	DECLARE_CLASS (ACStaffMissile, AActor)
public:
	int DoSpecialDamage (AActor *target, int damage);
};

IMPLEMENT_CLASS (ACStaffMissile)

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
			P_LineAttack (pmo, angle, fixed_t(1.5*MELEERANGE), slope, damage, NAME_Melee, PClass::FindClass ("CStaffPuff"));
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
				P_SetPsprite (player, ps_weapon, weapon->FindState ("Drain"));
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
			P_LineAttack (pmo, angle, fixed_t(1.5*MELEERANGE), slope, damage, NAME_Melee, PClass::FindClass ("CStaffPuff"));
			pmo->angle = R_PointToAngle2 (pmo->x, pmo->y, 
				linetarget->x, linetarget->y);
			if (linetarget->player || linetarget->flags3&MF3_ISMONSTER)
			{
				newLife = player->health+(damage>>4);
				newLife = newLife > 100 ? 100 : newLife;
				pmo->health = player->health = newLife;
				P_SetPsprite (player, ps_weapon, weapon->FindState ("Drain"));
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
		P_SetPsprite (actor->player, ps_weapon, actor->FindState ("Blink"));
		actor->special1 = (pr_blink()+50)>>2;
	}
	else 
	{
		A_WeaponReady (actor);
	}
}
