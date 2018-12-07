/*
** p_pillar.cpp
** Handles pillars
**
**---------------------------------------------------------------------------
** Copyright 1998-2006 Randy Heit
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

#include "doomdef.h"
#include "p_local.h"
#include "p_spec.h"
#include "g_level.h"
#include "s_sndseq.h"
#include "serializer.h"
#include "r_data/r_interpolate.h"
#include "g_levellocals.h"

IMPLEMENT_CLASS(DPillar, false, true)

IMPLEMENT_POINTERS_START(DPillar)
	IMPLEMENT_POINTER(m_Interp_Floor)
	IMPLEMENT_POINTER(m_Interp_Ceiling)
IMPLEMENT_POINTERS_END

DPillar::DPillar ()
{
}

void DPillar::OnDestroy()
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
	Super::OnDestroy();
}

void DPillar::Serialize(FSerializer &arc)
{
	Super::Serialize (arc);
	arc.Enum("type", m_Type)
		("floorspeed", m_FloorSpeed)
		("ceilingspeed", m_CeilingSpeed)
		("floortarget", m_FloorTarget)
		("ceilingtarget", m_CeilingTarget)
		("crush", m_Crush)
		("hexencrush", m_Hexencrush)
		("interp_floor", m_Interp_Floor)
		("interp_ceiling", m_Interp_Ceiling);
}

void DPillar::Tick ()
{
	EMoveResult r, s;
	double oldfloor, oldceiling;

	oldfloor = m_Sector->floorplane.fD();
	oldceiling = m_Sector->ceilingplane.fD();

	if (m_Type == pillarBuild)
	{
		r = m_Sector->MoveFloor (m_FloorSpeed, m_FloorTarget, m_Crush, 1, m_Hexencrush);
		s = m_Sector->MoveCeiling (m_CeilingSpeed, m_CeilingTarget, m_Crush, -1, m_Hexencrush);
	}
	else
	{
		r = m_Sector->MoveFloor (m_FloorSpeed, m_FloorTarget, m_Crush, -1, m_Hexencrush);
		s = m_Sector->MoveCeiling (m_CeilingSpeed, m_CeilingTarget, m_Crush, 1, m_Hexencrush);
	}

	if (r == EMoveResult::pastdest && s == EMoveResult::pastdest)
	{
		SN_StopSequence (m_Sector, CHAN_FLOOR);
		Destroy ();
	}
	else
	{
		if (r == EMoveResult::crushed)
		{
			m_Sector->MoveFloor (m_FloorSpeed, oldfloor, -1, -1, m_Hexencrush);
		}
		if (s == EMoveResult::crushed)
		{
			m_Sector->MoveCeiling (m_CeilingSpeed, oldceiling, -1, 1, m_Hexencrush);
		}
	}
}

DPillar::DPillar (sector_t *sector, EPillar type, double speed,
				  double floordist, double ceilingdist, int crush, bool hexencrush)
	: DMover (sector)
{
	double newheight;
	vertex_t *spot;

	sector->floordata = sector->ceilingdata = this;
	m_Interp_Floor = sector->SetInterpolation(sector_t::FloorMove, true);
	m_Interp_Ceiling = sector->SetInterpolation(sector_t::CeilingMove, true);

	m_Type = type;
	m_Crush = crush;
	m_Hexencrush = hexencrush;

	if (type == pillarBuild)
	{
		// If the pillar height is 0, have the floor and ceiling meet halfway
		if (floordist == 0)
		{
			newheight = (sector->CenterFloor () + sector->CenterCeiling ()) / 2;
			m_FloorTarget = sector->floorplane.PointToDist (sector->centerspot, newheight);
			m_CeilingTarget = sector->ceilingplane.PointToDist (sector->centerspot, newheight);
			floordist = newheight - sector->CenterFloor ();
		}
		else
		{
			newheight = sector->CenterFloor () + floordist;
			m_FloorTarget = sector->floorplane.PointToDist (sector->centerspot, newheight);
			m_CeilingTarget = sector->ceilingplane.PointToDist (sector->centerspot, newheight);
		}
		ceilingdist = sector->CenterCeiling () - newheight;
	}
	else
	{
		// If one of the heights is 0, figure it out based on the
		// surrounding sectors
		if (floordist == 0)
		{
			newheight = FindLowestFloorSurrounding (sector, &spot);
			m_FloorTarget = sector->floorplane.PointToDist (spot, newheight);
			floordist = sector->floorplane.ZatPoint (spot) - newheight;
		}
		else
		{
			newheight = sector->CenterFloor() - floordist;
			m_FloorTarget = sector->floorplane.PointToDist (sector->centerspot, newheight);
		}
		if (ceilingdist == 0)
		{
			newheight = FindHighestCeilingSurrounding (sector, &spot);
			m_CeilingTarget = sector->ceilingplane.PointToDist (spot, newheight);
			ceilingdist = newheight - sector->ceilingplane.ZatPoint (spot);
		}
		else
		{
			newheight = sector->CenterCeiling() + ceilingdist;
			m_CeilingTarget = sector->ceilingplane.PointToDist (sector->centerspot, newheight);
		}
	}

	// The speed parameter applies to whichever part of the pillar
	// travels the farthest. The other part's speed is then set so
	// that it arrives at its destination at the same time.
	if (floordist > ceilingdist)
	{
		m_FloorSpeed = speed;
		m_CeilingSpeed = speed * ceilingdist / floordist;
	}
	else
	{
		m_CeilingSpeed = speed;
		m_FloorSpeed = speed * floordist / ceilingdist;
	}

	if (!(m_Sector->Flags & SECF_SILENTMOVE))
	{
		if (sector->seqType >= 0)
		{
			SN_StartSequence(sector, CHAN_FLOOR, sector->seqType, SEQ_PLATFORM, 0);
		}
		else if (sector->SeqName != NAME_None)
		{
			SN_StartSequence(sector, CHAN_FLOOR, sector->SeqName, 0);
		}
		else
		{
			SN_StartSequence(sector, CHAN_FLOOR, "Floor", 0);
		}
	}
}

bool EV_DoPillar (DPillar::EPillar type, line_t *line, int tag,
				  double speed, double height, double height2, int crush, bool hexencrush)
{
	int secnum;
	sector_t *sec;
	bool rtn = false;

	// check if a manual trigger; if so do just the sector on the backside
	FSectorTagIterator itr(tag, line);
	while ((secnum = itr.Next()) >= 0)
	{
		sec = &level.sectors[secnum];

		if (sec->PlaneMoving(sector_t::floor) || sec->PlaneMoving(sector_t::ceiling))
			continue;

		double flor, ceil;

		flor = sec->CenterFloor ();
		ceil = sec->CenterCeiling ();

		if (type == DPillar::pillarBuild && flor == ceil)
			continue;

		if (type == DPillar::pillarOpen && flor != ceil)
			continue;

		rtn = true;
		Create<DPillar> (sec, type, speed, height, height2, crush, hexencrush);
	}
	return rtn;
}
