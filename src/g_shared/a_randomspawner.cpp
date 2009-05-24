/*
** a_randomspawner.cpp
** A thing that randomly spawns one item in a list of many, before disappearing.
*/

#include "actor.h"
#include "info.h"
#include "m_random.h"
#include "p_local.h"
#include "p_enemy.h"
#include "s_sound.h"
#include "statnums.h"
#include "gstrings.h"
#include "a_action.h"
#include "thingdef/thingdef.h"

#define MAX_RANDOMSPAWNERS_RECURSION 32 // Should be largely more than enough, honestly.
static FRandom pr_randomspawn("RandomSpawn");

class ARandomSpawner : public AActor
{
	DECLARE_CLASS (ARandomSpawner, AActor)

	void PostBeginPlay()
	{
		AActor *newmobj = NULL;
		FDropItem *di;   // di will be our drop item list iterator
		FDropItem *drop; // while drop stays as the reference point.
		int n=0;

		Super::PostBeginPlay();

		drop = di = GetDropItems();
		if (di != NULL)
		{
			while (di != NULL)
			{
				if (di->Name != NAME_None)
				{
					if (di->amount < 0) di->amount = 1; // default value is -1, we need a positive value.
					n += di->amount; // this is how we can weight the list.
					di = di->Next;
				}
			}
			// Then we reset the iterator to the start position...
			di = drop;
			// Take a random number...
			n = pr_randomspawn(n);
			// And iterate in the array up to the random number chosen.
			while (n > 0)
			{
				if (di->Name != NAME_None)
				{
					n -= di->amount;
					if (di->Next != NULL) di = di->Next; else n=0;
				}
			}
			// So now we can spawn the dropped item.
			if (special1 >= MAX_RANDOMSPAWNERS_RECURSION)	// Prevents infinite recursions
				Spawn("Unknown", x, y, z, NO_REPLACE);		// Show that there's a problem.
			else if (pr_randomspawn() <= di->probability)	// prob 255 = always spawn, prob 0 = never spawn.
			{
				newmobj = Spawn(di->Name, x, y, z, ALLOW_REPLACE);
				// copy everything relevant
				newmobj->SpawnAngle = newmobj->angle = angle;
				newmobj->special    = special;
				newmobj->args[0]    = args[0];
				newmobj->args[1]    = args[1];
				newmobj->args[2]    = args[2];
				newmobj->args[3]    = args[3];
				newmobj->args[4]    = args[4];
				newmobj->SpawnFlags = SpawnFlags;
				newmobj->HandleSpawnFlags();
				newmobj->tid        = tid;
				newmobj->AddToHash();
				newmobj->momx = momx;
				newmobj->momy = momy;
				newmobj->momz = momz;
				newmobj->master = master;	// For things such as DamageMaster/DamageChildren, transfer mastery.
				newmobj->target = target;
				newmobj->tracer = tracer;
				newmobj->CopyFriendliness(this, false);
				if (!(flags & MF_DROPPED)) newmobj->flags &= ~MF_DROPPED;
				// Special1 is used to count how many recursions we're in.
				if (newmobj->IsKindOf(PClass::FindClass("RandomSpawner")))
					newmobj->special1 = ++special1;

			}
		}
		if ((newmobj != NULL) && ((newmobj->flags4 & MF4_BOSSDEATH) || (newmobj->flags2 & MF2_BOSS)))
			this->target = newmobj; // If the spawned actor has either of those flags, it's a boss.
		else Destroy();	// "else" because a boss-replacing spawner must wait until it can call A_BossDeath.
	}

	void Tick()	// This function is needed for handling boss replacers
	{
		Super::Tick();
		if (target == NULL || target->health <= 0)
		{
			health = 0;
			CALL_ACTION(A_BossDeath, this);
			Destroy();
		}
	}

};

IMPLEMENT_CLASS (ARandomSpawner)

