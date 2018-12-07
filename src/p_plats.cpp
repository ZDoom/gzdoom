//-----------------------------------------------------------------------------
//
// Copyright 1993-1996 id Software
// Copyright 1994-1996 Raven Software
// Copyright 1998-1998 Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
// Copyright 1999-2016 Randy Heit
// Copyright 2002-2016 Christoph Oelckers
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//		Plats (i.e. elevator platforms) code, raising/lowering.
//
//-----------------------------------------------------------------------------

#include "i_system.h"
#include "m_random.h"
#include "p_local.h"
#include "p_lnspec.h"
#include "s_sndseq.h"
#include "doomstat.h"
#include "r_state.h"
#include "serializer.h"
#include "p_spec.h"
#include "g_levellocals.h"

static FRandom pr_doplat ("DoPlat");

IMPLEMENT_CLASS(DPlat, false, false)

DPlat::DPlat ()
{
}

void DPlat::Serialize(FSerializer &arc)
{
	Super::Serialize (arc);
	arc.Enum("type", m_Type)
		("speed", m_Speed)
		("low", m_Low)
		("high", m_High)
		("wait", m_Wait)
		("count", m_Count)
		.Enum("status", m_Status)
		.Enum("oldstatus", m_OldStatus)
		("crush", m_Crush)
		("tag", m_Tag);
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
	EMoveResult res;
		
	switch (m_Status)
	{
	case up:
		res = m_Sector->MoveFloor (m_Speed, m_High, m_Crush, 1, false);
										
		if (res == EMoveResult::crushed && (m_Crush == -1))
		{
			m_Count = m_Wait;
			m_Status = down;
			PlayPlatSound ("Platform");
		}
		else if (res == EMoveResult::pastdest)
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
		res = m_Sector->MoveFloor (m_Speed, m_Low, -1, -1, false);

		if (res == EMoveResult::pastdest)
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
		else if (res == EMoveResult::crushed && m_Crush < 0 && m_Type != platToggle)
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
			if (m_Sector->floorplane.fD() == m_Low)
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
bool EV_DoPlat (int tag, line_t *line, DPlat::EPlatType type, double height,
				double speed, int delay, int lip, int change)
{
	DPlat *plat;
	int secnum;
	sector_t *sec;
	bool rtn = false;
	bool manual = false;
	double newheight = 0;
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
		sec = &level.sectors[secnum];

		if (sec->PlaneMoving(sector_t::floor))
		{
			continue;
		}

		// Find lowest & highest floors around sector
		rtn = true;
		plat = Create<DPlat> (sec);

		plat->m_Type = type;
		plat->m_Crush = -1;
		plat->m_Tag = tag;
		plat->m_Speed = speed;
		plat->m_Wait = delay;

		//jff 1/26/98 Avoid raise plat bouncing a head off a ceiling and then
		//going down forever -- default lower to plat height when triggered
		plat->m_Low = sec->floorplane.fD();

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
			newheight = FindNextHighestFloor (sec, &spot);
			plat->m_High = sec->floorplane.PointToDist (spot, newheight);
			plat->m_Low = sec->floorplane.fD();
			plat->m_Status = DPlat::up;
			plat->PlayPlatSound ("Floor");
			sec->ClearSpecial();
			break;

		case DPlat::platUpByValue:
		case DPlat::platUpByValueStay:
			newheight = sec->floorplane.ZatPoint (sec->centerspot) + height;
			plat->m_High = sec->floorplane.PointToDist (sec->centerspot, newheight);
			plat->m_Low = sec->floorplane.fD();
			plat->m_Status = DPlat::up;
			plat->PlayPlatSound ("Floor");
			break;
		
		case DPlat::platDownByValue:
			newheight = sec->floorplane.ZatPoint (sec->centerspot) - height;
			plat->m_Low = sec->floorplane.PointToDist (sec->centerspot, newheight);
			plat->m_High = sec->floorplane.fD();
			plat->m_Status = DPlat::down;
			plat->PlayPlatSound ("Floor");
			break;

		case DPlat::platDownWaitUpStay:
		case DPlat::platDownWaitUpStayStone:
			newheight = FindLowestFloorSurrounding (sec, &spot) + lip;
			plat->m_Low = sec->floorplane.PointToDist (spot, newheight);

			if (plat->m_Low < sec->floorplane.fD())
				plat->m_Low = sec->floorplane.fD();

			plat->m_High = sec->floorplane.fD();
			plat->m_Status = DPlat::down;
			plat->PlayPlatSound (type == DPlat::platDownWaitUpStay ? "Platform" : "Floor");
			break;
		
		case DPlat::platUpNearestWaitDownStay:
			newheight = FindNextHighestFloor (sec, &spot);
			// Intentional fall-through

		case DPlat::platUpWaitDownStay:
			if (type == DPlat::platUpWaitDownStay)
			{
				newheight = FindHighestFloorSurrounding (sec, &spot);
			}
			plat->m_High = sec->floorplane.PointToDist (spot, newheight);
			plat->m_Low = sec->floorplane.fD();

			if (plat->m_High > sec->floorplane.fD())
				plat->m_High = sec->floorplane.fD();

			plat->m_Status = DPlat::up;
			plat->PlayPlatSound ("Platform");
			break;

		case DPlat::platPerpetualRaise:
			newheight = FindLowestFloorSurrounding (sec, &spot) + lip;
			plat->m_Low =  sec->floorplane.PointToDist (spot, newheight);

			if (plat->m_Low < sec->floorplane.fD())
				plat->m_Low = sec->floorplane.fD();

			newheight = FindHighestFloorSurrounding (sec, &spot);
			plat->m_High =  sec->floorplane.PointToDist (spot, newheight);

			if (plat->m_High > sec->floorplane.fD())
				plat->m_High = sec->floorplane.fD();

			plat->m_Status = pr_doplat() & 1 ? DPlat::up : DPlat::down;

			plat->PlayPlatSound ("Platform");
			break;

		case DPlat::platToggle:	//jff 3/14/98 add new type to support instant toggle
			plat->m_Crush = 10;	//jff 3/14/98 crush anything in the way

			// set up toggling between ceiling, floor inclusive
			newheight = FindLowestCeilingPoint(sec, &spot);
			plat->m_Low = sec->floorplane.PointToDist (spot, newheight);
			plat->m_High = sec->floorplane.fD();
			plat->m_Status = DPlat::down;
			SN_StartSequence (sec, CHAN_FLOOR, "Silence", 0);
			break;

		case DPlat::platDownToNearestFloor:
			newheight = FindNextLowestFloor (sec, &spot) + lip;
			plat->m_Low = sec->floorplane.PointToDist (spot, newheight);
			plat->m_Status = DPlat::down;
			plat->m_High = sec->floorplane.fD();
			plat->PlayPlatSound ("Platform");
			break;

		case DPlat::platDownToLowestCeiling:
			newheight = FindLowestCeilingSurrounding (sec, &spot);
		    plat->m_Low = sec->floorplane.PointToDist (spot, newheight);
			plat->m_High = sec->floorplane.fD();

			if (plat->m_Low < sec->floorplane.fD())
				plat->m_Low = sec->floorplane.fD();

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

void EV_StopPlat (int tag, bool remove)
{
	DPlat *scan;
	TThinkerIterator<DPlat> iterator;

	scan = iterator.Next();
	while (scan != nullptr)
	{
		DPlat *next = iterator.Next();
		if (scan->m_Status != DPlat::in_stasis && scan->m_Tag == tag)
		{
			if (!remove) scan->Stop();
			else scan->Destroy();
		}
		scan = next;
	}
}

