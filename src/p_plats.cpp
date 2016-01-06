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
//		Plats (i.e. elevator platforms) code, raising/lowering.
//
//-----------------------------------------------------------------------------

#include "i_system.h"
#include "m_random.h"
#include "doomdef.h"
#include "p_local.h"
#include "p_lnspec.h"
#include "s_sndseq.h"
#include "doomstat.h"
#include "r_state.h"
#include "gi.h"
#include "farchive.h"

static FRandom pr_doplat ("DoPlat");

IMPLEMENT_CLASS (DPlat)

inline FArchive &operator<< (FArchive &arc, DPlat::EPlatType &type)
{
	BYTE val = (BYTE)type;
	arc << val;
	type = (DPlat::EPlatType)val;
	return arc;
}
inline FArchive &operator<< (FArchive &arc, DPlat::EPlatState &state)
{
	BYTE val = (BYTE)state;
	arc << val;
	state = (DPlat::EPlatState)val;
	return arc;
}

DPlat::DPlat ()
{
}

void DPlat::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << m_Speed
		<< m_Low
		<< m_High
		<< m_Wait
		<< m_Count
		<< m_Status
		<< m_OldStatus
		<< m_Crush
		<< m_Tag
		<< m_Type;
}

void DPlat::PlayPlatSound (const char *sound)
{
	if (m_Sector->Flags & SECF_SILENTMOVE) return;

	if (m_Sector->seqType >= 0)
	{
		SN_StartSequence (m_Sector, CHAN_FLOOR, m_Sector->seqType, SEQ_PLATFORM, 0);
	}
	else if (m_Sector->SeqName != NAME_None)
	{
		SN_StartSequence (m_Sector, CHAN_FLOOR, m_Sector->SeqName, 0);
	}
	else
	{
		SN_StartSequence (m_Sector, CHAN_FLOOR, sound, 0);
	}
}

//
// Move a plat up and down
//
void DPlat::Tick ()
{
	EResult res;
		
	switch (m_Status)
	{
	case up:
		res = MoveFloor (m_Speed, m_High, m_Crush, 1, false);
										
		if (res == crushed && (m_Crush == -1))
		{
			m_Count = m_Wait;
			m_Status = down;
			PlayPlatSound ("Platform");
		}
		else if (res == pastdest)
		{
			SN_StopSequence (m_Sector, CHAN_FLOOR);
			if (m_Type != platToggle)
			{
				m_Count = m_Wait;
				m_Status = waiting;

				switch (m_Type)
				{
					case platRaiseAndStayLockout:
						// Instead of keeping the dead thinker like Heretic did let's 
						// better use a flag to avoid problems elsewhere. For example,
						// keeping the thinker would make tagwait wait indefinitely.
						m_Sector->planes[sector_t::floor].Flags |= PLANEF_BLOCKED; 
					case platRaiseAndStay:
					case platDownByValue:
					case platDownWaitUpStay:
					case platDownWaitUpStayStone:
					case platUpByValueStay:
					case platDownToNearestFloor:
					case platDownToLowestCeiling:
						Destroy ();
						break;
					default:
						break;
				}
			}
			else
			{
				m_OldStatus = m_Status;		//jff 3/14/98 after action wait  
				m_Status = in_stasis;		//for reactivation of toggle
			}
		}
		break;
		
	case down:
		res = MoveFloor (m_Speed, m_Low, -1, -1, false);

		if (res == pastdest)
		{
			SN_StopSequence (m_Sector, CHAN_FLOOR);
			// if not an instant toggle, start waiting
			if (m_Type != platToggle)		//jff 3/14/98 toggle up down
			{								// is silent, instant, no waiting
				m_Count = m_Wait;
				m_Status = waiting;

				switch (m_Type)
				{
					case platUpWaitDownStay:
					case platUpNearestWaitDownStay:
					case platUpByValue:
						Destroy ();
						break;
					default:
						break;
				}
			}
			else
			{	// instant toggles go into stasis awaiting next activation
				m_OldStatus = m_Status;		//jff 3/14/98 after action wait  
				m_Status = in_stasis;		//for reactivation of toggle
			}
		}
		else if (res == crushed && m_Crush < 0 && m_Type != platToggle)
		{
			m_Status = up;
			m_Count = m_Wait;
			PlayPlatSound ("Platform");
		}

		//jff 1/26/98 remove the plat if it bounced so it can be tried again
		//only affects plats that raise and bounce

		// remove the plat if it's a pure raise type
		switch (m_Type)
		{
			case platUpByValueStay:
			case platRaiseAndStay:
			case platRaiseAndStayLockout:
				Destroy ();
			default:
				break;
		}

		break;
		
	case waiting:
		if (m_Count > 0 && !--m_Count)
		{
			if (m_Sector->floorplane.d == m_Low)
				m_Status = up;
			else
				m_Status = down;

			if (m_Type == platToggle)
				SN_StartSequence (m_Sector, CHAN_FLOOR, "Silence", 0);
			else
				PlayPlatSound ("Platform");
		}
		break;

	case in_stasis:
		break;
	}
}

DPlat::DPlat (sector_t *sector)
	: DMovingFloor (sector)
{
}

//
// Do Platforms
//	[RH] Changed amount to height and added delay,
//		 lip, change, tag, and speed parameters.
//
bool EV_DoPlat (int tag, line_t *line, DPlat::EPlatType type, int height,
				int speed, int delay, int lip, int change)
{
	DPlat *plat;
	int secnum;
	sector_t *sec;
	bool rtn = false;
	bool manual = false;
	fixed_t newheight = 0;
	vertex_t *spot;

	if (tag != 0)
	{
		//	Activate all <type> plats that are in_stasis
		switch (type)
		{
		case DPlat::platToggle:
			rtn = true;
		case DPlat::platPerpetualRaise:
			P_ActivateInStasis (tag);
			break;

		default:
			break;
		}
	}


	// [RH] If tag is zero, use the sector on the back side
	//		of the activating line (if any).
	FSectorTagIterator itr(tag, line);
	while ((secnum = itr.Next()) >= 0)
	{
		sec = &sectors[secnum];

		if (sec->PlaneMoving(sector_t::floor))
		{
			continue;
		}

		// Find lowest & highest floors around sector
		rtn = true;
		plat = new DPlat (sec);

		plat->m_Type = type;
		plat->m_Crush = -1;
		plat->m_Tag = tag;
		plat->m_Speed = speed;
		plat->m_Wait = delay;

		//jff 1/26/98 Avoid raise plat bouncing a head off a ceiling and then
		//going down forever -- default lower to plat height when triggered
		plat->m_Low = sec->floorplane.d;

		if (change)
		{
			if (line)
				sec->SetTexture(sector_t::floor, line->sidedef[0]->sector->GetTexture(sector_t::floor));
			if (change == 1) sec->ClearSpecial();
		}

		switch (type)
		{
		case DPlat::platRaiseAndStay:
		case DPlat::platRaiseAndStayLockout:
			newheight = sec->FindNextHighestFloor (&spot);
			plat->m_High = sec->floorplane.PointToDist (spot, newheight);
			plat->m_Low = sec->floorplane.d;
			plat->m_Status = DPlat::up;
			plat->PlayPlatSound ("Floor");
			sec->ClearSpecial();
			break;

		case DPlat::platUpByValue:
		case DPlat::platUpByValueStay:
			newheight = sec->floorplane.ZatPoint (0, 0) + height;
			plat->m_High = sec->floorplane.PointToDist (0, 0, newheight);
			plat->m_Low = sec->floorplane.d;
			plat->m_Status = DPlat::up;
			plat->PlayPlatSound ("Floor");
			break;
		
		case DPlat::platDownByValue:
			newheight = sec->floorplane.ZatPoint (0, 0) - height;
			plat->m_Low = sec->floorplane.PointToDist (0, 0, newheight);
			plat->m_High = sec->floorplane.d;
			plat->m_Status = DPlat::down;
			plat->PlayPlatSound ("Floor");
			break;

		case DPlat::platDownWaitUpStay:
		case DPlat::platDownWaitUpStayStone:
			newheight = sec->FindLowestFloorSurrounding (&spot) + lip*FRACUNIT;
			plat->m_Low = sec->floorplane.PointToDist (spot, newheight);

			if (plat->m_Low < sec->floorplane.d)
				plat->m_Low = sec->floorplane.d;

			plat->m_High = sec->floorplane.d;
			plat->m_Status = DPlat::down;
			plat->PlayPlatSound (type == DPlat::platDownWaitUpStay ? "Platform" : "Floor");
			break;
		
		case DPlat::platUpNearestWaitDownStay:
			newheight = sec->FindNextHighestFloor (&spot);
			// Intentional fall-through

		case DPlat::platUpWaitDownStay:
			if (type == DPlat::platUpWaitDownStay)
			{
				newheight = sec->FindHighestFloorSurrounding (&spot);
			}
			plat->m_High = sec->floorplane.PointToDist (spot, newheight);
			plat->m_Low = sec->floorplane.d;

			if (plat->m_High > sec->floorplane.d)
				plat->m_High = sec->floorplane.d;

			plat->m_Status = DPlat::up;
			plat->PlayPlatSound ("Platform");
			break;

		case DPlat::platPerpetualRaise:
			newheight = sec->FindLowestFloorSurrounding (&spot) + lip*FRACUNIT;
			plat->m_Low =  sec->floorplane.PointToDist (spot, newheight);

			if (plat->m_Low < sec->floorplane.d)
				plat->m_Low = sec->floorplane.d;

			newheight = sec->FindHighestFloorSurrounding (&spot);
			plat->m_High =  sec->floorplane.PointToDist (spot, newheight);

			if (plat->m_High > sec->floorplane.d)
				plat->m_High = sec->floorplane.d;

			plat->m_Status = pr_doplat() & 1 ? DPlat::up : DPlat::down;

			plat->PlayPlatSound ("Platform");
			break;

		case DPlat::platToggle:	//jff 3/14/98 add new type to support instant toggle
			plat->m_Crush = 10;	//jff 3/14/98 crush anything in the way

			// set up toggling between ceiling, floor inclusive
			newheight = sec->FindLowestCeilingPoint (&spot);
			plat->m_Low = sec->floorplane.PointToDist (spot, newheight);
			plat->m_High = sec->floorplane.d;
			plat->m_Status = DPlat::down;
			SN_StartSequence (sec, CHAN_FLOOR, "Silence", 0);
			break;

		case DPlat::platDownToNearestFloor:
			newheight = sec->FindNextLowestFloor (&spot) + lip*FRACUNIT;
			plat->m_Low = sec->floorplane.PointToDist (spot, newheight);
			plat->m_Status = DPlat::down;
			plat->m_High = sec->floorplane.d;
			plat->PlayPlatSound ("Platform");
			break;

		case DPlat::platDownToLowestCeiling:
			newheight = sec->FindLowestCeilingSurrounding (&spot);
		    plat->m_Low = sec->floorplane.PointToDist (spot, newheight);
			plat->m_High = sec->floorplane.d;

			if (plat->m_Low < sec->floorplane.d)
				plat->m_Low = sec->floorplane.d;

			plat->m_Status = DPlat::down;
			plat->PlayPlatSound ("Platform");
			break;

		default:
			break;
		}
	}
	return rtn;
}

void DPlat::Reactivate ()
{
	if (m_Type == platToggle)	//jff 3/14/98 reactivate toggle type
		m_Status = m_OldStatus == up ? down : up;
	else
		m_Status = m_OldStatus;
}

void P_ActivateInStasis (int tag)
{
	DPlat *scan;
	TThinkerIterator<DPlat> iterator;

	while ( (scan = iterator.Next ()) )
	{
		if (scan->m_Tag == tag && scan->m_Status == DPlat::in_stasis)
			scan->Reactivate ();
	}
}

void DPlat::Stop ()
{
	m_OldStatus = m_Status;
	m_Status = in_stasis;
}

void EV_StopPlat (int tag)
{
	DPlat *scan;
	TThinkerIterator<DPlat> iterator;

	while ( (scan = iterator.Next ()) )
	{
		if (scan->m_Status != DPlat::in_stasis && scan->m_Tag == tag)
			scan->Stop ();
	}
}

