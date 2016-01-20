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

struct FTraceInfo
{
	fixed_t StartX, StartY, StartZ;
	fixed_t Vx, Vy, Vz;
	ActorFlags ActorMask;
	DWORD WallMask;
	AActor *IgnoreThis;
	FTraceResults *Results;
	sector_t *CurSector;
	fixed_t MaxDist;
	fixed_t EnterDist;
	ETraceStatus (*TraceCallback)(FTraceResults &res, void *data);
	void *TraceCallbackData;
	DWORD TraceFlags;
	int inshootthrough;

	// These are required for 3D-floor checking
	// to create a fake sector with a floor 
	// or ceiling plane coming from a 3D-floor
	sector_t DummySector[2];	
	int sectorsel;		

	bool TraceTraverse (int ptflags);
	bool CheckPlane(const secplane_t &plane);
	bool CheckSectorPlane (const sector_t *sector, bool checkFloor);
	bool Check3DFloorPlane(const F3DFloor *ffloor, bool checkBottom);
};

static bool EditTraceResult (DWORD flags, FTraceResults &res);


bool Trace (fixed_t x, fixed_t y, fixed_t z, sector_t *sector,
			fixed_t vx, fixed_t vy, fixed_t vz, fixed_t maxDist,
			ActorFlags actorMask, DWORD wallMask, AActor *ignore,
			FTraceResults &res,
			DWORD flags, ETraceStatus (*callback)(FTraceResults &res, void *), void *callbackdata)
{
	int ptflags;
	FTraceInfo inf;

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
	inf.inshootthrough = true;
	inf.sectorsel=0;
	memset(&res, 0, sizeof(res));
	/* // Redundant with the memset
	res.HitType = TRACE_HitNone;
	res.CrossedWater = NULL;
	res.Crossed3DWater = NULL;
	*/

	// Do a 3D floor check in the starting sector
	TDeletingArray<F3DFloor*> &ff = sector->e->XFloor.ffloors;

	if (ff.Size())
	{
		memcpy(&inf.DummySector[0],sector,sizeof(sector_t));
		inf.CurSector=sector=&inf.DummySector[0];
		inf.sectorsel=1;
		fixed_t bf = sector->floorplane.ZatPoint (x, y);
		fixed_t bc = sector->ceilingplane.ZatPoint (x, y);

		for(unsigned int i=0;i<ff.Size();i++)
		{
			F3DFloor * rover=ff[i];
			if (!(rover->flags&FF_EXISTS))
				continue;

			if (rover->flags&FF_SWIMMABLE && res.Crossed3DWater == NULL)
			{
				if (inf.Check3DFloorPlane(rover, false))
					res.Crossed3DWater = rover;
			}

			if (!(rover->flags&FF_SHOOTTHROUGH))
			{
				fixed_t ff_bottom=rover->bottom.plane->ZatPoint(x, y);
				fixed_t ff_top=rover->top.plane->ZatPoint(x, y);
				// clip to the part of the sector we are in
				if (z>ff_top)
				{
					// above
					if (bf<ff_top)
					{
						sector->floorplane=*rover->top.plane;
						sector->SetTexture(sector_t::floor, *rover->top.texture, false);
						bf=ff_top;
					}
				}
				else if (z<ff_bottom)
				{
					//below
					if (bc>ff_bottom)
					{
						sector->ceilingplane=*rover->bottom.plane;
						sector->SetTexture(sector_t::ceiling, *rover->bottom.texture, false);
						bc=ff_bottom;
					}
				}
				else
				{
					// inside
					if (bf<ff_bottom)
					{
						sector->floorplane=*rover->bottom.plane;
						sector->SetTexture(sector_t::floor, *rover->bottom.texture, false);
						bf=ff_bottom;
					}

					if (bc>ff_top)
					{
						sector->ceilingplane=*rover->top.plane;
						sector->SetTexture(sector_t::ceiling, *rover->top.texture, false);
						bc=ff_top;
					}
					inf.inshootthrough = false;
				}
			}
		}
	}

	// check for overflows and clip if necessary
	SQWORD xd = (SQWORD)x + ( ( SQWORD(vx) * SQWORD(maxDist) )>>16);

	if (xd>SQWORD(32767)*FRACUNIT)
	{
		maxDist = inf.MaxDist = FixedDiv(FIXED_MAX - x, vx);
	}
	else if (xd<-SQWORD(32767)*FRACUNIT)
	{
		maxDist = inf.MaxDist = FixedDiv(FIXED_MIN - x, vx);
	}


	SQWORD yd = (SQWORD)y + ( ( SQWORD(vy) * SQWORD(maxDist) )>>16);

	if (yd>SQWORD(32767)*FRACUNIT)
	{
		maxDist = inf.MaxDist=FixedDiv(FIXED_MAX-y,vy);
	}
	else if (yd<-SQWORD(32767)*FRACUNIT)
	{
		maxDist = inf.MaxDist=FixedDiv(FIXED_MIN-y,vy);
	}

	// recalculate the trace's end points for robustness
	if (inf.TraceTraverse (ptflags))
	{ // check for intersection with floor/ceiling
		res.Sector = &sectors[inf.CurSector->sectornum];

		if (inf.CheckSectorPlane (inf.CurSector, true))
		{
			res.HitType = TRACE_HitFloor;
			res.HitTexture = inf.CurSector->GetTexture(sector_t::floor);
			if (res.CrossedWater == NULL &&
				inf.CurSector->heightsec != NULL &&
				inf.CurSector->heightsec->floorplane.ZatPoint (res.X, res.Y) >= res.Z)
			{
				res.CrossedWater = &sectors[inf.CurSector->sectornum];
			}
		}
		else if (inf.CheckSectorPlane (inf.CurSector, false))
		{
			res.HitType = TRACE_HitCeiling;
			res.HitTexture = inf.CurSector->GetTexture(sector_t::ceiling);
		}
	}

	if (res.HitType != TRACE_HitNone)
	{
		if (flags)
		{
			return EditTraceResult (flags, res);
		}
		else
		{
			return true;
		}
	}
	else
	{
		res.HitType = TRACE_HitNone;
		res.X = x + FixedMul (vx, maxDist);
		res.Y = y + FixedMul (vy, maxDist);
		res.Z = z + FixedMul (vz, maxDist);
		res.Distance = maxDist;
		res.Fraction = FRACUNIT;
		return false;
	}
}

bool FTraceInfo::TraceTraverse (int ptflags)
{
	FPathTraverse it(StartX, StartY, FixedMul (Vx, MaxDist), FixedMul (Vy, MaxDist), ptflags | PT_DELTA);
	intercept_t *in;

	while ((in = it.Next()))
	{
		fixed_t hitx, hity, hitz;
		fixed_t dist;

		// Deal with splashes in 3D floors
		if (CurSector->e->XFloor.ffloors.Size())
		{
			for(unsigned int i=0;i<CurSector->e->XFloor.ffloors.Size();i++)
			{
				F3DFloor * rover=CurSector->e->XFloor.ffloors[i];
				if (!(rover->flags&FF_EXISTS))
					continue;

				// Deal with splashy stuff
				if (rover->flags&FF_SWIMMABLE && Results->Crossed3DWater == NULL)
				{
					if (Check3DFloorPlane(rover, false))
						Results->Crossed3DWater = rover;
				}
			}
		}

		if (in->isaline)
		{
			int lineside;
			sector_t *entersector;

			dist = FixedMul (MaxDist, in->frac);
			hitx = StartX + FixedMul (Vx, dist);
			hity = StartY + FixedMul (Vy, dist);
			hitz = StartZ + FixedMul (Vz, dist);

			fixed_t ff, fc, bf = 0, bc = 0;

			// CurSector may be a copy so we must compare the sector number, not the index!
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
					lineside = P_PointOnLineSide (StartX, StartY, in->d.line);
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
						P_ActivateLine (in->d.line, IgnoreThis, lineside, SPAC_PCross);
					}
					if (TraceFlags & TRACE_Impact)
					{
						P_ActivateLine (in->d.line, IgnoreThis, lineside, SPAC_Impact);
					}
					continue;
				}
			}

			ff = CurSector->floorplane.ZatPoint (hitx, hity);
			fc = CurSector->ceilingplane.ZatPoint (hitx, hity);

			if (entersector != NULL)
			{
				bf = entersector->floorplane.ZatPoint (hitx, hity);
				bc = entersector->ceilingplane.ZatPoint (hitx, hity);
			}

			sector_t *hsec = CurSector->GetHeightSec();
			if (Results->CrossedWater == NULL &&
				hsec != NULL && 
				//CurSector->heightsec->waterzone &&
				hitz <= hsec->floorplane.ZatPoint (hitx, hity))
			{
				// hit crossed a water plane
				Results->CrossedWater = &sectors[CurSector->sectornum];
			}

			if (hitz <= ff)
			{ // hit floor in front of wall
				Results->HitType = TRACE_HitFloor;
				Results->HitTexture = CurSector->GetTexture(sector_t::floor);
			}
			else if (hitz >= fc)
			{ // hit ceiling in front of wall
				Results->HitType = TRACE_HitCeiling;
				Results->HitTexture = CurSector->GetTexture(sector_t::ceiling);
			}
			else if (entersector == NULL ||
				hitz < bf || hitz > bc ||
				in->d.line->flags & WallMask)
			{ // hit the wall
				Results->HitType = TRACE_HitWall;
				Results->Tier =
					entersector == NULL ? TIER_Middle :
					hitz <= bf ? TIER_Lower :
					hitz >= bc ? TIER_Upper : TIER_Middle;
				if (TraceFlags & TRACE_Impact)
				{
					P_ActivateLine (in->d.line, IgnoreThis, lineside, SPAC_Impact);
				}
			}
			else
			{ 	// made it past the wall
				// check for 3D floors first
				if (entersector->e->XFloor.ffloors.Size())
				{
					memcpy(&DummySector[sectorsel],entersector,sizeof(sector_t));
					entersector=&DummySector[sectorsel];
					sectorsel^=1;

					for(unsigned int i=0;i<entersector->e->XFloor.ffloors.Size();i++)
					{
						F3DFloor * rover=entersector->e->XFloor.ffloors[i];
						int entershootthrough = !!(rover->flags&FF_SHOOTTHROUGH);

						if (entershootthrough != inshootthrough && rover->flags&FF_EXISTS)
						{
							fixed_t ff_bottom=rover->bottom.plane->ZatPoint(hitx, hity);
							fixed_t ff_top=rover->top.plane->ZatPoint(hitx, hity);

							// clip to the part of the sector we are in
							if (hitz>ff_top)
							{
								// above
								if (bf<ff_top)
								{
									entersector->floorplane=*rover->top.plane;
									entersector->SetTexture(sector_t::floor, *rover->top.texture, false);
									bf=ff_top;
								}
							}
							else if (hitz<ff_bottom)
							{
								//below
								if (bc>ff_bottom)
								{
									entersector->ceilingplane=*rover->bottom.plane;
									entersector->SetTexture(sector_t::ceiling, *rover->bottom.texture, false);
									bc=ff_bottom;
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
									P_ActivateLine (in->d.line, IgnoreThis, lineside, SPAC_Impact);
								}
								goto cont;
							}
						}
					}
				}

				Results->HitType = TRACE_HitNone;
				if (TraceFlags & TRACE_PCross)
				{
					P_ActivateLine (in->d.line, IgnoreThis, lineside, SPAC_PCross);
				}
				if (TraceFlags & TRACE_Impact)
				{ // This is incorrect for "impact", but Hexen did this, so
				  // we need to as well, for compatibility
					P_ActivateLine (in->d.line, IgnoreThis, lineside, SPAC_Impact);
				}
			}
cont:

			if (Results->HitType != TRACE_HitNone)
			{
				// We hit something, so figure out where exactly
				Results->Sector = &sectors[CurSector->sectornum];

				if (Results->HitType != TRACE_HitWall &&
					!CheckSectorPlane (CurSector, Results->HitType == TRACE_HitFloor))
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
						P_ActivateLine (in->d.line, IgnoreThis, lineside, SPAC_Impact);
					}
				}

				if (Results->HitType == TRACE_HitWall)
				{
					Results->X = hitx;
					Results->Y = hity;
					Results->Z = hitz;
					Results->Distance = dist;
					Results->Fraction = in->frac;
					Results->Line = in->d.line;
					Results->Side = lineside;
				}
			}

			if (Results->HitType == TRACE_HitNone)
			{
				CurSector = entersector;
				EnterDist = dist;
				continue;
			}

			if (TraceCallback != NULL)
			{
				switch (TraceCallback(*Results, TraceCallbackData))
				{
				case TRACE_Stop:	return false;
				case TRACE_Abort:	Results->HitType = TRACE_HitNone; return false;
				case TRACE_Skip:	Results->HitType = TRACE_HitNone; break;
				default:			break;
				}
			}
			else
			{
				return false;
			}
		}

		// Encountered an actor
		if (!(in->d.thing->flags & ActorMask) || in->d.thing == IgnoreThis)
		{
			continue;
		}

		dist = FixedMul (MaxDist, in->frac);
		hitx = StartX + FixedMul (Vx, dist);
		hity = StartY + FixedMul (Vy, dist);
		hitz = StartZ + FixedMul (Vz, dist);

		if (hitz > in->d.thing->Top())
		{ // trace enters above actor
			if (Vz >= 0) continue;      // Going up: can't hit
			
			// Does it hit the top of the actor?
			dist = FixedDiv(in->d.thing->Top() - StartZ, Vz);

			if (dist > MaxDist) continue;
			in->frac = FixedDiv(dist, MaxDist);

			hitx = StartX + FixedMul (Vx, dist);
			hity = StartY + FixedMul (Vy, dist);
			hitz = StartZ + FixedMul (Vz, dist);

			// calculated coordinate is outside the actor's bounding box
			if (abs(hitx - in->d.thing->X()) > in->d.thing->radius ||
				abs(hity - in->d.thing->Y()) > in->d.thing->radius) continue;
		}
		else if (hitz < in->d.thing->Z())
		{ // trace enters below actor
			if (Vz <= 0) continue;      // Going down: can't hit
			
			// Does it hit the bottom of the actor?
			dist = FixedDiv(in->d.thing->Z() - StartZ, Vz);
			if (dist > MaxDist) continue;
			in->frac = FixedDiv(dist, MaxDist);

			hitx = StartX + FixedMul (Vx, dist);
			hity = StartY + FixedMul (Vy, dist);
			hitz = StartZ + FixedMul (Vz, dist);

			// calculated coordinate is outside the actor's bounding box
			if (abs(hitx - in->d.thing->X()) > in->d.thing->radius ||
				abs(hity - in->d.thing->Y()) > in->d.thing->radius) continue;
		}

		// check for extrafloors first
		if (CurSector->e->XFloor.ffloors.Size())
		{
			fixed_t ff_floor=CurSector->floorplane.ZatPoint(hitx, hity);
			fixed_t ff_ceiling=CurSector->ceilingplane.ZatPoint(hitx, hity);

			if (hitz>ff_ceiling)	// actor is hit above the current ceiling
			{
				Results->HitType=TRACE_HitCeiling;
				Results->HitTexture = CurSector->GetTexture(sector_t::ceiling);
			}
			else if (hitz<ff_floor)	// actor is hit below the current floor
			{
				Results->HitType=TRACE_HitFloor;
				Results->HitTexture = CurSector->GetTexture(sector_t::floor);
			}
			else goto cont1;

			// the trace hit a 3D-floor before the thing.
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
		Results->X = hitx;
		Results->Y = hity;
		Results->Z = hitz;
		Results->Distance = dist;
		Results->Fraction = in->frac;
		Results->Actor = in->d.thing;

		if (TraceCallback != NULL)
		{
			switch (TraceCallback(*Results, TraceCallbackData))
			{
			case TRACE_Stop:	return false;
			case TRACE_Abort:	Results->HitType = TRACE_HitNone; return false;
			case TRACE_Skip:	Results->HitType = TRACE_HitNone; break;
			default:			break;
			}
		}
		else
		{
			return false;
		}
	}
	return true;
}

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
			Results->X = StartX + FixedMul (Vx, hitdist);
			Results->Y = StartY + FixedMul (Vy, hitdist);
			Results->Z = StartZ + FixedMul (Vz, hitdist);
			Results->Distance = hitdist;
			Results->Fraction = FixedDiv (hitdist, MaxDist);
			return true;
		}
	}
	return false;
}

bool FTraceInfo::CheckSectorPlane (const sector_t *sector, bool checkFloor)
{
	secplane_t plane;

	if (checkFloor)
	{
		plane = sector->floorplane;
	}
	else
	{
		plane = sector->ceilingplane;
	}

	return CheckPlane(plane);
}

bool FTraceInfo::Check3DFloorPlane (const F3DFloor *ffloor, bool checkBottom)
{
	secplane_t plane;

	if (checkBottom)
	{
		plane = *(ffloor->bottom.plane);
	}
	else
	{
		plane = *(ffloor->top.plane);
	}

	return CheckPlane(plane);
}

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
