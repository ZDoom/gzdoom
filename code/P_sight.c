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
//
//-----------------------------------------------------------------------------


#include "doomdef.h"

#include "i_system.h"
#include "p_local.h"
#include "m_random.h"
#include "m_bbox.h"

// State.
#include "r_state.h"

//
// P_CheckSight
//
// killough 4/19/98:
// Convert LOS info to struct for reentrancy and efficiency of data locality

typedef struct {
	fixed_t sightzstart;			// eye z of looker
	fixed_t t2x, t2y;
	divline_t strace;				// from t1 to t2
	fixed_t topslope, bottomslope;	// slopes to top and bottom of target
	fixed_t bbox[4];
} los_t;


//
// P_DivlineSide
// Returns side 0 (front), 1 (back), or 2 (on).
//
static int P_DivlineSide (const fixed_t x, const fixed_t y, const divline_t *node)
{
	if (!node->dx)
	{
		return (x==node->x) ? 2 : ((x < node->x) ? node->dy > 0 : node->dy < 0);
	}
	else if (!node->dy)
	{
		return (y==node->y) ? 2 : ((y < node->y) ? node->dx < 0 : node->dx > 0);
		//	   ^^^^^^^^^^^^
		// [RH] Original code was (x==node->y), but that's obviously not right.
	}
	else
	{
		fixed_t left =	(node->dy>>FRACBITS) * ((x - node->x)>>FRACBITS);
		fixed_t right = ((y - node->y)>>FRACBITS) * (node->dx>>FRACBITS);

		return (right < left) ? 0 : ((left == right) ? 2 : 1);
	}
}


//
// P_InterceptVector2
// Returns the fractional intercept point
// along the first divline.
// This is only called by the addthings and addlines traversers.
//
//
// killough 4/19/98: made static, cleaned up

static fixed_t P_InterceptVector2(const divline_t *v2, const divline_t *v1)
{
	fixed_t den;
	return (den = FixedMul(v1->dy>>8, v2->dx) - FixedMul(v1->dx>>8, v2->dy)) ?
			FixedDiv(FixedMul((v1->x - v2->x)>>8, v1->dy) +
					 FixedMul((v2->y - v1->y)>>8, v1->dx), den) : 0;
}

//
// P_CrossSubsector
// Returns true
//	if strace crosses the given subsector successfully.
//
//
// killough 4/19/98: made static and cleaned up

static BOOL P_CrossSubsector(int num, register los_t *los)
{
	seg_t *seg = segs + subsectors[num].firstline;
	int count;

#ifdef RANGECHECK
	if (num >= numsubsectors)
		I_Error("P_CrossSubsector: ss %i with numss = %i", num, numsubsectors);
#endif

	for (count = subsectors[num].numlines; --count >= 0; seg++)	// check lines
	{
		line_t *line = seg->linedef;
		divline_t divl;
		fixed_t opentop, openbottom;
		const sector_t *front, *back;
		const vertex_t *v1,*v2;
		fixed_t frac;

		// already checked other side?
		if (line->validcount == validcount)
			continue;

		line->validcount = validcount;

		// OPTIMIZE: killough 4/20/98: Added quick bounding-box rejection test

		if (line->bbox[BOXLEFT  ] > los->bbox[BOXRIGHT ] ||
			line->bbox[BOXRIGHT ] < los->bbox[BOXLEFT  ] ||
			line->bbox[BOXBOTTOM] > los->bbox[BOXTOP   ] ||
			line->bbox[BOXTOP]    < los->bbox[BOXBOTTOM])
			continue;

		v1 = line->v1;
		v2 = line->v2;

		// line isn't crossed?
		if (P_DivlineSide(v1->x, v1->y, &los->strace) ==
			P_DivlineSide(v2->x, v2->y, &los->strace))
			continue;

		divl.dx = v2->x - (divl.x = v1->x);
		divl.dy = v2->y - (divl.y = v1->y);

		// line isn't crossed?
		if (P_DivlineSide(los->strace.x, los->strace.y, &divl) ==
			P_DivlineSide(los->t2x, los->t2y, &divl))
			continue;

		// stop because it is not two sided anyway
		if (!(line->flags & ML_TWOSIDED) || (line->flags & ML_BLOCKEVERYTHING))
			return false;

		// crosses a two sided line
		// no wall to block sight with?
		if ((front = seg->frontsector)->floorheight ==
			(back = seg->backsector)->floorheight &&
			front->ceilingheight == back->ceilingheight)
			continue;

		// possible occluder
		// because of ceiling height differences
		opentop = front->ceilingheight < back->ceilingheight ?
				  front->ceilingheight : back->ceilingheight ;

		// because of floor height differences
		openbottom = front->floorheight > back->floorheight ?
					 front->floorheight : back->floorheight ;

		// quick test for totally closed doors
		if (openbottom >= opentop)
			return false;				// stop

		frac = P_InterceptVector2(&los->strace, &divl);

		if (front->floorheight != back->floorheight)
		{
			fixed_t slope = FixedDiv(openbottom - los->sightzstart , frac);
			if (slope > los->bottomslope)
				los->bottomslope = slope;
		}

		if (front->ceilingheight != back->ceilingheight)
		{
			fixed_t slope = FixedDiv(opentop - los->sightzstart , frac);
			if (slope < los->topslope)
				los->topslope = slope;
		}

		if (los->topslope <= los->bottomslope)
			return false;				// stop
	}
	// passed the subsector ok
	return true;
}



//
// P_CrossBSPNode
// Returns true
//	if strace crosses the given node successfully.
//
// killough 4/20/98: rewritten to remove tail recursion, clean up, and optimize

static BOOL P_CrossBSPNode(int bspnum, register los_t *los)
{
	while (!(bspnum & NF_SUBSECTOR))
	{
		register const node_t *bsp = nodes + bspnum;
		int side = P_DivlineSide(los->strace.x,los->strace.y,(divline_t *)bsp)&1;
		if (side == P_DivlineSide(los->t2x, los->t2y, (divline_t *) bsp))
			bspnum = bsp->children[side]; // doesn't touch the other side
		else {				// the partition plane is crossed here
			if (!P_CrossBSPNode(bsp->children[side], los))
				return 0;	// cross the starting side
			else
				bspnum = bsp->children[side^1];	// cross the ending side
		}
	}
	return P_CrossSubsector(bspnum == -1 ? 0 : bspnum & ~NF_SUBSECTOR, los);
}


//
// P_CheckSight
// Returns true
//	if a straight line between t1 and t2 is unobstructed.
// Uses REJECT.
//
// killough 4/20/98: cleaned up, made to use new LOS struct

BOOL P_CheckSight (const mobj_t *t1, const mobj_t *t2, BOOL ignoreInvisibility)
{
	const sector_t *s1 = t1->subsector->sector;
	const sector_t *s2 = t2->subsector->sector;
	int pnum = (s1-sectors)*numsectors + (s2-sectors);
	los_t los;

	// First check for trivial rejection.
	// Determine subsector entries in REJECT table.
	// Check in REJECT table.

	if (rejectmatrix[pnum>>3] & (1 << (pnum&7)))	// can't possibly be connected
		return false;

	// [RH] Andy Baker's stealth monsters
	// Cannot see an invisible object
	if (!ignoreInvisibility && (t2->flags2 & MF2_INVISIBLE)
		&& P_Random (pr_checksight) > 50)	//^^^^^^^^ small chance of an attack being made anyway
		return false;

	// killough 4/19/98: make fake floors and ceilings block monster view

	if ((s1->heightsec != -1 &&
		((t1->z + t1->height <= sectors[s1->heightsec].floorheight &&
		  t2->z >= sectors[s1->heightsec].floorheight) ||
		 (t1->z >= sectors[s1->heightsec].ceilingheight &&
		  t2->z + t1->height <= sectors[s1->heightsec].ceilingheight)))
		||
		(s2->heightsec != -1 &&
		 ((t2->z + t2->height <= sectors[s2->heightsec].floorheight &&
		   t1->z >= sectors[s2->heightsec].floorheight) ||
		  (t2->z >= sectors[s2->heightsec].ceilingheight &&
		   t1->z + t2->height <= sectors[s2->heightsec].ceilingheight))))
		return false;

	// An unobstructed LOS is possible.
	// Now look from eyes of t1 to any part of t2.

	validcount++;
		
	los.topslope = (los.bottomslope = t2->z - (los.sightzstart =
											   t1->z + t1->height -
											   (t1->height>>2))) + t2->height;
	los.strace.dx = (los.t2x = t2->x) - (los.strace.x = t1->x);
	los.strace.dy = (los.t2y = t2->y) - (los.strace.y = t1->y);

	if (t1->x > t2->x)
		los.bbox[BOXRIGHT] = t1->x, los.bbox[BOXLEFT] = t2->x;
	else
		los.bbox[BOXRIGHT] = t2->x, los.bbox[BOXLEFT] = t1->x;

	if (t1->y > t2->y)
		los.bbox[BOXTOP] = t1->y, los.bbox[BOXBOTTOM] = t2->y;
	else
		los.bbox[BOXTOP] = t2->y, los.bbox[BOXBOTTOM] = t1->y;

	// the head node is the last node output
	return P_CrossBSPNode (numnodes-1, &los); 
}
