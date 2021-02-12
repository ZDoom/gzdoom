/*
** p_trace.cpp
** Generalized trace function, like most 3D games have
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

#include "p_trace.h"
#include "p_local.h"
#include "r_sky.h"
#include "doomstat.h"
#include "p_maputl.h"
#include "p_spec.h"
#include "g_levellocals.h"
#include "p_terrain.h"
#include "vm.h"

//==========================================================================
//
//
//
//==========================================================================

struct FTraceInfo
{
	FLevelLocals *Level;
	DVector3 Start;
	DVector3 Vec;
	ActorFlags ActorMask;
	uint32_t WallMask;
	AActor *IgnoreThis;
	FTraceResults *Results;
	sector_t *CurSector;
	double MaxDist;
	double EnterDist;
	ETraceStatus (*TraceCallback)(FTraceResults &res, void *data);
	void *TraceCallbackData;
	uint32_t TraceFlags;
	int inshootthrough;
	double startfrac;
	double limitz;
	int ptflags;

	// These are required for 3D-floor checking
	// to create a fake sector with a floor 
	// or ceiling plane coming from a 3D-floor
	sector_t DummySector[2];	
	int sectorsel;		

	void Setup3DFloors();
	bool LineCheck(intercept_t *in, double dist, DVector3 hit, bool special3dpass);
	bool ThingCheck(intercept_t *in, double dist, DVector3 hit);
	bool TraceTraverse (int ptflags);
	bool CheckPlane(const secplane_t &plane);
	void EnterLinePortal(FPathTraverse &pt, intercept_t *in);
	void EnterSectorPortal(FPathTraverse &pt, int position, double frac, sector_t *entersec);


	bool CheckSectorPlane(const sector_t *sector, bool checkFloor)
	{
		return CheckPlane(checkFloor ? sector->floorplane : sector->ceilingplane);
	}

	bool Check3DFloorPlane(const F3DFloor *ffloor, bool checkBottom)
	{
		return CheckPlane(checkBottom? *(ffloor->bottom.plane) : *(ffloor->top.plane));
	}

	void SetSourcePosition()
	{
		Results->SrcFromTarget = Start;
		Results->HitVector = Vec;
		Results->SrcAngleFromTarget = Results->HitVector.Angle();
	}


};

static bool EditTraceResult (uint32_t flags, FTraceResults &res);



static void GetPortalTransition(DVector3 &pos, sector_t *&sec)
{
	bool moved = false;
	double testz = pos.Z;

	while (!sec->PortalBlocksMovement(sector_t::ceiling))
	{
		if (pos.Z > sec->GetPortalPlaneZ(sector_t::ceiling))
		{
			pos += sec->GetPortalDisplacement(sector_t::ceiling);
			sec = sec->Level->PointInSector(pos);
			moved = true;
		}
		else break;
	}
	if (!moved)
	{
		while (!sec->PortalBlocksMovement(sector_t::floor))
		{
			if (pos.Z <= sec->GetPortalPlaneZ(sector_t::floor))
			{
				pos += sec->GetPortalDisplacement(sector_t::floor);
				sec = sec->Level->PointInSector(pos);
			}
			else break;
		}
	}
}

static bool isLiquid(F3DFloor *ff)
{
	if (ff->flags & FF_SWIMMABLE) return true;
	auto terrain = ff->model->GetTerrain(ff->flags & FF_INVERTPLANES ? sector_t::floor : sector_t::ceiling);
	return Terrains[terrain].IsLiquid && Terrains[terrain].Splash != -1;
}

//==========================================================================
//
// Trace entry point
//
//==========================================================================

bool Trace(const DVector3 &start, sector_t *sector, const DVector3 &direction, double maxDist,
	ActorFlags actorMask, uint32_t wallMask, AActor *ignore, FTraceResults &res, uint32_t flags,
	ETraceStatus(*callback)(FTraceResults &res, void *), void *callbackdata)
{
	FTraceInfo inf;
	FTraceResults tempResult;

	memset(&tempResult, 0, sizeof(tempResult));
	tempResult.Fraction = tempResult.Distance = NO_VALUE;

	inf.Level = sector->Level;
	inf.Start = start;
	GetPortalTransition(inf.Start, sector);
	inf.ptflags = actorMask ? PT_ADDLINES|PT_ADDTHINGS|PT_COMPATIBLE : PT_ADDLINES;
	inf.Vec = direction;
	inf.ActorMask = actorMask;
	inf.WallMask = wallMask;
	inf.IgnoreThis = ignore;
	inf.CurSector = sector;
	inf.MaxDist = maxDist;
	inf.EnterDist = 0;
	inf.TraceCallback = callback;
	inf.TraceCallbackData = callbackdata;
	inf.TraceFlags = flags;
	inf.Results = &res;
	inf.inshootthrough = true;
	inf.sectorsel=0;
	inf.startfrac = 0;
	inf.limitz = inf.Start.Z;
	memset(&res, 0, sizeof(res));

	if ((flags & TRACE_ReportPortals) && callback != NULL)
	{
		tempResult.HitType = TRACE_CrossingPortal;
		tempResult.HitPos = tempResult.SrcFromTarget = inf.Start;
		tempResult.HitVector = inf.Vec;
		callback(tempResult, inf.TraceCallbackData);
	}
	bool reslt = inf.TraceTraverse(inf.ptflags);

	if ((flags & TRACE_ReportPortals) && callback != NULL)
	{
		tempResult.HitType = TRACE_CrossingPortal;
		tempResult.HitPos = tempResult.SrcFromTarget = inf.Results->HitPos;
		tempResult.HitVector = inf.Vec;
		callback(tempResult, inf.TraceCallbackData);
	}

	if (reslt)
	{ 
		return flags ? EditTraceResult(flags, res) : true;
	}
	else
	{
		return false;
	}
}


//============================================================================
//
// traverses a sector portal
//
//============================================================================

void FTraceInfo::EnterSectorPortal(FPathTraverse &pt, int position, double frac, sector_t *entersec)
{
	DVector2 displacement = entersec->GetPortalDisplacement(position);;
	double enterdist = MaxDist * frac;
	DVector3 exit = Start + enterdist * Vec;
	DVector3 enter = exit + displacement;

	Start += displacement;
	CurSector = entersec->Level->PointInSector(enter);
	inshootthrough = true;
	startfrac = frac;
	EnterDist = enterdist;
	pt.PortalRelocate(entersec->GetPortal(position)->mDisplacement, ptflags, frac);

	if ((TraceFlags & TRACE_ReportPortals) && TraceCallback != NULL)
	{
		enterdist = MaxDist * frac;
		Results->HitType = TRACE_CrossingPortal;
		Results->HitPos = exit;
		Results->SrcFromTarget = enter;
		Results->HitVector = Vec;
		TraceCallback(*Results, TraceCallbackData);
	}

	Setup3DFloors();
}

//============================================================================
//
// traverses a line portal
//
//============================================================================

void FTraceInfo::EnterLinePortal(FPathTraverse &pt, intercept_t *in)
{
	line_t *li = in->d.line;
	FLinePortal *port = li->getPortal();

	double frac = in->frac + EQUAL_EPSILON;
	double enterdist = MaxDist * frac;
	DVector3 exit = Start + MaxDist * in->frac * Vec;

	P_TranslatePortalXY(li, Start.X, Start.Y);
	P_TranslatePortalZ(li, Start.Z);
	P_TranslatePortalVXVY(li, Vec.X, Vec.Y);
	P_TranslatePortalZ(li, limitz);

	CurSector = li->GetLevel()->PointInSector(Start + enterdist * Vec);
	EnterDist = enterdist;
	inshootthrough = true;
	startfrac = frac;
	Results->unlinked |= (port->mType != PORTT_LINKED);
	pt.PortalRelocate(in, ptflags);

	if ((TraceFlags & TRACE_ReportPortals) && TraceCallback != NULL)
	{
		enterdist = MaxDist * in->frac;
		Results->HitType = TRACE_CrossingPortal;
		Results->HitPos = exit;
		P_TranslatePortalXY(li, exit.X, exit.Y);
		P_TranslatePortalZ(li, exit.Z);
		Results->SrcFromTarget = exit;
		Results->HitVector = Vec;
		TraceCallback(*Results, TraceCallbackData);
	}
}

//==========================================================================
//
// Checks 3D floors at trace start and sets up related data
//
//==========================================================================

void FTraceInfo::Setup3DFloors()
{
	TDeletingArray<F3DFloor*> &ff = CurSector->e->XFloor.ffloors;

	if (ff.Size())
	{
		memcpy(&DummySector[0], CurSector, sizeof(sector_t));
		CurSector = &DummySector[0];
		sectorsel = 1;

		double sdist = MaxDist * startfrac;
		DVector3 pos = Start + Vec * sdist;


		double bf = CurSector->floorplane.ZatPoint(pos);
		double bc = CurSector->ceilingplane.ZatPoint(pos);

		for (auto rover : ff)
		{
			if (!(rover->flags&FF_EXISTS))
				continue;

			if (Results->Crossed3DWater == NULL)
			{
				if (Check3DFloorPlane(rover, false) && isLiquid(rover))
				{
					// only consider if the plane is above the actual floor.
					if (rover->top.plane->ZatPoint(Results->HitPos) > bf)
					{
						Results->Crossed3DWater = rover;
						Results->Crossed3DWaterPos = Results->HitPos;
						Results->Distance = 0;
					}
				}
			}

			if (!(rover->flags&FF_SHOOTTHROUGH))
			{
				double ff_bottom = rover->bottom.plane->ZatPoint(pos);
				double ff_top = rover->top.plane->ZatPoint(pos);
				// clip to the part of the sector we are in
				if (pos.Z > ff_top)
				{
					// above
					if (bf < ff_top)
					{
						CurSector->floorplane = *rover->top.plane;
						CurSector->SetTexture(sector_t::floor, *rover->top.texture, false);
						CurSector->ClearPortal(sector_t::floor);
						bf = ff_top;
					}
				}
				else if (pos.Z < ff_bottom)
				{
					//below
					if (bc > ff_bottom)
					{
						CurSector->ceilingplane = *rover->bottom.plane;
						CurSector->SetTexture(sector_t::ceiling, *rover->bottom.texture, false);
						bc = ff_bottom;
						CurSector->ClearPortal(sector_t::ceiling);
					}
				}
				else
				{
					// inside
					if (bf < ff_bottom)
					{
						CurSector->floorplane = *rover->bottom.plane;
						CurSector->SetTexture(sector_t::floor, *rover->bottom.texture, false);
						CurSector->ClearPortal(sector_t::floor);
						bf = ff_bottom;
					}

					if (bc > ff_top)
					{
						CurSector->ceilingplane = *rover->top.plane;
						CurSector->SetTexture(sector_t::ceiling, *rover->top.texture, false);
						CurSector->ClearPortal(sector_t::ceiling);
						bc = ff_top;
					}
					inshootthrough = false;
				}
			}
		}
	}
}


//==========================================================================
//
// Processes one line intercept
//
//==========================================================================

bool FTraceInfo::LineCheck(intercept_t *in, double dist, DVector3 hit, bool special3dpass)
{
	int lineside;
	sector_t *entersector;

	double ff, fc, bf = 0, bc = 0;

	if (in->d.line->frontsector->sectornum == CurSector->sectornum)
	{
		lineside = 0;
	}
	else if (in->d.line->backsector && in->d.line->backsector->sectornum == CurSector->sectornum)
	{
		lineside = 1;
	}
	else
	{ // Dammit. Why does Doom have to allow non-closed sectors?
		if (in->d.line->backsector == NULL)
		{
			lineside = 0;
			CurSector = in->d.line->frontsector;
		}
		else
		{
			lineside = P_PointOnLineSide(Start, in->d.line);
			CurSector = lineside ? in->d.line->backsector : in->d.line->frontsector;
		}
	}

	if (!(in->d.line->flags & ML_TWOSIDED))
	{
		entersector = NULL;
	}
	else
	{
		entersector = (lineside == 0) ? in->d.line->backsector : in->d.line->frontsector;

		// For backwards compatibility: Ignore lines with the same sector on both sides.
		// This is the way Doom.exe did it and some WADs (e.g. Alien Vendetta MAP15) need it.
		if (Level->i_compatflags & COMPATF_TRACE && in->d.line->backsector == in->d.line->frontsector && !special3dpass)
		{
			// We must check special activation here because the code below is never reached.
			if (TraceFlags & TRACE_PCross)
			{
				P_ActivateLine(in->d.line, IgnoreThis, lineside, SPAC_PCross);
			}
			if (TraceFlags & TRACE_Impact)
			{
				P_ActivateLine(in->d.line, IgnoreThis, lineside, SPAC_Impact);
			}
			return true;
		}
	}

	ff = CurSector->floorplane.ZatPoint(hit);
	fc = CurSector->ceilingplane.ZatPoint(hit);

	if (entersector != NULL)
	{
		bf = entersector->floorplane.ZatPoint(hit);
		bc = entersector->ceilingplane.ZatPoint(hit);
	}

	sector_t *hsec = CurSector->GetHeightSec();
	if (Results->CrossedWater == NULL &&
		hsec != NULL &&
		Start.Z > hsec->floorplane.ZatPoint(Start) &&
		hit.Z <= hsec->floorplane.ZatPoint(hit))
	{
		// hit crossed a water plane
		if (CheckSectorPlane(hsec, true))
		{
			Results->CrossedWater = &Level->sectors[CurSector->sectornum];
			Results->CrossedWaterPos = Results->HitPos;
			Results->Distance = 0;
		}
	}

	if (hit.Z <= ff)
	{
		// hit floor in front of wall
		Results->HitType = TRACE_HitFloor;
		Results->HitTexture = CurSector->GetTexture(sector_t::floor);
	}
	else if (hit.Z >= fc)
	{
		// hit ceiling in front of wall
		Results->HitType = TRACE_HitCeiling;
		Results->HitTexture = CurSector->GetTexture(sector_t::ceiling);
	}
	else if (entersector == NULL ||
		hit.Z < bf || hit.Z > bc ||
		((in->d.line->flags & WallMask) && !special3dpass))
	{
		// hit the wall
		Results->HitType = TRACE_HitWall;
		Results->Tier =
			entersector == NULL ? TIER_Middle :
			hit.Z <= bf ? TIER_Lower :
			hit.Z >= bc ? TIER_Upper : TIER_Middle;
		if ((TraceFlags & TRACE_Impact) && !special3dpass)
		{
			P_ActivateLine(in->d.line, IgnoreThis, lineside, SPAC_Impact);
		}
	}
	else
	{ 	// made it past the wall
		// check for 3D floors first
		if (entersector->e->XFloor.ffloors.Size())
		{
			memcpy(&DummySector[sectorsel], entersector, sizeof(sector_t));
			entersector = &DummySector[sectorsel];
			sectorsel ^= 1;

			for (auto rover : entersector->e->XFloor.ffloors)
			{
				int entershootthrough = !!(rover->flags&FF_SHOOTTHROUGH);

				if (entershootthrough != inshootthrough && rover->flags&FF_EXISTS)
				{
					double ff_bottom = rover->bottom.plane->ZatPoint(hit);
					double ff_top = rover->top.plane->ZatPoint(hit);

					// clip to the part of the sector we are in
					if (hit.Z > ff_top)
					{
						// 3D floor height is the same as the floor height. We need to test a second spot to see if it is above or below
						if (fabs(bf - ff_top) < EQUAL_EPSILON)
						{
							double cf = entersector->floorplane.ZatPoint(entersector->centerspot);
							double ffc = rover->top.plane->ZatPoint(entersector->centerspot);
							if (ffc > cf)
							{
								bf = ff_top - EQUAL_EPSILON;
							}
						}
						
						// above
						if (bf < ff_top)
						{
							entersector->floorplane = *rover->top.plane;
							entersector->SetTexture(sector_t::floor, *rover->top.texture, false);
							entersector->ClearPortal(sector_t::floor);
							bf = ff_top;
						}
					}
					else if (hit.Z < ff_bottom)
					{
						// 3D floor height is the same as the ceiling height. We need to test a second spot to see if it is above or below
						if (fabs(bc - ff_bottom) < EQUAL_EPSILON)
						{
							double cc = entersector->ceilingplane.ZatPoint(entersector->centerspot);
							double fcc = rover->bottom.plane->ZatPoint(entersector->centerspot);
							if (fcc < cc)
							{
								bc = ff_bottom + EQUAL_EPSILON;
							}
						}

						//below
						if (bc > ff_bottom)
						{
							entersector->ceilingplane = *rover->bottom.plane;
							entersector->SetTexture(sector_t::ceiling, *rover->bottom.texture, false);
							entersector->ClearPortal(sector_t::ceiling);
							bc = ff_bottom;
						}
					}
					else
					{
						//hit the edge - equivalent to hitting the wall
						Results->HitType = TRACE_HitWall;
						Results->Tier = TIER_FFloor;
						Results->ffloor = rover;
						if ((TraceFlags & TRACE_Impact) && in->d.line->special)
						{
							P_ActivateLine(in->d.line, IgnoreThis, lineside, SPAC_Impact);
						}
						goto cont;
					}
				}
			}
		}

		Results->HitType = TRACE_HitNone;
		if (!special3dpass)
		{
			if (TraceFlags & TRACE_PCross)
			{
				P_ActivateLine(in->d.line, IgnoreThis, lineside, SPAC_PCross);
			}
			if (TraceFlags & TRACE_Impact)
			{ // This is incorrect for "impact", but Hexen did this, so
			  // we need to as well, for compatibility
				P_ActivateLine(in->d.line, IgnoreThis, lineside, SPAC_Impact);
			}
		}
	}
cont:

	if (Results->HitType != TRACE_HitNone)
	{
		// We hit something, so figure out where exactly
		Results->Sector = &Level->sectors[CurSector->sectornum];

		if (Results->HitType != TRACE_HitWall &&
			!CheckSectorPlane(CurSector, Results->HitType == TRACE_HitFloor))
		{ // trace is parallel to the plane (or right on it)
			if (entersector == NULL)
			{
				Results->HitType = TRACE_HitWall;
				Results->Tier = TIER_Middle;
			}
			else
			{
				if (hit.Z <= bf || hit.Z >= bc)
				{
					Results->HitType = TRACE_HitWall;
					Results->Tier =
						hit.Z <= bf ? TIER_Lower :
						hit.Z >= bc ? TIER_Upper : TIER_Middle;
				}
				else
				{
					Results->HitType = TRACE_HitNone;
				}
			}
			if (Results->HitType == TRACE_HitWall && TraceFlags & TRACE_Impact && (!special3dpass || Results->Tier != TIER_FFloor))
			{
				P_ActivateLine(in->d.line, IgnoreThis, lineside, SPAC_Impact);
			}
		}

		if (Results->HitType == TRACE_HitWall)
		{
			Results->HitPos = hit;
			SetSourcePosition();
			Results->Distance = dist;
			Results->Fraction = in->frac;
			Results->Line = in->d.line;
			Results->Side = lineside;
		}
	}

	if (TraceCallback != NULL && Results->HitType != TRACE_HitNone)
	{
		switch (TraceCallback(*Results, TraceCallbackData))
		{
		case TRACE_Stop:
			return false;

		case TRACE_Abort:
			Results->HitType = TRACE_HitNone;
			return false;

		case TRACE_Skip:
			Results->HitType = TRACE_HitNone;
			if (!special3dpass && (TraceFlags & TRACE_3DCallback) && entersector && entersector->e->XFloor.ffloors.Size())
				return LineCheck(in, dist, hit, true);
			break;

		default:
			break;
		}
	}

	if (Results->HitType == TRACE_HitNone && entersector)
	{
		CurSector = entersector;
		EnterDist = dist;
		return true;
	}
	else
	{
		return false;
	}
}

	
//==========================================================================
//
//
//
//==========================================================================

bool FTraceInfo::ThingCheck(intercept_t *in, double dist, DVector3 hit)
{
	if (hit.Z > in->d.thing->Top())
	{
		// trace enters above actor
		if (Vec.Z >= 0) return true;      // Going up: can't hit

		// Does it hit the top of the actor?
		dist = (in->d.thing->Top() - Start.Z) / Vec.Z;

		if (dist > MaxDist) return true;
		in->frac = dist / MaxDist;

		hit = Start + Vec * dist;

		// calculated coordinate is outside the actor's bounding box
		if (fabs(hit.X - in->d.thing->X()) > in->d.thing->radius ||
			fabs(hit.Y - in->d.thing->Y()) > in->d.thing->radius) return true;
	}
	else if (hit.Z < in->d.thing->Z())
	{ // trace enters below actor
		if (Vec.Z <= 0) return true;      // Going down: can't hit

		// Does it hit the bottom of the actor?
		dist = (in->d.thing->Z() - Start.Z) / Vec.Z;
		if (dist > MaxDist) return true;
		in->frac = dist / MaxDist;

		hit = Start + Vec * dist;

		// calculated coordinate is outside the actor's bounding box
		if (fabs(hit.X - in->d.thing->X()) > in->d.thing->radius ||
			fabs(hit.Y - in->d.thing->Y()) > in->d.thing->radius) return true;
	}

	if (CurSector->e->XFloor.ffloors.Size())
	{
		// check for 3D floor hits first.
		double ff_floor = CurSector->floorplane.ZatPoint(hit);
		double ff_ceiling = CurSector->ceilingplane.ZatPoint(hit);

		if (hit.Z > ff_ceiling && CurSector->PortalBlocksMovement(sector_t::ceiling))	// actor is hit above the current ceiling
		{
			Results->HitType = TRACE_HitCeiling;
			Results->HitTexture = CurSector->GetTexture(sector_t::ceiling);
		}
		else if (hit.Z < ff_floor && CurSector->PortalBlocksMovement(sector_t::floor))	// actor is hit below the current floor
		{
			Results->HitType = TRACE_HitFloor;
			Results->HitTexture = CurSector->GetTexture(sector_t::floor);
		}
		else goto cont1;

		// the trace hit a 3D floor before the thing.
		// Calculate an intersection and abort.
		Results->Sector = &Level->sectors[CurSector->sectornum];
		if (!CheckSectorPlane(CurSector, Results->HitType == TRACE_HitFloor))
		{
			Results->HitType = TRACE_HitNone;
		}
		if (TraceCallback != NULL)
		{
			switch (TraceCallback(*Results, TraceCallbackData))
			{
			case TRACE_Continue: return true;
			case TRACE_Stop:	 return false;
			case TRACE_Abort:	 Results->HitType = TRACE_HitNone; return false;
			case TRACE_Skip:	 Results->HitType = TRACE_HitNone; return true;
			}
		}
		else
		{
			return false;
		}
	}
cont1:


	Results->HitType = TRACE_HitActor;
	Results->HitPos = hit;
	SetSourcePosition();
	Results->Distance = dist;
	Results->Fraction = in->frac;
	Results->Actor = in->d.thing;

	if (TraceCallback != NULL)
	{
		switch (TraceCallback(*Results, TraceCallbackData))
		{
		case TRACE_Stop:	return false;
		case TRACE_Abort:	Results->HitType = TRACE_HitNone; return false;
		case TRACE_Skip:	Results->HitType = TRACE_HitNone; return true;
		default:			return true;
		}
	}
	else
	{
		return false;
	}
}

//==========================================================================
//
//
//
//==========================================================================

bool FTraceInfo::TraceTraverse (int ptflags)
{
	// Do a 3D floor check in the starting sector
	Setup3DFloors();

	FPathTraverse it(Level, Start.X, Start.Y, Vec.X * MaxDist, Vec.Y * MaxDist, ptflags | PT_DELTA, startfrac);
	intercept_t *in;
	int lastsplashsector = -1;

	while ((in = it.Next()))
	{
		// Deal with splashes in 3D floors (but only run once per sector, not each iteration - and stop if something was found.)
		if (Results->Crossed3DWater == NULL && lastsplashsector != CurSector->sectornum)
		{
			for (auto rover : CurSector->e->XFloor.ffloors)
			{
				if ((rover->flags & FF_EXISTS) && isLiquid(rover))
				{
					if (Check3DFloorPlane(rover, false))
					{
						// only consider if the plane is above the actual floor.
						if (rover->top.plane->ZatPoint(Results->HitPos) > CurSector->floorplane.ZatPoint(Results->HitPos))
						{
							Results->Crossed3DWater = rover;
							Results->Crossed3DWaterPos = Results->HitPos;
							Results->Distance = 0;
						}
					}
				}
			}
			lastsplashsector = CurSector->sectornum;
		}

		double dist = MaxDist * in->frac;
		DVector3 hit = Start + Vec * dist;

		// Crossed a floor portal? 
		if (Vec.Z < 0 && !CurSector->PortalBlocksMovement(sector_t::floor))
		{
			// calculate position where the portal is crossed
			double portz = CurSector->GetPortalPlaneZ(sector_t::floor);
			if (hit.Z < portz && limitz > portz)
			{
				limitz = portz;
				EnterSectorPortal(it, sector_t::floor, (portz - Start.Z) / (Vec.Z * MaxDist), CurSector);
				continue;
			}
		}
		// ... or a ceiling portal?
		else if (Vec.Z > 0 && !CurSector->PortalBlocksMovement(sector_t::ceiling))
		{
			// calculate position where the portal is crossed
			double portz = CurSector->GetPortalPlaneZ(sector_t::ceiling);
			if (hit.Z > portz && limitz < portz)
			{
				limitz = portz;
				EnterSectorPortal(it, sector_t::ceiling, (portz - Start.Z) / (Vec.Z * MaxDist), CurSector);
				continue;
			}
		}

		if (in->isaline)
		{
			if (in->d.line->isLinePortal() && P_PointOnLineSidePrecise(Start, in->d.line) == 0)
			{
				sector_t* entersector = in->d.line->backsector;
				if (entersector == NULL || (hit.Z >= entersector->floorplane.ZatPoint(hit) && hit.Z <= entersector->ceilingplane.ZatPoint(hit)))
				{
					FLinePortal* port = in->d.line->getPortal();
					// The caller cannot handle portals without global offset.
					if (port->mType == PORTT_LINKED || !(TraceFlags & TRACE_PortalRestrict))
					{
						EnterLinePortal(it, in);
						continue;
					}
				}
			}
			if (!LineCheck(in, dist, hit, false)) break;
		}
		else if (((in->d.thing->flags & ActorMask) || ActorMask == 0xffffffff) && in->d.thing != IgnoreThis)
		{
			if (!ThingCheck(in, dist, hit)) break;
		}
	}

	if (Results->HitType == TRACE_HitNone)
	{
		if (CurSector->PortalBlocksMovement(sector_t::floor) && CheckSectorPlane(CurSector, true))
		{
			Results->HitType = TRACE_HitFloor;
			Results->HitTexture = CurSector->GetTexture(sector_t::floor);
		}
		else if (CurSector->PortalBlocksMovement(sector_t::ceiling) && CheckSectorPlane(CurSector, false))
		{
			Results->HitType = TRACE_HitCeiling;
			Results->HitTexture = CurSector->GetTexture(sector_t::ceiling);
		}
	}

	// check for intersection with floor/ceiling
	Results->Sector = &Level->sectors[CurSector->sectornum];

	if (Results->CrossedWater == NULL &&
		CurSector->heightsec != NULL &&
		CurSector->heightsec->floorplane.ZatPoint(Start) < Start.Z &&
		CurSector->heightsec->floorplane.ZatPoint(Results->HitPos) >= Results->HitPos.Z)
	{
		// Save the result so that the water check doesn't destroy it.
		FTraceResults *res = Results;
		FTraceResults hsecResult;
		Results = &hsecResult;

		if (CheckSectorPlane(CurSector->heightsec, true))
		{
			Results->CrossedWater = &Level->sectors[CurSector->sectornum];
			Results->CrossedWaterPos = Results->HitPos;
			Results->Distance = 0;
		}
		Results = res;
	}
	if (Results->HitType == TRACE_HitNone && Results->Distance == 0)
	{
		Results->HitPos = Start + Vec * MaxDist;
		SetSourcePosition();
		Results->Distance = MaxDist;
		Results->Fraction = 1.;
	}

	// [MK] set 3d floor on plane hits (if any)
	// modders will need this to get accurate plane normals on slopes
	if (Results->HitType == TRACE_HitFloor)
	{
		double secbottom = Results->Sector->floorplane.ZatPoint(Results->HitPos);
		for (auto rover : Results->Sector->e->XFloor.ffloors)
		{
			if (!(rover->flags&FF_EXISTS))
				continue;
			double ff_top = rover->top.plane->ZatPoint(Results->HitPos);
			if (fabs(ff_top-secbottom) < EQUAL_EPSILON)
				continue;
			if (fabs(ff_top-Results->HitPos.Z) > EQUAL_EPSILON)
				continue;
			Results->ffloor = rover;
			break;
		}
	}
	else if (Results->HitType == TRACE_HitCeiling)
	{
		double sectop = Results->Sector->ceilingplane.ZatPoint(Results->HitPos);
		for (auto rover : Results->Sector->e->XFloor.ffloors)
		{
			if (!(rover->flags&FF_EXISTS))
				continue;
			double ff_bottom = rover->bottom.plane->ZatPoint(Results->HitPos);
			if (fabs(ff_bottom-sectop) < EQUAL_EPSILON)
				continue;
			if (fabs(ff_bottom-Results->HitPos.Z) > EQUAL_EPSILON)
				continue;
			Results->ffloor = rover;
			break;
		}
	}
	return Results->HitType != TRACE_HitNone;
}

//==========================================================================
//
//
//
//==========================================================================

bool FTraceInfo::CheckPlane (const secplane_t &plane)
{
	double den = plane.Normal() | Vec;

	if (den != 0)
	{
		double num = (plane.Normal() | Start) + plane.fD();

		double hitdist = -num / den;

		if (hitdist > EnterDist && hitdist < MaxDist)
		{
			Results->HitPos = Start + Vec * hitdist;
			SetSourcePosition();
			Results->Distance = hitdist;
			Results->Fraction = hitdist / MaxDist;
			return true;
		}
	}
	return false;
}

//==========================================================================
//
//
//
//==========================================================================

static bool EditTraceResult (uint32_t flags, FTraceResults &res)
{
	if (flags & TRACE_HitSky)
	{ // Throw away sky hits
		if (res.HitType == TRACE_HitFloor || res.HitType == TRACE_HitCeiling)
		{
			if (res.HitTexture == skyflatnum)
			{
				res.HitType = TRACE_HasHitSky;
				return true;
			}
		}
		else if (res.HitType == TRACE_HitWall)
		{
			if (res.Tier == TIER_Upper &&
				res.Line->frontsector->GetTexture(sector_t::ceiling) == skyflatnum &&
				res.Line->backsector->GetTexture(sector_t::ceiling) == skyflatnum)
			{
				res.HitType = TRACE_HasHitSky;
				return true;
			}
		}
	}
	if (flags & TRACE_NoSky)
	{ // Throw away sky hits
		if (res.HitType == TRACE_HitFloor || res.HitType == TRACE_HitCeiling)
		{
			if (res.HitTexture == skyflatnum)
			{
				res.HitType = TRACE_HitNone;
				return false;
			}
		}
		else if (res.HitType == TRACE_HitWall)
		{
			if (res.Tier == TIER_Upper &&
				res.Line->frontsector->GetTexture(sector_t::ceiling) == skyflatnum &&
				res.Line->backsector->GetTexture(sector_t::ceiling) == skyflatnum)
			{
				res.HitType = TRACE_HitNone;
				return false;
			}
		}
	}
	return true;
}

//==========================================================================
//
//  [ZZ] here go the methods for the ZScript interface
//
//==========================================================================
IMPLEMENT_CLASS(DLineTracer, false, false)
DEFINE_FIELD(DLineTracer, Results)

// define TraceResults fields
DEFINE_FIELD_NAMED_X(TraceResults, FTraceResults, Sector, HitSector)
DEFINE_FIELD_X(TraceResults, FTraceResults, HitTexture)
DEFINE_FIELD_X(TraceResults, FTraceResults, HitPos)
DEFINE_FIELD_X(TraceResults, FTraceResults, HitVector)
DEFINE_FIELD_X(TraceResults, FTraceResults, SrcFromTarget)
DEFINE_FIELD_X(TraceResults, FTraceResults, SrcAngleFromTarget)
DEFINE_FIELD_X(TraceResults, FTraceResults, Distance)
DEFINE_FIELD_X(TraceResults, FTraceResults, Fraction)
DEFINE_FIELD_NAMED_X(TraceResults, FTraceResults, Actor, HitActor)
DEFINE_FIELD_NAMED_X(TraceResults, FTraceResults, Line, HitLine)
DEFINE_FIELD_X(TraceResults, FTraceResults, Side)
DEFINE_FIELD_X(TraceResults, FTraceResults, Tier)
DEFINE_FIELD_X(TraceResults, FTraceResults, unlinked)
DEFINE_FIELD_X(TraceResults, FTraceResults, HitType)
DEFINE_FIELD_X(TraceResults, FTraceResults, ffloor)
DEFINE_FIELD_X(TraceResults, FTraceResults, CrossedWater)
DEFINE_FIELD_X(TraceResults, FTraceResults, CrossedWaterPos)
DEFINE_FIELD_X(TraceResults, FTraceResults, Crossed3DWater)
DEFINE_FIELD_X(TraceResults, FTraceResults, Crossed3DWaterPos)

DEFINE_ACTION_FUNCTION(DLineTracer, Trace)
{
	PARAM_SELF_PROLOGUE(DLineTracer);
	/*bool Trace(const DVector3 &start, sector_t *sector, const DVector3 &direction, double maxDist,
	ActorFlags ActorMask, uint32_t WallMask, AActor *ignore, FTraceResults &res, uint32_t traceFlags = 0,
	ETraceStatus(*callback)(FTraceResults &res, void *) = NULL, void *callbackdata = NULL);*/
	PARAM_FLOAT(start_x);
	PARAM_FLOAT(start_y);
	PARAM_FLOAT(start_z);
	PARAM_POINTER_NOT_NULL(sector, sector_t);
	PARAM_FLOAT(direction_x);
	PARAM_FLOAT(direction_y);
	PARAM_FLOAT(direction_z);
	PARAM_FLOAT(maxDist);
	// actor flags and wall flags are not supported due to how flags are implemented on the ZScript side.
	// say thanks to oversimplifying the user API.
	PARAM_INT(traceFlags);

	// these are internal hacks.
	traceFlags &= ~(TRACE_PCross | TRACE_Impact);
	// this too
	traceFlags |= TRACE_3DCallback;

	// Trace(vector3 start, Sector sector, vector3 direction, double maxDist, ETraceFlags traceFlags)
	bool res = Trace(DVector3(start_x, start_y, start_z), sector, DVector3(direction_x, direction_y, direction_z), maxDist,
					 (ActorFlag)0xFFFFFFFF, 0xFFFFFFFF, nullptr, self->Results, traceFlags, &DLineTracer::TraceCallback, self);
	ACTION_RETURN_BOOL(res);
}

ETraceStatus DLineTracer::TraceCallback(FTraceResults& res, void* pthis)
{
	DLineTracer* self = (DLineTracer*)pthis;
	// "res" here should refer to self->Results anyway.

	// patch results a bit. modders don't expect it to work like this most likely.
	// code by MarisaKirisame
	if (res.HitType == TRACE_HitWall)
	{
		int txpart;
		switch (res.Tier)
		{
		case TIER_Middle:
			res.HitTexture = res.Line->sidedef[res.Side]->textures[1].texture;
			break;
		case TIER_Upper:
			res.HitTexture = res.Line->sidedef[res.Side]->textures[0].texture;
			break;
		case TIER_Lower:
			res.HitTexture = res.Line->sidedef[res.Side]->textures[2].texture;
			break;
		case TIER_FFloor:
			txpart = (res.ffloor->flags & FF_UPPERTEXTURE) ? 0 : (res.ffloor->flags & FF_LOWERTEXTURE) ? 2 : 1;
			res.HitTexture = res.ffloor->master->sidedef[0]->textures[txpart].texture;
			break;
		}
	}

	return self->CallZScriptCallback();
}

ETraceStatus DLineTracer::CallZScriptCallback()
{
	IFVIRTUAL(DLineTracer, TraceCallback)
	{
		int status;
		VMReturn results[1] = { &status };
		VMValue params[1] = { (DLineTracer*)this };
		VMCall(func, params, 1, results, 1);
		return (ETraceStatus)status;
	}

	return TRACE_Stop;
}
