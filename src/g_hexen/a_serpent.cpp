#include "actor.h"
#include "info.h"
#include "p_local.h"
#include "s_sound.h"
#include "p_enemy.h"
#include "a_action.h"
#include "m_random.h"
#include "p_terrain.h"

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

void A_SerpentUnHide (AActor *actor)
{
	actor->renderflags &= ~RF_INVISIBLE;
	actor->floorclip = 24*FRACUNIT;
}

//============================================================================
//
// A_SerpentHide
//
//============================================================================

void A_SerpentHide (AActor *actor)
{
	actor->renderflags |= RF_INVISIBLE;
	actor->floorclip = 0;
}

//============================================================================
//
// A_SerpentRaiseHump
// 
// Raises the hump above the surface by raising the floorclip level
//============================================================================

void A_SerpentRaiseHump (AActor *actor)
{
	actor->floorclip -= 4*FRACUNIT;
}

//============================================================================
//
// A_SerpentLowerHump
// 
//============================================================================

void A_SerpentLowerHump (AActor *actor)
{
	actor->floorclip += 4*FRACUNIT;
}

//============================================================================
//
// A_SerpentHumpDecide
//
//		Decided whether to hump up, or if the mobj is a serpent leader, 
//			to missile attack
//============================================================================

void A_SerpentHumpDecide (AActor *actor)
{
	if (actor->MissileState != NULL)
	{
		if (pr_serpenthump() > 30)
		{
			return;
		}
		else if (pr_serpenthump() < 40)
		{ // Missile attack
			actor->SetState (actor->MeleeState);
			return;
		}
	}
	else if (pr_serpenthump() > 3)
	{
		return;
	}
	if (!actor->CheckMeleeRange ())
	{ // The hump shouldn't occur when within melee range
		if (actor->MissileState != NULL && pr_serpenthump() < 128)
		{
			actor->SetState (actor->MeleeState);
		}
		else
		{	
			actor->SetState (actor->FindState ("Hump"));
			S_Sound (actor, CHAN_BODY, "SerpentActive", 1, ATTN_NORM);
		}
	}
}

//============================================================================
//
// A_SerpentCheckForAttack
//
//============================================================================

void A_SerpentCheckForAttack (AActor *actor)
{
	if (!actor->target)
	{
		return;
	}
	if (actor->MissileState != NULL)
	{
		if (!actor->CheckMeleeRange ())
		{
			actor->SetState (actor->FindState ("Attack"));
			return;
		}
	}
	if (P_CheckMeleeRange2 (actor))
	{
		actor->SetState (actor->FindState ("Walk"));
	}
	else if (actor->CheckMeleeRange ())
	{
		if (pr_serpentattack() < 32)
		{
			actor->SetState (actor->FindState ("Walk"));
		}
		else
		{
			actor->SetState (actor->FindState ("Attack"));
		}
	}
}

//============================================================================
//
// A_SerpentChooseAttack
//
//============================================================================

void A_SerpentChooseAttack (AActor *actor)
{
	if (!actor->target || actor->CheckMeleeRange())
	{
		return;
	}
	if (actor->MissileState != NULL)
	{
		actor->SetState (actor->MissileState);
	}
}
	
//============================================================================
//
// A_SerpentMeleeAttack
//
//============================================================================

void A_SerpentMeleeAttack (AActor *actor)
{
	if (!actor->target)
	{
		return;
	}
	if (actor->CheckMeleeRange ())
	{
		int damage = pr_serpentmeattack.HitDice (5);
		P_DamageMobj (actor->target, actor, actor, damage, NAME_Melee);
		P_TraceBleed (damage, actor->target, actor);
		S_Sound (actor, CHAN_BODY, "SerpentMeleeHit", 1, ATTN_NORM);
	}
	if (pr_serpentmeattack() < 96)
	{
		A_SerpentCheckForAttack (actor);
	}
}

//============================================================================
//
// A_SerpentSpawnGibs
//
//============================================================================

void A_SerpentSpawnGibs (AActor *actor)
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
		mo = Spawn (GibTypes[i],
			actor->x+((pr_serpentgibs()-128)<<12), 
			actor->y+((pr_serpentgibs()-128)<<12),
			actor->floorz+FRACUNIT, ALLOW_REPLACE);
		if (mo)
		{
			mo->momx = (pr_serpentgibs()-128)<<6;
			mo->momy = (pr_serpentgibs()-128)<<6;
			mo->floorclip = 6*FRACUNIT;
		}
	}
}

//============================================================================
//
// A_FloatGib
//
//============================================================================

void A_FloatGib (AActor *actor)
{
	actor->floorclip -= FRACUNIT;
}

//============================================================================
//
// A_SinkGib
//
//============================================================================

void A_SinkGib (AActor *actor)
{
	actor->floorclip += FRACUNIT;
}

//============================================================================
//
// A_DelayGib
//
//============================================================================

void A_DelayGib (AActor *actor)
{
	actor->tics -= pr_delaygib()>>2;
}

//============================================================================
//
// A_SerpentHeadCheck
//
//============================================================================

void A_SerpentHeadCheck (AActor *actor)
{
	if (actor->z <= actor->floorz)
	{
		if (Terrains[P_GetThingFloorType(actor)].IsLiquid)
		{
			P_HitFloor (actor);
			actor->SetState (NULL);
		}
		else
		{
			actor->SetState (actor->FindState(NAME_Death));
		}
	}
}

