#include "actor.h"
#include "info.h"
#include "m_random.h"
#include "p_local.h"
#include "p_enemy.h"
#include "s_sound.h"
#include "a_doomglobal.h"
#include "statnums.h"
#include "gstrings.h"
#include "a_action.h"
#include "thingdef/thingdef.h"

/*
- in the decorate definition define multiple drop items
- use the function GetDropItems to get the first drop item, then iterate over them and count how many dropitems are defined.
- with M_Random( ) % NUMBEROFDROPITEMS you get a random drop item number (let's call it n)
- use GetDropItems again to get the first drop item, then iterate to the n-th drop item
*/
static FRandom pr_randomspawn("RandomSpawn");

class ARandomSpawner : public AActor
{
	DECLARE_STATELESS_ACTOR (ARandomSpawner, AActor)

	void PostBeginPlay()
	{
		AActor *newmobj;
		FDropItem *di;   // di will be our drop item list iterator
		FDropItem *drop; // while drop stays as the reference point.
		int n=0;

		Super::PostBeginPlay();

		drop = di = GetDropItems(RUNTIME_TYPE(this));
		// Always make sure it actually exists.
		if (di != NULL)
		{
			// First, we get the size of the array...
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
					di = di->Next;
				}
			}
			// So now we can spawn the dropped item.
			if (pr_randomspawn() <= di->probability) // prob 255 = always spawn, prob 0 = never spawn.
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
				newmobj->momx = momx;
				newmobj->momy = momy;
				newmobj->momz = momz;
				newmobj->CopyFriendliness(this, false);
			}
		}
	}
};

IMPLEMENT_STATELESS_ACTOR (ARandomSpawner, Any, -1, 0)
END_DEFAULTS

