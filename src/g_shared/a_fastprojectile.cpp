
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
	fixed_t xfrac;
	fixed_t yfrac;
	fixed_t zfrac;
	int changexy;

	ClearInterpolation();
	fixed_t oldz = _f_Z();

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

	int shift = 3;
	int count = 8;
	if (_f_radius() > 0)
	{
		while ( ((abs(_f_velx()) >> shift) > _f_radius()) || ((abs(_f_vely()) >> shift) > _f_radius()))
		{
			// we need to take smaller steps.
			shift++;
			count<<=1;
		}
	}

	// Handle movement
	if (Vel.X != 0 || Vel.Y != 0 || (_f_Z() != floorz) || _f_velz())
	{
		xfrac = _f_velx() >> shift;
		yfrac = _f_vely() >> shift;
		zfrac = _f_velz() >> shift;
		changexy = xfrac || yfrac;
		int ripcount = count >> 3;
		for (i = 0; i < count; i++)
		{
			if (changexy)
			{
				if (--ripcount <= 0)
				{
					tm.LastRipped.Clear();	// [RH] Do rip damage each step, like Hexen
				}
				
				if (!P_TryMove (this, _f_X() + xfrac,_f_Y() + yfrac, true, NULL, tm))
				{ // Blocked move
					if (!(flags3 & MF3_SKYEXPLODE))
					{
						if (tm.ceilingline &&
							tm.ceilingline->backsector &&
							tm.ceilingline->backsector->GetTexture(sector_t::ceiling) == skyflatnum &&
							_f_Z() >= tm.ceilingline->backsector->ceilingplane.ZatPoint(PosRelative(tm.ceilingline)))
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
			_f_AddZ(zfrac);
			UpdateWaterLevel (oldz);
			oldz = _f_Z();
			if (_f_Z() <= floorz)
			{ // Hit the floor

				if (floorpic == skyflatnum && !(flags3 & MF3_SKYEXPLODE))
				{
					// [RH] Just remove the missile without exploding it
					//		if this is a sky floor.
					Destroy ();
					return;
				}

				_f_SetZ(floorz);
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

				SetZ(ceilingz - _Height());
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
	//if (pr_smoke() < 128)	// [RH] I think it looks better if it's consistent
	{
		FName name = GetClass()->MissileName;
		if (name != NAME_None)
		{
			fixed_t hitz = _f_Z()-8*FRACUNIT;

			if (hitz < floorz)
			{
				hitz = floorz;
			}
			// Do not clip this offset to the floor.
			hitz += GetClass()->MissileHeight;
		
			PClassActor *trail = PClass::FindActor(name);
			if (trail != NULL)
			{
				AActor *act = Spawn (trail, _f_X(), _f_Y(), hitz, ALLOW_REPLACE);
				if (act != NULL)
				{
					act->Angles.Yaw = Angles.Yaw;
				}
			}
		}
	}
}

