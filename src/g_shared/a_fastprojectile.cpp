
#include "a_sharedglobal.h"
#include "p_local.h"
#include "g_level.h"
#include "r_sky.h"
#include "p_lnspec.h"


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

	PrevX = x;
	PrevY = y;
	PrevZ = z;
	fixed_t oldz = z;
	PrevAngle = angle;

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
	if (radius > 0)
	{
		while ( ((abs(velx) >> shift) > radius) || ((abs(vely) >> shift) > radius))
		{
			// we need to take smaller steps.
			shift++;
			count<<=1;
		}
	}

	// Handle movement
	if (velx || vely || (z != floorz) || velz)
	{
		xfrac = velx >> shift;
		yfrac = vely >> shift;
		zfrac = velz >> shift;
		changexy = xfrac || yfrac;
		int ripcount = count >> 3;
		for (i = 0; i < count; i++)
		{
			if (changexy)
			{
				if (--ripcount <= 0)
				{
					tm.LastRipped = NULL;	// [RH] Do rip damage each step, like Hexen
				}
				
				if (!P_TryMove (this, x + xfrac,y + yfrac, true, NULL, tm))
				{ // Blocked move
					if (!(flags3 & MF3_SKYEXPLODE))
					{
						if (tm.ceilingline &&
							tm.ceilingline->backsector &&
							tm.ceilingline->backsector->GetTexture(sector_t::ceiling) == skyflatnum &&
							z >= tm.ceilingline->backsector->ceilingplane.ZatPoint (x, y))
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
			z += zfrac;
			UpdateWaterLevel (oldz);
			oldz = z;
			if (z <= floorz)
			{ // Hit the floor

				if (floorpic == skyflatnum && !(flags3 & MF3_SKYEXPLODE))
				{
					// [RH] Just remove the missile without exploding it
					//		if this is a sky floor.
					Destroy ();
					return;
				}

				z = floorz;
				P_HitFloor (this);
				P_ExplodeMissile (this, NULL, NULL);
				return;
			}
			if (z + height > ceilingz)
			{ // Hit the ceiling

				if (ceilingpic == skyflatnum &&  !(flags3 & MF3_SKYEXPLODE))
				{
					Destroy ();
					return;
				}

				z = ceilingz - height;
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
		FName name = (ENamedName) this->GetClass()->Meta.GetMetaInt (ACMETA_MissileName, NAME_None);
		if (name != NAME_None)
		{
			fixed_t hitz = z-8*FRACUNIT;

			if (hitz < floorz)
			{
				hitz = floorz;
			}
			// Do not clip this offset to the floor.
			hitz += GetClass()->Meta.GetMetaFixed (ACMETA_MissileHeight);
		
			const PClass *trail = PClass::FindClass(name);
			if (trail != NULL)
			{
				AActor *act = Spawn (trail, x, y, hitz, ALLOW_REPLACE);
				if (act != NULL)
				{
					act->angle = this->angle;
				}
			}
		}
	}
}

