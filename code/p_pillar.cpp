// [RH] p_pillar.c: New file to handle pillars

#include "z_zone.h"
#include "doomdef.h"
#include "p_local.h"
#include "p_spec.h"
#include "g_level.h"
#include "s_sndseq.h"

IMPLEMENT_CLASS (DPillar)

DPillar::DPillar ()
{
}

void DPillar::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << m_Type
		<< m_FloorSpeed
		<< m_CeilingSpeed
		<< m_FloorTarget
		<< m_CeilingTarget
		<< m_Crush;
}

void DPillar::RunThink ()
{
	int r, s;
	fixed_t oldfloor, oldceiling;

	oldfloor = m_Sector->floorplane.d;
	oldceiling = m_Sector->ceilingplane.d;

	if (m_Type == pillarBuild)
	{
		r = MoveFloor (m_FloorSpeed, m_FloorTarget, m_Crush, 1);
		s = MoveCeiling (m_CeilingSpeed, m_CeilingTarget, m_Crush, -1);
	}
	else
	{
		r = MoveFloor (m_FloorSpeed, m_FloorTarget, m_Crush, -1);
		s = MoveCeiling (m_CeilingSpeed, m_CeilingTarget, m_Crush, 1);
	}

	if (r == pastdest && s == pastdest)
	{
		SN_StopSequence (m_Sector);
		Destroy ();
	}
	else
	{
		if (r == crushed)
		{
			MoveFloor (m_FloorSpeed, oldfloor, -1, -1);
		}
		if (s == crushed)
		{
			MoveCeiling (m_CeilingSpeed, oldceiling, -1, 1);
		}
	}
}

DPillar::DPillar (sector_t *sector, EPillar type, fixed_t speed,
				  fixed_t floordist, fixed_t ceilingdist, int crush)
	: DMover (sector)
{
	fixed_t newheight;
	vertex_t *spot;

	sector->floordata = sector->ceilingdata = this;

	m_Type = type;
	m_Crush = crush;

	if (type == pillarBuild)
	{
		// If the pillar height is 0, have the floor and ceiling meet halfway
		if (floordist == 0)
		{
			newheight = (sector->CenterFloor () + sector->CenterCeiling ()) / 2;
			m_FloorTarget = sector->floorplane.PointToDist (sector->soundorg[0], sector->soundorg[1], newheight);
			m_CeilingTarget = sector->ceilingplane.PointToDist (sector->soundorg[0], sector->soundorg[1], newheight);
			floordist = newheight - sector->CenterFloor ();
		}
		else
		{
			newheight = sector->CenterFloor () + floordist;
			m_FloorTarget = sector->floorplane.PointToDist (sector->soundorg[0], sector->soundorg[1], newheight);
			m_CeilingTarget = sector->ceilingplane.PointToDist (sector->soundorg[0], sector->soundorg[1], newheight);
		}
		ceilingdist = sector->CenterCeiling () - newheight;
	}
	else
	{
		// If one of the heights is 0, figure it out based on the
		// surrounding sectors
		if (floordist == 0)
		{
			newheight = sector->FindLowestFloorSurrounding (&spot);
			m_FloorTarget = sector->floorplane.PointToDist (spot, newheight);
			floordist = sector->floorplane.ZatPoint (spot) - newheight;
		}
		else
		{
			newheight = sector->floorplane.ZatPoint (0, 0) - floordist;
			m_FloorTarget = sector->floorplane.PointToDist (0, 0, newheight);
		}
		if (ceilingdist == 0)
		{
			newheight = sector->FindHighestCeilingSurrounding (&spot);
			m_CeilingTarget = sector->ceilingplane.PointToDist (spot, newheight);
			ceilingdist = newheight - sector->ceilingplane.ZatPoint (spot);
		}
		else
		{
			newheight = sector->ceilingplane.ZatPoint (0, 0) + ceilingdist;
			m_CeilingTarget = sector->ceilingplane.PointToDist (0, 0, newheight);
		}
	}

	// The speed parameter applies to whichever part of the pillar
	// travels the farthest. The other part's speed is then set so
	// that it arrives at its destination at the same time.
	if (floordist > ceilingdist)
	{
		m_FloorSpeed = speed;
		m_CeilingSpeed = Scale (speed, ceilingdist, floordist);
	}
	else
	{
		m_CeilingSpeed = speed;
		m_FloorSpeed = Scale (speed, floordist, ceilingdist);
	}

	if (sector->seqType >= 0)
		SN_StartSequence (sector, sector->seqType, SEQ_PLATFORM);
	else
		SN_StartSequence (sector, "Floor");
}

bool EV_DoPillar (DPillar::EPillar type, int tag, fixed_t speed, fixed_t height,
				  fixed_t height2, int crush)
{
	bool rtn = false;
	int secnum = -1;

	while ((secnum = P_FindSectorFromTag (tag, secnum)) >= 0)
	{
		sector_t *sec = &sectors[secnum];

		if (sec->floordata || sec->ceilingdata)
			continue;

		fixed_t flor, ceil;

		flor = sec->CenterFloor ();
		ceil = sec->CenterCeiling ();

		if (type == DPillar::pillarBuild && flor == ceil)
			continue;

		if (type == DPillar::pillarOpen && flor != ceil)
			continue;

		rtn = true;
		new DPillar (sec, type, speed, height, height2, crush);
	}
	return rtn;
}