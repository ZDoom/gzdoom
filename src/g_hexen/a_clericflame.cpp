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

const double FLAMESPEED	= 0.45;
const double FLAMEROTSPEED	= 2.;

static FRandom pr_missile ("CFlameMissile");

void A_CFlameAttack (AActor *);
void A_CFlameRotate (AActor *);
void A_CFlamePuff (AActor *);
void A_CFlameMissile (AActor *);

// Flame Missile ------------------------------------------------------------

class ACFlameMissile : public AFastProjectile
{
	DECLARE_CLASS (ACFlameMissile, AFastProjectile)
public:
	void BeginPlay ();
	void Effect ();
};

IMPLEMENT_CLASS (ACFlameMissile)

void ACFlameMissile::BeginPlay ()
{
	special1 = 2;
}

void ACFlameMissile::Effect ()
{
	if (!--special1)
	{
		special1 = 4;
		double newz = Z() - 12;
		if (newz < floorz)
		{
			newz = floorz;
		}
		AActor *mo = Spawn ("CFlameFloor", PosAtZ(newz), ALLOW_REPLACE);
		if (mo)
		{
			mo->Angles.Yaw = Angles.Yaw;
		}
	}
}

//============================================================================
//
// A_CFlameAttack
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_CFlameAttack)
{
	PARAM_ACTION_PROLOGUE;

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
	P_SpawnPlayerMissile (self, RUNTIME_CLASS(ACFlameMissile));
	S_Sound (self, CHAN_WEAPON, "ClericFlameFire", 1, ATTN_NORM);
	return 0;
}

//============================================================================
//
// A_CFlamePuff
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_CFlamePuff)
{
	PARAM_ACTION_PROLOGUE;

	self->renderflags &= ~RF_INVISIBLE;
	self->Vel.Zero();
	S_Sound (self, CHAN_BODY, "ClericFlameExplode", 1, ATTN_NORM);
	return 0;
}

//============================================================================
//
// A_CFlameMissile
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_CFlameMissile)
{
	PARAM_ACTION_PROLOGUE;

	int i;
	DAngle an;
	double dist;
	AActor *mo;
	
	self->renderflags &= ~RF_INVISIBLE;
	S_Sound (self, CHAN_BODY, "ClericFlameExplode", 1, ATTN_NORM);
	AActor *BlockingMobj = self->BlockingMobj;
	if (BlockingMobj && BlockingMobj->flags&MF_SHOOTABLE)
	{ // Hit something, so spawn the flame circle around the thing
		dist = BlockingMobj->radius + 18;
		for (i = 0; i < 4; i++)
		{
			an = i*45.;
			mo = Spawn ("CircleFlame", BlockingMobj->Vec3Angle(dist, an, 5), ALLOW_REPLACE);
			if (mo)
			{
				mo->Angles.Yaw = an;
				mo->target = self->target;
				mo->VelFromAngle(FLAMESPEED);
				mo->specialf1 = mo->Vel.X;
				mo->specialf2 = mo->Vel.Y;
				mo->tics -= pr_missile()&3;
			}
			mo = Spawn("CircleFlame", BlockingMobj->Vec3Angle(dist, an, 5), ALLOW_REPLACE);
			if(mo)
			{
				mo->Angles.Yaw = an + 180.;
				mo->target = self->target;
				mo->VelFromAngle(-FLAMESPEED);
				mo->specialf1 = mo->Vel.X;
				mo->specialf2 = mo->Vel.Y;
				mo->tics -= pr_missile()&3;
			}
		}
		self->SetState (self->SpawnState);
	}
	return 0;
}

//============================================================================
//
// A_CFlameRotate
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_CFlameRotate)
{
	PARAM_ACTION_PROLOGUE;

	DAngle an = self->Angles.Yaw + 90.;
	self->VelFromAngle(an, FLAMEROTSPEED);
	self->Vel += DVector2(self->specialf1, self->specialf2);

	self->Angles.Yaw += 6.;
	return 0;
}
