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

#include "m_alloc.h"
#include "i_system.h"
#include "z_zone.h"
#include "m_random.h"
#include "doomdef.h"
#include "p_local.h"
#include "s_sndseq.h"
#include "doomstat.h"
#include "r_state.h"

IMPLEMENT_SERIAL (DPlat, DMovingFloor)

DPlat::DPlat ()
{
}

void DPlat::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	if (arc.IsStoring ())
	{
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
	else
	{
		arc >> m_Speed
			>> m_Low
			>> m_High
			>> m_Wait
			>> m_Count
			>> m_Status
			>> m_OldStatus
			>> m_Crush
			>> m_Tag
			>> m_Type;
	}
}

void DPlat::PlayPlatSound (const char *sound)
{
	if (m_Sector->seqType >= 0)
		SN_StartSequence (m_Sector, m_Sector->seqType, SEQ_PLATFORM);
	else
		SN_StartSequence (m_Sector, sound);
}

//
// Move a plat up and down
//
void DPlat::RunThink ()
{
	EResult res;
		
	switch (m_Status)
	{
	case up:
		res = MoveFloor (m_Speed, m_High, m_Crush, 1);
										
		if (res == crushed && (m_Crush == -1))
		{
			m_Count = m_Wait;
			m_Status = down;
			PlayPlatSound ("Platform");
		}
		else if (res == pastdest)
		{
			SN_StopSequence (m_Sector);
			if (m_Type != platToggle)
			{
				m_Count = m_Wait;
				m_Status = waiting;

				switch (m_Type)
				{
					case platDownWaitUpStay:
					case platRaiseAndStay:
					case platUpByValueStay:
					case platDownToNearestFloor:
					case platDownToLowestCeiling:
						delete this;
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
		res = MoveFloor (m_Speed, m_Low, -1, -1);

		if (res == pastdest)
		{
			SN_StopSequence (m_Sector);
			// if not an instant toggle, start waiting
			if (m_Type != platToggle)		//jff 3/14/98 toggle up down
			{								// is silent, instant, no waiting
				m_Count = m_Wait;
				m_Status = waiting;

				switch (m_Type)
				{
					case platUpWaitDownStay:
					case platUpByValue:
						delete this;
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

		//jff 1/26/98 remove the plat if it bounced so it can be tried again
		//only affects plats that raise and bounce

		// remove the plat if it's a pure raise type
		switch (m_Type)
		{
			case platUpByValueStay:
			case platRaiseAndStay:
				delete this;
			default:
				break;
		}

		break;
		
	case waiting:
		if (!--m_Count)
		{
			if (m_Sector->floorheight == m_Low)
				m_Status = up;
			else
				m_Status = down;

			if (m_Type == platToggle)
				SN_StartSequence (m_Sector, "Silence");
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
BOOL EV_DoPlat (int tag, line_t *line, DPlat::EPlatType type, int height,
				int speed, int delay, int lip, int change)
{
	DPlat *plat;
	int secnum;
	sector_t *sec;
	int rtn = false;
	BOOL manual = false;

	// [RH] If tag is zero, use the sector on the back side
	//		of the activating line (if any).
	if (!tag)
	{
		if (!line || !(sec = line->backsector))
			return false;
		secnum = sec - sectors;
		manual = true;
		goto manual_plat;
	}

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
		
	secnum = -1;
	while ((secnum = P_FindSectorFromTag (tag, secnum)) >= 0)
	{
		sec = &sectors[secnum];

manual_plat:
		if (sec->floordata)
			continue;
		
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
		plat->m_Low = sec->floorheight;

		if (change)
		{
			if (line)
				sec->floorpic = sides[line->sidenum[0]].sector->floorpic;
			if (change == 1)
				sec->special = 0;	// Stop damage and other stuff, if any
		}

		switch (type)
		{
		case DPlat::platRaiseAndStay:
			plat->m_High = P_FindNextHighestFloor (sec, sec->floorheight);
			plat->m_Status = DPlat::up;
			plat->PlayPlatSound ("Floor");
			break;

		case DPlat::platUpByValue:
		case DPlat::platUpByValueStay:
			plat->m_High = sec->floorheight + height;
			plat->m_Status = DPlat::up;
			plat->PlayPlatSound ("Floor");
			break;
		
		case DPlat::platDownByValue:
			plat->m_Low = sec->floorheight - height;
			plat->m_Status = DPlat::down;
			plat->PlayPlatSound ("Floor");
			break;

		case DPlat::platDownWaitUpStay:
			plat->m_Low = P_FindLowestFloorSurrounding (sec) + lip*FRACUNIT;

			if (plat->m_Low > sec->floorheight)
				plat->m_Low = sec->floorheight;

			plat->m_High = sec->floorheight;
			plat->m_Status = DPlat::down;
			plat->PlayPlatSound ("Platform");
			break;
		
		case DPlat::platUpWaitDownStay:
			plat->m_High = P_FindHighestFloorSurrounding (sec);

			if (plat->m_High < sec->floorheight)
				plat->m_High = sec->floorheight;

			plat->m_Status = DPlat::up;
			plat->PlayPlatSound ("Platform");
			break;

		case DPlat::platPerpetualRaise:
			plat->m_Low = P_FindLowestFloorSurrounding (sec) + lip*FRACUNIT;

			if (plat->m_Low > sec->floorheight)
				plat->m_Low = sec->floorheight;

			plat->m_High = P_FindHighestFloorSurrounding (sec);

			if (plat->m_High < sec->floorheight)
				plat->m_High = sec->floorheight;

			plat->m_Status = P_Random (pr_doplat) & 1 ? DPlat::up : DPlat::down;

			plat->PlayPlatSound ("Platform");
			break;

		case DPlat::platToggle:	//jff 3/14/98 add new type to support instant toggle
			plat->m_Crush = 10;	//jff 3/14/98 crush anything in the way

			// set up toggling between ceiling, floor inclusive
			plat->m_Low = sec->ceilingheight;
			plat->m_High = sec->floorheight;
			plat->m_Status = DPlat::down;
			SN_StartSequence (sec, "Silence");
			break;

		case DPlat::platDownToNearestFloor:
			plat->m_Low = P_FindNextLowestFloor (sec, sec->floorheight) + lip*FRACUNIT;
			plat->m_Status = DPlat::down;
			plat->m_High = sec->floorheight;
			plat->PlayPlatSound ("Platform");
			break;

		case DPlat::platDownToLowestCeiling:
		    plat->m_Low = P_FindLowestCeilingSurrounding (sec);
			plat->m_High = sec->floorheight;

			if (plat->m_Low > sec->floorheight)
				plat->m_Low = sec->floorheight;

			plat->m_Status = DPlat::down;
			plat->PlayPlatSound ("Platform");
			break;

		default:
			break;
		}
		if (manual)
			return rtn;
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

