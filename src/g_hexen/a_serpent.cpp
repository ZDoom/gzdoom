/*
#include "actor.h"
#include "info.h"
#include "p_local.h"
#include "s_sound.h"
#include "p_enemy.h"
#include "a_action.h"
#include "m_random.h"
#include "p_terrain.h"
#include "thingdef/thingdef.h"
*/

static FRandom pr_serpentchase ("SerpentChase");
static FRandom pr_serpenthump ("SerpentHump");
static FRandom pr_serpentattack ("SerpentAttack");
static FRandom pr_serpentmeattack ("SerpentMeAttack");
static FRandom pr_serpentgibs ("SerpentGibs");
static FRandom pr_delaygib ("DelayGib");

//============================================================================
//
// A_SerpentUnHide
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_SerpentUnHide)
{
	PARAM_ACTION_PROLOGUE;

	self->renderflags &= ~RF_INVISIBLE;
	self->Floorclip = 24;
	return 0;
}

//============================================================================
//
// A_SerpentHide
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_SerpentHide)
{
	PARAM_ACTION_PROLOGUE;

	self->renderflags |= RF_INVISIBLE;
	self->Floorclip = 0;
	return 0;
}

//============================================================================
//
// A_SerpentRaiseHump
// 
// Raises the hump above the surface by raising the floorclip level
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_SerpentRaiseHump)
{
	PARAM_ACTION_PROLOGUE;

	self->Floorclip -= 4;
	return 0;
}

//============================================================================
//
// A_SerpentLowerHump
// 
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_SerpentLowerHump)
{
	PARAM_ACTION_PROLOGUE;

	self->Floorclip += 4;
	return 0;
}

//============================================================================
//
// A_SerpentHumpDecide
//
//		Decided whether to hump up, or if the mobj is a serpent leader, 
//			to missile attack
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_SerpentHumpDecide)
{
	PARAM_ACTION_PROLOGUE;

	if (self->MissileState != NULL)
	{
		if (pr_serpenthump() > 30)
		{
			return 0;
		}
		else if (pr_serpenthump() < 40)
		{ // Missile attack
			self->SetState (self->MeleeState);
			return 0;
		}
	}
	else if (pr_serpenthump() > 3)
	{
		return 0;
	}
	if (!self->CheckMeleeRange ())
	{ // The hump shouldn't occur when within melee range
		if (self->MissileState != NULL && pr_serpenthump() < 128)
		{
			self->SetState (self->MeleeState);
		}
		else
		{	
			self->SetState (self->FindState ("Hump"));
			S_Sound (self, CHAN_BODY, "SerpentActive", 1, ATTN_NORM);
		}
	}
	return 0;
}

//============================================================================
//
// A_SerpentCheckForAttack
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_SerpentCheckForAttack)
{
	PARAM_ACTION_PROLOGUE;

	if (!self->target)
	{
		return 0;
	}
	if (self->MissileState != NULL)
	{
		if (!self->CheckMeleeRange ())
		{
			self->SetState (self->FindState ("Attack"));
			return 0;
		}
	}
	if (P_CheckMeleeRange2 (self))
	{
		self->SetState (self->FindState ("Walk"));
	}
	else if (self->CheckMeleeRange ())
	{
		if (pr_serpentattack() < 32)
		{
			self->SetState (self->FindState ("Walk"));
		}
		else
		{
			self->SetState (self->FindState ("Attack"));
		}
	}
	return 0;
}

//============================================================================
//
// A_SerpentChooseAttack
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_SerpentChooseAttack)
{
	PARAM_ACTION_PROLOGUE;

	if (!self->target || self->CheckMeleeRange())
	{
		return 0;
	}
	if (self->MissileState != NULL)
	{
		self->SetState (self->MissileState);
	}
	return 0;
}
	
//============================================================================
//
// A_SerpentMeleeAttack
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_SerpentMeleeAttack)
{
	PARAM_ACTION_PROLOGUE;

	if (!self->target)
	{
		return 0;
	}
	if (self->CheckMeleeRange ())
	{
		int damage = pr_serpentmeattack.HitDice (5);
		int newdam = P_DamageMobj (self->target, self, self, damage, NAME_Melee);
		P_TraceBleed (newdam > 0 ? newdam : damage, self->target, self);
		S_Sound (self, CHAN_BODY, "SerpentMeleeHit", 1, ATTN_NORM);
	}
	if (pr_serpentmeattack() < 96)
	{
		CALL_ACTION(A_SerpentCheckForAttack, self);
	}
	return 0;
}

//============================================================================
//
// A_SerpentSpawnGibs
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_SerpentSpawnGibs)
{
	PARAM_ACTION_PROLOGUE;

	AActor *mo;
	static const char *GibTypes[] =
	{
		"SerpentGib3",
		"SerpentGib2",
		"SerpentGib1"
	};

	for (int i = countof(GibTypes)-1; i >= 0; --i)
	{
		double x = (pr_serpentgibs() - 128) / 16.;
		double y = (pr_serpentgibs() - 128) / 16.;

		mo = Spawn (GibTypes[i], self->Vec2OffsetZ(x, y, self->floorz + 1), ALLOW_REPLACE);
		if (mo)
		{
			mo->Vel.X = (pr_serpentgibs() - 128) / 1024.f;
			mo->Vel.Y = (pr_serpentgibs() - 128) / 1024.f;
			mo->Floorclip = 6;
		}
	}
	return 0;
}

//============================================================================
//
// A_FloatGib
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_FloatGib)
{
	PARAM_ACTION_PROLOGUE;

	self->Floorclip -= 1;
	return 0;
}

//============================================================================
//
// A_SinkGib
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_SinkGib)
{
	PARAM_ACTION_PROLOGUE;

	self->Floorclip += 1;;
	return 0;
}

//============================================================================
//
// A_DelayGib
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_DelayGib)
{
	PARAM_ACTION_PROLOGUE;

	self->tics -= pr_delaygib()>>2;
	return 0;
}

//============================================================================
//
// A_SerpentHeadCheck
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_SerpentHeadCheck)
{
	PARAM_ACTION_PROLOGUE;

	if (self->Z() <= self->floorz)
	{
		if (Terrains[P_GetThingFloorType(self)].IsLiquid)
		{
			P_HitFloor (self);
			self->SetState (NULL);
		}
		else
		{
			self->SetState (self->FindState(NAME_Death));
		}
	}
	return 0;
}

