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

struct FTraceInfo
{
	fixed_t StartX, StartY, StartZ;
	fixed_t Vx, Vy, Vz;
	DWORD ActorMask, WallMask;
	AActor *IgnoreThis;
	FTraceResults *Results;
	sector_t *CurSector;
	fixed_t MaxDist;
	fixed_t EnterDist;
	bool (*TraceCallback)(FTraceResults &res);
	DWORD TraceFlags;

	bool TraceTraverse (int ptflags);
	bool CheckSectorPlane (const sector_t *sector, bool checkFloor);
};

static bool EditTraceResult (DWORD flags, FTraceResults &res);


bool Trace (fixed_t x, fixed_t y, fixed_t z, sector_t *sector,
			fixed_t vx, fixed_t vy, fixed_t vz, fixed_t maxDist,
			DWORD actorMask, DWORD wallMask, AActor *ignore,
			FTraceResults &res,
			DWORD flags, bool (*callback)(FTraceResults &res))
{
	int ptflags;
	FTraceInfo inf;

	ptflags = actorMask ? PT_ADDLINES|PT_ADDTHINGS : PT_ADDLINES;

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
	inf.TraceFlags = flags;
	res.CrossedWater = NULL;
	inf.Results = &res;

	res.HitType = TRACE_HitNone;


	if (inf.TraceTraverse (ptflags))
	{ // check for intersection with floor/ceiling
		res.Sector = inf.CurSector;

		if (inf.CheckSectorPlane (inf.CurSector, true))
		{
			res.HitType = TRACE_HitFloor;
			if (res.CrossedWater == NULL &&
				inf.CurSector->heightsec != NULL &&
				inf.CurSector->heightsec->floorplane.ZatPoint (res.X, res.Y) >= res.Z)
			{
				res.CrossedWater = inf.CurSector;
			}
		}
		else if (inf.CheckSectorPlane (inf.CurSector, false))
		{
			res.HitType = TRACE_HitCeiling;
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
	FPathTraverse it(StartX, StartY, StartX + FixedMul (Vx, MaxDist), StartY + FixedMul (Vy, MaxDist), ptflags);
	intercept_t *in;

	while ((in = it.Next()))
	{
		fixed_t hitx, hity, hitz;
		fixed_t dist;

		if (in->isaline)
		{
			int lineside;
			sector_t *entersector;

			dist = FixedMul (MaxDist, in->frac);
			hitx = StartX + FixedMul (Vx, dist);
			hity = StartY + FixedMul (Vy, dist);
			hitz = StartZ + FixedMul (Vz, dist);

			fixed_t ff, fc, bf = 0, bc = 0;

			if (in->d.line->frontsector == CurSector)
			{
				lineside = 0;
			}
			else if (in->d.line->backsector == CurSector)
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
				// This is the way Doom.exe did it and some WADs (e.g. Alien Vendetta MAP15 need it.
				if (i_compatflags & COMPATF_TRACE && in->d.line->backsector == in->d.line->frontsector)
				{
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

			if (Results->CrossedWater == NULL &&
				CurSector->heightsec &&
				!(CurSector->MoreFlags & SECF_IGNOREHEIGHTSEC) &&
				//CurSector->heightsec->waterzone &&
				hitz <= CurSector->heightsec->floorplane.ZatPoint (hitx, hity))
			{
				// hit crossed a water plane
				Results->CrossedWater = CurSector;
			}

			if (hitz <= ff)
			{ // hit floor in front of wall
				Results->HitType = TRACE_HitFloor;
			}
			else if (hitz >= fc)
			{ // hit ceiling in front of wall
				Results->HitType = TRACE_HitCeiling;
			}
			else if (entersector == NULL ||
				hitz <= bf || hitz >= bc ||
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
			{ // made it past the wall
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

			if (Results->HitType != TRACE_HitNone)
			{
				// We hit something, so figure out where exactly
				Results->Sector = CurSector;

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
				if (!TraceCallback (*Results)) return false;
			}
			else
			{
				return false;
			}
		}

		// Encountered an actor
		if (!(in->d.thing->flags & ActorMask) ||
			in->d.thing == IgnoreThis)
		{
			continue;
		}

		dist = FixedMul (MaxDist, in->frac);
		hitx = StartX + FixedMul (Vx, dist);
		hity = StartY + FixedMul (Vy, dist);
		hitz = StartZ + FixedMul (Vz, dist);

		if (hitz > in->d.thing->z + in->d.thing->height)
		{ // trace enters above actor
			if (Vz >= 0) continue;      // Going up: can't hit
			
			// Does it hit the top of the actor?
			dist = FixedDiv(in->d.thing->z + in->d.thing->height - StartZ, Vz);

			if (dist > MaxDist) continue;
			in->frac = FixedDiv(dist, MaxDist);

			hitx = StartX + FixedMul (Vx, dist);
			hity = StartY + FixedMul (Vy, dist);
			hitz = StartZ + FixedMul (Vz, dist);

			// calculated coordinate is outside the actor's bounding box
			if (abs(hitx - in->d.thing->x) > in->d.thing->radius ||
				abs(hity - in->d.thing->y) > in->d.thing->radius) continue;
		}
		else if (hitz < in->d.thing->z)
		{ // trace enters below actor
			if (Vz <= 0) continue;      // Going down: can't hit
			
			// Does it hit the bottom of the actor?
			dist = FixedDiv(in->d.thing->z - StartZ, Vz);
			if (dist > MaxDist) continue;
			in->frac = FixedDiv(dist, MaxDist);

			hitx = StartX + FixedMul (Vx, dist);
			hity = StartY + FixedMul (Vy, dist);
			hitz = StartZ + FixedMul (Vz, dist);

			// calculated coordinate is outside the actor's bounding box
			if (abs(hitx - in->d.thing->x) > in->d.thing->radius ||
				abs(hity - in->d.thing->y) > in->d.thing->radius) continue;
		}


		Results->HitType = TRACE_HitActor;
		Results->X = hitx;
		Results->Y = hity;
		Results->Z = hitz;
		Results->Distance = dist;
		Results->Fraction = in->frac;
		Results->Actor = in->d.thing;

		if (TraceCallback != NULL)
		{
			if (!TraceCallback (*Results)) return false;
		}
		else
		{
			return false;
		}
	}
	return true;
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

static bool EditTraceResult (DWORD flags, FTraceResults &res)
{
	if (flags & TRACE_NoSky)
	{ // Throw away sky hits
		if (res.HitType == TRACE_HitFloor)
		{
			if (res.Sector->GetTexture(sector_t::floor) == skyflatnum)
			{
				res.HitType = TRACE_HitNone;
				return false;
			}
		}
		else if (res.HitType == TRACE_HitCeiling)
		{
			if (res.Sector->GetTexture(sector_t::ceiling) == skyflatnum)
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
