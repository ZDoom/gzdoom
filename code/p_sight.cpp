// Emacs style mode select	 -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// $Log:$
//
// DESCRIPTION:
//		LineOfSight/Visibility checks, uses REJECT Lookup Table.
//		[RH] Switched over to Hexen's p_sight.c
//
//-----------------------------------------------------------------------------


#include "doomdef.h"

#include "i_system.h"
#include "p_local.h"
#include "m_random.h"
#include "m_bbox.h"

// State.
#include "r_state.h"

/*
==============================================================================

							P_CheckSight

This uses specialized forms of the maputils routines for optimized performance

==============================================================================
*/

fixed_t sightzstart;			// eye z of looker
fixed_t topslope, bottomslope;	// slopes to top and bottom of target

int sightcounts[3];

/*
==============
=
= PTR_SightTraverse
=
==============
*/

BOOL PTR_SightTraverse (intercept_t *in)
{
	line_t  *li;
	fixed_t slope;

	li = in->d.line;

	fixed_t oldbottom, oldtop;

//
// crosses a two sided line
//
	oldbottom = openbottom;
	oldtop = opentop;

	P_LineOpening (li, trace.x + FixedMul (trace.dx, in->frac),
		trace.y + FixedMul (trace.dy, in->frac));

	if (openrange <= 0)		// quick test for totally closed doors
		return false;		// stop

	if (oldbottom < openbottom)
	{
		slope = FixedDiv (openbottom - sightzstart, in->frac);
		if (slope > bottomslope)
			bottomslope = slope;
	}

	if (oldtop > opentop)
	{
		slope = FixedDiv (opentop - sightzstart, in->frac);
		if (slope < topslope)
			topslope = slope;
	}

	if (topslope <= bottomslope)
		return false;		// stop

	return true;			// keep going
}



/*
==================
=
= P_SightBlockLinesIterator
=
===================
*/

BOOL P_SightBlockLinesIterator (int x, int y)
{
	int offset;
	int *list;
	line_t *ld;
	int s1, s2;
	divline_t dl;

	polyblock_t *polyLink;
	seg_t **segList;
	int i;
	extern polyblock_t **PolyBlockMap;

	offset = y*bmapwidth+x;

	polyLink = PolyBlockMap[offset];
	while(polyLink)
	{
		if(polyLink->polyobj)
		{ // only check non-empty links
			if(polyLink->polyobj->validcount != validcount)
			{
				segList = polyLink->polyobj->segs;
				for(i = 0; i < polyLink->polyobj->numsegs; i++, segList++)
				{
					ld = (*segList)->linedef;
					if(ld->validcount == validcount)
					{
						continue;
					}
					ld->validcount = validcount;
					s1 = P_PointOnDivlineSide (ld->v1->x, ld->v1->y, &trace);
					s2 = P_PointOnDivlineSide (ld->v2->x, ld->v2->y, &trace);
					if (s1 == s2)
						continue;		// line isn't crossed
					P_MakeDivline (ld, &dl);
					s1 = P_PointOnDivlineSide (trace.x, trace.y, &dl);
					s2 = P_PointOnDivlineSide (trace.x+trace.dx, trace.y+trace.dy, &dl);
					if (s1 == s2)
						continue;		// line isn't crossed

				// try to early out the check
					if (!ld->backsector)
						return false;	// stop checking

				// store the line for later intersection testing
					intercept_t newintercept;
					newintercept.isaline = true;
					newintercept.d.line = ld;
					intercepts.Push (newintercept);
				}
				polyLink->polyobj->validcount = validcount;
			}
		}
		polyLink = polyLink->next;
	}

	offset = *(blockmap+offset);

	for (list = blockmaplump + offset; *list != -1; list++)
	{
		ld = &lines[*list];
		if (ld->validcount == validcount)
			continue;				// line has already been checked
		ld->validcount = validcount;

		s1 = P_PointOnDivlineSide (ld->v1->x, ld->v1->y, &trace);
		s2 = P_PointOnDivlineSide (ld->v2->x, ld->v2->y, &trace);
		if (s1 == s2)
			continue;				// line isn't crossed
		P_MakeDivline (ld, &dl);
		s1 = P_PointOnDivlineSide (trace.x, trace.y, &dl);
		s2 = P_PointOnDivlineSide (trace.x+trace.dx, trace.y+trace.dy, &dl);
		if (s1 == s2)
			continue;				// line isn't crossed

	// try to early out the check
		if (!ld->backsector)
			return false;	// stop checking

	// store the line for later intersection testing
		intercept_t newintercept;
		newintercept.isaline = true;
		newintercept.d.line = ld;
		intercepts.Push (newintercept);
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

BOOL P_SightTraverseIntercepts ()
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
	openbottom = MININT;
	opentop = MAXINT;

	while (count--)
	{
		dist = MAXINT;
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
		in->frac = MAXINT;
	}

	return true;			// everything was traversed
}



/*
==================
=
= P_SightPathTraverse
=
= Traces a line from x1,y1 to x2,y2, calling the traverser function for each
= Returns true if the traverser function returns true for all lines
==================
*/

BOOL P_SightPathTraverse (fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2)
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

		if (mapxstep == 0 && mapystep == 0)
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
=====================
*/

BOOL P_CheckSight (const AActor *t1, const AActor *t2, BOOL ignoreInvisibility)
{
	const sector_t *s1 = t1->subsector->sector;
	const sector_t *s2 = t2->subsector->sector;
	int pnum = (s1 - sectors) * numsectors + (s2 - sectors);

//
// check for trivial rejection
//
	if (rejectmatrix[pnum>>3] & (1 << (pnum & 7))) {
sightcounts[0]++;
		return false;			// can't possibly be connected
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
			return false;
	}

	// killough 4/19/98: make fake floors and ceilings block monster view

	if ((s1->heightsec  &&
		((t1->z + t1->height <= s1->heightsec->floorplane.ZatPoint (t1->x, t1->y) &&
		  t2->z >= s1->heightsec->floorplane.ZatPoint (t2->x, t2->y)) ||
		 (t1->z >= s1->heightsec->ceilingplane.ZatPoint (t1->x, t1->y) &&
		  t2->z + t1->height <= s1->heightsec->ceilingplane.ZatPoint (t2->x, t2->y))))
		||
		(s2->heightsec &&
		 ((t2->z + t2->height <= s2->heightsec->floorplane.ZatPoint (t2->x, t2->y) &&
		   t1->z >= s2->heightsec->floorplane.ZatPoint (t1->x, t1->y)) ||
		  (t2->z >= s2->heightsec->ceilingplane.ZatPoint (t2->x, t2->y) &&
		   t1->z + t2->height <= s2->heightsec->ceilingplane.ZatPoint (t1->x, t1->y)))))
	{
		return false;
	}

	// killough 11/98: shortcut for melee situations
	if (t1->subsector == t2->subsector)     // same subsector? obviously visible
		return true;

	sightzstart = t1->z + t1->height - (t1->height >> 2);
	bottomslope = (t2->z) - sightzstart;
	topslope = bottomslope + t2->height;

	return P_SightPathTraverse (t1->x, t1->y, t2->x, t2->y);
}
