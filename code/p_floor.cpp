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
//		Floor animation: raising stairs.
//
//-----------------------------------------------------------------------------

#include "z_zone.h"
#include "doomdef.h"
#include "p_local.h"
#include "p_lnspec.h"
#include "s_sound.h"
#include "s_sndseq.h"
#include "doomstat.h"
#include "r_state.h"
#include "tables.h"

extern fixed_t FloatBobOffsets[64];

//
// FLOORS
//

IMPLEMENT_SERIAL (DFloor, DMovingFloor)

DFloor::DFloor ()
{
}

void DFloor::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	if (arc.IsStoring ())
	{
		arc << m_Type
			<< m_Crush
			<< m_Direction
			<< m_NewSpecial
			<< m_Texture
			<< m_FloorDestHeight
			<< m_Speed
			<< m_ResetCount
			<< m_OrgHeight
			<< m_Delay
			<< m_PauseTime
			<< m_StepTime
			<< m_PerStepTime;
	}
	else
	{
		arc >> m_Type
			>> m_Crush
			>> m_Direction
			>> m_NewSpecial
			>> m_Texture
			>> m_FloorDestHeight
			>> m_Speed
			>> m_ResetCount
			>> m_OrgHeight
			>> m_Delay
			>> m_PauseTime
			>> m_StepTime
			>> m_PerStepTime;
	}
}

IMPLEMENT_SERIAL (DElevator, DMover)

DElevator::DElevator ()
{
}

void DElevator::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	if (arc.IsStoring ())
	{
		arc << m_Type
			<< m_Direction
			<< m_FloorDestHeight
			<< m_CeilingDestHeight
			<< m_Speed;
	}
	else
	{
		arc >> m_Type
			>> m_Direction
			>> m_FloorDestHeight
			>> m_CeilingDestHeight
			>> m_Speed;
	}
}

IMPLEMENT_SERIAL (DFloorWaggle, DMovingFloor)

DFloorWaggle::DFloorWaggle ()
{
}

void DFloorWaggle::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	if (arc.IsStoring ())
	{
		arc << m_OriginalHeight
			<< m_Accumulator
			<< m_AccDelta
			<< m_TargetScale
			<< m_Scale
			<< m_ScaleDelta
			<< m_Ticker
			<< m_State;
	}
	else
	{
		arc >> m_OriginalHeight
			>> m_Accumulator
			>> m_AccDelta
			>> m_TargetScale
			>> m_Scale
			>> m_ScaleDelta
			>> m_Ticker
			>> m_State;
	}
}


//
// MOVE A FLOOR TO IT'S DESTINATION (UP OR DOWN)
//
void DFloor::RunThink ()
{
	EResult res;

	// [RH] Handle resetting stairs
	if (m_Type == buildStair || m_Type == waitStair)
	{
		if (m_ResetCount)
		{
			if (--m_ResetCount == 0)
			{
				m_Type = resetStair;
				m_Direction = (m_Direction > 0) ? -1 : 1;
				m_FloorDestHeight = m_OrgHeight;
			}
		}
		if (m_PauseTime)
		{
			m_PauseTime--;
			return;
		}
		else if (m_StepTime)
		{
			if (--m_StepTime == 0)
			{
				m_PauseTime = m_Delay;
				m_StepTime = m_PerStepTime;
			}
		}
	}

	if (m_Type == waitStair)
		return;

	res = MoveFloor (m_Speed, m_FloorDestHeight, m_Crush, m_Direction);
	
	if (res == pastdest)
	{
		SN_StopSequence (m_Sector);

		if (m_Type == buildStair)
			m_Type = waitStair;

		if (m_Type != waitStair || m_ResetCount == 0)
		{
			if (m_Direction == 1)
			{
				switch (m_Type)
				{
				case donutRaise:
				case genFloorChgT:
				case genFloorChg0:
					m_Sector->special = m_NewSpecial;
					//fall thru
				case genFloorChg:
					m_Sector->floorpic = m_Texture;
					break;
				default:
					break;
				}
			}
			else if (m_Direction == -1)
			{
				switch(m_Type)
				{
				case floorLowerAndChange:
				case genFloorChgT:
				case genFloorChg0:
					m_Sector->special = m_NewSpecial;
					//fall thru
				case genFloorChg:
					m_Sector->floorpic = m_Texture;
					break;
				default:
					break;
				}
			}

			m_Sector->floordata = NULL; //jff 2/22/98
			delete this;

			//jff 2/26/98 implement stair retrigger lockout while still building
			// note this only applies to the retriggerable generalized stairs

			if (m_Sector->stairlock == -2)		// if this sector is stairlocked
			{
				sector_t *sec = m_Sector;
				sec->stairlock = -1;				// thinker done, promote lock to -1

				while (sec->prevsec != -1 && sectors[sec->prevsec].stairlock != -2)
					sec = &sectors[sec->prevsec];	// search for a non-done thinker
				if (sec->prevsec == -1)				// if all thinkers previous are done
				{
					sec = m_Sector;			// search forward
					while (sec->nextsec != -1 && sectors[sec->nextsec].stairlock != -2) 
						sec = &sectors[sec->nextsec];
					if (sec->nextsec == -1)			// if all thinkers ahead are done too
					{
						while (sec->prevsec != -1)	// clear all locks
						{
							sec->stairlock = 0;
							sec = &sectors[sec->prevsec];
						}
						sec->stairlock = 0;
					}
				}
			}
		}
	}
}

//
// T_MoveElevator()
//
// Move an elevator to it's destination (up or down)
// Called once per tick for each moving floor.
//
// Passed an elevator_t structure that contains all pertinent info about the
// move. See P_SPEC.H for fields.
// No return.
//
// jff 02/22/98 added to support parallel floor/ceiling motion
//
void DElevator::RunThink ()
{
	EResult res;

	if (m_Direction < 0)	// moving down
	{
		res = MoveCeiling (m_Speed, m_CeilingDestHeight, m_Direction);
		if (res == ok || res == pastdest)
			MoveFloor (m_Speed, m_FloorDestHeight, m_Direction);
	}
	else // up
	{
		res = MoveFloor (m_Speed, m_FloorDestHeight, m_Direction);
		if (res == ok || res == pastdest)
			MoveCeiling (m_Speed, m_CeilingDestHeight, m_Direction);
	}

	if (res == pastdest)	// if destination height acheived
	{
		// make floor stop sound
		SN_StopSequence (m_Sector);

		m_Sector->floordata = NULL;		//jff 2/22/98
		m_Sector->ceilingdata = NULL;	//jff 2/22/98
		delete this;	// remove elevator from actives
	}
}

static void StartFloorSound (sector_t *sec)
{
	if (sec->seqType >= 0)
	{
		SN_StartSequence (sec, sec->seqType, SEQ_PLATFORM);
	}
	else
	{
		SN_StartSequence (sec, "Floor");
	}
}

void DFloor::StartFloorSound ()
{
	::StartFloorSound (m_Sector);
}

void DElevator::StartFloorSound ()
{
	::StartFloorSound (m_Sector);
}

DFloor::DFloor (sector_t *sec)
	: DMovingFloor (sec)
{
}

//
// HANDLE FLOOR TYPES
// [RH] Added tag, speed, height, crush, change params.
//
BOOL EV_DoFloor (DFloor::EFloor floortype, line_t *line, int tag,
				 fixed_t speed, fixed_t height, int crush, int change)
{
	int 				secnum;
	BOOL 				rtn;
	sector_t*			sec;
	DFloor*				floor;
	BOOL				manual = false;

	rtn = false;

	// check if a manual trigger; if so do just the sector on the backside
	if (tag == 0)
	{
		if (!line || !(sec = line->backsector))
			return rtn;
		secnum = sec-sectors;
		manual = true;
		goto manual_floor;
	}

	secnum = -1;
	while ((secnum = P_FindSectorFromTag (tag, secnum)) >= 0)
	{
		sec = &sectors[secnum];

manual_floor:
		// ALREADY MOVING?	IF SO, KEEP GOING...
		if (sec->floordata)
			continue;
		
		// new floor thinker
		rtn = true;
		floor = new DFloor (sec);
		floor->m_Type = floortype;
		floor->m_Crush = -1;
		floor->m_Speed = speed;
		floor->m_ResetCount = 0;				// [RH]
		floor->m_OrgHeight = sec->floorheight;	// [RH]

		floor->StartFloorSound ();

		switch (floortype)
		{
		case DFloor::floorLowerToHighest:
			floor->m_Direction = -1;
			floor->m_FloorDestHeight = P_FindHighestFloorSurrounding(sec);
			// [RH] DOOM's turboLower type did this. I've just extended it
			//		to be applicable to all LowerToHighest types.
			if (floor->m_FloorDestHeight != sec->floorheight)
				floor->m_FloorDestHeight += height;
			break;

		case DFloor::floorLowerToLowest:
			floor->m_Direction = -1;
			floor->m_FloorDestHeight = P_FindLowestFloorSurrounding(sec);
			break;

		case DFloor::floorLowerToNearest:
			//jff 02/03/30 support lowering floor to next lowest floor
			floor->m_Direction = -1;
			floor->m_FloorDestHeight =
				P_FindNextLowestFloor (sec, sec->floorheight);
			break;

		case DFloor::floorLowerInstant:
			floor->m_Speed = height;
		case DFloor::floorLowerByValue:
			floor->m_Direction = -1;
			floor->m_FloorDestHeight = sec->floorheight - height;
			break;

		case DFloor::floorRaiseInstant:
			floor->m_Speed = height;
		case DFloor::floorRaiseByValue:
			floor->m_Direction = 1;
			floor->m_FloorDestHeight = sec->floorheight + height;
			break;

		case DFloor::floorMoveToValue:
			floor->m_Direction = (height - sec->floorheight) > 0 ? 1 : -1;
			floor->m_FloorDestHeight = height;
			break;

		case DFloor::floorRaiseAndCrush:
			floor->m_Crush = crush;
		case DFloor::floorRaiseToLowestCeiling:
			floor->m_Direction = 1;
			floor->m_FloorDestHeight = 
				P_FindLowestCeilingSurrounding(sec);
			if (floor->m_FloorDestHeight > sec->ceilingheight)
				floor->m_FloorDestHeight = sec->ceilingheight;
			if (floortype == DFloor::floorRaiseAndCrush)
				floor->m_FloorDestHeight -= 8 * FRACUNIT;
			break;

		case DFloor::floorRaiseToHighest:
			floor->m_Direction = 1;
			floor->m_FloorDestHeight = P_FindHighestFloorSurrounding(sec);
			break;

		case DFloor::floorRaiseToNearest:
			floor->m_Direction = 1;
			floor->m_FloorDestHeight = P_FindNextHighestFloor(sec,sec->floorheight);
			break;

		case DFloor::floorRaiseToLowest:
			floor->m_Direction = 1;
			floor->m_FloorDestHeight = P_FindLowestFloorSurrounding(sec);
			break;

		case DFloor::floorRaiseToCeiling:
			floor->m_Direction = 1;
			floor->m_FloorDestHeight = sec->ceilingheight;
			break;

		case DFloor::floorLowerToLowestCeiling:
			floor->m_Direction = -1;
			floor->m_FloorDestHeight = P_FindLowestCeilingSurrounding(sec);
			break;

		case DFloor::floorLowerByTexture:
			floor->m_Direction = -1;
			floor->m_FloorDestHeight = sec->floorheight -
				P_FindShortestTextureAround (secnum);
			break;

		case DFloor::floorLowerToCeiling:
			// [RH] Essentially instantly raises the floor to the ceiling
			floor->m_Direction = -1;
			floor->m_FloorDestHeight = sec->ceilingheight;
			break;

		case DFloor::floorRaiseByTexture:
			floor->m_Direction = 1;
			// [RH] Use P_FindShortestTextureAround from BOOM to do this
			//		since the code is identical to what was here. (Oddly
			//		enough, BOOM preserved the code here even though it
			//		also had this function.)
			floor->m_FloorDestHeight = sec->floorheight +
				P_FindShortestTextureAround (secnum);
			break;

		case DFloor::floorRaiseAndChange:
			floor->m_Direction = 1;
			floor->m_FloorDestHeight = sec->floorheight +	height;
			sec->floorpic = line->frontsector->floorpic;
			sec->special = line->frontsector->special;
			break;
		  
		case DFloor::floorLowerAndChange:
			floor->m_Direction = -1;
			floor->m_FloorDestHeight = 
				P_FindLowestFloorSurrounding (sec);
			floor->m_Texture = sec->floorpic;
			// jff 1/24/98 make sure floor->m_NewSpecial gets initialized
			// in case no surrounding sector is at floordestheight
			// --> should not affect compatibility <--
			floor->m_NewSpecial = sec->special; 

			//jff 5/23/98 use model subroutine to unify fixes and handling
			sec = P_FindModelFloorSector (floor->m_FloorDestHeight,sec-sectors);
			if (sec)
			{
				floor->m_Texture = sec->floorpic;
				floor->m_NewSpecial = sec->special;
			}
			break;

		  default:
			break;
		}

		if (change & 3)
		{
			// [RH] Need to do some transferring
			if (change & 4)
			{
				// Numeric model change
				sector_t *sec;

				sec = (floortype == DFloor::floorRaiseToLowestCeiling ||
					   floortype == DFloor::floorLowerToLowestCeiling ||
					   floortype == DFloor::floorRaiseToCeiling ||
					   floortype == DFloor::floorLowerToCeiling) ?
					  P_FindModelCeilingSector (floor->m_FloorDestHeight, secnum) :
					  P_FindModelFloorSector (floor->m_FloorDestHeight, secnum);

				if (sec) {
					floor->m_Texture = sec->floorpic;
					switch (change & 3) {
						case 1:
							floor->m_NewSpecial = 0;
							floor->m_Type = DFloor::genFloorChg0;
							break;
						case 2:
							floor->m_NewSpecial = sec->special;
							floor->m_Type = DFloor::genFloorChgT;
							break;
						case 3:
							floor->m_Type = DFloor::genFloorChg;
							break;
					}
				}
			} else if (line) {
				// Trigger model change
				floor->m_Texture = line->frontsector->floorpic;

				switch (change & 3) {
					case 1:
						floor->m_NewSpecial = 0;
						floor->m_Type = DFloor::genFloorChg0;
						break;
					case 2:
						floor->m_NewSpecial = sec->special;
						floor->m_Type = DFloor::genFloorChgT;
						break;
					case 3:
						floor->m_Type = DFloor::genFloorChg;
						break;
				}
			}
		}
		if (manual)
			return rtn;
	}
	return rtn;
}

// [RH]
// EV_FloorCrushStop
// Stop a floor from crushing!
//
BOOL EV_FloorCrushStop (int tag)
{
	int secnum = -1;

	while ((secnum = P_FindSectorFromTag (tag, secnum)) >= 0)
	{
		sector_t *sec = sectors + secnum;

		if (sec->floordata && sec->floordata->IsKindOf (RUNTIME_CLASS(DFloor)) &&
			static_cast<DFloor *>(sec->floordata)->m_Type == DFloor::floorRaiseAndCrush)
		{
			SN_StopSequence (sec);
			delete sec->floordata;
			sec->floordata = NULL;
		}
	}
	return true;
}

//
// EV_DoChange()
//
// Handle pure change types. These change floor texture and sector type
// by trigger or numeric model without moving the floor.
//
// The linedef causing the change and the type of change is passed
// Returns true if any sector changes
//
// jff 3/15/98 added to better support generalized sector types
// [RH] Added tag parameter.
//
BOOL EV_DoChange (line_t *line, EChange changetype, int tag)
{
	int			secnum;
	BOOL		rtn;
	sector_t	*sec;
	sector_t	*secm;

	secnum = -1;
	rtn = false;
	// change all sectors with the same tag as the linedef
	while ((secnum = P_FindSectorFromTag (tag, secnum)) >= 0)
	{
		sec = &sectors[secnum];
              
		rtn = true;

		// handle trigger or numeric change type
		switch(changetype)
		{
		case trigChangeOnly:
			if (line) { // [RH] if no line, no change
				sec->floorpic = line->frontsector->floorpic;
				sec->special = line->frontsector->special;
			}
			break;
		case numChangeOnly:
			secm = P_FindModelFloorSector (sec->floorheight,secnum);
			if (secm) // if no model, no change
			{
				sec->floorpic = secm->floorpic;
				sec->special = secm->special;
			}
			break;
		default:
			break;
		}
	}
	return rtn;
}


//
// BUILD A STAIRCASE!
// [RH] Added stairsize, srcspeed, delay, reset, igntxt, usespecials parameters
//		If usespecials is non-zero, then each sector in a stair is determined
//		by its special. If usespecials is 2, each sector stays in "sync" with
//		the others.
//
BOOL EV_BuildStairs (int tag, DFloor::EStair type, line_t *line,
					 fixed_t stairsize, fixed_t speed, int delay, int reset, int igntxt,
					 int usespecials)
{
	int 				secnum;
	int					osecnum;	//jff 3/4/98 save old loop index
	int 				height;
	int 				i;
	int 				newsecnum;
	int 				texture;
	int 				ok;
	int					persteptime;
	BOOL 				rtn = false;
	
	sector_t*			sec;
	sector_t*			tsec;
	sector_t*			prev = NULL;

	DFloor*				floor;
	BOOL				manual = false;

	if (speed == 0)
		return false;

	persteptime = FixedDiv (stairsize, speed) >> FRACBITS;

	// check if a manual trigger, if so do just the sector on the backside
	if (tag == 0)
	{
		if (!line || !(sec = line->backsector))
			return rtn;
		secnum = sec-sectors;
		manual = true;
		goto manual_stair;
	}

	secnum = -1;
	while ((secnum = P_FindSectorFromTag (tag, secnum)) >= 0)
	{
		sec = &sectors[secnum];

manual_stair:
		// ALREADY MOVING?	IF SO, KEEP GOING...
		//jff 2/26/98 add special lockout condition to wait for entire
		//staircase to build before retriggering
		if (sec->floordata || sec->stairlock)
		{
			if (!manual)
				continue;
			else
				return rtn;
		}
		
		// new floor thinker
		rtn = true;
		floor = new DFloor (sec);
		floor->m_Direction = (type == DFloor::buildUp) ? 1 : -1;
		floor->m_Type = DFloor::buildStair;	//jff 3/31/98 do not leave uninited
		floor->m_ResetCount = reset;	// [RH] Tics until reset (0 if never)
		floor->m_OrgHeight = sec->floorheight;	// [RH] Height to reset to
		// [RH] Set up delay values
		floor->m_Delay = delay;
		floor->m_PauseTime = 0;
		floor->m_StepTime = floor->m_PerStepTime = persteptime;

		floor->m_Crush = (!usespecials && speed == 4*FRACUNIT) ? 10 : -1; //jff 2/27/98 fix uninitialized crush field

		floor->m_Speed = speed;
		height = sec->floorheight + stairsize * floor->m_Direction;
		floor->m_FloorDestHeight = height;

		texture = sec->floorpic;
		osecnum = secnum;				//jff 3/4/98 preserve loop index
		
		// Find next sector to raise
		// 1.	Find 2-sided line with same sector side[0] (lowest numbered)
		// 2.	Other side is the next sector to raise
		// 3.	Unless already moving, or different texture, then stop building
		do
		{
			ok = 0;

			if (usespecials)
			{
				// [RH] Find the next sector by scanning for Stairs_Special?
				tsec = P_NextSpecialSector (sec,
						(sec->special & 0xff) == Stairs_Special1 ?
							Stairs_Special2 : Stairs_Special1, prev);

				if ( (ok = (tsec != NULL)) )
				{
					height += floor->m_Direction * stairsize;

					// if sector's floor already moving, look for another
					//jff 2/26/98 special lockout condition for retriggering
					if (tsec->floordata || tsec->stairlock)
					{
						prev = sec;
						sec = tsec;
						continue;
					}
				}
				newsecnum = tsec - sectors;
			}
			else
			{
				for (i = 0; i < sec->linecount; i++)
				{
					if ( !((sec->lines[i])->flags & ML_TWOSIDED) )
						continue;

					tsec = (sec->lines[i])->frontsector;
					if (!tsec) continue;	//jff 5/7/98 if no backside, continue
					newsecnum = tsec-sectors;

					if (secnum != newsecnum)
						continue;

					tsec = (sec->lines[i])->backsector;
					newsecnum = tsec - sectors;

					if (!igntxt && tsec->floorpic != texture)
						continue;

					height += floor->m_Direction * stairsize;

					// if sector's floor already moving, look for another
					//jff 2/26/98 special lockout condition for retriggering
					if (tsec->floordata || tsec->stairlock)
						continue;

					ok = true;
					break;
				}
			}

			if (ok)
			{
				// jff 2/26/98
				// link the stair chain in both directions
				// lock the stair sector until building complete
				sec->nextsec = newsecnum; // link step to next
				tsec->prevsec = secnum;   // link next back
				tsec->nextsec = -1;       // set next forward link as end
				tsec->stairlock = -2;     // lock the step

				prev = sec;
				sec = tsec;
				secnum = newsecnum;

				// create and initialize a thinker for the next step
				floor = new DFloor (sec);
				floor->StartFloorSound ();
				floor->m_Direction = (type == DFloor::buildUp) ? 1 : -1;
				floor->m_FloorDestHeight = height;
				// [RH] Set up delay values
				floor->m_Delay = delay;
				floor->m_PauseTime = 0;
				floor->m_StepTime = floor->m_PerStepTime = persteptime;

				if (usespecials == 2)
				{
					// [RH]
					fixed_t rise = (floor->m_FloorDestHeight - sec->floorheight)
									* floor->m_Direction;
					floor->m_Speed = FixedDiv (FixedMul (speed, rise), stairsize);
				}
				else
				{
					floor->m_Speed = speed;
				}
				floor->m_Type = DFloor::buildStair;	//jff 3/31/98 do not leave uninited
				//jff 2/27/98 fix uninitialized crush field
				floor->m_Crush = (!usespecials && speed == 4*FRACUNIT) ? 10 : -1;
				floor->m_ResetCount = reset;	// [RH] Tics until reset (0 if never)
				floor->m_OrgHeight = sec->floorheight;	// [RH] Height to reset to
			}
		} while(ok);
		if (manual)
			return rtn;
		secnum = osecnum;	//jff 3/4/98 restore loop index
	}
	return rtn;
}

// [RH] Added pillarspeed and slimespeed parameters
int EV_DoDonut (int tag, fixed_t pillarspeed, fixed_t slimespeed)
{
	sector_t*			s1;
	sector_t*			s2;
	sector_t*			s3;
	int 				secnum;
	int 				rtn;
	int 				i;
	DFloor*				floor;
		
	secnum = -1;
	rtn = 0;
	while ((secnum = P_FindSectorFromTag(tag,secnum)) >= 0)
	{
		s1 = &sectors[secnum];					// s1 is pillar's sector
				
		// ALREADY MOVING?	IF SO, KEEP GOING...
		if (s1->floordata)
			continue;
						
		rtn = 1;
		s2 = getNextSector (s1->lines[0], s1);	// s2 is pool's sector
		if (!s2)								// note lowest numbered line around
			continue;							// pillar must be two-sided

		for (i = 0; i < s2->linecount; i++)
		{
			if (!(s2->lines[i]->flags & ML_TWOSIDED) ||
				(s2->lines[i]->backsector == s1))
				continue;
			s3 = s2->lines[i]->backsector;
			
			//	Spawn rising slime
			floor = new DFloor (s2);
			floor->m_Type = DFloor::donutRaise;
			floor->m_Crush = -1;
			floor->m_Direction = 1;
			floor->m_Sector = s2;
			floor->m_Speed = slimespeed;
			floor->m_Texture = s3->floorpic;
			floor->m_NewSpecial = 0;
			floor->m_FloorDestHeight = s3->floorheight;
			floor->StartFloorSound ();
			
			//	Spawn lowering donut-hole
			floor = new DFloor (s1);
			floor->m_Type = DFloor::floorLowerToNearest;
			floor->m_Crush = -1;
			floor->m_Direction = -1;
			floor->m_Sector = s1;
			floor->m_Speed = pillarspeed;
			floor->m_FloorDestHeight = s3->floorheight;
			floor->StartFloorSound ();
			break;
		}
	}
	return rtn;
}

DElevator::DElevator (sector_t *sec)
	: Super (sec)
{
}

//
// EV_DoElevator
//
// Handle elevator linedef types
//
// Passed the linedef that triggered the elevator and the elevator action
//
// jff 2/22/98 new type to move floor and ceiling in parallel
// [RH] Added speed, tag, and height parameters and new types.
//
BOOL EV_DoElevator (line_t *line, DElevator::EElevator elevtype,
					fixed_t speed, fixed_t height, int tag)
{
	int			secnum;
	BOOL		rtn;
	sector_t*	sec;
	DElevator*	elevator;

	if (!line && (elevtype == DElevator::elevateCurrent))
		return false;

	secnum = -1;
	rtn = false;
	// act on all sectors with the same tag as the triggering linedef
	while ((secnum = P_FindSectorFromTag (tag, secnum)) >= 0)
	{
		sec = &sectors[secnum];

		// If either floor or ceiling is already activated, skip it
		if (sec->floordata || sec->ceilingdata) //jff 2/22/98
			continue;

		// create and initialize new elevator thinker
		rtn = true;
		elevator = new DElevator (sec);
		elevator->m_Type = elevtype;
		elevator->m_Speed = speed;
		elevator->StartFloorSound ();

		// set up the fields according to the type of elevator action
		switch (elevtype)
		{
		// elevator down to next floor
		case DElevator::elevateDown:
			elevator->m_Direction = -1;
			elevator->m_FloorDestHeight =
				P_FindNextLowestFloor (sec, sec->floorheight);
			elevator->m_CeilingDestHeight =
				elevator->m_FloorDestHeight + sec->ceilingheight - sec->floorheight;
			break;

		// elevator up to next floor
		case DElevator::elevateUp:
			elevator->m_Direction = 1;
			elevator->m_FloorDestHeight =
				P_FindNextHighestFloor (sec, sec->floorheight);
			elevator->m_CeilingDestHeight =
				elevator->m_FloorDestHeight + sec->ceilingheight - sec->floorheight;
			break;

		// elevator to floor height of activating switch's front sector
		case DElevator::elevateCurrent:
			elevator->m_FloorDestHeight = line->frontsector->floorheight;
			elevator->m_CeilingDestHeight =
				elevator->m_FloorDestHeight + sec->ceilingheight - sec->floorheight;
			elevator->m_Direction =
				elevator->m_FloorDestHeight>sec->floorheight ? 1 : -1;
			break;

		// [RH] elevate up by a specific amount
		case DElevator::elevateRaise:
			elevator->m_Direction = 1;
			elevator->m_FloorDestHeight = sec->floorheight + height;
			elevator->m_CeilingDestHeight = sec->ceilingheight + height;
			break;

		// [RH] elevate down by a specific amount
		case DElevator::elevateLower:
			elevator->m_Direction = -1;
			elevator->m_FloorDestHeight = sec->floorheight - height;
			elevator->m_CeilingDestHeight = sec->ceilingheight - height;
			break;
		}
	}
	return rtn;
}


//==========================================================================
//
// T_FloorWaggle
//
//==========================================================================

#define WGLSTATE_EXPAND 1
#define WGLSTATE_STABLE 2
#define WGLSTATE_REDUCE 3

DFloorWaggle::DFloorWaggle (sector_t *sec)
	: Super (sec)
{
}

void DFloorWaggle::RunThink ()
{
	switch (m_State)
	{
	case WGLSTATE_EXPAND:
		if ((m_Scale += m_ScaleDelta) >= m_TargetScale)
		{
			m_Scale = m_TargetScale;
			m_State = WGLSTATE_STABLE;
		}
		break;
	case WGLSTATE_REDUCE:
		if ((m_Scale -= m_ScaleDelta) <= 0)
		{ // Remove
			m_Sector->floorheight = m_OriginalHeight;
			P_ChangeSector (m_Sector, true);
			m_Sector->floordata = NULL;
			delete this;
			return;
		}
		break;
	case WGLSTATE_STABLE:
		if (m_Ticker != -1)
		{
			if (!--m_Ticker)
			{
				m_State = WGLSTATE_REDUCE;
			}
		}
		break;
	}
	m_Accumulator += m_AccDelta;
	m_Sector->floorheight = m_OriginalHeight
		+FixedMul(FloatBobOffsets[(m_Accumulator>>FRACBITS)&63],
		m_Scale);
	P_ChangeSector (m_Sector, true);
}

//==========================================================================
//
// EV_StartFloorWaggle
//
//==========================================================================

BOOL EV_StartFloorWaggle (int tag, int height, int speed, int offset,
	int timer)
{
	int sectorIndex;
	sector_t *sector;
	DFloorWaggle *waggle;
	BOOL retCode;

	retCode = false;
	sectorIndex = -1;
	while ((sectorIndex = P_FindSectorFromTag(tag, sectorIndex)) >= 0)
	{
		sector = &sectors[sectorIndex];
		if (sector->floordata)
		{ // Already busy with another thinker
			continue;
		}
		retCode = true;
		waggle = new DFloorWaggle (sector);
		waggle->m_OriginalHeight = sector->floorheight;
		waggle->m_Accumulator = offset*FRACUNIT;
		waggle->m_AccDelta = speed<<10;
		waggle->m_Scale = 0;
		waggle->m_TargetScale = height<<10;
		waggle->m_ScaleDelta = waggle->m_TargetScale
			/(35+((3*35)*height)/255);
		waggle->m_Ticker = timer ? timer*TICRATE : -1;
		waggle->m_State = WGLSTATE_EXPAND;
	}
	return retCode;
}
