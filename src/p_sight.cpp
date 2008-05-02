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

#include <assert.h>

#include "doomdef.h"
#include "i_system.h"
#include "p_local.h"
#include "m_random.h"
#include "m_bbox.h"
#include "p_lnspec.h"

// State.
#include "r_state.h"

#include "stats.h"

static FRandom pr_botchecksight ("BotCheckSight");
static FRandom pr_checksight ("CheckSight");

/*
==============================================================================

							P_CheckSight

This uses specialized forms of the maputils routines for optimized performance

==============================================================================
*/

static TArray<intercept_t> intercepts (128);
static divline_t trace;

static fixed_t sightzstart;				// eye z of looker
static fixed_t topslope, bottomslope;	// slopes to top and bottom of target
static int SeePastBlockEverything, SeePastShootableLines;

// Performance meters
static int sightcounts[6];
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
	FLineOpening open;

	li = in->d.line;

//
// crosses a two sided line
//
	P_LineOpening (open, NULL, li, trace.x + FixedMul (trace.dx, in->frac),
		trace.y + FixedMul (trace.dy, in->frac));

	if (open.range <= 0)		// quick test for totally closed doors
		return false;		// stop

	// check bottom
	slope = FixedDiv (open.bottom - sightzstart, in->frac);
	if (slope > bottomslope)
		bottomslope = slope;

	// check top
	slope = FixedDiv (open.top - sightzstart, in->frac);
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
	if (!ld->backsector || !(ld->flags & ML_TWOSIDED))
		return false;	// stop checking

	// [RH] don't see past block everything lines
	if (ld->flags & ML_BLOCKEVERYTHING)
	{
		if (!SeePastBlockEverything)
		{
			return false;
		}
		if (SeePastShootableLines &&
			!(ld->activation & SPAC_Impact) ||
			(ld->special != ACS_Execute && ld->special != ACS_ExecuteAlways) ||
			(ld->args[1] != 0 && ld->args[1] != level.levelnum))
		{
			// Pretend the other side is invisible if this is not an impact line
			// or it does not run a script on the current map. Used to prevent
			// monsters from trying to attack through a block everything line
			// unless there's a chance their attack will make it nonblocking.
			return false;
		}
	}

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

	offset = *(blockmap + offset);

	for (list = blockmaplump + offset + 1; *list != -1; list++)
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
	size_t count;
	fixed_t dist;
	intercept_t *scan, *in;
	unsigned int scanpos;
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
// [RH] Is it really necessary to go through in order? All we care about is if
// the trace is obstructed, not what specifically obstructed it.
//
	in = NULL;

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

		if (in != NULL)
		{
			if (!PTR_SightTraverse (in))
				return false;					// don't bother going farther
			in->frac = FIXED_MAX;
		}
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
	fixed_t partialx, partialy;
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
		partialx = FRACUNIT - ((x1>>MAPBTOFRAC)&(FRACUNIT-1));
		ystep = FixedDiv (y2-y1,abs(x2-x1));
	}
	else if (xt2 < xt1)
	{
		mapxstep = -1;
		partialx = (x1>>MAPBTOFRAC)&(FRACUNIT-1);
		ystep = FixedDiv (y2-y1,abs(x2-x1));
	}
	else
	{
		mapxstep = 0;
		partialx = FRACUNIT;
		ystep = 256*FRACUNIT;
	}
	yintercept = (y1>>MAPBTOFRAC) + FixedMul (partialx, ystep);


	if (yt2 > yt1)
	{
		mapystep = 1;
		partialy = FRACUNIT - ((y1>>MAPBTOFRAC)&(FRACUNIT-1));
		xstep = FixedDiv (x2-x1,abs(y2-y1));
	}
	else if (yt2 < yt1)
	{
		mapystep = -1;
		partialy = (y1>>MAPBTOFRAC)&(FRACUNIT-1);
		xstep = FixedDiv (x2-x1,abs(y2-y1));
	}
	else
	{
		mapystep = 0;
		partialy = FRACUNIT;
		xstep = 256*FRACUNIT;
	}
	xintercept = (x1>>MAPBTOFRAC) + FixedMul (partialy, xstep);

	// [RH] Fix for traces that pass only through blockmap corners. In that case,
	// xintercept and yintercept can both be set ahead of mapx and mapy, so the
	// for loop would never advance anywhere.

	if (abs(xstep) == FRACUNIT && abs(ystep) == FRACUNIT)
	{
		if (ystep < 0)
		{
			partialx = FRACUNIT - partialx;
		}
		if (xstep < 0)
		{
			partialy = FRACUNIT - partialy;
		}
		if (partialx == partialy)
		{
			xintercept = xt1 << FRACBITS;
			yintercept = yt1 << FRACBITS;
		}
	}

//
// step through map blocks
// Count is present to prevent a round off error from skipping the break
	mapx = xt1;
	mapy = yt1;

	for (count = 0 ; count < 100 ; count++)
	{
		if (!P_SightBlockLinesIterator (mapx, mapy))
		{
sightcounts[1]++;
			return false;	// early out
		}

		if ((mapxstep | mapystep) == 0)
			break;

		switch ((((yintercept >> FRACBITS) == mapy) << 1) | ((xintercept >> FRACBITS) == mapx))
		{
		case 0:		// neither xintercept nor yintercept match!
sightcounts[5]++;
			// Continuing won't make things any better, so we might as well stop right here
			count = 100;
			break;

		case 1:		// xintercept matches
			xintercept += xstep;
			mapy += mapystep;
			if (mapy == yt2)
				mapystep = 0;
			break;

		case 2:		// yintercept matches
			yintercept += ystep;
			mapx += mapxstep;
			if (mapx == xt2)
				mapxstep = 0;
			break;

		case 3:		// xintercept and yintercept both match
			sightcounts[4]++;
			// The trace is exiting a block through its corner. Not only does the block
			// being entered need to be checked (which will happen when this loop
			// continues), but the other two blocks adjacent to the corner also need to
			// be checked.
			if (!P_SightBlockLinesIterator (mapx + mapxstep, mapy) ||
				!P_SightBlockLinesIterator (mapx, mapy + mapystep))
			{
sightcounts[1]++;
				return false;
			}
			xintercept += xstep;
			yintercept += ystep;
			mapx += mapxstep;
			mapy += mapystep;
			if (mapx == xt2)
				mapxstep = 0;
			if (mapy == yt2)
				mapystep = 0;
			break;
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

bool P_CheckSight (const AActor *t1, const AActor *t2, int flags)
{
	clock (SightCycles);

	bool res;

#ifdef _DEBUG
	assert (t1 != NULL);
	assert (t2 != NULL);
#else
	if (t1 == NULL || t2 == NULL)
	{
		return false;
	}
#endif

	const sector_t *s1 = t1->Sector;
	const sector_t *s2 = t2->Sector;
	int pnum = int(s1 - sectors) * numsectors + int(s2 - sectors);

//
// check for trivial rejection
//
	if (rejectmatrix != NULL &&
		(rejectmatrix[pnum>>3] & (1 << (pnum & 7))))
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
	if ((flags & 1) == 0 && ((t2->renderflags & RF_INVISIBLE) || !t2->RenderStyle.IsVisible(t2->alpha)))
	{ // small chance of an attack being made anyway
		if ((bglobal.m_Thinking ? pr_botchecksight() : pr_checksight()) > 50)
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

	SeePastBlockEverything = flags & 6;
	SeePastShootableLines = flags & 4;
	res = P_SightPathTraverse (t1->x, t1->y, t2->x, t2->y);
	SeePastBlockEverything = 0;
	SeePastShootableLines = 0;

done:
	unclock (SightCycles);
	return res;
}

ADD_STAT (sight)
{
	FString out;
	out.Format ("%04.1f ms (%04.1f max), %5d %2d%4d%4d%4d%4d\n",
		(double)SightCycles * 1000 * SecondsPerCycle,
		(double)MaxSightCycles * 1000 * SecondsPerCycle,
		sightcounts[3], sightcounts[0], sightcounts[1], sightcounts[2], sightcounts[4], sightcounts[5]);
	return out;
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
	memset (sightcounts, 0, sizeof(sightcounts));
}
