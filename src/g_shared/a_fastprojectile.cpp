
#include "a_sharedglobal.h"
#include "p_local.h"
#include "g_level.h"
#include "r_sky.h"
#include "p_lnspec.h"
#include "b_bot.h"
#include "p_checkposition.h"


IMPLEMENT_CLASS(AFastProjectile)


//----------------------------------------------------------------------------
//
// AFastProjectile :: Tick
//
// Thinker for the ultra-fast projectiles used by Heretic and Hexen
//
//----------------------------------------------------------------------------

void AFastProjectile::Tick ()
{
	int i;
	DVector3 frac;
	int changexy;

	ClearInterpolation();
	double oldz = Z();

	if (!(flags5 & MF5_NOTIMEFREEZE))
	{
		//Added by MC: Freeze mode.
		if (bglobal.freeze || level.flags2 & LEVEL2_FROZEN)
		{
			return;
		}
	}


	// [RH] Ripping is a little different than it was in Hexen
	FCheckPosition tm(!!(flags2 & MF2_RIP));

	int count = 8;
	if (radius > 0)
	{
		while ( fabs(Vel.X) > radius * count || fabs(Vel.Y) > radius * count)
		{
			// we need to take smaller steps.
			count += count;
		}
	}

	// Handle movement
	if (!Vel.isZero() || (Z() != floorz))
	{
		frac = Vel / count;
		changexy = frac.X != 0 || frac.Y != 0;
		int ripcount = count / 8;
		for (i = 0; i < count; i++)
		{
			if (changexy)
			{
				if (--ripcount <= 0)
				{
					tm.LastRipped.Clear();	// [RH] Do rip damage each step, like Hexen
				}
				
				if (!P_TryMove (this, Pos() + frac, true, NULL, tm))
				{ // Blocked move
					if (!(flags3 & MF3_SKYEXPLODE))
					{
						if (tm.ceilingline &&
							tm.ceilingline->backsector &&
							tm.ceilingline->backsector->GetTexture(sector_t::ceiling) == skyflatnum &&
							Z() >= tm.ceilingline->backsector->ceilingplane.ZatPoint(PosRelative(tm.ceilingline)))
						{
							// Hack to prevent missiles exploding against the sky.
							// Does not handle sky floors.
							Destroy ();
							return;
						}
						// [RH] Don't explode on horizon lines.
						if (BlockingLine != NULL && BlockingLine->special == Line_Horizon)
						{
							Destroy ();
							return;
						}
					}

					P_ExplodeMissile (this, BlockingLine, BlockingMobj);
					return;
				}
			}
			AddZ(frac.Z);
			UpdateWaterLevel ();
			oldz = Z();
			if (oldz <= floorz)
			{ // Hit the floor

				if (floorpic == skyflatnum && !(flags3 & MF3_SKYEXPLODE))
				{
					// [RH] Just remove the missile without exploding it
					//		if this is a sky floor.
					Destroy ();
					return;
				}

				SetZ(floorz);
				P_HitFloor (this);
				P_ExplodeMissile (this, NULL, NULL);
				return;
			}
			if (Top() > ceilingz)
			{ // Hit the ceiling

				if (ceilingpic == skyflatnum &&  !(flags3 & MF3_SKYEXPLODE))
				{
					Destroy ();
					return;
				}

				SetZ(ceilingz - Height);
				P_ExplodeMissile (this, NULL, NULL);
				return;
			}
			if (changexy && ripcount <= 0) 
			{
				ripcount = count >> 3;
				Effect();
			}
		}
	}
	if (!CheckNoDelay())
		return;		// freed itself
	// Advance the state
	if (tics != -1)
	{
		if (tics > 0) tics--;
		while (!tics)
		{
			if (!SetState (state->GetNextState ()))
			{ // mobj was removed
				return;
			}
		}
	}
}


void AFastProjectile::Effect()
{
	FName name = GetClass()->MissileName;
	if (name != NAME_None)
	{
		double hitz = Z()-8;

		if (hitz < floorz)
		{
			hitz = floorz;
		}
		// Do not clip this offset to the floor.
		hitz += GetClass()->MissileHeight;
		
		PClassActor *trail = PClass::FindActor(name);
		if (trail != NULL)
		{
			AActor *act = Spawn (trail, PosAtZ(hitz), ALLOW_REPLACE);
			if (act != NULL)
			{
				act->Angles.Yaw = Angles.Yaw;
			}
		}
	}
}

