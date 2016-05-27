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
	int DoSpecialDamage (AActor *target, int damage, FName damagetype);
};

IMPLEMENT_CLASS (ACStaffMissile)

int ACStaffMissile::DoSpecialDamage (AActor *target, int damage, FName damagetype)
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
	PARAM_ACTION_PROLOGUE;

	APlayerPawn *pmo;
	int damage;
	int newLife, max;
	DAngle angle;
	DAngle slope;
	int i;
	player_t *player;
	FTranslatedLineTarget t;
	PClassActor *puff;

	if (nullptr == (player = self->player))
	{
		return 0;
	}
	AWeapon *weapon = self->player->ReadyWeapon;

	pmo = player->mo;
	damage = 20 + (pr_staffcheck() & 15);
	max = pmo->GetMaxHealth();
	puff = PClass::FindActor("CStaffPuff");
	for (i = 0; i < 3; i++)
	{
		for (int j = 1; j >= -1; j -= 2)
		{
			angle = pmo->Angles.Yaw + j*i*(45. / 16);
			slope = P_AimLineAttack(pmo, angle, 1.5 * MELEERANGE, &t, 0., ALF_CHECK3D);
			if (t.linetarget)
			{
				P_LineAttack(pmo, angle, 1.5 * MELEERANGE, slope, damage, NAME_Melee, puff, false, &t);
				if (t.linetarget != nullptr)
				{
					pmo->Angles.Yaw = t.angleFromSource;
					if (((t.linetarget->player && (!t.linetarget->IsTeammate(pmo) || level.teamdamage != 0)) || t.linetarget->flags3&MF3_ISMONSTER)
						&& (!(t.linetarget->flags2&(MF2_DORMANT | MF2_INVULNERABLE))))
					{
						newLife = player->health + (damage >> 3);
						newLife = newLife > max ? max : newLife;
						if (newLife > player->health)
						{
							pmo->health = player->health = newLife;
						}
						if (weapon != nullptr)
						{
							FState * newstate = weapon->FindState("Drain");
							if (newstate != nullptr) P_SetPsprite(player, PSP_WEAPON, newstate);
						}
					}
					if (weapon != nullptr)
					{
						weapon->DepleteAmmo(weapon->bAltFire, false);
					}
				}
				return 0;
			}
		}
	}
	return 0;
}

//============================================================================
//
// A_CStaffAttack
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_CStaffAttack)
{
	PARAM_ACTION_PROLOGUE;

	AActor *mo;
	player_t *player;

	if (NULL == (player = self->player))
	{
		return 0;
	}

	AWeapon *weapon = self->player->ReadyWeapon;
	if (weapon != NULL)
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire))
			return 0;
	}
	mo = P_SpawnPlayerMissile (self, RUNTIME_CLASS(ACStaffMissile), self->Angles.Yaw - 3.0);
	if (mo)
	{
		mo->WeaveIndexXY = 32;
	}
	mo = P_SpawnPlayerMissile (self, RUNTIME_CLASS(ACStaffMissile), self->Angles.Yaw + 3.0);
	if (mo)
	{
		mo->WeaveIndexXY = 0;
	}
	S_Sound (self, CHAN_WEAPON, "ClericCStaffFire", 1, ATTN_NORM);
	return 0;
}

//============================================================================
//
// A_CStaffMissileSlither
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_CStaffMissileSlither)
{
	PARAM_ACTION_PROLOGUE;

	A_Weave(self, 3, 0, 1., 0.);
	return 0;
}

//============================================================================
//
// A_CStaffInitBlink
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_CStaffInitBlink)
{
	PARAM_ACTION_PROLOGUE;

	self->weaponspecial = (pr_blink()>>1)+20;
	return 0;
}

//============================================================================
//
// A_CStaffCheckBlink
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_CStaffCheckBlink)
{
	PARAM_ACTION_PROLOGUE;

	if (self->player && self->player->ReadyWeapon)
	{
		if (!--self->weaponspecial)
		{
			P_SetPsprite(self->player, PSP_WEAPON, self->player->ReadyWeapon->FindState ("Blink"));
			self->weaponspecial = (pr_blink()+50)>>2;
		}
		else 
		{
			DoReadyWeapon(self);
		}
	}
	return 0;
}
