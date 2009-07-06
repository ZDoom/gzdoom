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

static FRandom pr_staffcheck ("CStaffCheck");
static FRandom pr_blink ("CStaffBlink");

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

DEFINE_ACTION_FUNCTION(AActor, A_CStaffCheck)
{
	AActor *pmo;
	int damage;
	int newLife;
	angle_t angle;
	int slope;
	int i;
	player_t *player;
	AActor *linetarget;

	if (NULL == (player = self->player))
	{
		return;
	}
	AWeapon *weapon = self->player->ReadyWeapon;

	pmo = player->mo;
	damage = 20+(pr_staffcheck()&15);
	for (i = 0; i < 3; i++)
	{
		angle = pmo->angle+i*(ANG45/16);
		slope = P_AimLineAttack (pmo, angle, fixed_t(1.5*MELEERANGE), &linetarget, 0, false, true);
		if (linetarget)
		{
			P_LineAttack (pmo, angle, fixed_t(1.5*MELEERANGE), slope, damage, NAME_Melee, PClass::FindClass ("CStaffPuff"));
			pmo->angle = R_PointToAngle2 (pmo->x, pmo->y, 
				linetarget->x, linetarget->y);
			if (((linetarget->player && (!linetarget->IsTeammate (pmo) || level.teamdamage != 0))|| linetarget->flags3&MF3_ISMONSTER)
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
		slope = P_AimLineAttack (player->mo, angle, fixed_t(1.5*MELEERANGE), &linetarget, 0, false, true);
		if (linetarget)
		{
			P_LineAttack (pmo, angle, fixed_t(1.5*MELEERANGE), slope, damage, NAME_Melee, PClass::FindClass ("CStaffPuff"));
			pmo->angle = R_PointToAngle2 (pmo->x, pmo->y, 
				linetarget->x, linetarget->y);
			if ((linetarget->player && (!linetarget->IsTeammate (pmo) || level.teamdamage != 0)) || linetarget->flags3&MF3_ISMONSTER)
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

DEFINE_ACTION_FUNCTION(AActor, A_CStaffAttack)
{
	AActor *mo;
	player_t *player;

	if (NULL == (player = self->player))
	{
		return;
	}

	AWeapon *weapon = self->player->ReadyWeapon;
	if (weapon != NULL)
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire))
			return;
	}
	mo = P_SpawnPlayerMissile (self, RUNTIME_CLASS(ACStaffMissile), self->angle-(ANG45/15));
	if (mo)
	{
		mo->special2 = 32;
	}
	mo = P_SpawnPlayerMissile (self, RUNTIME_CLASS(ACStaffMissile), self->angle+(ANG45/15));
	if (mo)
	{
		mo->special2 = 0;
	}
	S_Sound (self, CHAN_WEAPON, "ClericCStaffFire", 1, ATTN_NORM);
}

//============================================================================
//
// A_CStaffMissileSlither
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_CStaffMissileSlither)
{
	fixed_t newX, newY;
	int weaveXY;
	int angle;

	weaveXY = self->special2;
	angle = (self->angle+ANG90)>>ANGLETOFINESHIFT;
	newX = self->x-FixedMul(finecosine[angle], FloatBobOffsets[weaveXY]);
	newY = self->y-FixedMul(finesine[angle], FloatBobOffsets[weaveXY]);
	weaveXY = (weaveXY+3)&63;
	newX += FixedMul(finecosine[angle], FloatBobOffsets[weaveXY]);
	newY += FixedMul(finesine[angle], FloatBobOffsets[weaveXY]);
	P_TryMove (self, newX, newY, true);
	self->special2 = weaveXY;
}

//============================================================================
//
// A_CStaffInitBlink
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_CStaffInitBlink)
{
	self->special1 = (pr_blink()>>1)+20;
}

//============================================================================
//
// A_CStaffCheckBlink
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_CStaffCheckBlink)
{
	if (self->player && self->player->ReadyWeapon)
	{
		if (!--self->special1)
		{
			P_SetPsprite (self->player, ps_weapon, self->player->ReadyWeapon->FindState ("Blink"));
			self->special1 = (pr_blink()+50)>>2;
		}
		else 
		{
			DoReadyWeaponToFire(self);
			DoReadyWeaponToBob(self);
		}
	}
}
