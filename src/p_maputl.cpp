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
#include "templates.h"

static AActor *RoughBlockCheck (AActor *mo, int index);


//==========================================================================
//
// P_AproxDistance
//
// Gives an estimation of distance (not exact)
// 
//==========================================================================

fixed_t P_AproxDistance (fixed_t dx, fixed_t dy)
{
	dx = abs(dx);
	dy = abs(dy);
	return (dx < dy) ? dx+dy-(dx>>1) : dx+dy-(dy>>1);
}

//==========================================================================
//
// P_BoxOnLineSide
//
// Considers the line to be infinite
// Returns side 0 or 1, -1 if box crosses the line.
//
//==========================================================================

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
	default:	// Just to assure GCC that p1 and p2 really do get initialized
		p1 = P_PointOnLineSide (tmbox[BOXRIGHT], tmbox[BOXTOP], ld);
		p2 = P_PointOnLineSide (tmbox[BOXLEFT], tmbox[BOXBOTTOM], ld);
		break;
	}

	return (p1 == p2) ? p1 : -1;
}

//==========================================================================
//
// P_InterceptVector
//
// Returns the fractional intercept point along the first divline.
// This is only called by the addthings and addlines traversers.
//
//==========================================================================

fixed_t P_InterceptVector (const divline_t *v2, const divline_t *v1)
{
#if 1	// [RH] Use 64 bit ints, so long divlines don't overflow

	SQWORD den = ((SQWORD)v1->dy*v2->dx - (SQWORD)v1->dx*v2->dy) >> FRACBITS;
	if (den == 0)
		return 0;		// parallel
	SQWORD num = ((SQWORD)(v1->x - v2->x)*v1->dy + (SQWORD)(v2->y - v1->y)*v1->dx);
	return (fixed_t)(num / den);

#elif 1	// This is the original Doom version

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

//==========================================================================
//
// P_LineOpening
//
// Sets opentop and openbottom to the window
// through a two sided line.
// OPTIMIZE: keep this precalculated
//
//==========================================================================

fixed_t opentop;
fixed_t openbottom;
fixed_t openrange;
fixed_t lowfloor;
extern int tmfloorpic;
sector_t *openbottomsec;

void P_LineOpening (const line_t *linedef, fixed_t x, fixed_t y, fixed_t refx, fixed_t refy)
{
	sector_t *front, *back;
	fixed_t fc, ff, bc, bf;

	if (linedef->sidenum[1] == NO_INDEX)
	{
		// single sided line
		openrange = 0;
		return;
	}

	front = linedef->frontsector;
	back = linedef->backsector;

	fc = front->ceilingplane.ZatPoint (x, y);
	ff = front->floorplane.ZatPoint (x, y);
	bc = back->ceilingplane.ZatPoint (x, y);
	bf = back->floorplane.ZatPoint (x, y);

	opentop = fc < bc ? fc : bc;

	bool usefront;

	// [RH] fudge a bit for actors that are moving across lines
	// bordering a slope/non-slope that meet on the floor. Note
	// that imprecisions in the plane equation mean there is a
	// good chance that even if a slope and non-slope look like
	// they line up, they won't be perfectly aligned.
	if (refx == FIXED_MIN ||
		abs (ff-bf) > 256)
	{
		usefront = (ff > bf);
	}
	else
	{
		if ((front->floorplane.a | front->floorplane.b) == 0)
			usefront = true;
		else if ((back->floorplane.a | front->floorplane.b) == 0)
			usefront = false;
		else
			usefront = !P_PointOnLineSide (refx, refy, linedef);
	}

	if (usefront)
	{
		openbottom = ff;
		openbottomsec = front;
		lowfloor = bf;
		tmfloorpic = front->floorpic;
	}
	else
	{
		openbottom = bf;
		openbottomsec = back;
		lowfloor = ff;
		tmfloorpic = back->floorpic;
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
void AActor::UnlinkFromWorld ()
{
	sector_list = NULL;
	if (!(flags & MF_NOSECTOR))
	{
		// invisible things don't need to be in sector list
		// unlink from subsector

		// killough 8/11/98: simpler scheme using pointers-to-pointers for prev
		// pointers, allows head node pointers to be treated like everything else
		AActor **prev = sprev;
		AActor  *next = snext;
		if ((*prev = next))  // unlink from sector list
			next->sprev = prev;
		snext = NULL;
		sprev = NULL;

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

		sector_list = touching_sectorlist;
		touching_sectorlist = NULL; //to be restored by P_SetThingPosition
	}
		
	if (!(flags & MF_NOBLOCKMAP))
	{
		// killough 8/11/98: simpler scheme using pointers-to-pointers for prev
		// pointers, allows head node pointers to be treated like everything else
		//
		// Also more robust, since it doesn't depend on current position for
		// unlinking. Old method required computing head node based on position
		// at time of unlinking, assuming it was the same position as during
		// linking.

		AActor *_bnext, **_bprev = bprev;
		if (_bprev && (*_bprev = _bnext = bnext))
		{ // unlink from block map
			_bnext->bprev = _bprev;
		}
		bnext = NULL;
		bprev = NULL;
	}
}


//
// P_SetThingPosition
// Links a thing into both a block and a subsector based on it's x y.
// Sets thing->sector properly
//
void AActor::LinkToWorld ()
{
	// link into subsector
	sector_t *sec = R_PointInSubsector (x, y)->sector;
	Sector = sec;

	if ( !(flags & MF_NOSECTOR) )
	{
		// invisible things don't go into the sector links
		// killough 8/11/98: simpler scheme using pointer-to-pointer prev
		// pointers, allows head nodes to be treated like everything else

		AActor **link = &sec->thinglist;
		AActor *next = *link;
		if ((snext = next))
			next->sprev = &snext;
		sprev = link;
		*link = this;

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
		P_CreateSecNodeList (this, x, y);
		touching_sectorlist = sector_list;	// Attach to thing
		sector_list = NULL;		// clear for next time
    }

	
	// link into blockmap
	if ( !(flags & MF_NOBLOCKMAP) )
	{
		// inert things don't need to be in blockmap
		int blockx = (x - bmaporgx)>>MAPBLOCKSHIFT;
		int blocky = (y - bmaporgy)>>MAPBLOCKSHIFT;

		if (blockx >= 0 && blockx < bmapwidth && blocky >= 0 && blocky < bmapheight)
        {
			// killough 8/11/98: simpler scheme using pointer-to-pointer prev
			// pointers, allows head nodes to be treated like everything else

			AActor **link = &blocklinks[blocky*bmapwidth+blockx];
			AActor *_bnext = *link;

			if ((bnext = _bnext))
				_bnext->bprev = &bnext;
			bprev = link;
			*link = this;
		}
		else
		{ // thing is off the map
			bnext = NULL, bprev = NULL;
		}
	}
}

void AActor::SetOrigin (fixed_t ix, fixed_t iy, fixed_t iz)
{
	UnlinkFromWorld ();
	x = ix;
	y = iy;
	z = iz;
	LinkToWorld ();
	floorz = Sector->floorplane.ZatPoint (ix, iy);
	ceilingz = Sector->ceilingplane.ZatPoint (ix, iy);
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
	{
		return true;
	}
	else
	{
		int	offset;
		int *list;

		/* [RH] Polyobj stuff from Hexen --> */
		polyblock_t *polyLink;

		offset = y*bmapwidth + x;
		if (PolyBlockMap)
		{
			polyLink = PolyBlockMap[offset];
			while (polyLink)
			{
				if (polyLink->polyobj && polyLink->polyobj->validcount != validcount)
				{
					int i;
					seg_t **tempSeg = polyLink->polyobj->segs;
					polyLink->polyobj->validcount = validcount;

					for (i = polyLink->polyobj->numsegs; i; i--, tempSeg++)
					{
						if ((*tempSeg)->linedef->validcount != validcount)
						{
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

			if (ld->validcount != validcount)
			{
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
BOOL P_BlockThingsIterator (int x, int y, BOOL(*func)(AActor*), AActor *actor)
{
	if (x<0 || y<0 || x>=bmapwidth || y>=bmapheight)
	{
		return true;
	}
	else
	{
		if (actor == NULL)
		{
			actor = blocklinks[y*bmapwidth+x];
		}
		while (actor != NULL)
		{
			AActor *next = actor->bnext;
			if (!func (actor))
				return false;
			actor = next;
		}
	}
	return true;
}



//
// INTERCEPT ROUTINES
//
TArray<intercept_t> intercepts (128);

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
	

	intercept_t newintercept;

	newintercept.frac = frac;
	newintercept.isaline = true;
	newintercept.d.line = ld;
	intercepts.Push (newintercept);

	return true;		// continue
}



//
// PIT_AddThingIntercepts
//
BOOL PIT_AddThingIntercepts (AActor* thing)
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

	intercept_t newintercept;
	newintercept.frac = frac;
	newintercept.isaline = false;
	newintercept.d.thing = thing;
	intercepts.Push (newintercept);

	return true;				// keep going
}


//
// P_TraverseIntercepts
// Returns true if the traverser function returns true
// for all lines.
// 
BOOL P_TraverseIntercepts (traverser_t func, fixed_t maxfrac)
{
	size_t 		 count;
	fixed_t 	 dist;
	size_t		 scanpos;
	intercept_t *scan;
	intercept_t *in = NULL;

	count = intercepts.Size ();

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
		
		if (dist > maxfrac || in == NULL)
			return true;		// checked everything in range			

		if (!func (in))
			return false;		// don't bother going farther

		in->frac = FIXED_MAX;
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
	intercepts.Clear ();
		
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
			if (!P_BlockLinesIterator (mapx, mapy, PIT_AddLineIntercepts))
				return false;	// early out
		}
		
		if (flags & PT_ADDTHINGS)
		{
			if (!P_BlockThingsIterator (mapx, mapy, PIT_AddThingIntercepts))
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

//===========================================================================
//
// P_RoughMonsterSearch
//
// Searches though the surrounding mapblocks for monsters/players
//		distance is in MAPBLOCKUNITS
//===========================================================================

AActor *P_RoughMonsterSearch (AActor *mo, int distance)
{
	int blockX;
	int blockY;
	int startX, startY;
	int blockIndex;
	int firstStop;
	int secondStop;
	int thirdStop;
	int finalStop;
	int count;
	AActor *target;

	startX = (mo->x-bmaporgx)>>MAPBLOCKSHIFT;
	startY = (mo->y-bmaporgy)>>MAPBLOCKSHIFT;
	
	if (startX >= 0 && startX < bmapwidth && startY >= 0 && startY < bmapheight)
	{
		if ( (target = RoughBlockCheck (mo, startY*bmapwidth+startX)) )
		{ // found a target right away
			return target;
		}
	}
	for (count = 1; count <= distance; count++)
	{
		blockX = clamp (startX-count, 0, bmapwidth-1);
		blockY = clamp (startY-count, 0, bmapheight-1);

		blockIndex = blockY*bmapwidth+blockX;
		firstStop = startX+count;
		if (firstStop < 0)
		{
			continue;
		}
		if (firstStop >= bmapwidth)
		{
			firstStop = bmapwidth-1;
		}
		secondStop = startY+count;
		if (secondStop < 0)
		{
			continue;
		}
		if (secondStop >= bmapheight)
		{
			secondStop = bmapheight-1;
		}
		thirdStop = secondStop*bmapwidth+blockX;
		secondStop = secondStop*bmapwidth+firstStop;
		firstStop += blockY*bmapwidth;
		finalStop = blockIndex;		

		// Trace the first block section (along the top)
		for (; blockIndex <= firstStop; blockIndex++)
		{
			if ( (target = RoughBlockCheck (mo, blockIndex)) )
			{
				return target;
			}
		}
		// Trace the second block section (right edge)
		for (blockIndex--; blockIndex <= secondStop; blockIndex += bmapwidth)
		{
			if ( (target = RoughBlockCheck(mo, blockIndex)) )
			{
				return target;
			}
		}		
		// Trace the third block section (bottom edge)
		for (blockIndex -= bmapwidth; blockIndex >= thirdStop; blockIndex--)
		{
			if ( (target = RoughBlockCheck(mo, blockIndex)) )
			{
				return target;
			}
		}
		// Trace the final block section (left edge)
		for (blockIndex++; blockIndex > finalStop; blockIndex -= bmapwidth)
		{
			if ( (target = RoughBlockCheck (mo, blockIndex)) )
			{
				return target;
			}
		}
	}
	return NULL;	
}

//===========================================================================
//
// RoughBlockCheck
//
//===========================================================================

static AActor *RoughBlockCheck (AActor *mo, int index)
{
	AActor *link;

	link = blocklinks[index];
	for (link = blocklinks[index]; link != NULL; link = link->bnext)
	{
		if (link != mo && mo->IsOkayToAttack (link))
		{
			return link;
		}
	}
/*
		else if (mo->type == MT_MSTAFF_FX2)		// bloodscourge
		{
			if ((link->flags&MF_COUNTKILL ||
				(link->player && link != mo->target))
				&& !(link->flags2&MF2_DORMANT))
			{
				if (!(link->flags&MF_SHOOTABLE))
				{
					link = link->bnext;
					continue;
				}
				if(netgame && !deathmatch && link->player)
				{
					link = link->bnext;
					continue;
				}
				else if (P_CheckSight (mo, link))
				{
					master = mo->target;
					angle = R_PointToAngle2(master->x, master->y,
									link->x, link->y) - master->angle;
					angle >>= 24;
					if (angle>226 || angle<30)
					{
						return link;
					}
				}
			}
			link = link->bnext;
		}*/
	return NULL;
}
