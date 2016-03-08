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
#include "i_system.h"
#include "r_sky.h"
#include "doomstat.h"
#include "p_maputl.h"
#include "r_defs.h"
#include "p_spec.h"

//==========================================================================
//
//
//
//==========================================================================

struct FTraceInfo
{
	fixed_t StartX, StartY, StartZ;
	fixed_t Vx, Vy, Vz;
	ActorFlags ActorMask;
	DWORD WallMask;
	AActor *IgnoreThis;
	FTraceResults *Results;
	FTraceResults *TempResults;
	sector_t *CurSector;
	fixed_t MaxDist;
	fixed_t EnterDist;
	ETraceStatus (*TraceCallback)(FTraceResults &res, void *data);
	void *TraceCallbackData;
	DWORD TraceFlags;
	int inshootthrough;
	fixed_t startfrac;
	int aimdir;
	fixed_t limitz;

	// These are required for 3D-floor checking
	// to create a fake sector with a floor 
	// or ceiling plane coming from a 3D-floor
	sector_t DummySector[2];	
	int sectorsel;		

	void Setup3DFloors();
	bool LineCheck(intercept_t *in);
	bool ThingCheck(intercept_t *in);
	bool TraceTraverse (int ptflags);
	bool CheckPlane(const secplane_t &plane);
	int EnterLinePortal(line_t *li, fixed_t frac);
	void EnterSectorPortal(int position, fixed_t frac, sector_t *entersec);


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
		Results->SrcFromTarget = { StartX, StartY, StartZ };
		Results->HitVector = { Vx, Vy, Vz };
		Results->SrcAngleToTarget = R_PointToAngle2(0, 0, Results->HitPos.x - StartX, Results->HitPos.y - StartY);
	}


};

static bool EditTraceResult (DWORD flags, FTraceResults &res);


//==========================================================================
//
// Trace entry point
//
//==========================================================================

bool Trace (fixed_t x, fixed_t y, fixed_t z, sector_t *sector,
			fixed_t vx, fixed_t vy, fixed_t vz, fixed_t maxDist,
			ActorFlags actorMask, DWORD wallMask, AActor *ignore,
			FTraceResults &res,
			DWORD flags, ETraceStatus (*callback)(FTraceResults &res, void *), void *callbackdata)
{
	int ptflags;
	FTraceInfo inf;
	FTraceResults tempResult;

	memset(&tempResult, 0, sizeof(tempResult));
	tempResult.Fraction = tempResult.Distance = FIXED_MAX;

	ptflags = actorMask ? PT_ADDLINES|PT_ADDTHINGS|PT_COMPATIBLE : PT_ADDLINES;

	inf.StartX = x;
	inf.StartY = y;
	inf.StartZ = z;
	inf.Vx = vx;
	inf.Vy = vy;
	inf.Vz = vz;
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
	inf.TempResults = &tempResult;
	inf.inshootthrough = true;
	inf.sectorsel=0;
	inf.aimdir = -1;
	inf.startfrac = 0;
	memset(&res, 0, sizeof(res));

	// check for overflows and clip if necessary
	SQWORD xd = (SQWORD)x + ((SQWORD(vx) * SQWORD(maxDist)) >> 16);

	if (xd>SQWORD(32767)*FRACUNIT)
	{
		inf.MaxDist = FixedDiv(FIXED_MAX - x, vx);
	}
	else if (xd<-SQWORD(32767)*FRACUNIT)
	{
		inf.MaxDist = FixedDiv(FIXED_MIN - x, vx);
	}


	SQWORD yd = (SQWORD)y + ((SQWORD(vy) * SQWORD(maxDist)) >> 16);

	if (yd>SQWORD(32767)*FRACUNIT)
	{
		inf.MaxDist = FixedDiv(FIXED_MAX - y, vy);
	}
	else if (yd<-SQWORD(32767)*FRACUNIT)
	{
		inf.MaxDist = FixedDiv(FIXED_MIN - y, vy);
	}

	if (inf.TraceTraverse (ptflags))
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

void FTraceInfo::EnterSectorPortal(int position, fixed_t frac, sector_t *entersec)
{
	if (aimdir != -1 && aimdir != position) return;
	AActor *portal = entersec->SkyBoxes[position];

	if (aimdir == sector_t::ceiling && portal->threshold < limitz) return;
	else if (aimdir == sector_t::floor && portal->threshold > limitz) return;

	FTraceInfo newtrace;
	FTraceResults results;

	memset(&results, 0, sizeof(results));

	newtrace.StartX = StartX + portal->scaleX;
	newtrace.StartY = StartY + portal->scaleY;
	newtrace.StartZ = StartZ;

	frac += FixedDiv(FRACUNIT, MaxDist);
	fixed_t enterdist = FixedMul(MaxDist, frac);
	fixed_t enterX = newtrace.StartX + FixedMul(enterdist, Vx);
	fixed_t enterY = newtrace.StartY + FixedMul(enterdist, Vy);

	newtrace.Vx = Vx;
	newtrace.Vy = Vy;
	newtrace.Vz = Vz;
	newtrace.ActorMask = ActorMask;
	newtrace.WallMask = WallMask;
	newtrace.IgnoreThis = IgnoreThis;
	newtrace.Results = &results;
	newtrace.TempResults = TempResults;
	newtrace.CurSector = P_PointInSector(enterX ,enterY);
	newtrace.MaxDist = MaxDist;
	newtrace.EnterDist = EnterDist;
	newtrace.TraceCallback = TraceCallback;
	newtrace.TraceCallbackData = TraceCallbackData;
	newtrace.TraceFlags = TraceFlags;
	newtrace.inshootthrough = true;
	newtrace.startfrac = frac;
	newtrace.aimdir = position;
	newtrace.limitz = portal->threshold;
	newtrace.sectorsel = 0;

	if (newtrace.TraceTraverse(ActorMask ? PT_ADDLINES | PT_ADDTHINGS | PT_COMPATIBLE : PT_ADDLINES))
	{
		TempResults->CopyIfCloser(newtrace.Results);
	}
}

//============================================================================
//
// traverses a line portal
// simply calling PortalRelocate does not work here because more needs to be set up
//
//============================================================================

int FTraceInfo::EnterLinePortal(line_t *li, fixed_t frac)
{
	FLinePortal *port = li->getPortal();

	// The caller cannot handle portals without global offset.
	if (port->mType != PORTT_LINKED && (TraceFlags & TRACE_PortalRestrict)) return -1;

	FTraceInfo newtrace;

	newtrace.StartX = StartX;
	newtrace.StartY = StartY;
	newtrace.StartZ = StartZ;
	newtrace.Vx = Vx;
	newtrace.Vy = Vy;
	newtrace.Vz = Vz;

	P_TranslatePortalXY(li, newtrace.StartX, newtrace.StartY);
	P_TranslatePortalZ(li, newtrace.StartZ);
	P_TranslatePortalVXVY(li, newtrace.Vx, newtrace.Vy);

	frac += FixedDiv(FRACUNIT, MaxDist);
	fixed_t enterdist = FixedMul(MaxDist, frac);
	fixed_t enterX = newtrace.StartX + FixedMul(enterdist, Vx);
	fixed_t enterY = newtrace.StartY + FixedMul(enterdist, Vy);

	newtrace.ActorMask = ActorMask;
	newtrace.WallMask = WallMask;
	newtrace.IgnoreThis = IgnoreThis;
	newtrace.Results = Results;
	newtrace.TempResults = TempResults;
	newtrace.CurSector = P_PointInSector(enterX, enterY);
	newtrace.MaxDist = MaxDist;
	newtrace.EnterDist = EnterDist;
	newtrace.TraceCallback = TraceCallback;
	newtrace.TraceCallbackData = TraceCallbackData;
	newtrace.TraceFlags = TraceFlags;
	newtrace.inshootthrough = true;
	newtrace.startfrac = frac;
	newtrace.aimdir = aimdir;
	newtrace.limitz = limitz;
	P_TranslatePortalZ(li, newtrace.limitz);
	newtrace.sectorsel = 0;
	Results->unlinked = true;
	return newtrace.TraceTraverse(ActorMask ? PT_ADDLINES | PT_ADDTHINGS | PT_COMPATIBLE : PT_ADDLINES);
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

		fixed_t sdist = FixedMul(MaxDist, startfrac);
		fixed_t x = StartX + FixedMul(Vx, sdist);
		fixed_t y = StartY + FixedMul(Vy, sdist);
		fixed_t z = StartZ + FixedMul(Vz, sdist);


		fixed_t bf = CurSector->floorplane.ZatPoint(x, y);
		fixed_t bc = CurSector->ceilingplane.ZatPoint(x, y);

		for (auto rover : ff)
		{
			if (!(rover->flags&FF_EXISTS))
				continue;

			if (rover->flags&FF_SWIMMABLE && Results->Crossed3DWater == NULL)
			{
				if (Check3DFloorPlane(rover, false))
				{
					Results->Crossed3DWater = rover;
					Results->Crossed3DWaterPos = Results->HitPos;
				}
			}

			if (!(rover->flags&FF_SHOOTTHROUGH))
			{
				fixed_t ff_bottom = rover->bottom.plane->ZatPoint(x, y);
				fixed_t ff_top = rover->top.plane->ZatPoint(x, y);
				// clip to the part of the sector we are in
				if (z > ff_top)
				{
					// above
					if (bf < ff_top)
					{
						CurSector->floorplane = *rover->top.plane;
						CurSector->SetTexture(sector_t::floor, *rover->top.texture, false);
						CurSector->SkyBoxes[sector_t::floor] == NULL;
						bf = ff_top;
					}
				}
				else if (z < ff_bottom)
				{
					//below
					if (bc > ff_bottom)
					{
						CurSector->ceilingplane = *rover->bottom.plane;
						CurSector->SetTexture(sector_t::ceiling, *rover->bottom.texture, false);
						bc = ff_bottom;
						CurSector->SkyBoxes[sector_t::ceiling] == NULL;
					}
				}
				else
				{
					// inside
					if (bf < ff_bottom)
					{
						CurSector->floorplane = *rover->bottom.plane;
						CurSector->SetTexture(sector_t::floor, *rover->bottom.texture, false);
						CurSector->SkyBoxes[sector_t::floor] == NULL;
						bf = ff_bottom;
					}

					if (bc > ff_top)
					{
						CurSector->ceilingplane = *rover->top.plane;
						CurSector->SetTexture(sector_t::ceiling, *rover->top.texture, false);
						CurSector->SkyBoxes[sector_t::ceiling] == NULL;
						bc = ff_top;
					}
					inshootthrough = false;
				}
			}
		}
	}
	if (!CurSector->PortalBlocksMovement(sector_t::ceiling))
	{
		EnterSectorPortal(sector_t::ceiling, startfrac, CurSector);
	}
	if (!CurSector->PortalBlocksMovement(sector_t::floor))
	{
		EnterSectorPortal(sector_t::floor, startfrac, CurSector);
	}
}


//==========================================================================
//
// Processes one line intercept
//
//==========================================================================

bool FTraceInfo::LineCheck(intercept_t *in)
{
	int lineside;
	sector_t *entersector;

	fixed_t dist = FixedMul(MaxDist, in->frac);
	fixed_t hitx = StartX + FixedMul(Vx, dist);
	fixed_t hity = StartY + FixedMul(Vy, dist);
	fixed_t hitz = StartZ + FixedMul(Vz, dist);

	fixed_t ff, fc, bf = 0, bc = 0;

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
			lineside = P_PointOnLineSide(StartX, StartY, in->d.line);
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
		if (i_compatflags & COMPATF_TRACE && in->d.line->backsector == in->d.line->frontsector)
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

	ff = CurSector->floorplane.ZatPoint(hitx, hity);
	fc = CurSector->ceilingplane.ZatPoint(hitx, hity);

	if (entersector != NULL)
	{
		bf = entersector->floorplane.ZatPoint(hitx, hity);
		bc = entersector->ceilingplane.ZatPoint(hitx, hity);
	}

	sector_t *hsec = CurSector->GetHeightSec();
	if (Results->CrossedWater == NULL &&
		hsec != NULL &&
		//CurSector->heightsec->waterzone &&
		hitz <= hsec->floorplane.ZatPoint(hitx, hity))
	{
		// hit crossed a water plane
		if (CheckSectorPlane(hsec, true))
		{
			Results->CrossedWater = &sectors[CurSector->sectornum];
			Results->CrossedWaterPos = Results->HitPos;
		}
	}

	if (hitz <= ff)
	{
		if (CurSector->PortalBlocksMovement(sector_t::floor))
		{
			// hit floor in front of wall
			Results->HitType = TRACE_HitFloor;
			Results->HitTexture = CurSector->GetTexture(sector_t::floor);
		}
		else if (entersector == NULL || entersector->PortalBlocksMovement(sector_t::floor))
		{
			// hit beyond a portal plane. This needs to be taken care of by the trace spawned on the other side.
			Results->HitType = TRACE_HitNone;
			return false;
		}
	}
	else if (hitz >= fc)
	{
		if (CurSector->PortalBlocksMovement(sector_t::ceiling))
		{
			// hit ceiling in front of wall
			Results->HitType = TRACE_HitCeiling;
			Results->HitTexture = CurSector->GetTexture(sector_t::ceiling);
		}
		else if (entersector == NULL || entersector->PortalBlocksMovement(sector_t::ceiling))
		{
			// hit beyond a portal plane. This needs to be taken care of by the trace spawned on the other side.
			Results->HitType = TRACE_HitNone;
			return false;
		}
	}
	else if (in->d.line->isLinePortal())
	{
		if (entersector == NULL || (hitz >= bf && hitz <= bc))
		{
			int res = EnterLinePortal(in->d.line, in->frac);
			if (res != -1)
			{
				aimdir = INT_MAX;	// flag for ending the traverse
				return !!res;
			}
		}
		goto normalline;	// hit upper or lower tier.
	}
	else if (entersector == NULL ||
		hitz < bf || hitz > bc ||
		in->d.line->flags & WallMask)
	{
normalline:
		// hit the wall
		Results->HitType = TRACE_HitWall;
		Results->Tier =
			entersector == NULL ? TIER_Middle :
			hitz <= bf ? TIER_Lower :
			hitz >= bc ? TIER_Upper : TIER_Middle;
		if (TraceFlags & TRACE_Impact)
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
					fixed_t ff_bottom = rover->bottom.plane->ZatPoint(hitx, hity);
					fixed_t ff_top = rover->top.plane->ZatPoint(hitx, hity);

					// clip to the part of the sector we are in
					if (hitz > ff_top)
					{
						// above
						if (bf < ff_top)
						{
							entersector->floorplane = *rover->top.plane;
							entersector->SetTexture(sector_t::floor, *rover->top.texture, false);
							entersector->SkyBoxes[sector_t::floor] = NULL;
							bf = ff_top;
						}
					}
					else if (hitz < ff_bottom)
					{
						//below
						if (bc > ff_bottom)
						{
							entersector->ceilingplane = *rover->bottom.plane;
							entersector->SetTexture(sector_t::ceiling, *rover->bottom.texture, false);
							entersector->SkyBoxes[sector_t::ceiling] = NULL;
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
		if (TraceFlags & TRACE_PCross)
		{
			P_ActivateLine(in->d.line, IgnoreThis, lineside, SPAC_PCross);
		}
		if (TraceFlags & TRACE_Impact)
		{ // This is incorrect for "impact", but Hexen did this, so
		  // we need to as well, for compatibility
			P_ActivateLine(in->d.line, IgnoreThis, lineside, SPAC_Impact);
		}

		if (!entersector->PortalBlocksMovement(sector_t::ceiling))
		{
			EnterSectorPortal(sector_t::ceiling, in->frac, entersector);
		}
		if (!entersector->PortalBlocksMovement(sector_t::floor))
		{
			EnterSectorPortal(sector_t::floor, in->frac, entersector);
		}
	}
cont:

	if (Results->HitType != TRACE_HitNone)
	{
		// We hit something, so figure out where exactly
		Results->Sector = &sectors[CurSector->sectornum];

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
				if (hitz <= bf || hitz >= bc)
				{
					Results->HitType = TRACE_HitWall;
					Results->Tier =
						hitz <= bf ? TIER_Lower :
						hitz >= bc ? TIER_Upper : TIER_Middle;
				}
				else
				{
					Results->HitType = TRACE_HitNone;
				}
			}
			if (Results->HitType == TRACE_HitWall && TraceFlags & TRACE_Impact)
			{
				P_ActivateLine(in->d.line, IgnoreThis, lineside, SPAC_Impact);
			}
		}

		if (Results->HitType == TRACE_HitWall)
		{
			Results->HitPos = { hitx, hity, hitz };
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
		case TRACE_Stop:	return false;
		case TRACE_Abort:	Results->HitType = TRACE_HitNone; return false;
		case TRACE_Skip:	Results->HitType = TRACE_HitNone; break;
		default:			break;
		}
	}

	if (Results->HitType == TRACE_HitNone)
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

bool FTraceInfo::ThingCheck(intercept_t *in)
{
	fixed_t dist = FixedMul(MaxDist, in->frac);
	fixed_t hitx = StartX + FixedMul(Vx, dist);
	fixed_t hity = StartY + FixedMul(Vy, dist);
	fixed_t hitz = StartZ + FixedMul(Vz, dist);

	if (hitz > in->d.thing->Top())
	{
		// trace enters above actor
		if (Vz >= 0) return true;      // Going up: can't hit

		// Does it hit the top of the actor?
		dist = FixedDiv(in->d.thing->Top() - StartZ, Vz);

		if (dist > MaxDist) return true;
		in->frac = FixedDiv(dist, MaxDist);

		hitx = StartX + FixedMul(Vx, dist);
		hity = StartY + FixedMul(Vy, dist);
		hitz = StartZ + FixedMul(Vz, dist);

		// calculated coordinate is outside the actor's bounding box
		if (abs(hitx - in->d.thing->X()) > in->d.thing->radius ||
			abs(hity - in->d.thing->Y()) > in->d.thing->radius) return true;
	}
	else if (hitz < in->d.thing->Z())
	{ // trace enters below actor
		if (Vz <= 0) return true;      // Going down: can't hit

		// Does it hit the bottom of the actor?
		dist = FixedDiv(in->d.thing->Z() - StartZ, Vz);
		if (dist > MaxDist) return true;
		in->frac = FixedDiv(dist, MaxDist);

		hitx = StartX + FixedMul(Vx, dist);
		hity = StartY + FixedMul(Vy, dist);
		hitz = StartZ + FixedMul(Vz, dist);

		// calculated coordinate is outside the actor's bounding box
		if (abs(hitx - in->d.thing->X()) > in->d.thing->radius ||
			abs(hity - in->d.thing->Y()) > in->d.thing->radius) return true;
	}

	if (CurSector->e->XFloor.ffloors.Size())
	{
		// check for 3D floor hits first.
		fixed_t ff_floor = CurSector->floorplane.ZatPoint(hitx, hity);
		fixed_t ff_ceiling = CurSector->ceilingplane.ZatPoint(hitx, hity);

		if (hitz > ff_ceiling && CurSector->PortalBlocksMovement(sector_t::ceiling))	// actor is hit above the current ceiling
		{
			Results->HitType = TRACE_HitCeiling;
			Results->HitTexture = CurSector->GetTexture(sector_t::ceiling);
		}
		else if (hitz < ff_floor && CurSector->PortalBlocksMovement(sector_t::floor))	// actor is hit below the current floor
		{
			Results->HitType = TRACE_HitFloor;
			Results->HitTexture = CurSector->GetTexture(sector_t::floor);
		}
		else goto cont1;

		// the trace hit a 3D floor before the thing.
		// Calculate an intersection and abort.
		Results->Sector = &sectors[CurSector->sectornum];
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
	Results->HitPos = { hitx, hity, hitz };
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

	FPathTraverse it(StartX, StartY, FixedMul (Vx, MaxDist), FixedMul (Vy, MaxDist), ptflags | PT_DELTA, startfrac);
	intercept_t *in;
	int lastsplashsector = -1;

	while ((in = it.Next()))
	{
		// Deal with splashes in 3D floors (but only run once per sector, not each iteration - and stop if something was found.)
		if (Results->Crossed3DWater == NULL && lastsplashsector != CurSector->sectornum)
		{
			for (auto rover : CurSector->e->XFloor.ffloors)
			{
				if ((rover->flags & FF_EXISTS) && (rover->flags&FF_SWIMMABLE))
				{
					if (Check3DFloorPlane(rover, false))
					{
						Results->Crossed3DWater = rover;
						Results->Crossed3DWaterPos = Results->HitPos;
					}
				}
			}
			lastsplashsector = CurSector->sectornum;
		}

		// We have something closer in the storage for portal subtraces.
		if (TempResults->HitType != TRACE_HitNone && in->frac > TempResults->Fraction)
		{
			break;
		}
		else if (in->isaline)
		{
			bool res = LineCheck(in);
			if (aimdir == INT_MAX) return res;	// signal for immediate abort 
			if (!res) break;
		}
		else if ((in->d.thing->flags & ActorMask) && in->d.thing != IgnoreThis)
		{
			if (!ThingCheck(in)) break;
		}
	}

	// Check if one subtrace through a portal yielded a better result.
	if (TempResults->HitType != TRACE_HitNone && 
		(Results->HitType == TRACE_HitNone || Results->Fraction > TempResults->Fraction))
	{
		// We still need to do a water check here or this may get missed on occasion
		if (Results->CrossedWater == NULL &&
			CurSector->heightsec != NULL &&
			CurSector->heightsec->floorplane.ZatPoint(Results->HitPos) >= Results->HitPos.z)
		{
			// Save the result so that the water check doesn't destroy it.
			FTraceResults *res = Results;
			FTraceResults hsecResult;
			Results = &hsecResult;

			if (CheckSectorPlane(CurSector->heightsec, true))
			{
				Results->CrossedWater = &sectors[CurSector->sectornum];
				Results->CrossedWaterPos = Results->HitPos;
			}
			Results = res;
		}

		Results->CopyIfCloser(TempResults);
		return true;
	}
	else if (Results->HitType == TRACE_HitNone)
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
	Results->Sector = &sectors[CurSector->sectornum];

	if (Results->CrossedWater == NULL &&
		CurSector->heightsec != NULL &&
		CurSector->heightsec->floorplane.ZatPoint(Results->HitPos) >= Results->HitPos.z)
	{
		// Save the result so that the water check doesn't destroy it.
		FTraceResults *res = Results;
		FTraceResults hsecResult;
		Results = &hsecResult;

		if (CheckSectorPlane(CurSector->heightsec, true))
		{
			Results->CrossedWater = &sectors[CurSector->sectornum];
			Results->CrossedWaterPos = Results->HitPos;
		}
		Results = res;
	}
	if (Results->HitType == TRACE_HitNone && Results->Distance == 0)
	{
		Results->HitPos = {
			StartX + FixedMul(Vx, MaxDist),
			StartY + FixedMul(Vy, MaxDist),
			StartZ + FixedMul(Vz, MaxDist) };
		SetSourcePosition();
		Results->Distance = MaxDist;
		Results->Fraction = FRACUNIT;
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
	fixed_t den = TMulScale16 (plane.a, Vx, plane.b, Vy, plane.c, Vz);

	if (den != 0)
	{
		fixed_t num = TMulScale16 (plane.a, StartX,
								   plane.b, StartY,
								   plane.c, StartZ) + plane.d;

		fixed_t hitdist = FixedDiv (-num, den);

		if (hitdist > EnterDist && hitdist < MaxDist)
		{
			Results->HitPos = {
				StartX + FixedMul(Vx, hitdist),
				StartY + FixedMul(Vy, hitdist),
				StartZ + FixedMul(Vz, hitdist) };
			SetSourcePosition();
			Results->Distance = hitdist;
			Results->Fraction = FixedDiv (hitdist, MaxDist);
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

static bool EditTraceResult (DWORD flags, FTraceResults &res)
{
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
