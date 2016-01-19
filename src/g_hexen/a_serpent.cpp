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
	self->renderflags &= ~RF_INVISIBLE;
	self->floorclip = 24*FRACUNIT;
}

//============================================================================
//
// A_SerpentHide
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_SerpentHide)
{
	self->renderflags |= RF_INVISIBLE;
	self->floorclip = 0;
}

//============================================================================
//
// A_SerpentRaiseHump
// 
// Raises the hump above the surface by raising the floorclip level
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_SerpentRaiseHump)
{
	self->floorclip -= 4*FRACUNIT;
}

//============================================================================
//
// A_SerpentLowerHump
// 
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_SerpentLowerHump)
{
	self->floorclip += 4*FRACUNIT;
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
	if (self->MissileState != NULL)
	{
		if (pr_serpenthump() > 30)
		{
			return;
		}
		else if (pr_serpenthump() < 40)
		{ // Missile attack
			self->SetState (self->MeleeState);
			return;
		}
	}
	else if (pr_serpenthump() > 3)
	{
		return;
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
}

//============================================================================
//
// A_SerpentCheckForAttack
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_SerpentCheckForAttack)
{
	if (!self->target)
	{
		return;
	}
	if (self->MissileState != NULL)
	{
		if (!self->CheckMeleeRange ())
		{
			self->SetState (self->FindState ("Attack"));
			return;
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
}

//============================================================================
//
// A_SerpentChooseAttack
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_SerpentChooseAttack)
{
	if (!self->target || self->CheckMeleeRange())
	{
		return;
	}
	if (self->MissileState != NULL)
	{
		self->SetState (self->MissileState);
	}
}
	
//============================================================================
//
// A_SerpentMeleeAttack
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_SerpentMeleeAttack)
{
	if (!self->target)
	{
		return;
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
}

//============================================================================
//
// A_SerpentSpawnGibs
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_SerpentSpawnGibs)
{
	AActor *mo;
	static const char *GibTypes[] =
	{
		"SerpentGib3",
		"SerpentGib2",
		"SerpentGib1"
	};

	for (int i = countof(GibTypes)-1; i >= 0; --i)
	{
		fixedvec2 pos = self->Vec2Offset(
			((pr_serpentgibs() - 128) << 12),
			((pr_serpentgibs() - 128) << 12));

		mo = Spawn (GibTypes[i], pos.x, pos.y,
			self->floorz+FRACUNIT, ALLOW_REPLACE);
		if (mo)
		{
			mo->velx = (pr_serpentgibs()-128)<<6;
			mo->vely = (pr_serpentgibs()-128)<<6;
			mo->floorclip = 6*FRACUNIT;
		}
	}
}

//============================================================================
//
// A_FloatGib
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_FloatGib)
{
	self->floorclip -= FRACUNIT;
}

//============================================================================
//
// A_SinkGib
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_SinkGib)
{
	self->floorclip += FRACUNIT;
}

//============================================================================
//
// A_DelayGib
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_DelayGib)
{
	self->tics -= pr_delaygib()>>2;
}

//============================================================================
//
// A_SerpentHeadCheck
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_SerpentHeadCheck)
{
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
}

