/*
** Decorations that do special things
*/

/*
#include "actor.h"
#include "info.h"
#include "a_action.h"
#include "m_random.h"
#include "s_sound.h"
#include "p_local.h"
#include "p_lnspec.h"
#include "a_hexenglobal.h"
#include "thingdef/thingdef.h"
#include "g_level.h"
#include "doomstat.h"
*/

static FRandom pr_pottery ("PotteryExplode");
static FRandom pr_bit ("PotteryChooseBit");
static FRandom pr_drip ("CorpseBloodDrip");
static FRandom pr_foo ("CorpseExplode");
static FRandom pr_leaf ("LeafSpawn");
static FRandom pr_leafthrust ("LeafThrust");
static FRandom pr_leafcheck ("LeafCheck");
static FRandom pr_shroom ("PoisonShroom");
static FRandom pr_soaexplode ("SoAExplode");

// Pottery1 ------------------------------------------------------------------

void A_PotteryExplode (AActor *);
void A_PotteryChooseBit (AActor *);
void A_PotteryCheck (AActor *);

class APottery1 : public AActor
{
	DECLARE_CLASS (APottery1, AActor)
public:
	void HitFloor ();
};

IMPLEMENT_CLASS (APottery1)

void APottery1::HitFloor ()
{
	Super::HitFloor ();
	P_DamageMobj (this, NULL, NULL, 25, NAME_None);
}

//============================================================================
//
// A_PotteryExplode
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_PotteryExplode)
{
	PARAM_ACTION_PROLOGUE;

	AActor *mo = NULL;
	int i;

	for(i = (pr_pottery()&3)+3; i; i--)
	{
		mo = Spawn ("PotteryBit", self->Pos(), ALLOW_REPLACE);
		if (mo)
		{
			mo->SetState (mo->SpawnState + (pr_pottery()%5));
			mo->Vel.X = pr_pottery.Random2() / 64.;
			mo->Vel.Y = pr_pottery.Random2() / 64.;
			mo->Vel.Z = ((pr_pottery() & 7) + 5) * 0.75;
		}
	}
	S_Sound (mo, CHAN_BODY, "PotteryExplode", 1, ATTN_NORM);
	// Spawn an item?
	PClassActor *type = P_GetSpawnableType(self->args[0]);
	if (type != NULL)
	{
		if (!((level.flags2 & LEVEL2_NOMONSTERS) || (dmflags & DF_NO_MONSTERS))
		|| !(GetDefaultByType (type)->flags3 & MF3_ISMONSTER))
		{ // Only spawn monsters if not -nomonsters
			Spawn (type, self->Pos(), ALLOW_REPLACE);
		}
	}
	return 0;
}

//============================================================================
//
// A_PotteryChooseBit
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_PotteryChooseBit)
{
	PARAM_ACTION_PROLOGUE;

	self->SetState (self->FindState(NAME_Death) + 1 + 2*(pr_bit()%5));
	self->tics = 256+(pr_bit()<<1);
	return 0;
}

//============================================================================
//
// A_PotteryCheck
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_PotteryCheck)
{
	PARAM_ACTION_PROLOGUE;

	int i;

	for(i = 0; i < MAXPLAYERS; i++)
	{
		if (playeringame[i])
		{
			AActor *pmo = players[i].mo;
			if (P_CheckSight (self, pmo) && (absangle(pmo->AngleTo(self), pmo->Angles.Yaw) <= 45))
			{ // Previous state (pottery bit waiting state)
				self->SetState (self->state - 1);
				return 0;
			}
		}
	}
	return 0;
}

// Lynched corpse (no heart) ------------------------------------------------

class AZCorpseLynchedNoHeart : public AActor
{
DECLARE_CLASS (AZCorpseLynchedNoHeart, AActor)
public:
	void PostBeginPlay ();
};

IMPLEMENT_CLASS (AZCorpseLynchedNoHeart)

void AZCorpseLynchedNoHeart::PostBeginPlay ()
{
	Super::PostBeginPlay ();
	Spawn ("BloodPool", PosAtZ(floorz), ALLOW_REPLACE);
}

//============================================================================
//
// A_CorpseBloodDrip
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_CorpseBloodDrip)
{
	PARAM_ACTION_PROLOGUE;

	if (pr_drip() <= 128)
	{
		Spawn ("CorpseBloodDrip", self->PosPlusZ(self->Height / 2), ALLOW_REPLACE);
	}
	return 0;
}

//============================================================================
//
// A_CorpseExplode
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_CorpseExplode)
{
	PARAM_ACTION_PROLOGUE;

	AActor *mo;
	int i;

	for (i = (pr_foo()&3)+3; i; i--)
	{
		mo = Spawn ("CorpseBit", self->Pos(), ALLOW_REPLACE);
		if (mo)
		{
			mo->SetState (mo->SpawnState + (pr_foo()%3));
			mo->Vel.X = pr_foo.Random2() / 64.;
			mo->Vel.Y = pr_foo.Random2() / 64.;
			mo->Vel.Z = ((pr_foo() & 7) + 5) * 0.75;
		}
	}
	// Spawn a skull
	mo = Spawn ("CorpseBit", self->Pos(), ALLOW_REPLACE);
	if (mo)
	{
		mo->SetState (mo->SpawnState + 3);
		mo->Vel.X = pr_foo.Random2() / 64.;
		mo->Vel.Y = pr_foo.Random2() / 64.;
		mo->Vel.Z = ((pr_foo() & 7) + 5) * 0.75;
	}
	S_Sound (self, CHAN_BODY, self->DeathSound, 1, ATTN_IDLE);
	self->Destroy ();
	return 0;
}

//============================================================================
//
// A_LeafSpawn
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_LeafSpawn)
{
	PARAM_ACTION_PROLOGUE;

	AActor *mo;
	int i;

	for (i = (pr_leaf()&3)+1; i; i--)
	{
		double xo = pr_leaf.Random2() / 4.;
		double yo = pr_leaf.Random2() / 4.;
		double zo = pr_leaf() / 4.;
		mo = Spawn (pr_leaf()&1 ? PClass::FindActor ("Leaf1") : PClass::FindActor ("Leaf2"),
			self->Vec3Offset(xo, yo, zo), ALLOW_REPLACE);

		if (mo)
		{
			mo->Thrust(pr_leaf() / 128. + 3);
			mo->target = self;
			mo->special1 = 0;
		}
	}
	return 0;
}

//============================================================================
//
// A_LeafThrust
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_LeafThrust)
{
	PARAM_ACTION_PROLOGUE;

	if (pr_leafthrust() <= 96)
	{
		self->Vel.Z += pr_leafthrust() / 128. + 1;
	}
	return 0;
}

//============================================================================
//
// A_LeafCheck
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_LeafCheck)
{
	PARAM_ACTION_PROLOGUE;

	self->special1++;
	if (self->special1 >= 20)
	{
		self->SetState (NULL);
		return 0;
	}
	DAngle ang = self->target ? self->target->Angles.Yaw : self->Angles.Yaw;
	if (pr_leafcheck() > 64)
	{
		if (self->Vel.X == 0 && self->Vel.Y == 0)
		{
			self->Thrust(ang, pr_leafcheck() / 128. + 1);
		}
		return 0;
	}
	self->SetState (self->SpawnState + 7);
	self->Vel.Z = pr_leafcheck() / 128. + 1;
	self->Thrust(ang, pr_leafcheck() / 128. + 2);
	self->flags |= MF_MISSILE;
	return 0;
}

//===========================================================================
//
// A_PoisonShroom
//
//===========================================================================

DEFINE_ACTION_FUNCTION(AActor, A_PoisonShroom)
{
	PARAM_ACTION_PROLOGUE;

	self->tics = 128 + (pr_shroom() << 1);
	return 0;
}

//===========================================================================
//
// A_SoAExplode - Suit of Armor Explode
//
//===========================================================================

DEFINE_ACTION_FUNCTION(AActor, A_SoAExplode)
{
	PARAM_ACTION_PROLOGUE;

	AActor *mo;
	int i;

	for (i = 0; i < 10; i++)
	{
		double xo = (pr_soaexplode() - 128) / 16.;
		double yo = (pr_soaexplode() - 128) / 16.;
		double zo = pr_soaexplode()*self->Height / 256.;
		mo = Spawn ("ZArmorChunk", self->Vec3Offset(xo, yo, zo), ALLOW_REPLACE);
		if (mo)
		{
			mo->SetState (mo->SpawnState + i);
			mo->Vel.X = pr_soaexplode.Random2() / 64.;
			mo->Vel.Y = pr_soaexplode.Random2() / 64.;
			mo->Vel.Z = (pr_soaexplode() & 7) + 5;
		}
	}
	// Spawn an item?
	PClassActor *type = P_GetSpawnableType(self->args[0]);
	if (type != NULL)
	{
		if (!((level.flags2 & LEVEL2_NOMONSTERS) || (dmflags & DF_NO_MONSTERS))
		|| !(GetDefaultByType (type)->flags3 & MF3_ISMONSTER))
		{ // Only spawn monsters if not -nomonsters
			Spawn (type, self->Pos(), ALLOW_REPLACE);
		}
	}
	S_Sound (self, CHAN_BODY, self->DeathSound, 1, ATTN_NORM);
	self->Destroy ();
	return 0;
}

// Bell ---------------------------------------------------------------------

class AZBell : public AActor
{
	DECLARE_CLASS (AZBell, AActor)
public:
	void Activate (AActor *activator);
};

IMPLEMENT_CLASS (AZBell)

void AZBell::Activate (AActor *activator)
{
	if (health > 0)
	{
		P_DamageMobj (this, activator, activator, 10, NAME_Melee, DMG_THRUSTLESS); // 'ring' the bell
	}
}

//===========================================================================
//
// A_BellReset1
//
//===========================================================================

DEFINE_ACTION_FUNCTION(AActor, A_BellReset1)
{
	PARAM_ACTION_PROLOGUE;

	self->flags |= MF_NOGRAVITY;
	self->Height *= 4;
	if (self->special)
	{ // Initiate death action
		P_ExecuteSpecial(self->special, NULL, NULL, false, self->args[0],
			self->args[1], self->args[2], self->args[3], self->args[4]);
		self->special = 0;
	}
	return 0;
}

//===========================================================================
//
// A_BellReset2
//
//===========================================================================

DEFINE_ACTION_FUNCTION(AActor, A_BellReset2)
{
	PARAM_ACTION_PROLOGUE;

	self->flags |= MF_SHOOTABLE;
	self->flags &= ~MF_CORPSE;
	self->flags6 &= ~MF6_KILLED;
	self->health = 5;
	return 0;
}

