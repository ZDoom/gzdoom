#include "actor.h"
#include "info.h"
#include "p_local.h"
#include "s_sound.h"
#include "p_enemy.h"
#include "a_action.h"
#include "m_random.h"
#include "a_sharedglobal.h"

static FRandom pr_stealhealth ("StealHealth");
static FRandom pr_wraithfx2 ("WraithFX2");
static FRandom pr_wraithfx3 ("WraithFX3");
static FRandom pr_wraithfx4 ("WraithFX4");

//============================================================================
// Wraith Variables
//
//	special1				Internal index into floatbob
//============================================================================

//============================================================================
//
// A_WraithInit
//
//============================================================================

void A_WraithInit (AActor *actor)
{
	actor->z += 48<<FRACBITS;

	// [RH] Make sure the wraith didn't go into the ceiling
	if (actor->z + actor->height > actor->ceilingz)
	{
		actor->z = actor->ceilingz - actor->height;
	}

	actor->special1 = 0;			// index into floatbob
}

//============================================================================
//
// A_WraithRaiseInit
//
//============================================================================

void A_WraithRaiseInit (AActor *actor)
{
	actor->renderflags &= ~RF_INVISIBLE;
	actor->flags2 &= ~MF2_NONSHOOTABLE;
	actor->flags3 &= ~MF3_DONTBLAST;
	actor->flags |= MF_SHOOTABLE|MF_SOLID;
	actor->floorclip = actor->height;
}

//============================================================================
//
// A_WraithRaise
//
//============================================================================

void A_WraithRaise (AActor *actor)
{
	if (A_RaiseMobj (actor, 2*FRACUNIT))
	{
		// Reached it's target height
		// [RH] Once a buried wraith is fully raised, it should be
		// morphable, right?
		actor->flags3 &= ~(MF3_DONTMORPH|MF3_SPECIALFLOORCLIP);
		actor->SetState (actor->FindState("Chase"));
		// [RH] Reset PainChance to a normal wraith's.
		actor->PainChance = GetDefaultByName ("Wraith")->PainChance;
	}

	P_SpawnDirt (actor, actor->radius);
}

//============================================================================
//
// A_WraithMelee
//
//============================================================================

void A_WraithMelee (AActor *actor)
{
	int amount;

	// Steal health from target and give to self
	if (actor->CheckMeleeRange() && (pr_stealhealth()<220))
	{
		amount = pr_stealhealth.HitDice (2);
		P_DamageMobj (actor->target, actor, actor, amount, NAME_Melee);
		actor->health += amount;
	}
}

//============================================================================
//
// A_WraithFX2 - spawns sparkle tail of missile
//
//============================================================================

void A_WraithFX2 (AActor *actor)
{
	AActor *mo;
	angle_t angle;
	int i;

	for (i = 2; i; --i)
	{
		mo = Spawn ("WraithFX2", actor->x, actor->y, actor->z, ALLOW_REPLACE);
		if(mo)
		{
			if (pr_wraithfx2 ()<128)
			{
				 angle = actor->angle+(pr_wraithfx2()<<22);
			}
			else
			{
				 angle = actor->angle-(pr_wraithfx2()<<22);
			}
			mo->momz = 0;
			mo->momx = FixedMul((pr_wraithfx2()<<7)+FRACUNIT,
				 finecosine[angle>>ANGLETOFINESHIFT]);
			mo->momy = FixedMul((pr_wraithfx2()<<7)+FRACUNIT, 
				 finesine[angle>>ANGLETOFINESHIFT]);
			mo->target = actor;
			mo->floorclip = 10*FRACUNIT;
		}
	}
}

//============================================================================
//
// A_WraithFX3
//
// Spawn an FX3 around the actor during attacks
//
//============================================================================

void A_WraithFX3 (AActor *actor)
{
	AActor *mo;
	int numdropped = pr_wraithfx3()%15;

	while (numdropped-- > 0)
	{
		mo = Spawn ("WraithFX3", actor->x, actor->y, actor->z, ALLOW_REPLACE);
		if (mo)
		{
			mo->x += (pr_wraithfx3()-128)<<11;
			mo->y += (pr_wraithfx3()-128)<<11;
			mo->z += (pr_wraithfx3()<<10);
			mo->target = actor;
		}
	}
}

//============================================================================
//
// A_WraithFX4
//
// Spawn an FX4 during movement
//
//============================================================================

void A_WraithFX4 (AActor *actor)
{
	AActor *mo;
	int chance = pr_wraithfx4();
	bool spawn4, spawn5;

	if (chance < 10)
	{
		spawn4 = true;
		spawn5 = false;
	}
	else if (chance < 20)
	{
		spawn4 = false;
		spawn5 = true;
	}
	else if (chance < 25)
	{
		spawn4 = true;
		spawn5 = true;
	}
	else
	{
		spawn4 = false;
		spawn5 = false;
	}

	if (spawn4)
	{
		mo = Spawn ("WraithFX4", actor->x, actor->y, actor->z, ALLOW_REPLACE);
		if (mo)
		{
			mo->x += (pr_wraithfx4()-128)<<12;
			mo->y += (pr_wraithfx4()-128)<<12;
			mo->z += (pr_wraithfx4()<<10);
			mo->target = actor;
		}
	}
	if (spawn5)
	{
		mo = Spawn ("WraithFX5", actor->x, actor->y, actor->z, ALLOW_REPLACE);
		if (mo)
		{
			mo->x += (pr_wraithfx4()-128)<<11;
			mo->y += (pr_wraithfx4()-128)<<11;
			mo->z += (pr_wraithfx4()<<10);
			mo->target = actor;
		}
	}
}

//============================================================================
//
// A_WraithChase
//
//============================================================================

void A_WraithChase (AActor *actor)
{
	int weaveindex = actor->special1;
	actor->z += FloatBobOffsets[weaveindex];
	actor->special1 = (weaveindex+2)&63;
//	if (actor->floorclip > 0)
//	{
//		P_SetMobjState(actor, S_WRAITH_RAISE2);
//		return;
//	}
	A_Chase (actor);
	A_WraithFX4 (actor);
}
