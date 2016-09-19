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
#include "farchive.h"
#include "p_spec.h"

//============================================================================
//
// 
//
//============================================================================

inline FArchive &operator<< (FArchive &arc, DCeiling::ECeiling &type)
{
	BYTE val = (BYTE)type;
	arc << val;
	type = (DCeiling::ECeiling)val;
	return arc;
}

inline FArchive &operator<< (FArchive &arc, DCeiling::ECrushMode &type)
{
	BYTE val = (BYTE)type;
	arc << val;
	type = (DCeiling::ECrushMode)val;
	return arc;
}

//============================================================================
//
// CEILINGS
//
//============================================================================

IMPLEMENT_CLASS (DCeiling)

DCeiling::DCeiling ()
{
}

//============================================================================
//
// 
//
//============================================================================

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
		<< m_CrushMode;
}

//============================================================================
//
// 
//
//============================================================================

void DCeiling::PlayCeilingSound ()
{
	if (m_Sector->Flags & SECF_SILENTMOVE) return;
	if (m_Sector->seqType >= 0)
	{
		SN_StartSequence (m_Sector, CHAN_CEILING, m_Sector->seqType, SEQ_PLATFORM, 0, false);
	}
	else if (m_Sector->SeqName != NAME_None)
	{
		SN_StartSequence (m_Sector, CHAN_CEILING, m_Sector->SeqName, 0);
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

//============================================================================
//
// DCeiling :: Tick
//
//============================================================================

void DCeiling::Tick ()
{
	EMoveResult res;
		
	switch (m_Direction)
	{
	case 0:
		// IN STASIS
		break;
	case 1:
		// UP
		res = m_Sector->MoveCeiling (m_Speed, m_TopHeight, m_Direction);
		
		if (res == EMoveResult::pastdest)
		{
			switch (m_Type)
			{
			case DCeiling::ceilCrushAndRaise:
				m_Direction = -1;
				m_Speed = m_Speed1;
				if (!SN_IsMakingLoopingSound (m_Sector))
					PlayCeilingSound ();
				break;
				
			// movers with texture change, change the texture then get removed
			case genCeilingChgT:
			case genCeilingChg0:
				m_Sector->SetSpecial(&m_NewSpecial);
				// fall through
			case genCeilingChg:
				m_Sector->SetTexture(sector_t::ceiling, m_Texture);
				// fall through
			default:
				SN_StopSequence (m_Sector, CHAN_CEILING);
				Destroy ();
				break;
			}
		}
		break;
		
	case -1:
		// DOWN
		res = m_Sector->MoveCeiling (m_Speed, m_BottomHeight, m_Crush, m_Direction, m_CrushMode == ECrushMode::crushHexen);
		
		if (res == EMoveResult::pastdest)
		{
			switch (m_Type)
			{
			case DCeiling::ceilCrushAndRaise:
			case DCeiling::ceilCrushRaiseAndStay:
				m_Speed = m_Speed2;
				m_Direction = 1;
				if (!SN_IsMakingLoopingSound (m_Sector))
					PlayCeilingSound ();
				break;

			// in the case of ceiling mover/changer, change the texture
			// then remove the active ceiling
			case genCeilingChgT:
			case genCeilingChg0:
				m_Sector->SetSpecial(&m_NewSpecial);
				// fall through
			case genCeilingChg:
				m_Sector->SetTexture(sector_t::ceiling, m_Texture);
				// fall through
			default:
				SN_StopSequence (m_Sector, CHAN_CEILING);
				Destroy ();
				break;
			}
		}
		else // ( res != pastdest )
		{
			if (res == EMoveResult::crushed)
			{
				switch (m_Type)
				{
				case DCeiling::ceilCrushAndRaise:
				case DCeiling::ceilLowerAndCrush:
					if (m_CrushMode == ECrushMode::crushSlowdown)
						m_Speed = 1. / 8;
						break;

				default:
					break;
				}
			}
		}
		break;
	}
}

//============================================================================
//
// 
//
//============================================================================

DCeiling::DCeiling (sector_t *sec)
	: DMovingCeiling (sec)
{
}

DCeiling::DCeiling (sector_t *sec, double speed1, double speed2, int silent)
	: DMovingCeiling (sec)
{
	m_Crush = -1;
	m_CrushMode = ECrushMode::crushDoom;
	m_Speed = m_Speed1 = speed1;
	m_Speed2 = speed2;
	m_Silent = silent;
}

//============================================================================
//
// 
//
//============================================================================

bool P_CreateCeiling(sector_t *sec, DCeiling::ECeiling type, line_t *line, int tag, 
				   double speed, double speed2, double height,
				   int crush, int silent, int change, DCeiling::ECrushMode hexencrush)
{
	double		targheight = 0;	// Silence, GCC

	// if ceiling already moving, don't start a second function on it
	if (sec->PlaneMoving(sector_t::ceiling))
	{
		return false;
	}
	
	// new door thinker
	DCeiling *ceiling = new DCeiling (sec, speed, speed2, silent & ~4);
	vertex_t *spot = sec->lines[0]->v1;

	switch (type)
	{
	case DCeiling::ceilCrushAndRaise:
	case DCeiling::ceilCrushRaiseAndStay:
		ceiling->m_TopHeight = sec->ceilingplane.fD();
	case DCeiling::ceilLowerAndCrush:
		targheight = sec->FindHighestFloorPoint (&spot);
		targheight += height;
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
			double diff = height - sec->ceilingplane.ZatPoint (spot);

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
		targheight = sec->FindHighestFloorSurrounding (&spot) + height;
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
		targheight = sec->FindHighestFloorPoint (&spot) + height;
		ceiling->m_BottomHeight = sec->ceilingplane.PointToDist (spot, targheight);
		ceiling->m_Direction = -1;
		break;

	case DCeiling::ceilRaiseToFloor:	// [RH] What's this for?
		targheight = sec->FindHighestFloorPoint (&spot) + height;
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
	ceiling->m_CrushMode = hexencrush;

	// Do not interpolate instant movement ceilings.
	// Note for ZDoomGL: Check to make sure that you update the sector
	// after the ceiling moves, because it hasn't actually moved yet.
	double movedist;

	if (ceiling->m_Direction < 0)
	{
		movedist = sec->ceilingplane.fD() - ceiling->m_BottomHeight;
	}
	else
	{
		movedist = ceiling->m_TopHeight - sec->ceilingplane.fD();
	}
	if (ceiling->m_Speed >= movedist)
	{
		ceiling->StopInterpolation(true);
		if (silent & 4) ceiling->m_Silent = 2;
	}

	// set texture/type change properties
	if (change & 3)		// if a texture change is indicated
	{
		if (change & 4)	// if a numeric model change
		{
			sector_t *modelsec;

			//jff 5/23/98 find model with floor at target height if target
			//is a floor type
			modelsec = (/*type == ceilRaiseToHighest ||*/
				   type == DCeiling::ceilRaiseToFloor ||
				   /*type == ceilLowerToHighest ||*/
				   type == DCeiling::ceilLowerToFloor) ?
				sec->FindModelFloorSector (targheight) :
				sec->FindModelCeilingSector (targheight);
			if (modelsec != NULL)
			{
				ceiling->m_Texture = modelsec->GetTexture(sector_t::ceiling);
				switch (change & 3)
				{
					case 1:		// type is zeroed
						ceiling->m_NewSpecial.Clear();
						ceiling->m_Type = DCeiling::genCeilingChg0;
						break;
					case 2:		// type is copied
						sec->GetSpecial(&ceiling->m_NewSpecial);
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
					ceiling->m_NewSpecial.Clear();
					ceiling->m_Type = DCeiling::genCeilingChg0;
					break;
				case 2:		// type is copied
					line->frontsector->GetSpecial(&ceiling->m_NewSpecial);
					ceiling->m_Type = DCeiling::genCeilingChgT;
					break;
				case 3:		// type is left alone
					ceiling->m_Type = DCeiling::genCeilingChg;
					break;
			}
		}
	}

	ceiling->PlayCeilingSound ();
	return ceiling != NULL;
}

//============================================================================
//
// EV_DoCeiling
// Move a ceiling up/down and all around!
//
// [RH] Added tag, speed, speed2, height, crush, silent, change params
//
//============================================================================

bool EV_DoCeiling (DCeiling::ECeiling type, line_t *line,
				   int tag, double speed, double speed2, double height,
				   int crush, int silent, int change, DCeiling::ECrushMode hexencrush)
{
	int 		secnum;
	bool 		rtn;
	sector_t*	sec;
		
	rtn = false;

	// check if a manual trigger, if so do just the sector on the backside
	if (tag == 0)
	{
		if (!line || !(sec = line->backsector))
			return rtn;
		secnum = (int)(sec-sectors);
		// [RH] Hack to let manual crushers be retriggerable, too
		tag ^= secnum | 0x1000000;
		P_ActivateInStasisCeiling (tag);
		return P_CreateCeiling(sec, type, line, tag, speed, speed2, height, crush, silent, change, hexencrush);
	}
	
	//	Reactivate in-stasis ceilings...for certain types.
	// This restarts a crusher after it has been stopped
	if (type == DCeiling::ceilCrushAndRaise)
	{
		P_ActivateInStasisCeiling (tag);
	}

	// affects all sectors with the same tag as the linedef
	FSectorTagIterator it(tag);
	while ((secnum = it.Next()) >= 0)
	{
		rtn |= P_CreateCeiling(&sectors[secnum], type, line, tag, speed, speed2, height, crush, silent, change, hexencrush);
	}
	return rtn;
}


//============================================================================
//
// Restart a ceiling that's in-stasis
// [RH] Passed a tag instead of a line and rewritten to use a list
//
//============================================================================

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

//============================================================================
//
// EV_CeilingCrushStop
// Stop a ceiling from crushing!
// [RH] Passed a tag instead of a line and rewritten to use a list
//
//============================================================================

bool EV_CeilingCrushStop (int tag, bool remove)
{
	bool rtn = false;
	DCeiling *scan;
	TThinkerIterator<DCeiling> iterator;

	scan = iterator.Next();
	while (scan != nullptr)
	{
		DCeiling *next = iterator.Next();
		if (scan->m_Tag == tag && scan->m_Direction != 0)
		{
			if (!remove)
			{
				SN_StopSequence(scan->m_Sector, CHAN_CEILING);
				scan->m_OldDirection = scan->m_Direction;
				scan->m_Direction = 0;		// in-stasis;
			}
			else
			{
				scan->Destroy();
			}
			rtn = true;
		}
		scan = next;
	}

	return rtn;
}
