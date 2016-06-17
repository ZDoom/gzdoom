// Emacs style mode select	 -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// $Log:$
//
// DESCRIPTION:
//		Base class for effects on sectors.
//		[RH] Created this class hierarchy.
//
//-----------------------------------------------------------------------------

#include "dsectoreffect.h"
#include "gi.h"
#include "p_local.h"
#include "p_3dmidtex.h"
#include "r_data/r_interpolate.h"
#include "statnums.h"
#include "farchive.h"
#include "doomstat.h"

IMPLEMENT_CLASS (DSectorEffect)

DSectorEffect::DSectorEffect ()
: DThinker(STAT_SECTOREFFECT)
{
	m_Sector = NULL;
}

void DSectorEffect::Destroy()
{
	if (m_Sector)
	{
		if (m_Sector->floordata == this)
		{
			m_Sector->floordata = NULL;
		}
		if (m_Sector->ceilingdata == this)
		{
			m_Sector->ceilingdata = NULL;
		}
		if (m_Sector->lightingdata == this)
		{
			m_Sector->lightingdata = NULL;
		}
	}
	Super::Destroy();
}

DSectorEffect::DSectorEffect (sector_t *sector)
{
	m_Sector = sector;
}

void DSectorEffect::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << m_Sector;
}

IMPLEMENT_POINTY_CLASS (DMover)
	DECLARE_POINTER(interpolation)
END_POINTERS

DMover::DMover ()
{
}

DMover::DMover (sector_t *sector)
	: DSectorEffect (sector)
{
	interpolation = NULL;
}

void DMover::Destroy()
{
	StopInterpolation();
	Super::Destroy();
}

void DMover::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << interpolation;
}

void DMover::StopInterpolation(bool force)
{
	if (interpolation != NULL)
	{
		interpolation->DelRef(force);
		interpolation = NULL;
	}
}



IMPLEMENT_CLASS (DMovingFloor)

DMovingFloor::DMovingFloor ()
{
}

DMovingFloor::DMovingFloor (sector_t *sector)
	: DMover (sector)
{
	sector->floordata = this;
	interpolation = sector->SetInterpolation(sector_t::FloorMove, true);
}

IMPLEMENT_CLASS (DMovingCeiling)

DMovingCeiling::DMovingCeiling ()
{
}

DMovingCeiling::DMovingCeiling (sector_t *sector, bool interpolate)
	: DMover (sector)
{
	sector->ceilingdata = this;
	if (interpolate) interpolation = sector->SetInterpolation(sector_t::CeilingMove, true);
}

bool sector_t::MoveAttached(int crush, double move, int floorOrCeiling, bool resetfailed)
{
	if (!P_Scroll3dMidtex(this, crush, move, !!floorOrCeiling) && resetfailed)
	{
		P_Scroll3dMidtex(this, crush, -move, !!floorOrCeiling);
		return false;
	}
	if (!P_MoveLinkedSectors(this, crush, move, !!floorOrCeiling) && resetfailed)
	{
		P_MoveLinkedSectors(this, crush, -move, !!floorOrCeiling);
		P_Scroll3dMidtex(this, crush, -move, !!floorOrCeiling);
		return false;
	}
	return true;
}

//
// Move a plane (floor or ceiling) and check for crushing
// [RH] Crush specifies the actual amount of crushing damage inflictable.
//		(Use -1 to prevent it from trying to crush)
//		dest is the desired d value for the plane
//
EMoveResult sector_t::MoveFloor(double speed, double dest, int crush, int direction, bool hexencrush)
{
	bool	 	flag;
	double 	lastpos;
	double		movedest;
	double		move;
	//double		destheight;	//jff 02/04/98 used to keep floors/ceilings
							// from moving thru each other
	lastpos = floorplane.fD();
	switch (direction)
	{
	case -1:
		// DOWN
		movedest = floorplane.GetChangedHeight(-speed);
		if (movedest >= dest)
		{
			move = floorplane.HeightDiff(lastpos, dest);

			if (!MoveAttached(crush, move, 0, true)) return EMoveResult::crushed;

			floorplane.setD(dest);
			flag = P_ChangeSector(this, crush, move, 0, false);
			if (flag)
			{
				floorplane.setD(lastpos);
				P_ChangeSector(this, crush, -move, 0, true);
				MoveAttached(crush, -move, 0, false);
			}
			else
			{
				ChangePlaneTexZ(sector_t::floor, move);
				AdjustFloorClip();
			}
			return EMoveResult::pastdest;
		}
		else
		{
			if (!MoveAttached(crush, -speed, 0, true)) return EMoveResult::crushed;

			floorplane.setD(movedest);

			flag = P_ChangeSector(this, crush, -speed, 0, false);
			if (flag)
			{
				floorplane.setD(lastpos);
				P_ChangeSector(this, crush, speed, 0, true);
				MoveAttached(crush, speed, 0, false);
				return EMoveResult::crushed;
			}
			else
			{
				ChangePlaneTexZ(sector_t::floor, floorplane.HeightDiff(lastpos));
				AdjustFloorClip();
			}
		}
		break;

	case 1:
		// UP
		// jff 02/04/98 keep floor from moving thru ceilings
		// [RH] not so easy with arbitrary planes
		//destheight = (dest < ceilingheight) ? dest : ceilingheight;
		if (!ceilingplane.isSlope() && !floorplane.isSlope() &&
			!PortalIsLinked(sector_t::ceiling) &&
			(!(i_compatflags2 & COMPATF2_FLOORMOVE) && -dest > ceilingplane.fD()))
		{
			dest = -ceilingplane.fD();
		}

		movedest = floorplane.GetChangedHeight(speed);

		if (movedest <= dest)
		{
			move = floorplane.HeightDiff(lastpos, dest);

			if (!MoveAttached(crush, move, 0, true)) return EMoveResult::crushed;

			floorplane.setD(dest);

			flag = P_ChangeSector(this, crush, move, 0, false);
			if (flag)
			{
				floorplane.setD(lastpos);
				P_ChangeSector(this, crush, -move, 0, true);
				MoveAttached(crush, -move, 0, false);
			}
			else
			{
				ChangePlaneTexZ(sector_t::floor, move);
				AdjustFloorClip();
			}
			return EMoveResult::pastdest;
		}
		else
		{
			if (!MoveAttached(crush, speed, 0, true)) return EMoveResult::crushed;

			floorplane.setD(movedest);

			// COULD GET CRUSHED
			flag = P_ChangeSector(this, crush, speed, 0, false);
			if (flag)
			{
				if (crush >= 0 && !hexencrush)
				{
					ChangePlaneTexZ(sector_t::floor, floorplane.HeightDiff(lastpos));
					AdjustFloorClip();
					return EMoveResult::crushed;
				}
				floorplane.setD(lastpos);
				P_ChangeSector(this, crush, -speed, 0, true);
				MoveAttached(crush, -speed, 0, false);
				return EMoveResult::crushed;
			}
			ChangePlaneTexZ(sector_t::floor, floorplane.HeightDiff(lastpos));
			AdjustFloorClip();
		}
		break;
	}
	return EMoveResult::ok;
}

EMoveResult sector_t::MoveCeiling(double speed, double dest, int crush, int direction, bool hexencrush)
{
	bool	 	flag;
	double 	lastpos;
	double		movedest;
	double		move;
	//double		destheight;	//jff 02/04/98 used to keep floors/ceilings
	// from moving thru each other

	lastpos = ceilingplane.fD();
	switch (direction)
	{
	case -1:
		// DOWN
		// jff 02/04/98 keep ceiling from moving thru floors
		// [RH] not so easy with arbitrary planes
		//destheight = (dest > floorheight) ? dest : floorheight;
		if (!ceilingplane.isSlope() && !floorplane.isSlope() &&
			!PortalIsLinked(sector_t::floor) &&
			(!(i_compatflags2 & COMPATF2_FLOORMOVE) && dest < -floorplane.fD()))
		{
			dest = -floorplane.fD();
		}
		movedest = ceilingplane.GetChangedHeight (-speed);
		if (movedest <= dest)
		{
			move = ceilingplane.HeightDiff (lastpos, dest);

			if (!MoveAttached(crush, move, 1, true)) return EMoveResult::crushed;

			ceilingplane.setD(dest);
			flag = P_ChangeSector (this, crush, move, 1, false);

			if (flag)
			{
				ceilingplane.setD(lastpos);
				P_ChangeSector (this, crush, -move, 1, true);
				MoveAttached(crush, -move, 1, false);
			}
			else
			{
				ChangePlaneTexZ(sector_t::ceiling, move);
			}
			return EMoveResult::pastdest;
		}
		else
		{
			if (!MoveAttached(crush, -speed, 1, true)) return EMoveResult::crushed;

			ceilingplane.setD(movedest);

			// COULD GET CRUSHED
			flag = P_ChangeSector (this, crush, -speed, 1, false);
			if (flag)
			{
				if (crush >= 0 && !hexencrush)
				{
					ChangePlaneTexZ(sector_t::ceiling, ceilingplane.HeightDiff (lastpos));
					return EMoveResult::crushed;
				}
				ceilingplane.setD(lastpos);
				P_ChangeSector (this, crush, speed, 1, true);
				MoveAttached(crush, speed, 1, false);
				return EMoveResult::crushed;
			}
			ChangePlaneTexZ(sector_t::ceiling, ceilingplane.HeightDiff (lastpos));
		}
		break;
												
	case 1:
		// UP
		movedest = ceilingplane.GetChangedHeight (speed);
		if (movedest >= dest)
		{
			move = ceilingplane.HeightDiff (lastpos, dest);

			if (!MoveAttached(crush, move, 1, true)) return EMoveResult::crushed;

			ceilingplane.setD(dest);

			flag = P_ChangeSector (this, crush, move, 1, false);
			if (flag)
			{
				ceilingplane.setD(lastpos);
				P_ChangeSector (this, crush, move, 1, true);
				MoveAttached(crush, move, 1, false);
			}
			else
			{
				ChangePlaneTexZ(sector_t::ceiling, move);
			}
			return EMoveResult::pastdest;
		}
		else
		{
			if (!MoveAttached(crush, speed, 1, true)) return EMoveResult::crushed;

			ceilingplane.setD(movedest);

			flag = P_ChangeSector (this, crush, speed, 1, false);
			if (flag)
			{
				ceilingplane.setD(lastpos);
				P_ChangeSector (this, crush, -speed, 1, true);
				MoveAttached(crush, -speed, 1, false);
				return EMoveResult::crushed;
			}
			ChangePlaneTexZ(sector_t::ceiling, ceilingplane.HeightDiff (lastpos));
		}
		break;
	}
	return EMoveResult::ok;
}
