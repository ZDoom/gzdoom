/*
** a_randomspawner.cpp
** A thing that randomly spawns one item in a list of many, before disappearing.
** bouncecount is used to keep track of recursions (so as to prevent infinite loops).
** Species is used to store the index of the spawned actor's name.
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
#include "v_text.h"
#include "doomstat.h"
#include "doomdata.h"

#define MAX_RANDOMSPAWNERS_RECURSION 32 // Should be largely more than enough, honestly.
static FRandom pr_randomspawn("RandomSpawn");

static bool IsMonster(DDropItem *di)
{
	const PClass *pclass = PClass::FindClass(di->Name);

	if (NULL == pclass)
	{
		return false;
	}

	return 0 != (GetDefaultByType(pclass)->flags3 & MF3_ISMONSTER);
}

class ARandomSpawner : public AActor
{
	DECLARE_CLASS (ARandomSpawner, AActor)

	// To handle "RandomSpawning" missiles, the code has to be split in two parts.
	// If the following code is not done in BeginPlay, missiles will use the
	// random spawner's velocity (0...) instead of their own.
	void BeginPlay()
	{
		DDropItem *di;   // di will be our drop item list iterator
		DDropItem *drop; // while drop stays as the reference point.
		int n = 0;
		bool nomonsters = (dmflags & DF_NO_MONSTERS) || (level.flags2 & LEVEL2_NOMONSTERS);

		Super::BeginPlay();
		drop = di = GetDropItems();
		if (di != NULL)
		{
			while (di != NULL)
			{
				if (di->Name != NAME_None)
				{
					if (!nomonsters || !IsMonster(di))
					{
						if (di->Amount < 0) di->Amount = 1; // default value is -1, we need a positive value.
						n += di->Amount; // this is how we can weight the list.
					}
					di = di->Next;
				}
			}
			if (n == 0)
			{ // Nothing left to spawn. They must have all been monsters, and monsters are disabled.
				Destroy();
				return;
			}
			// Then we reset the iterator to the start position...
			di = drop;
			// Take a random number...
			n = pr_randomspawn(n);
			// And iterate in the array up to the random number chosen.
			while (n > -1 && di != NULL)
			{
				if (di->Name != NAME_None &&
					(!nomonsters || !IsMonster(di)))
				{
					n -= di->Amount;
					if ((di->Next != NULL) && (n > -1))
						di = di->Next;
					else
						n = -1;
				}
				else
				{
					di = di->Next;
				}
			}
			// So now we can spawn the dropped item.
			if (di == NULL || bouncecount >= MAX_RANDOMSPAWNERS_RECURSION)	// Prevents infinite recursions
			{
				Spawn("Unknown", Pos(), NO_REPLACE);		// Show that there's a problem.
				Destroy();
				return;
			}
			else if (pr_randomspawn() <= di->Probability)	// prob 255 = always spawn, prob 0 = almost never spawn.
			{
				// Handle replacement here so as to get the proper speed and flags for missiles
				PClassActor *cls;
				cls = PClass::FindActor(di->Name);
				if (cls != NULL)
				{
					PClassActor *rep = cls->GetReplacement();
					if (rep != NULL)
					{
						cls = rep;
					}
				}
				if (cls != NULL)
				{
					Species = cls->TypeName;
					AActor *defmobj = GetDefaultByType(cls);
					this->Speed = defmobj->Speed;
					this->flags  |= (defmobj->flags  & MF_MISSILE);
					this->flags2 |= (defmobj->flags2 & MF2_SEEKERMISSILE);
					this->flags4 |= (defmobj->flags4 & MF4_SPECTRAL);
				}
				else
				{
					Printf(TEXTCOLOR_RED "Unknown item class %s to drop from a random spawner\n", di->Name.GetChars());
					Species = NAME_None;
				}
			}
		}
	}

	// The second half of random spawning. Now that the spawner is initialized, the
	// real actor can be created. If the following code were in BeginPlay instead,
	// missiles would not have yet obtained certain information that is absolutely
	// necessary to them -- such as their source and destination.
	void PostBeginPlay()
	{
		Super::PostBeginPlay();

		AActor *newmobj = NULL;
		bool boss = false;

		if (Species == NAME_None)
		{
			Destroy();
			return;
		}
		PClassActor *cls = PClass::FindActor(Species);
		if (this->flags & MF_MISSILE && target && target->target) // Attempting to spawn a missile.
		{
			if ((tracer == NULL) && (flags2 & MF2_SEEKERMISSILE))
			{
				tracer = target->target;
			}
			newmobj = P_SpawnMissileXYZ(Pos(), target, target->target, cls, false);
		}
		else 
		{		
			newmobj = Spawn(cls, Pos(), NO_REPLACE);
		}
		if (newmobj != NULL)
		{
			// copy everything relevant
			newmobj->SpawnAngle = SpawnAngle;
			newmobj->Angles		= Angles;
			newmobj->SpawnPoint = SpawnPoint;
			newmobj->special    = special;
			newmobj->args[0]    = args[0];
			newmobj->args[1]    = args[1];
			newmobj->args[2]    = args[2];
			newmobj->args[3]    = args[3];
			newmobj->args[4]    = args[4];
			newmobj->special1   = special1;
			newmobj->special2   = special2;
			newmobj->SpawnFlags = SpawnFlags & ~MTF_SECRET;	// MTF_SECRET needs special treatment to avoid incrementing the secret counter twice. It had already been processed for the spawner itself.
			newmobj->HandleSpawnFlags();
			newmobj->SpawnFlags = SpawnFlags;
			newmobj->tid        = tid;
			newmobj->AddToHash();
			newmobj->Vel	= Vel;
			newmobj->master = master;	// For things such as DamageMaster/DamageChildren, transfer mastery.
			newmobj->target = target;
			newmobj->tracer = tracer;
			newmobj->CopyFriendliness(this, false);
			// This handles things such as projectiles with the MF4_SPECTRAL flag that have
			// a health set to -2 after spawning, for internal reasons.
			if (health != SpawnHealth()) newmobj->health = health;
			if (!(flags & MF_DROPPED)) newmobj->flags &= ~MF_DROPPED;
			// Handle special altitude flags
			if (newmobj->flags & MF_SPAWNCEILING)
			{
				newmobj->SetZ(newmobj->ceilingz - newmobj->Height - SpawnPoint.Z);
			}
			else if (newmobj->flags2 & MF2_SPAWNFLOAT) 
			{
				double space = newmobj->ceilingz - newmobj->Height - newmobj->floorz;
				if (space > 48)
				{
					space -= 40;
					newmobj->SetZ((space * pr_randomspawn()) / 256. + newmobj->floorz + 40);
				}
				newmobj->AddZ(SpawnPoint.Z);
			}
			if (newmobj->flags & MF_MISSILE)
				P_CheckMissileSpawn(newmobj, 0);
			// Bouncecount is used to count how many recursions we're in.
			if (newmobj->IsKindOf(PClass::FindClass("RandomSpawner")))
				newmobj->bouncecount = ++bouncecount;
			// If the spawned actor has either of those flags, it's a boss.
			if ((newmobj->flags4 & MF4_BOSSDEATH) || (newmobj->flags2 & MF2_BOSS))
				boss = true;
			// If a replaced actor has either of those same flags, it's also a boss.
			AActor *rep = GetDefaultByType(GetClass()->GetReplacee());
			if (rep && ((rep->flags4 & MF4_BOSSDEATH) || (rep->flags2 & MF2_BOSS)))
				boss = true;
		}
		if (boss)
			this->tracer = newmobj;
		else	// "else" because a boss-replacing spawner must wait until it can call A_BossDeath.
			Destroy();
	}

	void Tick()	// This function is needed for handling boss replacers
	{
		Super::Tick();
		if (tracer == NULL || tracer->health <= 0)
		{
			A_BossDeath(this);
			Destroy();
		}
	}

};

IMPLEMENT_CLASS (ARandomSpawner)
