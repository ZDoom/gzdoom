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
// DESCRIPTION:  Ceiling animation (lowering, crushing, raising)
//
//-----------------------------------------------------------------------------



#include "doomdef.h"
#include "p_local.h"
#include "s_sound.h"
#include "s_sndseq.h"
#include "doomstat.h"
#include "r_state.h"
#include "gi.h"

//
// CEILINGS
//

IMPLEMENT_CLASS (DCeiling)

DCeiling::DCeiling ()
{
}

void DCeiling::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << m_Type
		<< m_BottomHeight
		<< m_TopHeight
		<< m_Speed
		<< m_Speed1
		<< m_Speed2
		<< m_Crush
		<< m_Silent
		<< m_Direction
		<< m_Texture
		<< m_NewSpecial
		<< m_Tag
		<< m_OldDirection
		<< m_Hexencrush;
}

void DCeiling::PlayCeilingSound ()
{
	if (m_Sector->seqType >= 0)
	{
		SN_StartSequence (m_Sector, CHAN_CEILING, m_Sector->seqType, SEQ_PLATFORM, 0, false);
	}
	else
	{
		if (m_Silent == 2)
			SN_StartSequence (m_Sector, CHAN_CEILING, "Silence", 0);
		else if (m_Silent == 1)
			SN_StartSequence (m_Sector, CHAN_CEILING, "CeilingSemiSilent", 0);
		else
			SN_StartSequence (m_Sector, CHAN_CEILING, "CeilingNormal", 0);
	}
}

//
// T_MoveCeiling
//
void DCeiling::Tick ()
{
	EResult res;
		
	switch (m_Direction)
	{
	case 0:
		// IN STASIS
		break;
	case 1:
		// UP
		res = MoveCeiling (m_Speed, m_TopHeight, m_Direction);
		
		if (res == pastdest)
		{
			switch (m_Type)
			{
			case ceilCrushAndRaise:
				m_Direction = -1;
				m_Speed = m_Speed1;
				if (!SN_IsMakingLoopingSound (m_Sector))
					PlayCeilingSound ();
				break;
				
			// movers with texture change, change the texture then get removed
			case genCeilingChgT:
			case genCeilingChg0:
				m_Sector->special = m_NewSpecial;
				// fall through
			case genCeilingChg:
				m_Sector->SetTexture(sector_t::ceiling, m_Texture);
				// fall through
			default:
				SN_StopSequence (m_Sector);
				Destroy ();
				break;
			}
		}
		break;
		
	case -1:
		// DOWN
		res = MoveCeiling (m_Speed, m_BottomHeight, m_Crush, m_Direction, m_Hexencrush);
		
		if (res == pastdest)
		{
			switch (m_Type)
			{
			case ceilCrushAndRaise:
			case ceilCrushRaiseAndStay:
				m_Speed = m_Speed2;
				m_Direction = 1;
				if (!SN_IsMakingLoopingSound (m_Sector))
					PlayCeilingSound ();
				break;

			// in the case of ceiling mover/changer, change the texture
			// then remove the active ceiling
			case genCeilingChgT:
			case genCeilingChg0:
				m_Sector->special = m_NewSpecial;
				// fall through
			case genCeilingChg:
				m_Sector->SetTexture(sector_t::ceiling, m_Texture);
				// fall through
			default:
				SN_StopSequence (m_Sector);
				Destroy ();
				break;
			}
		}
		else // ( res != pastdest )
		{
			if (res == crushed)
			{
				switch (m_Type)
				{
				case ceilCrushAndRaise:
				case ceilLowerAndCrush:
					if (m_Speed1 == FRACUNIT && m_Speed2 == FRACUNIT)
						m_Speed = FRACUNIT / 8;
						break;

				default:
					break;
				}
			}
		}
		break;
	}
}

DCeiling::DCeiling (sector_t *sec)
	: DMovingCeiling (sec)
{
}

DCeiling::DCeiling (sector_t *sec, fixed_t speed1, fixed_t speed2, int silent)
	: DMovingCeiling (sec)
{
	m_Crush = -1;
	m_Hexencrush = false;
	m_Speed = m_Speed1 = speed1;
	m_Speed2 = speed2;
	m_Silent = silent;
}

//
// EV_DoCeiling
// Move a ceiling up/down and all around!
//
// [RH] Added tag, speed, speed2, height, crush, silent, change params
bool EV_DoCeiling (DCeiling::ECeiling type, line_t *line,
				   int tag, fixed_t speed, fixed_t speed2, fixed_t height,
				   int crush, int silent, int change, bool hexencrush)
{
	int 		secnum;
	bool 		rtn;
	sector_t*	sec;
	DCeiling*	ceiling;
	bool		manual = false;
	fixed_t		targheight = 0;	// Silence, GCC
	vertex_t*	spot;
		
	rtn = false;

	// check if a manual trigger, if so do just the sector on the backside
	if (tag == 0)
	{
		if (!line || !(sec = line->backsector))
			return rtn;
		secnum = (int)(sec-sectors);
		manual = true;
		// [RH] Hack to let manual crushers be retriggerable, too
		tag ^= secnum | 0x1000000;
		P_ActivateInStasisCeiling (tag);
		goto manual_ceiling;
	}
	
	//	Reactivate in-stasis ceilings...for certain types.
	// This restarts a crusher after it has been stopped
	if (type == DCeiling::ceilCrushAndRaise)
	{
		P_ActivateInStasisCeiling (tag);
	}

	secnum = -1;
	// affects all sectors with the same tag as the linedef
	while ((secnum = P_FindSectorFromTag (tag, secnum)) >= 0)
	{
		sec = &sectors[secnum];
manual_ceiling:
		// if ceiling already moving, don't start a second function on it
		if (sec->PlaneMoving(sector_t::ceiling))
		{
			if (!manual)
				continue;
			else
				return false;
		}
		
		// new door thinker
		rtn = 1;
		ceiling = new DCeiling (sec, speed, speed2, silent);
		spot = sec->lines[0]->v1;

		switch (type)
		{
		case DCeiling::ceilCrushAndRaise:
		case DCeiling::ceilCrushRaiseAndStay:
			ceiling->m_TopHeight = sec->ceilingplane.d;
		case DCeiling::ceilLowerAndCrush:
			targheight = sec->FindHighestFloorPoint (&spot);
			if (type != DCeiling::ceilLowerAndCrush || gameinfo.gametype != GAME_Strife)
			{
				targheight += 8*FRACUNIT;
			}
			ceiling->m_BottomHeight = sec->ceilingplane.PointToDist (spot, targheight);
			ceiling->m_Direction = -1;
			break;

		case DCeiling::ceilRaiseToHighest:
			targheight = sec->FindHighestCeilingSurrounding (&spot);
			ceiling->m_TopHeight = sec->ceilingplane.PointToDist (spot, targheight);
			ceiling->m_Direction = 1;
			break;

		case DCeiling::ceilLowerByValue:
			targheight = sec->ceilingplane.ZatPoint (spot) - height;
			ceiling->m_BottomHeight = sec->ceilingplane.PointToDist (spot, targheight);
			ceiling->m_Direction = -1;
			break;

		case DCeiling::ceilRaiseByValue:
			targheight = sec->ceilingplane.ZatPoint (spot) + height;
			ceiling->m_TopHeight = sec->ceilingplane.PointToDist (spot, targheight);
			ceiling->m_Direction = 1;
			break;

		case DCeiling::ceilMoveToValue:
			{
				int diff = height - sec->ceilingplane.ZatPoint (spot);

				targheight = height;
				if (diff < 0)
				{
					ceiling->m_BottomHeight = sec->ceilingplane.PointToDist (spot, height);
					ceiling->m_Direction = -1;
				}
				else
				{
					ceiling->m_TopHeight = sec->ceilingplane.PointToDist (spot, height);
					ceiling->m_Direction = 1;
				}
			}
			break;

		case DCeiling::ceilLowerToHighestFloor:
			targheight = sec->FindHighestFloorSurrounding (&spot);
			ceiling->m_BottomHeight = sec->ceilingplane.PointToDist (spot, targheight);
			ceiling->m_Direction = -1;
			break;

		case DCeiling::ceilRaiseToHighestFloor:
			targheight = sec->FindHighestFloorSurrounding (&spot);
			ceiling->m_TopHeight = sec->ceilingplane.PointToDist (spot, targheight);
			ceiling->m_Direction = 1;
			break;

		case DCeiling::ceilLowerInstant:
			targheight = sec->ceilingplane.ZatPoint (spot) - height;
			ceiling->m_BottomHeight = sec->ceilingplane.PointToDist (spot, targheight);
			ceiling->m_Direction = -1;
			ceiling->m_Speed = height;
			break;

		case DCeiling::ceilRaiseInstant:
			targheight = sec->ceilingplane.ZatPoint (spot) + height;
			ceiling->m_TopHeight = sec->ceilingplane.PointToDist (spot, targheight);
			ceiling->m_Direction = 1;
			ceiling->m_Speed = height;
			break;

		case DCeiling::ceilLowerToNearest:
			targheight = sec->FindNextLowestCeiling (&spot);
			ceiling->m_BottomHeight = sec->ceilingplane.PointToDist (spot, targheight);
			ceiling->m_Direction = -1;
			break;

		case DCeiling::ceilRaiseToNearest:
			targheight = sec->FindNextHighestCeiling (&spot);
			ceiling->m_TopHeight = sec->ceilingplane.PointToDist (spot, targheight);
			ceiling->m_Direction = 1;
			break;

		case DCeiling::ceilLowerToLowest:
			targheight = sec->FindLowestCeilingSurrounding (&spot);
			ceiling->m_BottomHeight = sec->ceilingplane.PointToDist (spot, targheight);
			ceiling->m_Direction = -1;
			break;

		case DCeiling::ceilRaiseToLowest:
			targheight = sec->FindLowestCeilingSurrounding (&spot);
			ceiling->m_TopHeight = sec->ceilingplane.PointToDist (spot, targheight);
			ceiling->m_Direction = 1;
			break;

		case DCeiling::ceilLowerToFloor:
			targheight = sec->FindHighestFloorPoint (&spot);
			ceiling->m_BottomHeight = sec->ceilingplane.PointToDist (spot, targheight);
			ceiling->m_Direction = -1;
			break;

		case DCeiling::ceilRaiseToFloor:	// [RH] What's this for?
			targheight = sec->FindHighestFloorPoint (&spot);
			ceiling->m_TopHeight = sec->ceilingplane.PointToDist (spot, targheight);
			ceiling->m_Direction = 1;
			break;

		case DCeiling::ceilLowerToHighest:
			targheight = sec->FindHighestCeilingSurrounding (&spot);
			ceiling->m_BottomHeight = sec->ceilingplane.PointToDist (spot, targheight);
			ceiling->m_Direction = -1;
			break;

		case DCeiling::ceilLowerByTexture:
			targheight = sec->ceilingplane.ZatPoint (spot) - sec->FindShortestUpperAround ();
			ceiling->m_BottomHeight = sec->ceilingplane.PointToDist (spot, targheight);
			ceiling->m_Direction = -1;
			break;

		case DCeiling::ceilRaiseByTexture:
			targheight = sec->ceilingplane.ZatPoint (spot) + sec->FindShortestUpperAround ();
			ceiling->m_TopHeight = sec->ceilingplane.PointToDist (spot, targheight);
			ceiling->m_Direction = 1;
			break;

		default:
			break;	// Silence GCC
		}
				
		ceiling->m_Tag = tag;
		ceiling->m_Type = type;
		ceiling->m_Crush = crush;
		ceiling->m_Hexencrush = hexencrush;

		// Do not interpolate instant movement ceilings.
		// Note for ZDoomGL: Check to make sure that you update the sector
		// after the ceiling moves, because it hasn't actually moved yet.
		fixed_t movedist;

		if (ceiling->m_Direction < 0)
		{
			movedist = sec->ceilingplane.d - ceiling->m_BottomHeight;
		}
		else
		{
			movedist = ceiling->m_TopHeight - sec->ceilingplane.d;
		}
		if (ceiling->m_Speed >= movedist)
		{
			ceiling->StopInterpolation();
		}

		// set texture/type change properties
		if (change & 3)		// if a texture change is indicated
		{
			if (change & 4)	// if a numeric model change
			{
				sector_t *modelsec;

				//jff 5/23/98 find model with floor at target height if target
				//is a floor type
				modelsec = (/*type == DCeiling::ceilRaiseToHighest ||*/
					   type == DCeiling::ceilRaiseToFloor ||
					   /*type == DCeiling::ceilLowerToHighest ||*/
					   type == DCeiling::ceilLowerToFloor) ?
					sec->FindModelFloorSector (targheight) :
					sec->FindModelCeilingSector (targheight);
				if (modelsec != NULL)
				{
					ceiling->m_Texture = modelsec->GetTexture(sector_t::ceiling);
					switch (change & 3)
					{
						case 1:		// type is zeroed
							ceiling->m_NewSpecial = 0;
							ceiling->m_Type = DCeiling::genCeilingChg0;
							break;
						case 2:		// type is copied
							ceiling->m_NewSpecial = sec->special;
							ceiling->m_Type = DCeiling::genCeilingChgT;
							break;
						case 3:		// type is left alone
							ceiling->m_Type = DCeiling::genCeilingChg;
							break;
					}
				}
			}
			else if (line)	// else if a trigger model change
			{
				ceiling->m_Texture = line->frontsector->GetTexture(sector_t::ceiling);
				switch (change & 3)
				{
					case 1:		// type is zeroed
						ceiling->m_NewSpecial = 0;
						ceiling->m_Type = DCeiling::genCeilingChg0;
						break;
					case 2:		// type is copied
						ceiling->m_NewSpecial = line->frontsector->special;
						ceiling->m_Type = DCeiling::genCeilingChgT;
						break;
					case 3:		// type is left alone
						ceiling->m_Type = DCeiling::genCeilingChg;
						break;
				}
			}
		}

		ceiling->PlayCeilingSound ();

		if (manual)
			return rtn;
	}
	return rtn;
}


//
// Restart a ceiling that's in-stasis
// [RH] Passed a tag instead of a line and rewritten to use a list
//
void P_ActivateInStasisCeiling (int tag)
{
	DCeiling *scan;
	TThinkerIterator<DCeiling> iterator;

	while ( (scan = iterator.Next ()) )
	{
		if (scan->m_Tag == tag && scan->m_Direction == 0)
		{
			scan->m_Direction = scan->m_OldDirection;
			scan->PlayCeilingSound ();
		}
	}
}

//
// EV_CeilingCrushStop
// Stop a ceiling from crushing!
// [RH] Passed a tag instead of a line and rewritten to use a list
//
bool EV_CeilingCrushStop (int tag)
{
	bool rtn = false;
	DCeiling *scan;
	TThinkerIterator<DCeiling> iterator;

	while ( (scan = iterator.Next ()) )
	{
		if (scan->m_Tag == tag && scan->m_Direction != 0)
		{
			SN_StopSequence (scan->m_Sector);
			scan->m_OldDirection = scan->m_Direction;
			scan->m_Direction = 0;		// in-stasis;
			rtn = true;
		}
	}

	return rtn;
}
