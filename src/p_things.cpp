// [RH] new file to deal with general things

#include "doomtype.h"
#include "p_local.h"
#include "p_effect.h"
#include "info.h"
#include "s_sound.h"
#include "tables.h"
#include "doomstat.h"
#include "m_random.h"
#include "c_console.h"
#include "c_dispatch.h"
#include "a_sharedglobal.h"

// List of spawnable things for the Thing_Spawn and Thing_Projectile specials.
const TypeInfo *SpawnableThings[MAX_SPAWNABLES];

bool P_Thing_Spawn (int tid, int type, angle_t angle, bool fog, int newtid)
{
	int rtn = 0;
	const TypeInfo *kind;
	AActor *spot, *mobj;
	FActorIterator iterator (tid);

	if (type >= MAX_SPAWNABLES)
		return false;

	if ( (kind = SpawnableThings[type]) == NULL)
		return false;

	if ((GetDefaultByType (kind)->flags & MF_COUNTKILL) && (dmflags & DF_NO_MONSTERS))
		return false;

	while ( (spot = iterator.Next ()) )
	{
		mobj = Spawn (kind, spot->x, spot->y, spot->z);

		if (mobj)
		{
			if (P_TestMobjLocation (mobj))
			{
				rtn++;
				mobj->angle = angle;
				if (fog)
				{
					Spawn<ATeleportFog> (spot->x, spot->y, spot->z);
				}
				if (mobj->flags & MF_SPECIAL)
					mobj->flags |= MF_DROPPED;	// Don't respawn
				mobj->tid = newtid;
				mobj->AddToHash ();
			}
			else
			{
				mobj->Destroy ();
				rtn = false;
			}
		}
	}

	return rtn != 0;
}

// [BC] Added
// [RH] Fixed

bool P_Thing_Move (int tid, int mapspot)
{
	FActorIterator iterator1 (tid);
	FActorIterator iterator2 (mapspot);
	AActor *source, *target;

	source = iterator1.Next ();
	target = iterator2.Next ();

	if (source != NULL && target != NULL)
	{
		fixed_t oldx, oldy, oldz;

		oldx = source->x;
		oldy = source->y;
		oldz = source->z;

		source->SetOrigin (target->x, target->y, target->z);
		if (P_TestMobjLocation (source))
		{
			Spawn<ATeleportFog> (target->x, target->y, target->z);
			Spawn<ATeleportFog> (oldx, oldy, oldz);
			return true;
		}
		else
		{
			source->SetOrigin (oldx, oldy, oldz);
			return false;
		}
	}
	return false;
}

bool P_Thing_Projectile (int tid, int type, angle_t angle,
	fixed_t speed, fixed_t vspeed, bool gravity)
{
	int rtn = 0;
	const TypeInfo *kind;
	AActor *spot, *mobj;
	TActorIterator<AMapSpot> iterator (tid);

	if (type >= MAX_SPAWNABLES)
		return false;

	if ((kind = SpawnableThings[type]) == NULL)
		return false;

	if ((GetDefaultByType (kind)->flags & MF_COUNTKILL) && (dmflags & DF_NO_MONSTERS))
		return false;

	while ( (spot = iterator.Next ()) )
	{
		mobj = Spawn (kind, spot->x, spot->y, spot->z);

		if (mobj)
		{
			if (mobj->SeeSound)
			{
				S_SoundID (mobj, CHAN_VOICE, mobj->SeeSound, 1, ATTN_NORM);
			}
			if (gravity)
			{
				mobj->flags &= ~MF_NOGRAVITY;
				if (!(mobj->flags & MF_COUNTKILL))
					mobj->flags2 |= MF2_LOGRAV;
			}
			else
			{
				mobj->flags |= MF_NOGRAVITY;
			}
			mobj->target = spot;
			mobj->angle = angle;
			mobj->momx = FixedMul (speed, finecosine[angle>>ANGLETOFINESHIFT]);
			mobj->momy = FixedMul (speed, finesine[angle>>ANGLETOFINESHIFT]);
			mobj->momz = vspeed;
			if (mobj->flags & MF_SPECIAL)
				mobj->flags |= MF_DROPPED;
			if (mobj->flags & MF_MISSILE)
				rtn = P_CheckMissileSpawn (mobj);
			else if (!P_TestMobjLocation (mobj))
				mobj->Destroy ();
		} 
	}

	return rtn != 0;
}

CCMD (dumpspawnables)
{
	int i;

	for (i = 0; i < MAX_SPAWNABLES; i++)
	{
		if (SpawnableThings[i] != NULL)
		{
			Printf ("%d %s\n", i, SpawnableThings[i]->Name + 1);
		}
	}
}
