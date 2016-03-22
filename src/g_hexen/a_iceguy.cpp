/*
#include "actor.h"
#include "info.h"
#include "p_local.h"
#include "s_sound.h"
#include "p_enemy.h"
#include "a_action.h"
#include "m_random.h"
#include "thingdef/thingdef.h"
*/

static FRandom pr_iceguylook ("IceGuyLook");
static FRandom pr_iceguychase ("IceGuyChase");

static const char *WispTypes[2] =
{
	"IceGuyWisp1",
	"IceGuyWisp2",
};

//============================================================================
//
// A_IceGuyLook
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_IceGuyLook)
{
	PARAM_ACTION_PROLOGUE;

	double dist;
	DAngle an;

	CALL_ACTION(A_Look, self);
	if (pr_iceguylook() < 64)
	{
		dist = (pr_iceguylook() - 128) * self->radius / 128.;
		an = self->Angles.Yaw + 90;
		Spawn(WispTypes[pr_iceguylook() & 1], self->Vec3Angle(dist, an, 60.), ALLOW_REPLACE);
	}
	return 0;
}

//============================================================================
//
// A_IceGuyChase
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_IceGuyChase)
{
	PARAM_ACTION_PROLOGUE;

	double dist;
	DAngle an;
	AActor *mo;

	A_Chase(stack, self);
	if (pr_iceguychase() < 128)
	{
		dist = (pr_iceguychase() - 128) * self->radius / 128.;
		an = self->Angles.Yaw + 90;
		mo = Spawn(WispTypes[pr_iceguylook() & 1], self->Vec3Angle(dist, an, 60.), ALLOW_REPLACE);
		if (mo)
		{
			mo->Vel = self->Vel;
			mo->target = self;
		}
	}
	return 0;
}

//============================================================================
//
// A_IceGuyAttack
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_IceGuyAttack)
{
	PARAM_ACTION_PROLOGUE;

	if(!self->target) 
	{
		return 0;
	}
	P_SpawnMissileXYZ(self->Vec3Angle(self->radius / 2, self->Angles.Yaw + 90, 40.), self, self->target, PClass::FindActor("IceGuyFX"));
	P_SpawnMissileXYZ(self->Vec3Angle(self->radius / 2, self->Angles.Yaw - 90, 40.), self, self->target, PClass::FindActor("IceGuyFX"));
	S_Sound (self, CHAN_WEAPON, self->AttackSound, 1, ATTN_NORM);
	return 0;
}

//============================================================================
//
// A_IceGuyDie
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_IceGuyDie)
{
	PARAM_ACTION_PROLOGUE;

	self->Vel.Zero();
	self->Height = self->GetDefault()->Height;
	CALL_ACTION(A_FreezeDeathChunks, self);
	return 0;
}

//============================================================================
//
// A_IceGuyMissileExplode
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_IceGuyMissileExplode)
{
	PARAM_ACTION_PROLOGUE;

	AActor *mo;
	unsigned int i;

	for (i = 0; i < 8; i++)
	{
		mo = P_SpawnMissileAngleZ (self, self->Z()+3, PClass::FindActor("IceGuyFX2"), DAngle(i*45.), -0.3);
		if (mo)
		{
			mo->target = self->target;
		}
	}
	return 0;
}

