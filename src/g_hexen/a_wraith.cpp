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
//
// A_WraithInit
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_WraithInit)
{
	PARAM_ACTION_PROLOGUE;

	self->AddZ(48);

	// [RH] Make sure the wraith didn't go into the ceiling
	if (self->Top() > self->ceilingz)
	{
		self->SetZ(self->ceilingz - self->Height);
	}

	self->WeaveIndexZ = 0;			// index into floatbob
	return 0;
}

//============================================================================
//
// A_WraithRaiseInit
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_WraithRaiseInit)
{
	PARAM_ACTION_PROLOGUE;

	self->renderflags &= ~RF_INVISIBLE;
	self->flags2 &= ~MF2_NONSHOOTABLE;
	self->flags3 &= ~MF3_DONTBLAST;
	self->flags |= MF_SHOOTABLE|MF_SOLID;
	self->Floorclip = self->Height;
	return 0;
}

//============================================================================
//
// A_WraithRaise
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_WraithRaise)
{
	PARAM_ACTION_PROLOGUE;

	if (A_RaiseMobj (self, 2))
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
	return 0;
}

//============================================================================
//
// A_WraithMelee
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_WraithMelee)
{
	PARAM_ACTION_PROLOGUE;

	int amount;

	// Steal health from target and give to self
	if (self->CheckMeleeRange() && (pr_stealhealth()<220))
	{
		amount = pr_stealhealth.HitDice (2);
		P_DamageMobj (self->target, self, self, amount, NAME_Melee);
		self->health += amount;
	}
	return 0;
}

//============================================================================
//
// A_WraithFX2 - spawns sparkle tail of missile
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_WraithFX2)
{
	PARAM_ACTION_PROLOGUE;

	AActor *mo;
	DAngle angle;
	int i;

	for (i = 2; i; --i)
	{
		mo = Spawn ("WraithFX2", self->Pos(), ALLOW_REPLACE);
		if(mo)
		{
			angle = pr_wraithfx2() * (360 / 1024.f);
			if (pr_wraithfx2() >= 128)
			{
				angle = -angle;
			}
			angle += self->Angles.Yaw;
			mo->Vel.X = ((pr_wraithfx2() / 512.) + 1) * angle.Cos();
			mo->Vel.Y = ((pr_wraithfx2() / 512.) + 1) * angle.Sin();
			mo->Vel.Z = 0;
			mo->target = self;
			mo->Floorclip = 10;
		}
	}
	return 0;
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
	PARAM_ACTION_PROLOGUE;

	AActor *mo;
	int numdropped = pr_wraithfx3() % 15;

	while (numdropped-- > 0)
	{
		double xo = (pr_wraithfx3() - 128) / 32.;
		double yo = (pr_wraithfx3() - 128) / 32.;
		double zo = pr_wraithfx3() / 64.;

		mo = Spawn("WraithFX3", self->Vec3Offset(xo, yo, zo), ALLOW_REPLACE);
		if (mo)
		{
			mo->floorz = self->floorz;
			mo->ceilingz = self->ceilingz;
			mo->target = self;
		}
	}
	return 0;
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
		double xo = (pr_wraithfx4() - 128) / 16.;
		double yo = (pr_wraithfx4() - 128) / 16.;
		double zo = (pr_wraithfx4() / 64.);

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
		double xo = (pr_wraithfx4() - 128) / 32.;
		double yo = (pr_wraithfx4() - 128) / 32.;
		double zo = (pr_wraithfx4() / 64.);

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
	PARAM_ACTION_PROLOGUE;

	int weaveindex = self->WeaveIndexZ;
	self->AddZ(BobSin(weaveindex));
	self->WeaveIndexZ = (weaveindex + 2) & 63;
//	if (self->Floorclip > 0)
//	{
//		P_SetMobjState(self, S_WRAITH_RAISE2);
//		return;
//	}
	A_Chase (stack, self);
	A_WraithFX4 (self);
	return 0;
}
