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

#include "doomdef.h"
#include "p_local.h"
#include "p_lnspec.h"
#include "s_sound.h"
#include "s_sndseq.h"
#include "doomstat.h"
#include "r_state.h"
#include "tables.h"
#include "farchive.h"
#include "p_3dmidtex.h"
#include "r_data/r_interpolate.h"

//==========================================================================
//
//
//
//==========================================================================

inline FArchive &operator<< (FArchive &arc, DFloor::EFloor &type)
{
	BYTE val = (BYTE)type;
	arc << val;
	type = (DFloor::EFloor)val;
	return arc;
}

//==========================================================================
//
//
//
//==========================================================================

static void StartFloorSound (sector_t *sec)
{
	if (sec->Flags & SECF_SILENTMOVE) return;

	if (sec->seqType >= 0)
	{
		SN_StartSequence (sec, CHAN_FLOOR, sec->seqType, SEQ_PLATFORM, 0);
	}
	else if (sec->SeqName != NAME_None)
	{
		SN_StartSequence (sec, CHAN_FLOOR, sec->SeqName, 0);
	}
	else
	{
		SN_StartSequence (sec, CHAN_FLOOR, "Floor", 0);
	}
}


//==========================================================================
//
// FLOORS
//
//==========================================================================

IMPLEMENT_CLASS (DFloor)

DFloor::DFloor ()
{
}

void DFloor::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << m_Type
		<< m_Crush
		<< m_Direction
		<< m_NewSpecial
		<< m_Texture
		<< m_FloorDestDist
		<< m_Speed
		<< m_ResetCount
		<< m_OrgDist
		<< m_Delay
		<< m_PauseTime
		<< m_StepTime
		<< m_PerStepTime
		<< m_Hexencrush;
}

//==========================================================================
//
// MOVE A FLOOR TO ITS DESTINATION (UP OR DOWN)
//
//==========================================================================

void DFloor::Tick ()
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
				m_FloorDestDist = m_OrgDist;
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

	res = MoveFloor (m_Speed, m_FloorDestDist, m_Crush, m_Direction, m_Hexencrush);
	
	if (res == pastdest)
	{
		SN_StopSequence (m_Sector, CHAN_FLOOR);

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
					m_Sector->SetSpecial(&m_NewSpecial);
					//fall thru
				case genFloorChg:
					m_Sector->SetTexture(sector_t::floor, m_Texture);
					break;
				default:
					break;
				}
			}
			else if (m_Direction == -1)
			{
				switch (m_Type)
				{
				case floorLowerAndChange:
				case genFloorChgT:
				case genFloorChg0:
					m_Sector->SetSpecial(&m_NewSpecial);
					//fall thru
				case genFloorChg:
					m_Sector->SetTexture(sector_t::floor, m_Texture);
					break;
				default:
					break;
				}
			}

			m_Sector->floordata = NULL; //jff 2/22/98
			StopInterpolation();

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

			Destroy ();
		}
	}
}

//==========================================================================
//
//
//
//==========================================================================

void DFloor::SetFloorChangeType (sector_t *sec, int change)
{
	m_Texture = sec->GetTexture(sector_t::floor);

	switch (change & 3)
	{
	case 1:
		m_NewSpecial.Clear();
		m_Type = DFloor::genFloorChg0;
		break;
	case 2:
		m_Type = DFloor::genFloorChg;
		break;
	case 3:
		sec->GetSpecial(&m_NewSpecial);
		m_Type = DFloor::genFloorChgT;
		break;
	}
}

//==========================================================================
//
//
//
//==========================================================================

void DFloor::StartFloorSound ()
{
	::StartFloorSound (m_Sector);
}

//==========================================================================
//
//
//
//==========================================================================

DFloor::DFloor (sector_t *sec)
	: DMovingFloor (sec)
{
}

//==========================================================================
//
// HANDLE FLOOR TYPES
// [RH] Added tag, speed, height, crush, change params.
// This functions starts too many different things.
//
//==========================================================================

bool EV_DoFloor (DFloor::EFloor floortype, line_t *line, int tag,
				 fixed_t speed, fixed_t height, int crush, int change, bool hexencrush, bool hereticlower)
{
	int 		secnum;
	bool 		rtn;
	sector_t*	sec;
	DFloor*		floor;
	fixed_t		ceilingheight;
	fixed_t		newheight;
	vertex_t	*spot, *spot2;

	rtn = false;

	// check if a manual trigger; if so do just the sector on the backside
	FSectorTagIterator it(tag, line);
	while ((secnum = it.Next()) >= 0)
	{
		sec = &sectors[secnum];
		// ALREADY MOVING?	IF SO, KEEP GOING...
		if (sec->PlaneMoving(sector_t::floor))
		{
			continue;
		}
		
		
		// new floor thinker
		rtn = true;
		floor = new DFloor (sec);
		floor->m_Type = floortype;
		floor->m_Crush = crush;
		floor->m_Hexencrush = hexencrush;
		floor->m_Speed = speed;
		floor->m_ResetCount = 0;				// [RH]
		floor->m_OrgDist = sec->floorplane.d;	// [RH]

		switch (floortype)
		{
		case DFloor::floorLowerToHighest:
			floor->m_Direction = -1;
			newheight = sec->FindHighestFloorSurrounding (&spot);
			floor->m_FloorDestDist = sec->floorplane.PointToDist (spot, newheight);
			// [RH] DOOM's turboLower type did this. I've just extended it
			//		to be applicable to all LowerToHighest types.
			if (hereticlower || floor->m_FloorDestDist != sec->floorplane.d)
				floor->m_FloorDestDist = sec->floorplane.PointToDist (spot, newheight+height);
			break;

		case DFloor::floorLowerToLowest:
			floor->m_Direction = -1;
			newheight = sec->FindLowestFloorSurrounding (&spot);
			floor->m_FloorDestDist = sec->floorplane.PointToDist (spot, newheight);
			break;

		case DFloor::floorLowerToNearest:
			//jff 02/03/30 support lowering floor to next lowest floor
			floor->m_Direction = -1;
			newheight = sec->FindNextLowestFloor (&spot);
			floor->m_FloorDestDist = sec->floorplane.PointToDist (spot, newheight);
			break;

		case DFloor::floorLowerInstant:
			floor->m_Speed = height;
		case DFloor::floorLowerByValue:
			floor->m_Direction = -1;
			newheight = sec->floorplane.ZatPoint (sec->soundorg[0], sec->soundorg[1]) - height;
			floor->m_FloorDestDist = sec->floorplane.PointToDist (sec->soundorg[0], sec->soundorg[1], newheight);
			break;

		case DFloor::floorRaiseInstant:
			floor->m_Speed = height;
		case DFloor::floorRaiseByValue:
			floor->m_Direction = 1;
			newheight = sec->floorplane.ZatPoint (sec->soundorg[0], sec->soundorg[1]) + height;
			floor->m_FloorDestDist = sec->floorplane.PointToDist (sec->soundorg[0], sec->soundorg[1], newheight);
			break;

		case DFloor::floorMoveToValue:
			sec->FindHighestFloorPoint (&spot);
			floor->m_FloorDestDist = sec->floorplane.PointToDist (spot, height);
			floor->m_Direction = (floor->m_FloorDestDist > sec->floorplane.d) ? -1 : 1;
			break;

		case DFloor::floorRaiseAndCrushDoom:
		case DFloor::floorRaiseToLowestCeiling:
			floor->m_Direction = 1;
			newheight = sec->FindLowestCeilingSurrounding (&spot);
			if (floortype == DFloor::floorRaiseAndCrushDoom)
				newheight -= 8 * FRACUNIT;
			ceilingheight = sec->FindLowestCeilingPoint (&spot2);
			floor->m_FloorDestDist = sec->floorplane.PointToDist (spot, newheight);
			if (sec->floorplane.ZatPointDist (spot2, floor->m_FloorDestDist) > ceilingheight)
				floor->m_FloorDestDist = sec->floorplane.PointToDist (spot2,
					floortype == DFloor::floorRaiseAndCrushDoom ? ceilingheight - 8*FRACUNIT : ceilingheight);
			break;

		case DFloor::floorRaiseToHighest:
			floor->m_Direction = 1;
			newheight = sec->FindHighestFloorSurrounding (&spot);
			floor->m_FloorDestDist = sec->floorplane.PointToDist (spot, newheight);
			break;

		case DFloor::floorRaiseToNearest:
			floor->m_Direction = 1;
			newheight = sec->FindNextHighestFloor (&spot);
			floor->m_FloorDestDist = sec->floorplane.PointToDist (spot, newheight);
			break;

		case DFloor::floorRaiseToLowest:
			floor->m_Direction = 1;
			newheight = sec->FindLowestFloorSurrounding (&spot);
			floor->m_FloorDestDist = sec->floorplane.PointToDist (spot, newheight);
			break;

		case DFloor::floorRaiseAndCrush:
			floor->m_Direction = 1;
			newheight = sec->FindLowestCeilingPoint (&spot) - 8*FRACUNIT;
			floor->m_FloorDestDist = sec->floorplane.PointToDist (spot, newheight);
			break;

		case DFloor::floorRaiseToCeiling:
			floor->m_Direction = 1;
			newheight = sec->FindLowestCeilingPoint (&spot);
			floor->m_FloorDestDist = sec->floorplane.PointToDist (spot, newheight);
			break;

		case DFloor::floorLowerToLowestCeiling:
			floor->m_Direction = -1;
			newheight = sec->FindLowestCeilingSurrounding (&spot);
			floor->m_FloorDestDist = sec->floorplane.PointToDist (spot, newheight);
			break;

		case DFloor::floorLowerByTexture:
			floor->m_Direction = -1;
			newheight = sec->floorplane.ZatPoint (0, 0) - sec->FindShortestTextureAround ();
			floor->m_FloorDestDist = sec->floorplane.PointToDist (0, 0, newheight);
			break;

		case DFloor::floorLowerToCeiling:
			// [RH] Essentially instantly raises the floor to the ceiling
			floor->m_Direction = -1;
			newheight = sec->FindLowestCeilingPoint (&spot);
			floor->m_FloorDestDist = sec->floorplane.PointToDist (spot, newheight);
			break;

		case DFloor::floorRaiseByTexture:
			floor->m_Direction = 1;
			// [RH] Use P_FindShortestTextureAround from BOOM to do this
			//		since the code is identical to what was here. (Oddly
			//		enough, BOOM preserved the code here even though it
			//		also had this function.)
			newheight = sec->floorplane.ZatPoint (0, 0) + sec->FindShortestTextureAround ();
			floor->m_FloorDestDist = sec->floorplane.PointToDist (0, 0, newheight);
			break;

		case DFloor::floorRaiseAndChange:
			floor->m_Direction = 1;
			newheight = sec->floorplane.ZatPoint (0, 0) + height;
			floor->m_FloorDestDist = sec->floorplane.PointToDist (0, 0, newheight);
			if (line != NULL)
			{
				FTextureID oldpic = sec->GetTexture(sector_t::floor);
				sec->SetTexture(sector_t::floor, line->frontsector->GetTexture(sector_t::floor));
				sec->TransferSpecial(line->frontsector);
			}
			else
			{
				sec->ClearSpecial();
			}
			break;
		  
		case DFloor::floorLowerAndChange:
			floor->m_Direction = -1;
			newheight = sec->FindLowestFloorSurrounding (&spot);
			floor->m_FloorDestDist = sec->floorplane.PointToDist (spot, newheight);
			floor->m_Texture = sec->GetTexture(sector_t::floor);
			// jff 1/24/98 make sure floor->m_NewSpecial gets initialized
			// in case no surrounding sector is at floordestheight
			sec->GetSpecial(&floor->m_NewSpecial);

			//jff 5/23/98 use model subroutine to unify fixes and handling
			sector_t *modelsec;
			modelsec = sec->FindModelFloorSector (newheight);
			if (modelsec != NULL)
			{
				floor->m_Texture = modelsec->GetTexture(sector_t::floor);
				modelsec->GetSpecial(&floor->m_NewSpecial);
			}
			break;

		  default:
			break;
		}

		// Do not interpolate instant movement floors.
		bool silent = false;

		if ((floor->m_Direction>0 && floor->m_FloorDestDist>sec->floorplane.d) ||	// moving up but going down
			(floor->m_Direction<0 && floor->m_FloorDestDist<sec->floorplane.d) ||	// moving down but going up
			(floor->m_Speed >= abs(sec->floorplane.d - floor->m_FloorDestDist)))	// moving in one step
		{
			floor->StopInterpolation(true);

			// [Graf Zahl]
			// Don't make sounds for instant movement hacks but make an exception for
			// switches that activate their own back side. 
			if (!(i_compatflags & COMPATF_SILENT_INSTANT_FLOORS))
			{
				if (!line || !(line->activation & (SPAC_Use|SPAC_Push)) || line->backsector!=sec)
					silent = true;
			}
		}
		if (!silent) floor->StartFloorSound ();

		if (change & 3)
		{
			// [RH] Need to do some transferring
			if (change & 4)
			{
				// Numeric model change
				sector_t *modelsec;

				modelsec = (floortype == DFloor::floorRaiseToLowestCeiling ||
					   floortype == DFloor::floorLowerToLowestCeiling ||
					   floortype == DFloor::floorRaiseToCeiling ||
					   floortype == DFloor::floorLowerToCeiling) ?
					  sec->FindModelCeilingSector (-floor->m_FloorDestDist) :
					  sec->FindModelFloorSector (-floor->m_FloorDestDist);

				if (modelsec != NULL)
				{
					floor->SetFloorChangeType (modelsec, change);
				}
			}
			else if (line)
			{
				// Trigger model change
				floor->SetFloorChangeType (line->frontsector, change);
			}
		}
	}
	return rtn;
}

//==========================================================================
//
// [RH]
// EV_FloorCrushStop
// Stop a floor from crushing!
//
//==========================================================================

bool EV_FloorCrushStop (int tag)
{
	int secnum;
	FSectorTagIterator it(tag);
	while ((secnum = it.Next()) >= 0)
	{
		sector_t *sec = sectors + secnum;

		if (sec->floordata && sec->floordata->IsKindOf (RUNTIME_CLASS(DFloor)) &&
			barrier_cast<DFloor *>(sec->floordata)->m_Type == DFloor::floorRaiseAndCrush)
		{
			SN_StopSequence (sec, CHAN_FLOOR);
			sec->floordata->Destroy ();
			sec->floordata = NULL;
		}
	}
	return true;
}

//==========================================================================
//
// BUILD A STAIRCASE!
// [RH] Added stairsize, srcspeed, delay, reset, igntxt, usespecials parameters
//		If usespecials is non-zero, then each sector in a stair is determined
//		by its special. If usespecials is 2, each sector stays in "sync" with
//		the others.
//
//==========================================================================

bool EV_BuildStairs (int tag, DFloor::EStair type, line_t *line,
					 fixed_t stairsize, fixed_t speed, int delay, int reset, int igntxt,
					 int usespecials)
{
	int 				secnum = -1;
	int					osecnum;	//jff 3/4/98 save old loop index
	int 				height;
	fixed_t				stairstep;
	int 				i;
	int 				newsecnum = -1;
	FTextureID			texture;
	int 				ok;
	int					persteptime;
	bool 				rtn = false;
	
	sector_t*			sec;
	sector_t*			tsec = NULL;
	sector_t*			prev = NULL;

	DFloor*				floor;

	if (speed == 0)
		return false;

	persteptime = FixedDiv (stairsize, speed) >> FRACBITS;

	// check if a manual trigger, if so do just the sector on the backside
	FSectorTagIterator itr(tag, line);
	// The compatibility mode doesn't work with a hashing algorithm.
	// It needs the original linear search method. This was broken in Boom.
	bool compatible = tag != 0 && (i_compatflags & COMPATF_STAIRINDEX);
	while ((secnum = itr.NextCompat(compatible, secnum)) >= 0)
	{
		sec = &sectors[secnum];

		// ALREADY MOVING?	IF SO, KEEP GOING...
		//jff 2/26/98 add special lockout condition to wait for entire
		//staircase to build before retriggering
		if (sec->PlaneMoving(sector_t::floor) || sec->stairlock)
		{
			continue;
		}
		
		// new floor thinker
		rtn = true;
		floor = new DFloor (sec);
		floor->m_Direction = (type == DFloor::buildUp) ? 1 : -1;
		stairstep = stairsize * floor->m_Direction;
		floor->m_Type = DFloor::buildStair;	//jff 3/31/98 do not leave uninited
		floor->m_ResetCount = reset;	// [RH] Tics until reset (0 if never)
		floor->m_OrgDist = sec->floorplane.d;	// [RH] Height to reset to
		// [RH] Set up delay values
		floor->m_Delay = delay;
		floor->m_PauseTime = 0;
		floor->m_StepTime = floor->m_PerStepTime = persteptime;

		floor->m_Crush = (!usespecials && speed == 4*FRACUNIT) ? 10 : -1; //jff 2/27/98 fix uninitialized crush field
		floor->m_Hexencrush = false;

		floor->m_Speed = speed;
		height = sec->floorplane.ZatPoint (0, 0) + stairstep;
		floor->m_FloorDestDist = sec->floorplane.PointToDist (0, 0, height);

		texture = sec->GetTexture(sector_t::floor);
		osecnum = secnum;				//jff 3/4/98 preserve loop index
		
		// Find next sector to raise
		// 1. Find 2-sided line with same sector side[0] (lowest numbered)
		// 2. Other side is the next sector to raise
		// 3. Unless already moving, or different texture, then stop building
		do
		{
			ok = 0;

			if (usespecials)
			{
				// [RH] Find the next sector by scanning for Stairs_Special?
				tsec = sec->NextSpecialSector (
						sec->special == Stairs_Special1 ?
							Stairs_Special2 : Stairs_Special1, prev);

				if ( (ok = (tsec != NULL)) )
				{
					height += stairstep;

					// if sector's floor already moving, look for another
					//jff 2/26/98 special lockout condition for retriggering
					if (tsec->PlaneMoving(sector_t::floor) || tsec->stairlock)
					{
						prev = sec;
						sec = tsec;
						continue;
					}
					
				}
				newsecnum = (int)(tsec - sectors);
			}
			else
			{
				for (i = 0; i < sec->linecount; i++)
				{
					if ( !((sec->lines[i])->flags & ML_TWOSIDED) )
						continue;

					tsec = (sec->lines[i])->frontsector;
					newsecnum = (int)(tsec-sectors);

					if (secnum != newsecnum)
						continue;

					tsec = (sec->lines[i])->backsector;
					if (!tsec) continue;	//jff 5/7/98 if no backside, continue
					newsecnum = (int)(tsec - sectors);

					if (!igntxt && tsec->GetTexture(sector_t::floor) != texture)
						continue;

					// Doom bug: Height was changed before discarding the sector as part of the stairs.
					// Needs to be compatibility optioned because some maps (Eternall MAP25) depend on it.
					if (i_compatflags & COMPATF_STAIRINDEX) height += stairstep;

					// if sector's floor already moving, look for another
					//jff 2/26/98 special lockout condition for retriggering
					if (tsec->PlaneMoving(sector_t::floor) || tsec->stairlock)
						continue;

					if (!(i_compatflags & COMPATF_STAIRINDEX)) height += stairstep;

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
				floor->m_FloorDestDist = sec->floorplane.PointToDist (0, 0, height);
				// [RH] Set up delay values
				floor->m_Delay = delay;
				floor->m_PauseTime = 0;
				floor->m_StepTime = floor->m_PerStepTime = persteptime;

				if (usespecials == 2)
				{
					// [RH]
					fixed_t rise = height - sec->CenterFloor();
					floor->m_Speed = Scale (speed, rise, stairstep);
				}
				else
				{
					floor->m_Speed = speed;
				}
				floor->m_Type = DFloor::buildStair;	//jff 3/31/98 do not leave uninited
				//jff 2/27/98 fix uninitialized crush field
				floor->m_Crush = (!usespecials && speed == 4*FRACUNIT) ? 10 : -1;
				floor->m_Hexencrush = false;
				floor->m_ResetCount = reset;	// [RH] Tics until reset (0 if never)
				floor->m_OrgDist = sec->floorplane.d;	// [RH] Height to reset to
			}
		} while (ok);
		// [RH] make sure the first sector doesn't point to a previous one, otherwise
		// it can infinite loop when the first sector stops moving.
		sectors[osecnum].prevsec = -1;	
	}
	return rtn;
}

//==========================================================================
//
// [RH] Added pillarspeed and slimespeed parameters
//
//==========================================================================

bool EV_DoDonut (int tag, line_t *line, fixed_t pillarspeed, fixed_t slimespeed)
{
	sector_t*			s1;
	sector_t*			s2;
	sector_t*			s3;
	int 				secnum;
	bool 				rtn;
	int 				i;
	DFloor*				floor;
	vertex_t*			spot;
	fixed_t				height;
		
	rtn = false;

	FSectorTagIterator itr(tag, line);
	while ((secnum = itr.Next()) >= 0)
	{
		s1 = &sectors[secnum];					// s1 is pillar's sector

		// ALREADY MOVING?	IF SO, KEEP GOING...
		if (s1->PlaneMoving(sector_t::floor))
			continue; // safe now, because we check that tag is non-0 in the looping condition [fdari]
						
		rtn = true;
		s2 = getNextSector (s1->lines[0], s1);	// s2 is pool's sector
		if (!s2)								// note lowest numbered line around
			continue;							// pillar must be two-sided

		if (s2->PlaneMoving(sector_t::floor))
			continue;

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
			floor->m_Hexencrush = false;
			floor->m_Direction = 1;
			floor->m_Sector = s2;
			floor->m_Speed = slimespeed;
			floor->m_Texture = s3->GetTexture(sector_t::floor);
			floor->m_NewSpecial.Clear();
			height = s3->FindHighestFloorPoint (&spot);
			floor->m_FloorDestDist = s2->floorplane.PointToDist (spot, height);
			floor->StartFloorSound ();
			
			//	Spawn lowering donut-hole
			floor = new DFloor (s1);
			floor->m_Type = DFloor::floorLowerToNearest;
			floor->m_Crush = -1;
			floor->m_Hexencrush = false;
			floor->m_Direction = -1;
			floor->m_Sector = s1;
			floor->m_Speed = pillarspeed;
			height = s3->FindHighestFloorPoint (&spot);
			floor->m_FloorDestDist = s1->floorplane.PointToDist (spot, height);
			floor->StartFloorSound ();
			break;
		}
	}
	return rtn;
}

//==========================================================================
//
// Elevators
//
//==========================================================================

IMPLEMENT_POINTY_CLASS (DElevator)
	DECLARE_POINTER(m_Interp_Floor)
	DECLARE_POINTER(m_Interp_Ceiling)
END_POINTERS

inline FArchive &operator<< (FArchive &arc, DElevator::EElevator &type)
{
	BYTE val = (BYTE)type;
	arc << val;
	type = (DElevator::EElevator)val;
	return arc;
}

DElevator::DElevator ()
{
}

DElevator::DElevator (sector_t *sec)
	: Super (sec)
{
	sec->floordata = this;
	sec->ceilingdata = this;
	m_Interp_Floor = sec->SetInterpolation(sector_t::FloorMove, true);
	m_Interp_Ceiling = sec->SetInterpolation(sector_t::CeilingMove, true);
}

void DElevator::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << m_Type
		<< m_Direction
		<< m_FloorDestDist
		<< m_CeilingDestDist
		<< m_Speed
		<< m_Interp_Floor
		<< m_Interp_Ceiling;
}

//==========================================================================
//
//
//
//==========================================================================

void DElevator::Destroy()
{
	if (m_Interp_Ceiling != NULL)
	{
		m_Interp_Ceiling->DelRef();
		m_Interp_Ceiling = NULL;
	}
	if (m_Interp_Floor != NULL)
	{
		m_Interp_Floor->DelRef();
		m_Interp_Floor = NULL;
	}
	Super::Destroy();
}

//==========================================================================
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
//==========================================================================

void DElevator::Tick ()
{
	EResult res;

	fixed_t oldfloor, oldceiling;

	oldfloor = m_Sector->floorplane.d;
	oldceiling = m_Sector->ceilingplane.d;

	if (m_Direction < 0)	// moving down
	{
		res = MoveFloor (m_Speed, m_FloorDestDist, m_Direction);
		if (res == ok || res == pastdest)
		{
			res = MoveCeiling (m_Speed, m_CeilingDestDist, m_Direction);
			if (res == crushed)
			{
				MoveFloor (m_Speed, oldfloor, -m_Direction);
			}
		}
	}
	else // up
	{
		res = MoveCeiling (m_Speed, m_CeilingDestDist, m_Direction);
		if (res == ok || res == pastdest)
		{
			res = MoveFloor (m_Speed, m_FloorDestDist, m_Direction);
			if (res == crushed)
			{
				MoveCeiling (m_Speed, oldceiling, -m_Direction);
			}
		}
	}

	if (res == pastdest)	// if destination height acheived
	{
		// make floor stop sound
		SN_StopSequence (m_Sector, CHAN_FLOOR);

		m_Sector->floordata = NULL;		//jff 2/22/98
		m_Sector->ceilingdata = NULL;	//jff 2/22/98
		Destroy ();		// remove elevator from actives
	}
}

//==========================================================================
//
//
//
//==========================================================================

void DElevator::StartFloorSound ()
{
	::StartFloorSound (m_Sector);
}

//==========================================================================
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
//==========================================================================

bool EV_DoElevator (line_t *line, DElevator::EElevator elevtype,
					fixed_t speed, fixed_t height, int tag)
{
	int			secnum;
	bool		rtn;
	sector_t*	sec;
	DElevator*	elevator;
	fixed_t		floorheight, ceilingheight;
	fixed_t		newheight;
	vertex_t*	spot;

	if (!line && (elevtype == DElevator::elevateCurrent))
		return false;

	secnum = -1;
	rtn = false;

	FSectorTagIterator itr(tag, line);

	// act on all sectors with the same tag as the triggering linedef
	while ((secnum = itr.Next()) >= 0)
	{
		sec = &sectors[secnum];
		// If either floor or ceiling is already activated, skip it
		if (sec->PlaneMoving(sector_t::floor) || sec->ceilingdata) //jff 2/22/98
			continue; // the loop used to break at the end if tag were 0, but would miss that step if "continue" occured [FDARI]

		// create and initialize new elevator thinker
		rtn = true;
		elevator = new DElevator (sec);
		elevator->m_Type = elevtype;
		elevator->m_Speed = speed;
		elevator->StartFloorSound ();

		floorheight = sec->CenterFloor ();
		ceilingheight = sec->CenterCeiling ();

		// set up the fields according to the type of elevator action
		switch (elevtype)
		{
		// elevator down to next floor
		case DElevator::elevateDown:
			elevator->m_Direction = -1;
			newheight = sec->FindNextLowestFloor (&spot);
			elevator->m_FloorDestDist = sec->floorplane.PointToDist (spot, newheight);
			newheight += sec->ceilingplane.ZatPoint (spot) - sec->floorplane.ZatPoint (spot);
			elevator->m_CeilingDestDist = sec->ceilingplane.PointToDist (spot, newheight);
			break;

		// elevator up to next floor
		case DElevator::elevateUp:
			elevator->m_Direction = 1;
			newheight = sec->FindNextHighestFloor (&spot);
			elevator->m_FloorDestDist = sec->floorplane.PointToDist (spot, newheight);
			newheight += sec->ceilingplane.ZatPoint (spot) - sec->floorplane.ZatPoint (spot);
			elevator->m_CeilingDestDist = sec->ceilingplane.PointToDist (spot, newheight);
			break;

		// elevator to floor height of activating switch's front sector
		case DElevator::elevateCurrent:
			newheight = line->frontsector->floorplane.ZatPoint (line->v1);
			elevator->m_FloorDestDist = sec->floorplane.PointToDist (line->v1, newheight);
			newheight += sec->ceilingplane.ZatPoint (line->v1) - sec->floorplane.ZatPoint (line->v1);
			elevator->m_CeilingDestDist = sec->ceilingplane.PointToDist (line->v1, newheight);

			elevator->m_Direction =
				elevator->m_FloorDestDist > sec->floorplane.d ? -1 : 1;
			break;

		// [RH] elevate up by a specific amount
		case DElevator::elevateRaise:
			elevator->m_Direction = 1;
			elevator->m_FloorDestDist = sec->floorplane.PointToDist (sec->soundorg[0], sec->soundorg[1], floorheight + height);
			elevator->m_CeilingDestDist = sec->ceilingplane.PointToDist (sec->soundorg[0], sec->soundorg[1], ceilingheight + height);
			break;

		// [RH] elevate down by a specific amount
		case DElevator::elevateLower:
			elevator->m_Direction = -1;
			elevator->m_FloorDestDist = sec->floorplane.PointToDist (sec->soundorg[0], sec->soundorg[1], floorheight - height);
			elevator->m_CeilingDestDist = sec->ceilingplane.PointToDist (sec->soundorg[0], sec->soundorg[1], ceilingheight - height);
			break;
		}
	}
	return rtn;
}


//==========================================================================
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
//==========================================================================

bool EV_DoChange (line_t *line, EChange changetype, int tag)
{
	int			secnum;
	bool		rtn;
	sector_t	*sec;
	sector_t	*secm;

	rtn = false;
	// change all sectors with the same tag as the linedef
	FSectorTagIterator it(tag);
	while ((secnum = it.Next()) >= 0)
	{
		sec = &sectors[secnum];
              
		rtn = true;

		// handle trigger or numeric change type
		FTextureID oldpic = sec->GetTexture(sector_t::floor);

		switch(changetype)
		{
		case trigChangeOnly:
			if (line)
			{ // [RH] if no line, no change
				sec->SetTexture(sector_t::floor, line->frontsector->GetTexture(sector_t::floor));
				sec->TransferSpecial(line->frontsector);
			}
			break;
		case numChangeOnly:
			secm = sec->FindModelFloorSector (sec->CenterFloor());
			if (secm)
			{ // if no model, no change
				sec->SetTexture(sector_t::floor, secm->GetTexture(sector_t::floor));
				sec->TransferSpecial(secm);
			}
			break;
		default:
			break;
		}
	}
	return rtn;
}


//==========================================================================
//
//
//
//==========================================================================

IMPLEMENT_POINTY_CLASS (DWaggleBase)
	DECLARE_POINTER(m_Interpolation)
END_POINTERS

IMPLEMENT_CLASS (DFloorWaggle)
IMPLEMENT_CLASS (DCeilingWaggle)

DWaggleBase::DWaggleBase ()
{
}

void DWaggleBase::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << m_OriginalDist
		<< m_Accumulator
		<< m_AccDelta
		<< m_TargetScale
		<< m_Scale
		<< m_ScaleDelta
		<< m_Ticker
		<< m_State
		<< m_Interpolation;
}

//==========================================================================
//
// WaggleBase
//
//==========================================================================

#define WGLSTATE_EXPAND 1
#define WGLSTATE_STABLE 2
#define WGLSTATE_REDUCE 3

DWaggleBase::DWaggleBase (sector_t *sec)
	: Super (sec)
{
}

void DWaggleBase::Destroy()
{
	if (m_Interpolation != NULL)
	{
		m_Interpolation->DelRef();
		m_Interpolation = NULL;
	}
	Super::Destroy();
}

//==========================================================================
//
//
//
//==========================================================================

void DWaggleBase::DoWaggle (bool ceiling)
{
	secplane_t *plane;
	int pos;
	fixed_t dist;

	if (ceiling)
	{
		plane = &m_Sector->ceilingplane;
		pos = sector_t::ceiling;
	}
	else
	{
		plane = &m_Sector->floorplane;
		pos = sector_t::floor;
	}

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
			dist = FixedMul (m_OriginalDist - plane->d, plane->ic);
			m_Sector->ChangePlaneTexZ(pos, -plane->HeightDiff (m_OriginalDist));
			plane->d = m_OriginalDist;
			P_ChangeSector (m_Sector, true, dist, ceiling, false);
			if (ceiling)
			{
				m_Sector->ceilingdata = NULL;
			}
			else
			{
				m_Sector->floordata = NULL;
			}
			Destroy ();
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


	fixed_t mag = finesine[(m_Accumulator>>9)&8191]*8;

	dist = plane->d;
	plane->d = m_OriginalDist + plane->PointToDist (0, 0, FixedMul (mag, m_Scale));
	m_Sector->ChangePlaneTexZ(pos, plane->HeightDiff (dist));
	dist = plane->HeightDiff (dist);

	// Interesting: Hexen passes 'true' for the crunch parameter which really is crushing damage here...
	// Also, this does not reset the move if it blocks.
	P_Scroll3dMidtex(m_Sector, 1, dist, ceiling);
	P_MoveLinkedSectors(m_Sector, 1, dist, ceiling);
	P_ChangeSector (m_Sector, 1, dist, ceiling, false);
}

//==========================================================================
//
// FloorWaggle
//
//==========================================================================

DFloorWaggle::DFloorWaggle ()
{
}

DFloorWaggle::DFloorWaggle (sector_t *sec)
	: Super (sec)
{
	sec->floordata = this;
	m_Interpolation = sec->SetInterpolation(sector_t::FloorMove, true);
}

void DFloorWaggle::Tick ()
{
	DoWaggle (false);
}

//==========================================================================
//
// CeilingWaggle
//
//==========================================================================

DCeilingWaggle::DCeilingWaggle ()
{
}

DCeilingWaggle::DCeilingWaggle (sector_t *sec)
	: Super (sec)
{
	sec->ceilingdata = this;
	m_Interpolation = sec->SetInterpolation(sector_t::CeilingMove, true);
}

void DCeilingWaggle::Tick ()
{
	DoWaggle (true);
}

//==========================================================================
//
// EV_StartWaggle
//
//==========================================================================

bool EV_StartWaggle (int tag, line_t *line, int height, int speed, int offset,
	int timer, bool ceiling)
{
	int sectorIndex;
	sector_t *sector;
	DWaggleBase *waggle;
	bool retCode;

	retCode = false;

	FSectorTagIterator itr(tag, line);

	while ((sectorIndex = itr.Next()) >= 0)
	{
		sector = &sectors[sectorIndex];
		if ((!ceiling && sector->PlaneMoving(sector_t::floor)) || 
			(ceiling && sector->PlaneMoving(sector_t::ceiling)))
		{ // Already busy with another thinker
			continue;
		}
		retCode = true;
		if (ceiling)
		{
			waggle = new DCeilingWaggle (sector);
			waggle->m_OriginalDist = sector->ceilingplane.d;
		}
		else
		{
			waggle = new DFloorWaggle (sector);
			waggle->m_OriginalDist = sector->floorplane.d;
		}
		waggle->m_Accumulator = offset*FRACUNIT;
		waggle->m_AccDelta = speed << (FRACBITS-6);
		waggle->m_Scale = 0;
		waggle->m_TargetScale = height << (FRACBITS-6);
		waggle->m_ScaleDelta = waggle->m_TargetScale
			/(TICRATE+((3*TICRATE)*height)/255);
		waggle->m_Ticker = timer ? timer*TICRATE : -1;
		waggle->m_State = WGLSTATE_EXPAND;
	}
	return retCode;
}
