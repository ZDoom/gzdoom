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
//		Movement/collision utility functions,
//		as used by function in p_map.c. 
//		BLOCKMAP Iterator functions,
//		and some PIT_* functions to use for iteration.
//
//-----------------------------------------------------------------------------



#include <stdlib.h>


#include "m_bbox.h"

#include "doomdef.h"
#include "doomstat.h"
#include "p_local.h"


// State.
#include "r_state.h"

//
// P_AproxDistance
// Gives an estimation of distance (not exact)
//

fixed_t P_AproxDistance (fixed_t dx, fixed_t dy)
{
	dx = abs(dx);
	dy = abs(dy);
	if (dx < dy)
		return dx+dy-(dx>>1);
	return dx+dy-(dy>>1);
}


//
// P_PointOnLineSide
// Returns 0 (front) or 1 (back)
//
int P_PointOnLineSide (fixed_t x, fixed_t y, const line_t *line)
{
	if (!line->dx)
	{
		return (x <= line->v1->x) ? (line->dy > 0) : (line->dy < 0);
	}
	else if (!line->dy)
	{
		return (y <= line->v1->y) ? (line->dx < 0) : (line->dx > 0);
	}
	else
	{
		return FixedMul (line->dy >> FRACBITS, x - line->v1->x)
			   <= FixedMul (y - line->v1->y , line->dx >> FRACBITS);
	}
}



//
// P_BoxOnLineSide
// Considers the line to be infinite
// Returns side 0 or 1, -1 if box crosses the line.
//
int P_BoxOnLineSide (const fixed_t *tmbox, const line_t *ld)
{
	int p1;
	int p2;
		
	switch (ld->slopetype)
	{
	  case ST_HORIZONTAL:
		p1 = tmbox[BOXTOP] > ld->v1->y;
		p2 = tmbox[BOXBOTTOM] > ld->v1->y;
		if (ld->dx < 0)
		{
			p1 ^= 1;
			p2 ^= 1;
		}
		break;
		
	  case ST_VERTICAL:
		p1 = tmbox[BOXRIGHT] < ld->v1->x;
		p2 = tmbox[BOXLEFT] < ld->v1->x;
		if (ld->dy < 0)
		{
			p1 ^= 1;
			p2 ^= 1;
		}
		break;
		
	  case ST_POSITIVE:
		p1 = P_PointOnLineSide (tmbox[BOXLEFT], tmbox[BOXTOP], ld);
		p2 = P_PointOnLineSide (tmbox[BOXRIGHT], tmbox[BOXBOTTOM], ld);
		break;
		
	  case ST_NEGATIVE:
		p1 = P_PointOnLineSide (tmbox[BOXRIGHT], tmbox[BOXTOP], ld);
		p2 = P_PointOnLineSide (tmbox[BOXLEFT], tmbox[BOXBOTTOM], ld);
		break;
	}

	return (p1 == p2) ? p1 : -1;
}


//
// P_PointOnDivlineSide
// Returns 0 (front) or 1 (back).
//
int P_PointOnDivlineSide (fixed_t x, fixed_t y, const divline_t *line)
{
	if (!line->dx)
	{
		return (x <= line->x) ? (line->dy > 0) : (line->dy < 0);
	}
	else if (!line->dy)
	{
		return (y <= line->y) ? (line->dx < 0) : (line->dx > 0);
	}
	else
	{
		fixed_t dx = (x - line->x);
		fixed_t dy = (y - line->y);
		
		// try to quickly decide by looking at sign bits
		if ((line->dy ^ line->dx ^ dx ^ dy) & 0x80000000)
		{	// (left is negative)
			return ((line->dy ^ dx) & 0x80000000) ? 1 : 0;
		}
		else
		{	// if (left >= right), return 1, 0 otherwise
			return FixedMul (dy >> 8, line->dx >> 8) >= FixedMul (line->dy >> 8, dx >> 8);
		}
	}
}



//
// P_MakeDivline
//
void P_MakeDivline (const line_t *li, divline_t *dl)
{
	dl->x = li->v1->x;
	dl->y = li->v1->y;
	dl->dx = li->dx;
	dl->dy = li->dy;
}



//
// P_InterceptVector
// Returns the fractional intercept point along the first divline.
// This is only called by the addthings and addlines traversers.
//
fixed_t P_InterceptVector (const divline_t *v2, const divline_t *v1)
{
#if 1
	fixed_t 	frac;
	fixed_t 	num;
	fixed_t 	den;
		
	den = FixedMul (v1->dy>>8,v2->dx) - FixedMul(v1->dx>>8,v2->dy);

	if (den == 0)
		return 0;
	//	I_Error ("P_InterceptVector: parallel");
	
	num =
		FixedMul ( (v1->x - v2->x)>>8 ,v1->dy )
		+FixedMul ( (v2->y - v1->y)>>8, v1->dx );

	frac = FixedDiv (num , den);

	return frac;
#else	// UNUSED, float debug.
	float frac;
	float num;
	float den;
	float v1x = (float)v1->x/FRACUNIT;
	float v1y = (float)v1->y/FRACUNIT;
	float v1dx = (float)v1->dx/FRACUNIT;
	float v1dy = (float)v1->dy/FRACUNIT;
	float v2x = (float)v2->x/FRACUNIT;
	float v2y = (float)v2->y/FRACUNIT;
	float v2dx = (float)v2->dx/FRACUNIT;
	float v2dy = (float)v2->dy/FRACUNIT;
		
	den = v1dy*v2dx - v1dx*v2dy;

	if (den == 0)
		return 0;		// parallel
	
	num = (v1x - v2x)*v1dy + (v2y - v1y)*v1dx;
	frac = num / den;

	return frac*FRACUNIT;
#endif
}


//
// P_LineOpening
// Sets opentop and openbottom to the window
// through a two sided line.
// OPTIMIZE: keep this precalculated
//
fixed_t opentop;
fixed_t openbottom;
fixed_t openrange;
fixed_t lowfloor;

void P_LineOpening (const line_t *linedef)
{
	sector_t *front, *back;

	if (linedef->sidenum[1] == -1)
	{
		// single sided line
		openrange = 0;
		return;
	}

	front = linedef->frontsector;
	back = linedef->backsector;

	opentop = (front->ceilingheight < back->ceilingheight) ?
		opentop = front->ceilingheight :
		back->ceilingheight;

	if (front->floorheight > back->floorheight)
	{
		openbottom = front->floorheight;
		lowfloor = back->floorheight;
	}
	else
	{
		openbottom = back->floorheight;
		lowfloor = front->floorheight;
	}

	openrange = opentop - openbottom;
}


//
// THING POSITION SETTING
//


//
// P_UnsetThingPosition
// Unlinks a thing from block map and sectors.
// On each position change, BLOCKMAP and other
// lookups maintaining lists of things inside
// these structures need to be updated.
//
void P_UnsetThingPosition (mobj_t *thing)
{
	if (!(thing->flags & MF_NOSECTOR))
	{
		// invisible things don't need to be in sector list
		// unlink from subsector

		// killough 8/11/98: simpler scheme using pointers-to-pointers for prev
		// pointers, allows head node pointers to be treated like everything else
		mobj_t **sprev = thing->sprev;
		mobj_t  *snext = thing->snext;
		if ((*sprev = snext))  // unlink from sector list
			snext->sprev = sprev;

		// phares 3/14/98
		//
		// Save the sector list pointed to by touching_sectorlist.
		// In P_SetThingPosition, we'll keep any nodes that represent
		// sectors the Thing still touches. We'll add new ones then, and
		// delete any nodes for sectors the Thing has vacated. Then we'll
		// put it back into touching_sectorlist. It's done this way to
		// avoid a lot of deleting/creating for nodes, when most of the
		// time you just get back what you deleted anyway.
		//
		// If this Thing is being removed entirely, then the calling
		// routine will clear out the nodes in sector_list.

		sector_list = thing->touching_sectorlist;
		thing->touching_sectorlist = NULL; //to be restored by P_SetThingPosition
	}
		
	if ( !(thing->flags & MF_NOBLOCKMAP) )
	{
		// killough 8/11/98: simpler scheme using pointers-to-pointers for prev
		// pointers, allows head node pointers to be treated like everything else
		//
		// Also more robust, since it doesn't depend on current position for
		// unlinking. Old method required computing head node based on position
		// at time of unlinking, assuming it was the same position as during
		// linking.

		mobj_t *bnext, **bprev = thing->bprev;
		if (bprev && (*bprev = bnext = thing->bnext))	// unlink from block map
			bnext->bprev = bprev;
	}
}


//
// P_SetThingPosition
// Links a thing into both a block and a subsector based on it's x y.
// Sets thing->subsector properly
//
void P_SetThingPosition (mobj_t *thing)
{
	// link into subsector
	subsector_t *ss;
	
	ss = R_PointInSubsector (thing->x,thing->y);
	thing->subsector = ss;
	
	if ( !(thing->flags & MF_NOSECTOR) )
	{
		// invisible things don't go into the sector links
		// killough 8/11/98: simpler scheme using pointer-to-pointer prev
		// pointers, allows head nodes to be treated like everything else

		mobj_t **link = &ss->sector->thinglist;
		mobj_t *snext = *link;
		if ((thing->snext = snext))
			snext->sprev = &thing->snext;
		thing->sprev = link;
		*link = thing;

		// phares 3/16/98
		//
		// If sector_list isn't NULL, it has a collection of sector
		// nodes that were just removed from this Thing.

		// Collect the sectors the object will live in by looking at
		// the existing sector_list and adding new nodes and deleting
		// obsolete ones.

		// When a node is deleted, its sector links (the links starting
		// at sector_t->touching_thinglist) are broken. When a node is
		// added, new sector links are created.

		P_CreateSecNodeList (thing, thing->x, thing->y);
		thing->touching_sectorlist = sector_list;	// Attach to Thing's mobj_t
		sector_list = NULL;		// clear for next time
    }

	
	// link into blockmap
	if ( !(thing->flags & MF_NOBLOCKMAP) )
	{
		// inert things don't need to be in blockmap
		int blockx = (thing->x - bmaporgx)>>MAPBLOCKSHIFT;
		int blocky = (thing->y - bmaporgy)>>MAPBLOCKSHIFT;

		if (blockx>=0 && blockx < bmapwidth && blocky>=0 && blocky < bmapheight)
        {
			// killough 8/11/98: simpler scheme using pointer-to-pointer prev
			// pointers, allows head nodes to be treated like everything else

			mobj_t **link = &blocklinks[blocky*bmapwidth+blockx];
			mobj_t *bnext = *link;

			if ((thing->bnext = bnext))
				bnext->bprev = &thing->bnext;
			thing->bprev = link;
			*link = thing;
		}
		else		// thing is off the map
			thing->bnext = NULL, thing->bprev = NULL;
	}
}



//
// BLOCK MAP ITERATORS
// For each line/thing in the given mapblock,
// call the passed PIT_* function.
// If the function returns false,
// exit with false without checking anything else.
//


//
// P_BlockLinesIterator
// The validcount flags are used to avoid checking lines
// that are marked in multiple mapblocks,
// so increment validcount before the first call
// to P_BlockLinesIterator, then make one or more calls
// to it.
//
extern polyblock_t **PolyBlockMap;

BOOL P_BlockLinesIterator (int x, int y, BOOL(*func)(line_t*))
{
	if (x<0 || y<0 || x>=bmapwidth || y>=bmapheight)
		return true;
	else
	{
		int	offset;
		int *list;

		/* [RH] Polyobj stuff from Hexen --> */
		polyblock_t *polyLink;

		offset = y*bmapwidth + x;
		if (PolyBlockMap) {
			polyLink = PolyBlockMap[offset];
			while (polyLink) {
				if (polyLink->polyobj && polyLink->polyobj->validcount != validcount) {
					int i;
					seg_t **tempSeg = polyLink->polyobj->segs;
					polyLink->polyobj->validcount = validcount;

					for (i = polyLink->polyobj->numsegs; i; i--, tempSeg++) {
						if ((*tempSeg)->linedef->validcount != validcount) {
							(*tempSeg)->linedef->validcount = validcount;
							if (!func ((*tempSeg)->linedef))
								return false;
						}
					}
				}
				polyLink = polyLink->next;
			}
		}
		/* <-- Polyobj stuff from Hexen */

		offset = *(blockmap + offset);
		list = blockmaplump + offset;

		// [RH] Get past starting 0 (from BOOM)
		list++;

		for (; *list != -1; list++)
		{
			line_t *ld = &lines[*list];

			if (ld->validcount != validcount) {
				ld->validcount = validcount;
					
				if ( !func(ld) )
					return false;
			}
		}
	}
	return true;		// everything was checked
}


//
// P_BlockThingsIterator
//
BOOL P_BlockThingsIterator (int x, int y, BOOL(*func)(mobj_t*))
{
	if (x<0 || y<0 || x>=bmapwidth || y>=bmapheight)
		return true;
	else
	{
		mobj_t *mobj;
			
		for (mobj = blocklinks[y*bmapwidth+x] ;
			 mobj ;
			 mobj = mobj->bnext)
		{
			if (!func (mobj))
				return false;
		}
	}
	return true;
}



//
// INTERCEPT ROUTINES
//
intercept_t 	intercepts[MAXINTERCEPTS];
intercept_t*	intercept_p;

divline_t		trace;
BOOL 			earlyout;
int 			ptflags;

//
// PIT_AddLineIntercepts.
// Looks for lines in the given block
// that intercept the given trace
// to add to the intercepts list.
//
// A line is crossed if its endpoints
// are on opposite sides of the trace.
// Returns true if earlyout and a solid line hit.
//
BOOL PIT_AddLineIntercepts (line_t *ld)
{
	int 				s1;
	int 				s2;
	fixed_t 			frac;
	divline_t			dl;
		
	// avoid precision problems with two routines
	if ( trace.dx > FRACUNIT*16
		 || trace.dy > FRACUNIT*16
		 || trace.dx < -FRACUNIT*16
		 || trace.dy < -FRACUNIT*16)
	{
		s1 = P_PointOnDivlineSide (ld->v1->x, ld->v1->y, &trace);
		s2 = P_PointOnDivlineSide (ld->v2->x, ld->v2->y, &trace);
	}
	else
	{
		s1 = P_PointOnLineSide (trace.x, trace.y, ld);
		s2 = P_PointOnLineSide (trace.x+trace.dx, trace.y+trace.dy, ld);
	}
	
	if (s1 == s2)
		return true;	// line isn't crossed
	
	// hit the line
	P_MakeDivline (ld, &dl);
	frac = P_InterceptVector (&trace, &dl);

	if (frac < 0)
		return true;	// behind source
		
	// try to early out the check
	if (earlyout
		&& frac < FRACUNIT
		&& !ld->backsector)
	{
		return false;	// stop checking
	}
	
		
	intercept_p->frac = frac;
	intercept_p->isaline = true;
	intercept_p->d.line = ld;
	intercept_p++;

	return true;		// continue
}



//
// PIT_AddThingIntercepts
//
BOOL PIT_AddThingIntercepts (mobj_t* thing)
{
	fixed_t 		x1;
	fixed_t 		y1;
	fixed_t 		x2;
	fixed_t 		y2;
	
	int 			s1;
	int 			s2;
	
	BOOL 			tracepositive;

	divline_t		dl;
	
	fixed_t 		frac;
		
	tracepositive = (trace.dx ^ trace.dy)>0;
				
	// check a corner to corner crossection for hit
	if (tracepositive)
	{
		x1 = thing->x - thing->radius;
		y1 = thing->y + thing->radius;
				
		x2 = thing->x + thing->radius;
		y2 = thing->y - thing->radius;					
	}
	else
	{
		x1 = thing->x - thing->radius;
		y1 = thing->y - thing->radius;
				
		x2 = thing->x + thing->radius;
		y2 = thing->y + thing->radius;					
	}
	
	s1 = P_PointOnDivlineSide (x1, y1, &trace);
	s2 = P_PointOnDivlineSide (x2, y2, &trace);

	if (s1 == s2)
		return true;			// line isn't crossed
		
	dl.x = x1;
	dl.y = y1;
	dl.dx = x2-x1;
	dl.dy = y2-y1;
	
	frac = P_InterceptVector (&trace, &dl);

	if (frac < 0)
		return true;			// behind source

	intercept_p->frac = frac;
	intercept_p->isaline = false;
	intercept_p->d.thing = thing;
	intercept_p++;

	return true;				// keep going
}


//
// P_TraverseIntercepts
// Returns true if the traverser function returns true
// for all lines.
// 
BOOL P_TraverseIntercepts (traverser_t func, fixed_t maxfrac)
{
	int 				count;
	fixed_t 			dist;
	intercept_t*		scan;
	intercept_t*		in = 0;

	count = intercept_p - intercepts;

	while (count--)
	{
		dist = MAXINT;
		for (scan = intercepts ; scan<intercept_p ; scan++)
		{
			if (scan->frac < dist)
			{
				dist = scan->frac;
				in = scan;
			}
		}
		
		if (dist > maxfrac)
			return true;		// checked everything in range			

#if 0  // UNUSED
	{

		// don't check these yet, there may be others inserted
		in = scan = intercepts;
		for ( scan = intercepts ; scan<intercept_p ; scan++)
			if (scan->frac > maxfrac)
				*in++ = *scan;
		intercept_p = in;
		return false;
	}
#endif

		if ( !func (in) )
			return false;		// don't bother going farther

		in->frac = MAXINT;
	}
		
	return true;				// everything was traversed
}




//
// P_PathTraverse
// Traces a line from x1,y1 to x2,y2,
// calling the traverser function for each.
// Returns true if the traverser function returns true
// for all lines.
//
BOOL P_PathTraverse (fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2, int flags, BOOL (*trav) (intercept_t *))
{
	fixed_t 	xt1;
	fixed_t 	yt1;
	fixed_t 	xt2;
	fixed_t 	yt2;
	
	fixed_t 	xstep;
	fixed_t 	ystep;
	
	fixed_t 	partial;
	
	fixed_t 	xintercept;
	fixed_t 	yintercept;
	
	int 		mapx;
	int 		mapy;
	
	int 		mapxstep;
	int 		mapystep;

	int 		count;
				
	earlyout = flags & PT_EARLYOUT;
				
	validcount++;
	intercept_p = intercepts;
		
	if ( ((x1-bmaporgx)&(MAPBLOCKSIZE-1)) == 0)
		x1 += FRACUNIT; // don't side exactly on a line
	
	if ( ((y1-bmaporgy)&(MAPBLOCKSIZE-1)) == 0)
		y1 += FRACUNIT; // don't side exactly on a line

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
	
	// Step through map blocks.
	// Count is present to prevent a round off error
	// from skipping the break.
	mapx = xt1;
	mapy = yt1;
		
	for (count = 0 ; count < 64 ; count++)
	{
		if (flags & PT_ADDLINES)
		{
			if (!P_BlockLinesIterator (mapx, mapy,PIT_AddLineIntercepts))
				return false;	// early out
		}
		
		if (flags & PT_ADDTHINGS)
		{
			if (!P_BlockThingsIterator (mapx, mapy,PIT_AddThingIntercepts))
				return false;	// early out
		}
				
		if (mapx == xt2 && mapy == yt2)
		{
			break;
		}
		
		if ( (yintercept >> FRACBITS) == mapy)
		{
			yintercept += ystep;
			mapx += mapxstep;
		}
		else if ( (xintercept >> FRACBITS) == mapx)
		{
			xintercept += xstep;
			mapy += mapystep;
		}
				
	}
	// go through the sorted list
	return P_TraverseIntercepts ( trav, FRACUNIT );
}