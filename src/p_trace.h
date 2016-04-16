/*
** p_trace.h
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

#ifndef __P_TRACE_H__
#define __P_TRACE_H__

#include <stddef.h>
#include "actor.h"
#include "cmdlib.h"
#include "textures/textures.h"

struct sector_t;
struct line_t;
class AActor;
struct F3DFloor;

enum ETraceResult
{
	TRACE_HitNone,
	TRACE_HitFloor,
	TRACE_HitCeiling,
	TRACE_HitWall,
	TRACE_HitActor,
	TRACE_CrossingPortal,
};

enum
{
	TIER_Middle,
	TIER_Upper,
	TIER_Lower,
	TIER_FFloor,
};

struct FTraceResults
{
	sector_t *Sector;
	FTextureID HitTexture;
	DVector3 HitPos;
	DVector3 HitVector;
	DVector3 SrcFromTarget;
	DAngle SrcAngleFromTarget;

	double Distance;
	double Fraction;

	AActor *Actor;		// valid if hit an actor

	line_t *Line;		// valid if hit a line
	BYTE Side;
	BYTE Tier;
	bool unlinked;		// passed through a portal without static offset.
	ETraceResult HitType;
	F3DFloor *ffloor;

	sector_t *CrossedWater;		// For Boom-style, Transfer_Heights-based deep water
	DVector3 CrossedWaterPos;	// remember the position so that we can use it for spawning the splash
	F3DFloor *Crossed3DWater;	// For 3D floor-based deep water
	DVector3 Crossed3DWaterPos;
};
	

enum
{
	TRACE_NoSky			= 1,	// Hitting the sky returns TRACE_HitNone
	TRACE_PCross		= 2,	// Trigger SPAC_PCROSS lines
	TRACE_Impact		= 4,	// Trigger SPAC_IMPACT lines
	TRACE_PortalRestrict= 8,	// Cannot go through portals without a static link offset.
	TRACE_ReportPortals = 16,	// Report any portal crossing to the TraceCallback
};

// return values from callback
enum ETraceStatus
{
	TRACE_Stop,			// stop the trace, returning this hit
	TRACE_Continue,		// continue the trace, returning this hit if there are none further along
	TRACE_Skip,			// continue the trace; do not return this hit
	TRACE_Abort,		// stop the trace, returning no hits
};

bool Trace(const DVector3 &start, sector_t *sector, const DVector3 &direction, double maxDist,
	ActorFlags ActorMask, DWORD WallMask, AActor *ignore, FTraceResults &res, DWORD traceFlags = 0,
	ETraceStatus(*callback)(FTraceResults &res, void *) = NULL, void *callbackdata = NULL);

#endif //__P_TRACE_H__
