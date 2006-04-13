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

IMPLEMENT_CLASS (DSectorEffect)

DSectorEffect::DSectorEffect ()
{
	m_Sector = NULL;
}

DSectorEffect::~DSectorEffect ()
{
	if (m_Sector)
	{
		if (m_Sector->floordata == this)
		{
			m_Sector->floordata = NULL;
			stopinterpolation (INTERP_SectorFloor, m_Sector);
		}
		if (m_Sector->ceilingdata == this)
		{
			m_Sector->ceilingdata = NULL;
			stopinterpolation (INTERP_SectorCeiling, m_Sector);
		}
		if (m_Sector->lightingdata == this)
		{
			m_Sector->lightingdata = NULL;
		}
	}
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

IMPLEMENT_CLASS (DMover)

DMover::DMover ()
{
}

DMover::DMover (sector_t *sector)
	: DSectorEffect (sector)
{
}

IMPLEMENT_CLASS (DMovingFloor)

DMovingFloor::DMovingFloor ()
{
}

DMovingFloor::DMovingFloor (sector_t *sector)
	: DMover (sector)
{
	sector->floordata = this;
	setinterpolation (INTERP_SectorFloor, sector);
}

IMPLEMENT_CLASS (DMovingCeiling)

DMovingCeiling::DMovingCeiling ()
{
}

DMovingCeiling::DMovingCeiling (sector_t *sector)
	: DMover (sector)
{
	sector->ceilingdata = this;
	setinterpolation (INTERP_SectorCeiling, sector);
}

//
// Move a plane (floor or ceiling) and check for crushing
// [RH] Crush specifies the actual amount of crushing damage inflictable.
//		(Use -1 to prevent it from trying to crush)
//		dest is the desired d value for the plane
//
DMover::EResult DMover::MovePlane (fixed_t speed, fixed_t dest, int crush,
								   int floorOrCeiling, int direction)
{
	bool	 	flag;
	fixed_t 	lastpos;
	//fixed_t		destheight;	//jff 02/04/98 used to keep floors/ceilings
							// from moving thru each other
	switch (floorOrCeiling)
	{
	case 0:
		// FLOOR
		lastpos = m_Sector->floorplane.d;
		switch (direction)
		{
		case -1:
			// DOWN
			m_Sector->floorplane.ChangeHeight (-speed);
			if (m_Sector->floorplane.d >= dest)
			{
				m_Sector->floorplane.d = dest;
				flag = P_ChangeSector (m_Sector, crush,
					m_Sector->floorplane.HeightDiff (lastpos), 0);
				if (flag)
				{
					m_Sector->floorplane.d = lastpos;
					P_ChangeSector (m_Sector, crush,
						m_Sector->floorplane.HeightDiff (dest), 0);
				}
				else
				{
					m_Sector->floortexz += m_Sector->floorplane.HeightDiff (lastpos);
					m_Sector->AdjustFloorClip ();
				}
				return pastdest;
			}
			else
			{
				flag = P_ChangeSector (m_Sector, crush, -speed, 0);
				if (flag)
				{
					m_Sector->floorplane.d = lastpos;
					P_ChangeSector (m_Sector, crush, speed, 0);
					return crushed;
				}
				else
				{
					m_Sector->floortexz += m_Sector->floorplane.HeightDiff (lastpos);
					m_Sector->AdjustFloorClip ();
				}
			}
			break;
												
		case 1:
			// UP
			// jff 02/04/98 keep floor from moving thru ceilings
			// [RH] not so easy with arbitrary planes
			//destheight = (dest < m_Sector->ceilingheight) ? dest : m_Sector->ceilingheight;
			if ((m_Sector->ceilingplane.a | m_Sector->ceilingplane.b |
				 m_Sector->floorplane.a | m_Sector->floorplane.b) == 0 &&
				-dest > m_Sector->ceilingplane.d)
			{
				dest = -m_Sector->ceilingplane.d;
			}
			m_Sector->floorplane.ChangeHeight (speed);
			if (m_Sector->floorplane.d <= dest)
			{
				m_Sector->floorplane.d = dest;
				flag = P_ChangeSector (m_Sector, crush,
					m_Sector->floorplane.HeightDiff (lastpos), 0);
				if (flag)
				{
					m_Sector->floorplane.d = lastpos;
					P_ChangeSector (m_Sector, crush,
						m_Sector->floorplane.HeightDiff (dest), 0);
				}
				else
				{
					m_Sector->floortexz += m_Sector->floorplane.HeightDiff (lastpos);
					m_Sector->AdjustFloorClip ();
				}
				return pastdest;
			}
			else
			{
				// COULD GET CRUSHED
				flag = P_ChangeSector (m_Sector, crush, speed, 0);
				if (flag)
				{
					if (crush >= 0 && gameinfo.gametype != GAME_Hexen)
					{
						m_Sector->floortexz += m_Sector->floorplane.HeightDiff (lastpos);
						m_Sector->AdjustFloorClip ();
						return crushed;
					}
					m_Sector->floorplane.d = lastpos;
					P_ChangeSector (m_Sector, crush, -speed, 0);
					return crushed;
				}
				m_Sector->floortexz += m_Sector->floorplane.HeightDiff (lastpos);
				m_Sector->AdjustFloorClip ();
			}
			break;
		}
		break;
																		
	  case 1:
		// CEILING
		lastpos = m_Sector->ceilingplane.d;
		switch (direction)
		{
		case -1:
			// DOWN
			// jff 02/04/98 keep ceiling from moving thru floors
			// [RH] not so easy with arbitrary planes
			//destheight = (dest > m_Sector->floorheight) ? dest : m_Sector->floorheight;
			if ((m_Sector->ceilingplane.a | m_Sector->ceilingplane.b |
				 m_Sector->floorplane.a | m_Sector->floorplane.b) == 0 &&
				dest < -m_Sector->floorplane.d)
			{
				dest = -m_Sector->floorplane.d;
			}
			m_Sector->ceilingplane.ChangeHeight (-speed);
			if (m_Sector->ceilingplane.d <= dest)
			{
				m_Sector->ceilingplane.d = dest;
				flag = P_ChangeSector (m_Sector, crush,
					m_Sector->ceilingplane.HeightDiff (lastpos), 1);

				if (flag)
				{
					m_Sector->ceilingplane.d = lastpos;
					P_ChangeSector (m_Sector, crush,
						m_Sector->ceilingplane.HeightDiff (dest), 1);
				}
				else
				{
					m_Sector->ceilingtexz += m_Sector->ceilingplane.HeightDiff (lastpos);
				}
				return pastdest;
			}
			else
			{
				// COULD GET CRUSHED
				flag = P_ChangeSector (m_Sector, crush, -speed, 1);
				if (flag)
				{
					if (crush >= 0 && gameinfo.gametype != GAME_Hexen)
					{
						m_Sector->ceilingtexz += m_Sector->ceilingplane.HeightDiff (lastpos);
						return crushed;
					}
					m_Sector->ceilingplane.d = lastpos;
					P_ChangeSector (m_Sector, crush, speed, 1);
					return crushed;
				}
				m_Sector->ceilingtexz += m_Sector->ceilingplane.HeightDiff (lastpos);
			}
			break;
												
		case 1:
			// UP
			m_Sector->ceilingplane.ChangeHeight (speed);
			if (m_Sector->ceilingplane.d >= dest)
			{
				m_Sector->ceilingplane.d = dest;
				flag = P_ChangeSector (m_Sector, crush,
					m_Sector->ceilingplane.HeightDiff (lastpos), 1);
				if (flag)
				{
					m_Sector->ceilingplane.d = lastpos;
					P_ChangeSector (m_Sector, crush,
						m_Sector->ceilingplane.HeightDiff (dest), 1);
				}
				else
				{
					m_Sector->ceilingtexz += m_Sector->ceilingplane.HeightDiff (lastpos);
				}
				return pastdest;
			}
			else
			{
				flag = P_ChangeSector (m_Sector, crush, speed, 1);
// UNUSED
#if 0
				if (flag)
				{
					m_Sector->ceilingplane.d = lastpos;
					P_ChangeSector (m_Sector, crush, -speed, 1);
					return crushed;
				}
#endif
				m_Sector->ceilingtexz += m_Sector->ceilingplane.HeightDiff (lastpos);
			}
			break;
		}
		break;
				
	}
	return ok;
}
