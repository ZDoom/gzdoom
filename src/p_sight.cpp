//**************************************************************************
//**
//** p_sight.cpp : Heretic 2 : Raven Software, Corp.
//**
//** $RCSfile: p_sight.c,v $
//** $Revision: 1.1 $
//** $Date: 95/05/11 00:22:50 $
//** $Author: bgokey $
//**
//**************************************************************************

#include "doomdef.h"

#include "i_system.h"
#include "p_local.h"
#include "m_random.h"
#include "m_bbox.h"

// State.
#include "r_state.h"

#include "stats.h"


/*
==============================================================================

							P_CheckSight

This uses specialized forms of the maputils routines for optimized performance

==============================================================================
*/

static fixed_t sightzstart;				// eye z of looker
static fixed_t topslope, bottomslope;	// slopes to top and bottom of target

// Performance meters
static int sightcounts[4];
static cycle_t SightCycles;
static cycle_t MaxSightCycles;

static bool P_SightTraverseIntercepts ();

/*
==============
=
= PTR_SightTraverse
=
==============
*/

static bool PTR_SightTraverse (intercept_t *in)
{
	line_t  *li;
	fixed_t slope;

	li = in->d.line;

//
// crosses a two sided line
//
	P_LineOpening (li, trace.x + FixedMul (trace.dx, in->frac),
		trace.y + FixedMul (trace.dy, in->frac));

	if (openrange <= 0)		// quick test for totally closed doors
		return false;		// stop

	// check bottom
	slope = FixedDiv (openbottom - sightzstart, in->frac);
	if (slope > bottomslope)
		bottomslope = slope;

	// check top
	slope = FixedDiv (opentop - sightzstart, in->frac);
	if (slope < topslope)
		topslope = slope;

	if (topslope <= bottomslope)
		return false;		// stop

	return true;			// keep going
}



/*
==================
=
= P_SightCheckLine
=
===================
*/

static bool P_SightCheckLine (line_t *ld)
{
	divline_t dl;

	if (ld->validcount == validcount)
	{
		return true;
	}
	ld->validcount = validcount;
	if (P_PointOnDivlineSide (ld->v1->x, ld->v1->y, &trace) ==
		P_PointOnDivlineSide (ld->v2->x, ld->v2->y, &trace))
	{
		return true;		// line isn't crossed
	}
	P_MakeDivline (ld, &dl);
	if (P_PointOnDivlineSide (trace.x, trace.y, &dl) ==
		P_PointOnDivlineSide (trace.x+trace.dx, trace.y+trace.dy, &dl))
	{
		return true;		// line isn't crossed
	}

// try to early out the check
	if (!ld->backsector)
		return false;	// stop checking

	sightcounts[3]++;
// store the line for later intersection testing
	intercept_t newintercept;
	newintercept.isaline = true;
	newintercept.d.line = ld;
	intercepts.Push (newintercept);

	return true;
}

/*
==================
=
= P_SightBlockLinesIterator
=
===================
*/

static bool P_SightBlockLinesIterator (int x, int y)
{
	int offset;
	int *list;

	polyblock_t *polyLink;
	seg_t **segList;
	int i;
	extern polyblock_t **PolyBlockMap;

	offset = y*bmapwidth+x;

	polyLink = PolyBlockMap[offset];
	while (polyLink)
	{
		if (polyLink->polyobj)
		{ // only check non-empty links
			if (polyLink->polyobj->validcount != validcount)
			{
				polyLink->polyobj->validcount = validcount;
				segList = polyLink->polyobj->segs;
				for (i = 0; i < polyLink->polyobj->numsegs; i++, segList++)
				{
					if (!P_SightCheckLine ((*segList)->linedef))
						return false;
				}
			}
		}
		polyLink = polyLink->next;
	}

	offset = *(blockmap+offset);

	for (list = blockmaplump + offset; *list != -1; list++)
	{
		if (!P_SightCheckLine (&lines[*list]))
			return false;
	}

	return true;			// everything was checked
}

/*
====================
=
= P_SightTraverseIntercepts
=
= Returns true if the traverser function returns true for all lines
====================
*/

static bool P_SightTraverseIntercepts ()
{
	int count;
	fixed_t dist;
	intercept_t *scan, *in;
	size_t scanpos;
	divline_t dl;

	count = intercepts.Size ();
//
// calculate intercept distance
//
	for (scanpos = 0; scanpos < intercepts.Size (); scanpos++)
	{
		scan = &intercepts[scanpos];
		P_MakeDivline (scan->d.line, &dl);
		scan->frac = P_InterceptVector (&trace, &dl);
	}

//
// go through in order
//
	in = 0;					// shut up compiler warning

	while (count--)
	{
		dist = FIXED_MAX;
		for (scanpos = 0; scanpos < intercepts.Size (); scanpos++)
		{
			scan = &intercepts[scanpos];
			if (scan->frac < dist)
			{
				dist = scan->frac;
				in = scan;
			}
		}

		if (!PTR_SightTraverse (in))
			return false;					// don't bother going farther
		in->frac = FIXED_MAX;
	}

	return true;			// everything was traversed
}



/*
==================
=
= P_SightPathTraverse
=
= Traces a line from x1,y1 to x2,y2, calling the traverser function for each block
= Returns true if the traverser function returns true for all lines
==================
*/

static bool P_SightPathTraverse (fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2)
{
	fixed_t xt1,yt1,xt2,yt2;
	fixed_t xstep,ystep;
	fixed_t partial;
	fixed_t xintercept, yintercept;
	int mapx, mapy, mapxstep, mapystep;
	int count;

	validcount++;
	intercepts.Clear ();

	if ( ((x1-bmaporgx)&(MAPBLOCKSIZE-1)) == 0)
		x1 += FRACUNIT;							// don't side exactly on a line
	if ( ((y1-bmaporgy)&(MAPBLOCKSIZE-1)) == 0)
		y1 += FRACUNIT;							// don't side exactly on a line
	trace.x = x1;
	trace.y = y1;
	trace.dx = x2 - x1;
	trace.dy = y2 - y1;

	x1 -= bmaporgx;
	y1 -= bmaporgy;
	xt1 = x1>>MAPBLOCKSHIFT;
	yt1 = y1>>MAPBLOCKSHIFT;

	x2 -= bmaporgx;
	y2 -= bmaporgy;
	xt2 = x2>>MAPBLOCKSHIFT;
	yt2 = y2>>MAPBLOCKSHIFT;

// points should never be out of bounds, but check once instead of
// each block
	if (xt1<0 || yt1<0 || xt1>=bmapwidth || yt1>=bmapheight
	||  xt2<0 || yt2<0 || xt2>=bmapwidth || yt2>=bmapheight)
		return false;

	if (xt2 > xt1)
	{
		mapxstep = 1;
		partial = FRACUNIT - ((x1>>MAPBTOFRAC)&(FRACUNIT-1));
		ystep = FixedDiv (y2-y1,abs(x2-x1));
	}
	else if (xt2 < xt1)
	{
		mapxstep = -1;
		partial = (x1>>MAPBTOFRAC)&(FRACUNIT-1);
		ystep = FixedDiv (y2-y1,abs(x2-x1));
	}
	else
	{
		mapxstep = 0;
		partial = FRACUNIT;
		ystep = 256*FRACUNIT;
	}
	yintercept = (y1>>MAPBTOFRAC) + FixedMul (partial, ystep);


	if (yt2 > yt1)
	{
		mapystep = 1;
		partial = FRACUNIT - ((y1>>MAPBTOFRAC)&(FRACUNIT-1));
		xstep = FixedDiv (x2-x1,abs(y2-y1));
	}
	else if (yt2 < yt1)
	{
		mapystep = -1;
		partial = (y1>>MAPBTOFRAC)&(FRACUNIT-1);
		xstep = FixedDiv (x2-x1,abs(y2-y1));
	}
	else
	{
		mapystep = 0;
		partial = FRACUNIT;
		xstep = 256*FRACUNIT;
	}
	xintercept = (x1>>MAPBTOFRAC) + FixedMul (partial, xstep);

//
// step through map blocks
// Count is present to prevent a round off error from skipping the break
	mapx = xt1;
	mapy = yt1;

	for (count = 0 ; count < 64 ; count++)
	{
		if (!P_SightBlockLinesIterator (mapx, mapy))
		{
sightcounts[1]++;
			return false;	// early out
		}

		if ((mapxstep | mapystep) == 0)
			break;

		if ( (yintercept >> FRACBITS) == mapy)
		{
			yintercept += ystep;
			mapx += mapxstep;
			if (mapx == xt2)
				mapxstep = 0;
		}
		else if ( (xintercept >> FRACBITS) == mapx)
		{
			xintercept += xstep;
			mapy += mapystep;
			if (mapy == yt2)
				mapystep = 0;
		}

	}


//
// couldn't early out, so go through the sorted list
//
sightcounts[2]++;

	return P_SightTraverseIntercepts ( );
}

/*
=====================
=
= P_CheckSight
=
= Returns true if a straight line between t1 and t2 is unobstructed
= look from eyes of t1 to any part of t2
=
= killough 4/20/98: cleaned up, made to use new LOS struct
=
=====================
*/

bool P_CheckSight (const AActor *t1, const AActor *t2, BOOL ignoreInvisibility)
{
	clock (SightCycles);

	bool res;

	const sector_t *s1 = t1->Sector;
	const sector_t *s2 = t2->Sector;
	int pnum = (s1 - sectors) * numsectors + (s2 - sectors);

//
// check for trivial rejection
//
	if (rejectmatrix[pnum>>3] & (1 << (pnum & 7)))
	{
sightcounts[0]++;
		res = false;			// can't possibly be connected
		goto done;
	}

//
// check precisely
//
	// [RH] Andy Baker's stealth monsters:
	// Cannot see an invisible object
	if (!ignoreInvisibility &&
		(t2->RenderStyle == STYLE_None ||
		 (t2->RenderStyle >= STYLE_Translucent && t2->alpha == 0) ||
		 (t2->renderflags & RF_INVISIBLE)))
	{ // small chance of an attack being made anyway
		if (P_Random (bglobal.m_Thinking ? pr_botchecksight : pr_checksight) > 50)
		{
			res = false;
			goto done;
		}
	}

	// killough 4/19/98: make fake floors and ceilings block monster view

	if ((s1->heightsec && !(s1->heightsec->MoreFlags & SECF_IGNOREHEIGHTSEC) &&
		((t1->z + t1->height <= s1->heightsec->floorplane.ZatPoint (t1->x, t1->y) &&
		  t2->z >= s1->heightsec->floorplane.ZatPoint (t2->x, t2->y)) ||
		 (t1->z >= s1->heightsec->ceilingplane.ZatPoint (t1->x, t1->y) &&
		  t2->z + t1->height <= s1->heightsec->ceilingplane.ZatPoint (t2->x, t2->y))))
		||
		(s2->heightsec && !(s2->heightsec->MoreFlags & SECF_IGNOREHEIGHTSEC) &&
		 ((t2->z + t2->height <= s2->heightsec->floorplane.ZatPoint (t2->x, t2->y) &&
		   t1->z >= s2->heightsec->floorplane.ZatPoint (t1->x, t1->y)) ||
		  (t2->z >= s2->heightsec->ceilingplane.ZatPoint (t2->x, t2->y) &&
		   t1->z + t2->height <= s2->heightsec->ceilingplane.ZatPoint (t1->x, t1->y)))))
	{
		res = false;
		goto done;
	}

	// An unobstructed LOS is possible.
	// Now look from eyes of t1 to any part of t2.

	validcount++;

	sightzstart = t1->z + t1->height - (t1->height>>2);
	bottomslope = t2->z - sightzstart;
	topslope = bottomslope + t2->height;

	res = P_SightPathTraverse (t1->x, t1->y, t2->x, t2->y);

done:
	unclock (SightCycles);
	return res;
}

ADD_STAT (sight, out)
{
	sprintf (out, "%04.1f ms (%04.1f max), %5d %2d %3d %3d\n",
		(double)SightCycles * 1000 * SecondsPerCycle,
		(double)MaxSightCycles * 1000 * SecondsPerCycle,
		sightcounts[3], sightcounts[0], sightcounts[1], sightcounts[2]);
}

void P_ResetSightCounters (bool full)
{
	if (full)
	{
		MaxSightCycles = 0;
	}
	if (SightCycles > MaxSightCycles)
	{
		MaxSightCycles = SightCycles;
	}
	SightCycles = 0;
	sightcounts[0] = sightcounts[1] = sightcounts[2] = sightcounts[3] = 0;
}
