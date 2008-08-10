#include "actor.h"
#include "info.h"
#include "p_local.h"
#include "s_sound.h"
#include "p_enemy.h"
#include "a_action.h"
#include "m_random.h"

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

void A_IceGuyLook (AActor *actor)
{
	fixed_t dist;
	fixed_t an;

	A_Look (actor);
	if (pr_iceguylook() < 64)
	{
		dist = ((pr_iceguylook()-128)*actor->radius)>>7;
		an = (actor->angle+ANG90)>>ANGLETOFINESHIFT;

		Spawn (WispTypes[pr_iceguylook()&1],
			actor->x+FixedMul(dist, finecosine[an]),
			actor->y+FixedMul(dist, finesine[an]),
			actor->z+60*FRACUNIT, ALLOW_REPLACE);
	}
}

//============================================================================
//
// A_IceGuyChase
//
//============================================================================

void A_IceGuyChase (AActor *actor)
{
	fixed_t dist;
	fixed_t an;
	AActor *mo;

	A_Chase (actor);
	if (pr_iceguychase() < 128)
	{
		dist = ((pr_iceguychase()-128)*actor->radius)>>7;
		an = (actor->angle+ANG90)>>ANGLETOFINESHIFT;

		mo = Spawn (WispTypes[pr_iceguychase()&1],
			actor->x+FixedMul(dist, finecosine[an]),
			actor->y+FixedMul(dist, finesine[an]),
			actor->z+60*FRACUNIT, ALLOW_REPLACE);
		if (mo)
		{
			mo->momx = actor->momx;
			mo->momy = actor->momy;
			mo->momz = actor->momz;
			mo->target = actor;
		}
	}
}

//============================================================================
//
// A_IceGuyAttack
//
//============================================================================

void A_IceGuyAttack (AActor *actor)
{
	fixed_t an;

	if(!actor->target) 
	{
		return;
	}
	an = (actor->angle+ANG90)>>ANGLETOFINESHIFT;
	P_SpawnMissileXYZ(actor->x+FixedMul(actor->radius>>1,
		finecosine[an]), actor->y+FixedMul(actor->radius>>1,
		finesine[an]), actor->z+40*FRACUNIT, actor, actor->target,
		PClass::FindClass ("IceGuyFX"));
	an = (actor->angle-ANG90)>>ANGLETOFINESHIFT;
	P_SpawnMissileXYZ(actor->x+FixedMul(actor->radius>>1,
		finecosine[an]), actor->y+FixedMul(actor->radius>>1,
		finesine[an]), actor->z+40*FRACUNIT, actor, actor->target,
		PClass::FindClass ("IceGuyFX"));
	S_Sound (actor, CHAN_WEAPON, actor->AttackSound, 1, ATTN_NORM);
}

//============================================================================
//
// A_IceGuyDie
//
//============================================================================

void A_IceGuyDie (AActor *actor)
{
	actor->momx = 0;
	actor->momy = 0;
	actor->momz = 0;
	actor->height = actor->GetDefault()->height;
	A_FreezeDeathChunks (actor);
}

//============================================================================
//
// A_IceGuyMissileExplode
//
//============================================================================

void A_IceGuyMissileExplode (AActor *actor)
{
	AActor *mo;
	int i;

	for (i = 0; i < 8; i++)
	{
		mo = P_SpawnMissileAngleZ (actor, actor->z+3*FRACUNIT, 
			PClass::FindClass("IceGuyFX2"), i*ANG45, (fixed_t)(-0.3*FRACUNIT));
		if (mo)
		{
			mo->target = actor->target;
		}
	}
}

