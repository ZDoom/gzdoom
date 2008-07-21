#include "actor.h"
#include "info.h"
#include "p_enemy.h"
#include "a_doomglobal.h"
#include "a_hereticglobal.h"
#include "a_pickups.h"
#include "a_action.h"
#include "m_random.h"
#include "p_local.h"
#include "s_sound.h"
#include "gstrings.h"

static FRandom pr_podpain ("PodPain");
static FRandom pr_makepod ("MakePod");
static FRandom pr_teleg ("TeleGlitter");
static FRandom pr_teleg2 ("TeleGlitter2");
static FRandom pr_volcano ("VolcanoSet");
static FRandom pr_blast ("VolcanoBlast");
static FRandom pr_impact ("VolcBallImpact");

//----------------------------------------------------------------------------
//
// PROC A_PodPain
//
//----------------------------------------------------------------------------

void A_PodPain (AActor *actor)
{
	int count;
	int chance;
	AActor *goo;

	chance = pr_podpain ();
	if (chance < 128)
	{
		return;
	}
	for (count = chance > 240 ? 2 : 1; count; count--)
	{
		goo = Spawn("PodGoo", actor->x, actor->y, actor->z + 48*FRACUNIT, ALLOW_REPLACE);
		goo->target = actor;
		goo->momx = pr_podpain.Random2() << 9;
		goo->momy = pr_podpain.Random2() << 9;
		goo->momz = FRACUNIT/2 + (pr_podpain() << 9);
	}
}

//----------------------------------------------------------------------------
//
// PROC A_RemovePod
//
//----------------------------------------------------------------------------

void A_RemovePod (AActor *actor)
{
	AActor *mo;

	if ( (mo = actor->master))
	{
		if (mo->special1 > 0)
		{
			mo->special1--;
		}
	}
}

//----------------------------------------------------------------------------
//
// PROC A_MakePod
//
//----------------------------------------------------------------------------

#define MAX_GEN_PODS 16

void A_MakePod (AActor *actor)
{
	AActor *mo;
	fixed_t x;
	fixed_t y;
	fixed_t z;

	if (actor->special1 == MAX_GEN_PODS)
	{ // Too many generated pods
		return;
	}
	x = actor->x;
	y = actor->y;
	z = actor->z;
	mo = Spawn("Pod", x, y, ONFLOORZ, ALLOW_REPLACE);
	if (!P_CheckPosition (mo, x, y))
	{ // Didn't fit
		mo->Destroy ();
		return;
	}
	mo->SetState (mo->FindState("Grow"));
	P_ThrustMobj (mo, pr_makepod()<<24, (fixed_t)(4.5*FRACUNIT));
	S_Sound (mo, CHAN_BODY, "world/podgrow", 1, ATTN_IDLE);
	actor->special1++; // Increment generated pod count
	mo->master = actor; // Link the generator to the pod
	return;
}

//----------------------------------------------------------------------------
//
// PROC A_AccTeleGlitter
//
//----------------------------------------------------------------------------

void A_AccTeleGlitter (AActor *actor)
{
	if (++actor->health > 35)
	{
		actor->momz += actor->momz/2;
	}
}


//----------------------------------------------------------------------------
//
// PROC A_VolcanoSet
//
//----------------------------------------------------------------------------

void A_VolcanoSet (AActor *volcano)
{
	volcano->tics = 105 + (pr_volcano() & 127);
}

//----------------------------------------------------------------------------
//
// PROC A_VolcanoBlast
//
//----------------------------------------------------------------------------

void A_VolcanoBlast (AActor *volcano)
{
	int i;
	int count;
	AActor *blast;
	angle_t angle;

	count = 1 + (pr_blast() % 3);
	for (i = 0; i < count; i++)
	{
		blast = Spawn("VolcanoBlast", volcano->x, volcano->y,
			volcano->z + 44*FRACUNIT, ALLOW_REPLACE);
		blast->target = volcano;
		angle = pr_blast () << 24;
		blast->angle = angle;
		angle >>= ANGLETOFINESHIFT;
		blast->momx = FixedMul (1*FRACUNIT, finecosine[angle]);
		blast->momy = FixedMul (1*FRACUNIT, finesine[angle]);
		blast->momz = (FRACUNIT*5/2) + (pr_blast() << 10);
		S_Sound (blast, CHAN_BODY, "world/volcano/shoot", 1, ATTN_NORM);
		P_CheckMissileSpawn (blast);
	}
}

//----------------------------------------------------------------------------
//
// PROC A_VolcBallImpact
//
//----------------------------------------------------------------------------

void A_VolcBallImpact (AActor *ball)
{
	int i;
	AActor *tiny;
	angle_t angle;

	if (ball->z <= ball->floorz)
	{
		ball->flags |= MF_NOGRAVITY;
		ball->gravity = FRACUNIT;
		ball->z += 28*FRACUNIT;
		//ball->momz = 3*FRACUNIT;
	}
	P_RadiusAttack (ball, ball->target, 25, 25, NAME_Fire, true);
	for (i = 0; i < 4; i++)
	{
		tiny = Spawn("VolcanoTBlast", ball->x, ball->y, ball->z, ALLOW_REPLACE);
		tiny->target = ball;
		angle = i*ANG90;
		tiny->angle = angle;
		angle >>= ANGLETOFINESHIFT;
		tiny->momx = FixedMul (FRACUNIT*7/10, finecosine[angle]);
		tiny->momy = FixedMul (FRACUNIT*7/10, finesine[angle]);
		tiny->momz = FRACUNIT + (pr_impact() << 9);
		P_CheckMissileSpawn (tiny);
	}
}

