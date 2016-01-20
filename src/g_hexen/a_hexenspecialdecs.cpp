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
	AActor *mo = NULL;
	int i;

	for(i = (pr_pottery()&3)+3; i; i--)
	{
		mo = Spawn ("PotteryBit", self->Pos(), ALLOW_REPLACE);
		if (mo)
		{
			mo->SetState (mo->SpawnState + (pr_pottery()%5));
			mo->velz = ((pr_pottery()&7)+5)*(3*FRACUNIT/4);
			mo->velx = (pr_pottery.Random2())<<(FRACBITS-6);
			mo->vely = (pr_pottery.Random2())<<(FRACBITS-6);
		}
	}
	S_Sound (mo, CHAN_BODY, "PotteryExplode", 1, ATTN_NORM);
	// Spawn an item?
	const PClass *type = P_GetSpawnableType(self->args[0]);
	if (type != NULL)
	{
		if (!((level.flags2 & LEVEL2_NOMONSTERS) || (dmflags & DF_NO_MONSTERS))
		|| !(GetDefaultByType (type)->flags3 & MF3_ISMONSTER))
		{ // Only spawn monsters if not -nomonsters
			Spawn (type, self->Pos(), ALLOW_REPLACE);
		}
	}
}

//============================================================================
//
// A_PotteryChooseBit
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_PotteryChooseBit)
{
	self->SetState (self->FindState(NAME_Death) + 1 + 2*(pr_bit()%5));
	self->tics = 256+(pr_bit()<<1);
}

//============================================================================
//
// A_PotteryCheck
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_PotteryCheck)
{
	int i;

	for(i = 0; i < MAXPLAYERS; i++)
	{
		if (playeringame[i])
		{
			AActor *pmo = players[i].mo;
			if (P_CheckSight (self, pmo) && (absangle(pmo->AngleTo(self) - pmo->angle) <= ANGLE_45))
			{ // Previous state (pottery bit waiting state)
				self->SetState (self->state - 1);
				return;
			}
		}
	}
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
	Spawn ("BloodPool", X(), Y(), floorz, ALLOW_REPLACE);
}

//============================================================================
//
// A_CorpseBloodDrip
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_CorpseBloodDrip)
{
	if (pr_drip() <= 128)
	{
		Spawn ("CorpseBloodDrip", self->PosPlusZ(self->height/2), ALLOW_REPLACE);
	}
}

//============================================================================
//
// A_CorpseExplode
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_CorpseExplode)
{
	AActor *mo;
	int i;

	for (i = (pr_foo()&3)+3; i; i--)
	{
		mo = Spawn ("CorpseBit", self->Pos(), ALLOW_REPLACE);
		if (mo)
		{
			mo->SetState (mo->SpawnState + (pr_foo()%3));
			mo->velz = ((pr_foo()&7)+5)*(3*FRACUNIT/4);
			mo->velx = pr_foo.Random2()<<(FRACBITS-6);
			mo->vely = pr_foo.Random2()<<(FRACBITS-6);
		}
	}
	// Spawn a skull
	mo = Spawn ("CorpseBit", self->Pos(), ALLOW_REPLACE);
	if (mo)
	{
		mo->SetState (mo->SpawnState + 3);
		mo->velz = ((pr_foo()&7)+5)*(3*FRACUNIT/4);
		mo->velx = pr_foo.Random2()<<(FRACBITS-6);
		mo->vely = pr_foo.Random2()<<(FRACBITS-6);
	}
	S_Sound (self, CHAN_BODY, self->DeathSound, 1, ATTN_IDLE);
	self->Destroy ();
}

//============================================================================
//
// A_LeafSpawn
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_LeafSpawn)
{
	AActor *mo;
	int i;

	for (i = (pr_leaf()&3)+1; i; i--)
	{
		fixed_t xo = (pr_leaf.Random2() << 14);
		fixed_t yo = (pr_leaf.Random2() << 14);
		fixed_t zo = (pr_leaf() << 14);
		mo = Spawn (pr_leaf()&1 ? PClass::FindClass ("Leaf1") : PClass::FindClass ("Leaf2"),
			self->Vec3Offset(xo, yo, zo), ALLOW_REPLACE);

		if (mo)
		{
			P_ThrustMobj (mo, self->angle, (pr_leaf()<<9)+3*FRACUNIT);
			mo->target = self;
			mo->special1 = 0;
		}
	}
}

//============================================================================
//
// A_LeafThrust
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_LeafThrust)
{
	if (pr_leafthrust() <= 96)
	{
		self->velz += (pr_leafthrust()<<9)+FRACUNIT;
	}
}

//============================================================================
//
// A_LeafCheck
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_LeafCheck)
{
	self->special1++;
	if (self->special1 >= 20)
	{
		self->SetState (NULL);
		return;
	}
	angle_t ang = self->target ? self->target->angle : self->angle;
	if (pr_leafcheck() > 64)
	{
		if (!self->velx && !self->vely)
		{
			P_ThrustMobj (self, ang, (pr_leafcheck()<<9)+FRACUNIT);
		}
		return;
	}
	self->SetState (self->SpawnState + 7);
	self->velz = (pr_leafcheck()<<9)+FRACUNIT;
	P_ThrustMobj (self, ang, (pr_leafcheck()<<9)+2*FRACUNIT);
	self->flags |= MF_MISSILE;
}

//===========================================================================
//
// A_PoisonShroom
//
//===========================================================================

DEFINE_ACTION_FUNCTION(AActor, A_PoisonShroom)
{
	self->tics = 128+(pr_shroom()<<1);
}

//===========================================================================
//
// A_SoAExplode - Suit of Armor Explode
//
//===========================================================================

DEFINE_ACTION_FUNCTION(AActor, A_SoAExplode)
{
	AActor *mo;
	int i;

	for (i = 0; i < 10; i++)
	{
		fixed_t xo = ((pr_soaexplode() - 128) << 12);
		fixed_t yo = ((pr_soaexplode() - 128) << 12);
		fixed_t zo = (pr_soaexplode()*self->height / 256);
		mo = Spawn ("ZArmorChunk", self->Vec3Offset(xo, yo, zo), ALLOW_REPLACE);
		if (mo)
		{
			mo->SetState (mo->SpawnState + i);
			mo->velz = ((pr_soaexplode()&7)+5)*FRACUNIT;
			mo->velx = pr_soaexplode.Random2()<<(FRACBITS-6);
			mo->vely = pr_soaexplode.Random2()<<(FRACBITS-6);
		}
	}
	// Spawn an item?
	const PClass *type = P_GetSpawnableType(self->args[0]);
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
		P_DamageMobj (this, activator, activator, 10, NAME_Melee); // 'ring' the bell
	}
}

//===========================================================================
//
// A_BellReset1
//
//===========================================================================

DEFINE_ACTION_FUNCTION(AActor, A_BellReset1)
{
	self->flags |= MF_NOGRAVITY;
	self->height <<= 2;
	if (self->special)
	{ // Initiate death action
		P_ExecuteSpecial(self->special, NULL, NULL, false, self->args[0],
			self->args[1], self->args[2], self->args[3], self->args[4]);
		self->special = 0;
	}
}

//===========================================================================
//
// A_BellReset2
//
//===========================================================================

DEFINE_ACTION_FUNCTION(AActor, A_BellReset2)
{
	self->flags |= MF_SHOOTABLE;
	self->flags &= ~MF_CORPSE;
	self->flags6 &= ~MF6_KILLED;
	self->health = 5;
}

