#ifndef __P_TRACE_H__
#define __P_TRACE_H__

#include "r_defs.h"

enum ETraceResult
{
	TRACE_HitNone,
	TRACE_HitFloor,
	TRACE_HitCeiling,
	TRACE_HitWall,
	TRACE_HitActor
};

enum
{
	TIER_Middle,
	TIER_Upper,
	TIER_Lower
};

struct FTraceResults
{
	sector_t *Sector;
	fixed_t X, Y, Z;
	ETraceResult HitType;
	fixed_t Distance;
	fixed_t Fraction;
	
	AActor *Actor;		// valid if hit an actor

	line_t *Line;		// valid if hit a line
	byte Side;
	byte Tier;
};

enum
{
	TRACE_NoSky			= 1,	// Hitting the sky returns TRACE_HitNone
	TRACE_PCross		= 2,	// Trigger SPAC_PCROSS lines
	TRACE_Impact		= 4,	// Trigger SPAC_IMPACT lines
};

bool Trace (fixed_t x, fixed_t y, fixed_t z, sector_t *sector,
			fixed_t vx, fixed_t vy, fixed_t vz, fixed_t maxDist,
			DWORD ActorMask, DWORD WallMask, AActor *ignore,
			FTraceResults &res,
			DWORD traceFlags=0, bool (*callback)(FTraceResults &res)=NULL);

#endif //__P_TRACE_H__