/*
#include "actor.h"
#include "info.h"
#include "p_local.h"
#include "s_sound.h"
#include "p_enemy.h"
#include "a_action.h"
#include "m_random.h"
#include "a_sharedglobal.h"
#include "thingdef/thingdef.h"
*/

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

DEFINE_ACTION_FUNCTION(AActor, A_WraithInit)
{
	self->AddZ(48<<FRACBITS);

	// [RH] Make sure the wraith didn't go into the ceiling
	if (self->Top() > self->ceilingz)
	{
		self->SetZ(self->ceilingz - self->height);
	}

	self->special1 = 0;			// index into floatbob
}

//============================================================================
//
// A_WraithRaiseInit
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_WraithRaiseInit)
{
	self->renderflags &= ~RF_INVISIBLE;
	self->flags2 &= ~MF2_NONSHOOTABLE;
	self->flags3 &= ~MF3_DONTBLAST;
	self->flags |= MF_SHOOTABLE|MF_SOLID;
	self->floorclip = self->height;
}

//============================================================================
//
// A_WraithRaise
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_WraithRaise)
{
	if (A_RaiseMobj (self, 2*FRACUNIT))
	{
		// Reached it's target height
		// [RH] Once a buried wraith is fully raised, it should be
		// morphable, right?
		self->flags3 &= ~(MF3_DONTMORPH|MF3_SPECIALFLOORCLIP);
		self->SetState (self->FindState("Chase"));
		// [RH] Reset PainChance to a normal wraith's.
		self->PainChance = GetDefaultByName ("Wraith")->PainChance;
	}

	P_SpawnDirt (self, self->radius);
}

//============================================================================
//
// A_WraithMelee
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_WraithMelee)
{
	int amount;

	// Steal health from target and give to self
	if (self->CheckMeleeRange() && (pr_stealhealth()<220))
	{
		amount = pr_stealhealth.HitDice (2);
		P_DamageMobj (self->target, self, self, amount, NAME_Melee);
		self->health += amount;
	}
}

//============================================================================
//
// A_WraithFX2 - spawns sparkle tail of missile
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_WraithFX2)
{
	AActor *mo;
	angle_t angle;
	int i;

	for (i = 2; i; --i)
	{
		mo = Spawn ("WraithFX2", self->Pos(), ALLOW_REPLACE);
		if(mo)
		{
			if (pr_wraithfx2 ()<128)
			{
				 angle = self->angle+(pr_wraithfx2()<<22);
			}
			else
			{
				 angle = self->angle-(pr_wraithfx2()<<22);
			}
			mo->velz = 0;
			mo->velx = FixedMul((pr_wraithfx2()<<7)+FRACUNIT,
				 finecosine[angle>>ANGLETOFINESHIFT]);
			mo->vely = FixedMul((pr_wraithfx2()<<7)+FRACUNIT, 
				 finesine[angle>>ANGLETOFINESHIFT]);
			mo->target = self;
			mo->floorclip = 10*FRACUNIT;
		}
	}
}

//============================================================================
//
// A_WraithFX3
//
// Spawn an FX3 around the self during attacks
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_WraithFX3)
{
	AActor *mo;
	int numdropped = pr_wraithfx3()%15;

	while (numdropped-- > 0)
	{
		fixed_t xo = (pr_wraithfx3() - 128) << 11;
		fixed_t yo = (pr_wraithfx3() - 128) << 11;
		fixed_t zo = pr_wraithfx3() << 10;

		mo = Spawn ("WraithFX3", self->Vec3Offset(xo, yo, zo), ALLOW_REPLACE);
		if (mo)
		{
			mo->floorz = self->floorz;
			mo->ceilingz = self->ceilingz;
			mo->target = self;
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

void A_WraithFX4 (AActor *self)
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
		fixed_t xo = (pr_wraithfx4() - 128) << 12;
		fixed_t yo = (pr_wraithfx4() - 128) << 12;
		fixed_t zo = (pr_wraithfx4() << 10);

		mo = Spawn ("WraithFX4", self->Vec3Offset(xo, yo, zo), ALLOW_REPLACE);
		if (mo)
		{
			mo->floorz = self->floorz;
			mo->ceilingz = self->ceilingz;
			mo->target = self;
		}
	}
	if (spawn5)
	{
		fixed_t xo = (pr_wraithfx4() - 128) << 11;
		fixed_t yo = (pr_wraithfx4() - 128) << 11;
		fixed_t zo = (pr_wraithfx4()<<10);

		mo = Spawn ("WraithFX5", self->Vec3Offset(xo, yo, zo), ALLOW_REPLACE);
		if (mo)
		{
			mo->floorz = self->floorz;
			mo->ceilingz = self->ceilingz;
			mo->target = self;
		}
	}
}

//============================================================================
//
// A_WraithChase
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_WraithChase)
{
	int weaveindex = self->special1;
	self->AddZ(finesine[weaveindex << BOBTOFINESHIFT] * 8);
	self->special1 = (weaveindex + 2) & 63;
//	if (self->floorclip > 0)
//	{
//		P_SetMobjState(self, S_WRAITH_RAISE2);
//		return;
//	}
	A_Chase (self);
	A_WraithFX4 (self);
}
