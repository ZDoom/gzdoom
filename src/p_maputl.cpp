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
#include "p_3dmidtex.h"

// State.
#include "r_state.h"
#include "templates.h"
#include "po_man.h"

static AActor *RoughBlockCheck (AActor *mo, int index, void *);


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
// P_InterceptVector
//
// Returns the fractional intercept point along the first divline.
//
//==========================================================================

fixed_t P_InterceptVector (const divline_t *v2, const divline_t *v1)
{
#if 0	// [RH] Use 64 bit ints, so long divlines don't overflow

	SQWORD den = ( ((SQWORD)v1->dy*v2->dx - (SQWORD)v1->dx*v2->dy) >> FRACBITS );
	if (den == 0)
		return 0;		// parallel
	SQWORD num = ((SQWORD)(v1->x - v2->x)*v1->dy + (SQWORD)(v2->y - v1->y)*v1->dx);
	return (fixed_t)(num / den);

#elif 0	// This is the original Doom version

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

#else	// optimized version of the float debug version. A lot faster on modern systens.


	double frac;
	double num;
	double den;

	// There's no need to divide by FRACUNIT here.
	// At the end both num and den will contain a factor 
	// 1/(FRACUNIT*FRACUNIT) so they'll cancel each other out.
	double v1x = (double)v1->x;
	double v1y = (double)v1->y;
	double v1dx = (double)v1->dx;
	double v1dy = (double)v1->dy;
	double v2x = (double)v2->x;
	double v2y = (double)v2->y;
	double v2dx = (double)v2->dx;
	double v2dy = (double)v2->dy;
		
	den = v1dy*v2dx - v1dx*v2dy;

	if (den == 0)
		return 0;		// parallel
	
	num = (v1x - v2x)*v1dy + (v2y - v1y)*v1dx;
	frac = num / den;

	return FLOAT2FIXED(frac);
#endif
}


//==========================================================================
//
// P_LineOpening
//
// Sets opentop and openbottom to the window
// through a two sided line.
//
//==========================================================================

void P_LineOpening (FLineOpening &open, AActor *actor, const line_t *linedef, 
					fixed_t x, fixed_t y, fixed_t refx, fixed_t refy, int flags)
{
	if (!(flags & FFCF_ONLY3DFLOORS))
	{
		sector_t *front, *back;
		fixed_t fc, ff, bc, bf;

		if (linedef->sidedef[1] == NULL)
		{
			// single sided line
			open.range = 0;
			return;
		}

		front = linedef->frontsector;
		back = linedef->backsector;

		fc = front->ceilingplane.ZatPoint (x, y);
		ff = front->floorplane.ZatPoint (x, y);
		bc = back->ceilingplane.ZatPoint (x, y);
		bf = back->floorplane.ZatPoint (x, y);

		/*Printf ("]]]]]] %d %d\n", ff, bf);*/

		open.topsec = fc < bc? front : back;
		open.ceilingpic = open.topsec->GetTexture(sector_t::ceiling);
		open.top = fc < bc ? fc : bc;

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
			open.bottom = ff;
			open.bottomsec = front;
			open.floorpic = front->GetTexture(sector_t::floor);
			open.floorterrain = front->GetTerrain(sector_t::floor);
			open.lowfloor = bf;
		}
		else
		{
			open.bottom = bf;
			open.bottomsec = back;
			open.floorpic = back->GetTexture(sector_t::floor);
			open.floorterrain = back->GetTerrain(sector_t::floor);
			open.lowfloor = ff;
		}
	}
	else
	{ // Dummy stuff to have some sort of opening for the 3D checks to modify
		open.topsec = NULL;
		open.ceilingpic.SetInvalid();
		open.top = FIXED_MAX;
		open.bottomsec = NULL;
		open.floorpic.SetInvalid();
		open.floorterrain = -1;
		open.bottom = FIXED_MIN;
		open.lowfloor = FIXED_MAX;
	}

	// Check 3D floors
	if (actor != NULL)
	{
		P_LineOpening_XFloors(open, actor, linedef, x, y, refx, refy, !!(flags & FFCF_3DRESTRICT));
	}

	if (actor != NULL && linedef->frontsector != NULL && linedef->backsector != NULL && 
		linedef->flags & ML_3DMIDTEX)
	{
		open.touchmidtex = P_LineOpening_3dMidtex(actor, linedef, open, !!(flags & FFCF_3DRESTRICT));
	}
	else
	{
		open.abovemidtex = open.touchmidtex = false;
	}

	open.range = open.top - open.bottom;
}


//
// THING POSITION SETTING
//

//==========================================================================
//
// P_UnsetThingPosition
// Unlinks a thing from block map and sectors.
// On each position change, BLOCKMAP and other
// lookups maintaining lists of things inside
// these structures need to be updated.
//
//==========================================================================

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

		if (prev != NULL)	// prev will be NULL if this actor gets deleted due to cleaning up from a broken savegame
		{
			if ((*prev = next))  // unlink from sector list
				next->sprev = prev;
			snext = NULL;
			sprev = (AActor **)(size_t)0xBeefCafe;	// Woo! Bug-catching value!

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
	}
		
	if (!(flags & MF_NOBLOCKMAP))
	{
		// [RH] Unlink from all blocks this actor uses
		FBlockNode *block = this->BlockNode;

		while (block != NULL)
		{
			if (block->NextActor != NULL)
			{
				block->NextActor->PrevActor = block->PrevActor;
			}
			*(block->PrevActor) = block->NextActor;
			FBlockNode *next = block->NextBlock;
			block->Release ();
			block = next;
		}
		BlockNode = NULL;
	}
}


//==========================================================================
//
// P_SetThingPosition
// Links a thing into both a block and a subsector based on it's x y.
// Sets thing->sector properly
//
//==========================================================================

void AActor::LinkToWorld (bool buggy)
{
	// link into subsector
	sector_t *sec;

	if (!buggy || numgamenodes == 0)
	{
		sec = P_PointInSector (X(), Y());
	}
	else
	{
		sec = LinkToWorldForMapThing ();
	}

	LinkToWorld (sec);
}

void AActor::LinkToWorld (sector_t *sec)
{
	if (sec == NULL)
	{
		LinkToWorld ();
		return;
	}
	Sector = sec;
	subsector = R_PointInSubsector(X(), Y());	// this is from the rendering nodes, not the gameplay nodes!

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
		P_CreateSecNodeList (this, X(), Y());
		touching_sectorlist = sector_list;	// Attach to thing
		sector_list = NULL;		// clear for next time
    }

	
	// link into blockmap (inert things don't need to be in the blockmap)
	if ( !(flags & MF_NOBLOCKMAP) )
	{
		int x1 = GetSafeBlockX(X() - radius - bmaporgx);
		int x2 = GetSafeBlockX(X() + radius - bmaporgx);
		int y1 = GetSafeBlockY(Y() - radius - bmaporgy);
		int y2 = GetSafeBlockY(Y() + radius - bmaporgy);

		if (x1 >= bmapwidth || x2 < 0 || y1 >= bmapheight || y2 < 0)
		{ // thing is off the map
			BlockNode = NULL;
		}
		else
        { // [RH] Link into every block this actor touches, not just the center one
			FBlockNode **alink = &this->BlockNode;
			x1 = MAX (0, x1);
			y1 = MAX (0, y1);
			x2 = MIN (bmapwidth - 1, x2);
			y2 = MIN (bmapheight - 1, y2);
			for (int y = y1; y <= y2; ++y)
			{
				for (int x = x1; x <= x2; ++x)
				{
					FBlockNode **link = &blocklinks[y*bmapwidth + x];
					FBlockNode *node = FBlockNode::Create (this, x, y);

					// Link in to block
					if ((node->NextActor = *link) != NULL)
					{
						(*link)->PrevActor = &node->NextActor;
					}
					node->PrevActor = link;
					*link = node;

					// Link in to actor
					node->PrevBlock = alink;
					node->NextBlock = NULL;
					(*alink) = node;
					alink = &node->NextBlock;
				}
			}
		}
	}
}

//==========================================================================
//
// [RH] LinkToWorldForMapThing
//
// Emulate buggy PointOnLineSide and fix actors that lie on
// lines to compensate for some IWAD maps.
//
//==========================================================================

static int R_PointOnSideSlow (fixed_t x, fixed_t y, node_t *node)
{
	// [RH] This might have been faster than two multiplies and an
	// add on a 386/486, but it certainly isn't on anything newer than that.
	fixed_t	dx;
	fixed_t	dy;
	double	left;
	double	right;

	if (!node->dx)
	{
		if (x <= node->x)
			return node->dy > 0;

		return node->dy < 0;
	}
	if (!node->dy)
	{
		if (y <= node->y)
			return node->dx < 0;

		return node->dx > 0;
	}

	dx = (x - node->x);
	dy = (y - node->y);

	// Try to quickly decide by looking at sign bits.
	if ( (node->dy ^ node->dx ^ dx ^ dy)&0x80000000 )
	{
		if  ( (node->dy ^ dx) & 0x80000000 )
		{
			// (left is negative)
			return 1;
		}
		return 0;
	}

	// we must use doubles here because the fixed point code will produce errors due to loss of precision for extremely short linedefs.
	left = (double)node->dy * (double)dx;
	right = (double)dy * (double)node->dx;

	if (right < left)
	{
		// front side
		return 0;
	}
	// back side
	return 1;
}

sector_t *AActor::LinkToWorldForMapThing ()
{
	node_t *node = gamenodes + numgamenodes - 1;

	do
	{
		// Use original buggy point-on-side test when spawning
		// things at level load so that the map spots in the
		// emerald key room of Hexen MAP01 are spawned on the
		// window ledge instead of the blocking floor in front
		// of it. Why do I consider it buggy? Because a point
		// that lies directly on a line should always be
		// considered as "in front" of the line. The orientation
		// of the line should be irrelevant.
		node = (node_t *)node->children[R_PointOnSideSlow (X(), Y(), node)];
	}
	while (!((size_t)node & 1));

	subsector_t *ssec = (subsector_t *)((BYTE *)node - 1);

	if (flags4 & MF4_FIXMAPTHINGPOS)
	{
		// If the thing is exactly on a line, move it into the subsector
		// slightly in order to resolve clipping issues in the renderer.
		// This check needs to use the blockmap, because an actor on a
		// one-sided line might go into a subsector behind the line, so
		// the line would not be included as one of its subsector's segs.

		int blockx = GetSafeBlockX(X() - bmaporgx);
		int blocky = GetSafeBlockY(Y() - bmaporgy);

		if ((unsigned int)blockx < (unsigned int)bmapwidth &&
			(unsigned int)blocky < (unsigned int)bmapheight)
		{
			int *list;

			for (list = blockmaplump + blockmap[blocky*bmapwidth + blockx] + 1; *list != -1; ++list)
			{
				line_t *ldef = &lines[*list];

				if (ldef->frontsector == ldef->backsector)
				{ // Skip two-sided lines inside a single sector
					continue;
				}
				if (ldef->backsector != NULL)
				{
					if (ldef->frontsector->floorplane == ldef->backsector->floorplane &&
						ldef->frontsector->ceilingplane == ldef->backsector->ceilingplane)
					{ // Skip two-sided lines without any height difference on either side
						continue;
					}
				}

				// Not inside the line's bounding box
				if (X() + radius <= ldef->bbox[BOXLEFT]
					|| X() - radius >= ldef->bbox[BOXRIGHT]
					|| Y() + radius <= ldef->bbox[BOXBOTTOM]
					|| Y() - radius >= ldef->bbox[BOXTOP] )
					continue;

				// Get the exact distance to the line
				divline_t dll, dlv;
				fixed_t linelen = (fixed_t)sqrt((double)ldef->dx*ldef->dx + (double)ldef->dy*ldef->dy);

				P_MakeDivline (ldef, &dll);

				dlv.x = X();
				dlv.y = Y();
				dlv.dx = FixedDiv(dll.dy, linelen);
				dlv.dy = -FixedDiv(dll.dx, linelen);

				fixed_t distance = abs(P_InterceptVector(&dlv, &dll));

				if (distance < radius)
				{
					DPrintf ("%s at (%d,%d) lies on %s line %td, distance = %f\n",
						this->GetClass()->TypeName.GetChars(), X()>>FRACBITS, Y()>>FRACBITS, 
						ldef->dx == 0? "vertical" :	ldef->dy == 0? "horizontal" : "diagonal",
						ldef-lines, FIXED2FLOAT(distance));
					angle_t finean = R_PointToAngle2 (0, 0, ldef->dx, ldef->dy);
					if (ldef->backsector != NULL && ldef->backsector == ssec->sector)
					{
						finean += ANGLE_90;
					}
					else
					{
						finean -= ANGLE_90;
					}
					finean >>= ANGLETOFINESHIFT;

					// Get the distance we have to move the object away from the wall
					distance = radius - distance;
					SetXY(X() + FixedMul(distance, finecosine[finean]), Y() + FixedMul(distance, finesine[finean]));
					return P_PointInSector (X(), Y());
				}
			}
		}
	}

	return ssec->sector;
}

void AActor::SetOrigin (fixed_t ix, fixed_t iy, fixed_t iz, bool moving)
{
	UnlinkFromWorld ();
	SetXYZ(ix, iy, iz);
	if (moving) SetMovement(ix - X(), iy - Y(), iz - Z());
	LinkToWorld ();
	floorz = Sector->floorplane.ZatPoint (ix, iy);
	ceilingz = Sector->ceilingplane.ZatPoint (ix, iy);
	P_FindFloorCeiling(this, FFCF_ONLYSPAWNPOS);
}

FBlockNode *FBlockNode::FreeBlocks = NULL;

FBlockNode *FBlockNode::Create (AActor *who, int x, int y)
{
	FBlockNode *block;

	if (FreeBlocks != NULL)
	{
		block = FreeBlocks;
		FreeBlocks = block->NextBlock;
	}
	else
	{
		block = new FBlockNode;
	}
	block->BlockIndex = x + y*bmapwidth;
	block->Me = who;
	block->NextActor = NULL;
	block->PrevActor = NULL;
	block->PrevBlock = NULL;
	block->NextBlock = NULL;
	return block;
}

void FBlockNode::Release ()
{
	NextBlock = FreeBlocks;
	FreeBlocks = this;
}

//
// BLOCK MAP ITERATORS
// For each line/thing in the given mapblock,
// call the passed PIT_* function.
// If the function returns false,
// exit with false without checking anything else.
//


//===========================================================================
//
// FBlockLinesIterator
//
//===========================================================================
extern polyblock_t **PolyBlockMap;

FBlockLinesIterator::FBlockLinesIterator(int _minx, int _miny, int _maxx, int _maxy, bool keepvalidcount)
{
	if (!keepvalidcount) validcount++;
	minx = _minx;
	maxx = _maxx;
	miny = _miny;
	maxy = _maxy;
	Reset();
}

FBlockLinesIterator::FBlockLinesIterator(const FBoundingBox &box)
{
	validcount++;
	maxy = GetSafeBlockY(box.Top() - bmaporgy);
	miny = GetSafeBlockY(box.Bottom() - bmaporgy);
	maxx = GetSafeBlockX(box.Right() - bmaporgx);
	minx = GetSafeBlockX(box.Left() - bmaporgx);
	Reset();
}


//===========================================================================
//
// FBlockLinesIterator :: StartBlock
//
//===========================================================================

void FBlockLinesIterator::StartBlock(int x, int y) 
{ 
	curx = x; 
	cury = y; 
	if (x >= 0 && y >= 0 && x < bmapwidth && y <bmapheight)
	{
		int offset = y*bmapwidth + x;
		polyLink = PolyBlockMap? PolyBlockMap[offset] : NULL;
		polyIndex = 0;

		// There is an extra entry at the beginning of every block.
		// Apparently, id had originally intended for it to be used
		// to keep track of things, but the final code does not do that.
		list = blockmaplump + *(blockmap + offset) + 1;
	}
	else
	{
		// invalid block
		list = NULL;
		polyLink = NULL;
	}
}

//===========================================================================
//
// FBlockLinesIterator :: Next
//
//===========================================================================

line_t *FBlockLinesIterator::Next()
{
	while (true)
	{
		while (polyLink != NULL)
		{
			if (polyLink->polyobj)
			{
				if (polyIndex == 0)
				{
					if (polyLink->polyobj->validcount == validcount)
					{
						polyLink = polyLink->next;
						continue;
					}
					polyLink->polyobj->validcount = validcount;
				}

				line_t *ld = polyLink->polyobj->Linedefs[polyIndex];

				if (++polyIndex >= (int)polyLink->polyobj->Linedefs.Size())
				{
					polyLink = polyLink->next;
					polyIndex = 0;
				}

				if (ld->validcount == validcount)
				{
					continue;
				}
				else
				{
					ld->validcount = validcount;
					return ld;
				}
			}
			else polyLink = polyLink->next;
		}

		if (list != NULL)
		{
			while (*list != -1)
			{
				line_t *ld = &lines[*list];

				list++;
				if (ld->validcount != validcount)
				{
					ld->validcount = validcount;
					return ld;
				}
			}
		}

		if (++curx > maxx)
		{
			curx = minx;
			if (++cury > maxy) return NULL;
		}
		StartBlock(curx, cury);
	}
}

//===========================================================================
//
// FBlockThingsIterator :: FBlockThingsIterator
//
//===========================================================================

FBlockThingsIterator::FBlockThingsIterator()
: DynHash(0)
{
	minx = maxx = 0;
	miny = maxy = 0;
	ClearHash();
	block = NULL;
}

FBlockThingsIterator::FBlockThingsIterator(int _minx, int _miny, int _maxx, int _maxy)
: DynHash(0)
{
	minx = _minx;
	maxx = _maxx;
	miny = _miny;
	maxy = _maxy;
	ClearHash();
	Reset();
}

FBlockThingsIterator::FBlockThingsIterator(const FBoundingBox &box)
: DynHash(0)
{
	maxy = GetSafeBlockY(box.Top() - bmaporgy);
	miny = GetSafeBlockY(box.Bottom() - bmaporgy);
	maxx = GetSafeBlockX(box.Right() - bmaporgx);
	minx = GetSafeBlockX(box.Left() - bmaporgx);
	ClearHash();
	Reset();
}

//===========================================================================
//
// FBlockThingsIterator :: ClearHash
//
//===========================================================================

void FBlockThingsIterator::ClearHash()
{
	clearbuf(Buckets, countof(Buckets), -1);
	NumFixedHash = 0;
	DynHash.Clear();
}

//===========================================================================
//
// FBlockThingsIterator :: StartBlock
//
//===========================================================================

void FBlockThingsIterator::StartBlock(int x, int y) 
{ 
	curx = x; 
	cury = y; 
	if (x >= 0 && y >= 0 && x < bmapwidth && y <bmapheight)
	{
		block = blocklinks[y*bmapwidth + x];
	}
	else
	{
		// invalid block
		block = NULL;
	}
}

//===========================================================================
//
// FBlockThingsIterator :: SwitchBlock
//
//===========================================================================

void FBlockThingsIterator::SwitchBlock(int x, int y)
{
	minx = maxx = x;
	miny = maxy = y;
	StartBlock(x, y);
}

//===========================================================================
//
// FBlockThingsIterator :: Next
//
//===========================================================================

AActor *FBlockThingsIterator::Next(bool centeronly)
{
	for (;;)
	{
		while (block != NULL)
		{
			AActor *me = block->Me;
			FBlockNode *mynode = block;
			HashEntry *entry;
			int i;

			block = block->NextActor;
			// Don't recheck things that were already checked
			if (mynode->NextBlock == NULL && mynode->PrevBlock == &me->BlockNode)
			{ // This actor doesn't span blocks, so we know it can only ever be checked once.
				return me;
			}
			if (centeronly)
			{
				// Block boundaries for compatibility mode
				fixed_t blockleft = (curx << MAPBLOCKSHIFT) + bmaporgx;
				fixed_t blockright = blockleft + MAPBLOCKSIZE;
				fixed_t blockbottom = (cury << MAPBLOCKSHIFT) + bmaporgy;
				fixed_t blocktop = blockbottom + MAPBLOCKSIZE;

				// only return actors with the center in this block
				if (me->X() >= blockleft && me->X() < blockright &&
					me->Y() >= blockbottom && me->Y() < blocktop)
				{
					return me;
				}
			}
			else
			{
				size_t hash = ((size_t)me >> 3) % countof(Buckets);
				for (i = Buckets[hash]; i >= 0; )
				{
					entry = GetHashEntry(i);
					if (entry->Actor == me)
					{ // I've already been checked. Skip to the next actor.
						break;
					}
					i = entry->Next;
				}
				if (i < 0)
				{ // Add me to the hash table and return me.
					if (NumFixedHash < (int)countof(FixedHash))
					{
						entry = &FixedHash[NumFixedHash];
						entry->Next = Buckets[hash];
						Buckets[hash] = NumFixedHash++;
					}
					else
					{
						if (DynHash.Size() == 0)
						{
							DynHash.Grow(50);
						}
						i = DynHash.Reserve(1);
						entry = &DynHash[i];
						entry->Next = Buckets[hash];
						Buckets[hash] = i + countof(FixedHash);
					}
					entry->Actor = me;
					return me;
				}
			}
		}

		if (++curx > maxx)
		{
			curx = minx;
			if (++cury > maxy) return NULL;
		}
		StartBlock(curx, cury);
	}
}


//===========================================================================
//
// FPathTraverse :: Intercepts
//
//===========================================================================

TArray<intercept_t> FPathTraverse::intercepts(128);


//===========================================================================
//
// FPathTraverse :: AddLineIntercepts.
// Looks for lines in the given block
// that intercept the given trace
// to add to the intercepts list.
//
// A line is crossed if its endpoints
// are on opposite sides of the trace.
//
//===========================================================================

void FPathTraverse::AddLineIntercepts(int bx, int by)
{
	FBlockLinesIterator it(bx, by, bx, by, true);
	line_t *ld;

	while ((ld = it.Next()))
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
		
		if (s1 == s2) continue;	// line isn't crossed
		
		// hit the line
		P_MakeDivline (ld, &dl);
		frac = P_InterceptVector (&trace, &dl);

		if (frac < 0 || frac > FRACUNIT) continue;	// behind source or beyond end point
			
		intercept_t newintercept;

		newintercept.frac = frac;
		newintercept.isaline = true;
		newintercept.done = false;
		newintercept.d.line = ld;
		intercepts.Push (newintercept);
	}
}


//===========================================================================
//
// FPathTraverse :: AddThingIntercepts
//
//===========================================================================

void FPathTraverse::AddThingIntercepts (int bx, int by, FBlockThingsIterator &it, bool compatible)
{
	AActor *thing;

	it.SwitchBlock(bx, by);
	while ((thing = it.Next(compatible)))
	{
		int numfronts = 0;
		divline_t line;
		int i;


		if (!compatible)
		{
			// [RH] Don't check a corner to corner crossection for hit.
			// Instead, check against the actual bounding box (but not if compatibility optioned.)

			// There's probably a smarter way to determine which two sides
			// of the thing face the trace than by trying all four sides...
			for (i = 0; i < 4; ++i)
			{
				switch (i)
				{
				case 0:		// Top edge
					line.x = thing->X() + thing->radius;
					line.y = thing->Y() + thing->radius;
					line.dx = -thing->radius * 2;
					line.dy = 0;
					break;

				case 1:		// Right edge
					line.x = thing->X() + thing->radius;
					line.y = thing->Y() - thing->radius;
					line.dx = 0;
					line.dy = thing->radius * 2;
					break;

				case 2:		// Bottom edge
					line.x = thing->X() - thing->radius;
					line.y = thing->Y() - thing->radius;
					line.dx = thing->radius * 2;
					line.dy = 0;
					break;

				case 3:		// Left edge
					line.x = thing->X() - thing->radius;
					line.y = thing->Y() + thing->radius;
					line.dx = 0;
					line.dy = thing->radius * -2;
					break;
				}
				// Check if this side is facing the trace origin
				if (P_PointOnDivlineSidePrecise (trace.x, trace.y, &line) == 0)
				{
					numfronts++;

					// If it is, see if the trace crosses it
					if (P_PointOnDivlineSidePrecise (line.x, line.y, &trace) !=
						P_PointOnDivlineSidePrecise (line.x + line.dx, line.y + line.dy, &trace))
					{
						// It's a hit
						fixed_t frac = P_InterceptVector (&trace, &line);
						if (frac < 0)
						{ // behind source
							continue;
						}

						intercept_t newintercept;
						newintercept.frac = frac;
						newintercept.isaline = false;
						newintercept.done = false;
						newintercept.d.thing = thing;
						intercepts.Push (newintercept);
						continue;
					}
				}
			}

			// If none of the sides was facing the trace, then the trace
			// must have started inside the box, so add it as an intercept.
			if (numfronts == 0)
			{
				intercept_t newintercept;
				newintercept.frac = 0;
				newintercept.isaline = false;
				newintercept.done = false;
				newintercept.d.thing = thing;
				intercepts.Push (newintercept);
			}
		}
		else
		{
			// Old code for compatibility purposes
			fixed_t 		x1, y1, x2, y2;
			int 			s1, s2;
			divline_t		dl;
			fixed_t 		frac;
				
			bool tracepositive = (trace.dx ^ trace.dy)>0;
						
			// check a corner to corner crossection for hit
			if (tracepositive)
			{
				x1 = thing->X() - thing->radius;
				y1 = thing->Y() + thing->radius;
						
				x2 = thing->X() + thing->radius;
				y2 = thing->Y() - thing->radius;					
			}
			else
			{
				x1 = thing->X() - thing->radius;
				y1 = thing->Y() - thing->radius;
						
				x2 = thing->X() + thing->radius;
				y2 = thing->Y() + thing->radius;					
			}
			
			s1 = P_PointOnDivlineSide (x1, y1, &trace);
			s2 = P_PointOnDivlineSide (x2, y2, &trace);

			if (s1 != s2)
			{
				dl.x = x1;
				dl.y = y1;
				dl.dx = x2-x1;
				dl.dy = y2-y1;
				
				frac = P_InterceptVector (&trace, &dl);

				if (frac >= 0)
				{
					intercept_t newintercept;
					newintercept.frac = frac;
					newintercept.isaline = false;
					newintercept.done = false;
					newintercept.d.thing = thing;
					intercepts.Push (newintercept);
				}
			}
		}
	}
}


//===========================================================================
//
// FPathTraverse :: Next
// 
//===========================================================================

intercept_t *FPathTraverse::Next()
{
	intercept_t *in = NULL;

	fixed_t dist = FIXED_MAX;
	for (unsigned scanpos = intercept_index; scanpos < intercepts.Size (); scanpos++)
	{
		intercept_t *scan = &intercepts[scanpos];
		if (scan->frac < dist && !scan->done)
		{
			dist = scan->frac;
			in = scan;
		}
	}
	
	if (dist > FRACUNIT || in == NULL) return NULL;	// checked everything in range			
	in->done = true;
	return in;
}

//===========================================================================
//
// FPathTraverse
// Traces a line from x1,y1 to x2,y2,
//
//===========================================================================

FPathTraverse::FPathTraverse (fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2, int flags)
{
	fixed_t 	xt1, xt2;
	fixed_t 	yt1, yt2;
	long long	_x1, _x2, _y1, _y2;
	
	fixed_t 	xstep;
	fixed_t 	ystep;
	
	fixed_t 	partialx, partialy;
	
	fixed_t 	xintercept;
	fixed_t 	yintercept;
	
	int 		mapx;
	int 		mapy;
	
	int 		mapxstep;
	int 		mapystep;

	int 		count;
				
	validcount++;
	intercept_index = intercepts.Size();
		
	if ( ((x1-bmaporgx)&(MAPBLOCKSIZE-1)) == 0)
		x1 += FRACUNIT; // don't side exactly on a line
	
	if ( ((y1-bmaporgy)&(MAPBLOCKSIZE-1)) == 0)
		y1 += FRACUNIT; // don't side exactly on a line

	trace.x = x1;
	trace.y = y1;
	if (flags & PT_DELTA)
	{
		trace.dx = x2;
		trace.dy = y2;
	}
	else
	{
		trace.dx = x2 - x1;
		trace.dy = y2 - y1;
	}

	_x1 = (long long)x1 - bmaporgx;
	_y1 = (long long)y1 - bmaporgy;
	x1 -= bmaporgx;
	y1 -= bmaporgy;
	xt1 = int(_x1 >> MAPBLOCKSHIFT);
	yt1 = int(_y1 >> MAPBLOCKSHIFT);

	if (flags & PT_DELTA)
	{
		_x2 = _x1 + x2;
		_y2 = _y1 + y2;
		xt2 = int(_x2 >> MAPBLOCKSHIFT);
		yt2 = int(_y2 >> MAPBLOCKSHIFT);
		x2 = (int)_x2;
		y2 = (int)_y2;
	}
	else
	{
		_x2 = (long long)x2 - bmaporgx;
		_y2 = (long long)y2 - bmaporgy;
		x2 -= bmaporgx;
		y2 -= bmaporgy;
		xt2 = int(_x2 >> MAPBLOCKSHIFT);
		yt2 = int(_y2 >> MAPBLOCKSHIFT);
	}

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
	yintercept = int(_y1>>MAPBTOFRAC) + FixedMul (partialx, ystep);

		
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
	xintercept = int(_x1>>MAPBTOFRAC) + FixedMul (partialy, xstep);

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

	// Step through map blocks.
	// Count is present to prevent a round off error
	// from skipping the break statement.
	mapx = xt1;
	mapy = yt1;

	bool compatible = (flags & PT_COMPATIBLE) && (i_compatflags & COMPATF_HITSCAN);
		
	// we want to use one list of checked actors for the entire operation
	FBlockThingsIterator btit;
	for (count = 0 ; count < 100 ; count++)
	{
		if (flags & PT_ADDLINES)
		{
			AddLineIntercepts(mapx, mapy);
		}
		
		if (flags & PT_ADDTHINGS)
		{
			AddThingIntercepts(mapx, mapy, btit, compatible);
		}
				
		if (mapx == xt2 && mapy == yt2)
		{
			break;
		}

		// [RH] Handle corner cases properly instead of pretending they don't exist.
		switch ((((yintercept >> FRACBITS) == mapy) << 1) | ((xintercept >> FRACBITS) == mapx))
		{
		case 0:		// neither xintercept nor yintercept match!
			count = 100;	// Stop traversing, because somebody screwed up.
			break;

		case 1:		// xintercept matches
			xintercept += xstep;
			mapy += mapystep;
			break;

		case 2:		// yintercept matches
			yintercept += ystep;
			mapx += mapxstep;
			break;

		case 3:		// xintercept and yintercept both match
			// The trace is exiting a block through its corner. Not only does the block
			// being entered need to be checked (which will happen when this loop
			// continues), but the other two blocks adjacent to the corner also need to
			// be checked.
			if (!compatible)
			{
				if (flags & PT_ADDLINES)
				{
					AddLineIntercepts(mapx + mapxstep, mapy);
					AddLineIntercepts(mapx, mapy + mapystep);
				}
				
				if (flags & PT_ADDTHINGS)
				{
					AddThingIntercepts(mapx + mapxstep, mapy, btit, false);
					AddThingIntercepts(mapx, mapy + mapystep, btit, false);
				}
				xintercept += xstep;
				yintercept += ystep;
				mapx += mapxstep;
				mapy += mapystep;
			}
			else
			{
				count = 100; //	Doom originally did not handle this case so do the same in compatibility mode.
			}
			break;
		}
	}
}

FPathTraverse::~FPathTraverse()
{
	intercepts.Resize(intercept_index);
}


//===========================================================================
//
// P_RoughMonsterSearch
//
// Searches though the surrounding mapblocks for monsters/players
//		distance is in MAPBLOCKUNITS
//===========================================================================

AActor *P_RoughMonsterSearch (AActor *mo, int distance, bool onlyseekable)
{
	return P_BlockmapSearch (mo, distance, RoughBlockCheck, (void *)onlyseekable);
}

AActor *P_BlockmapSearch (AActor *mo, int distance, AActor *(*check)(AActor*, int, void *), void *params)
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

	startX = GetSafeBlockX(mo->X()-bmaporgx);
	startY = GetSafeBlockY(mo->Y()-bmaporgy);
	validcount++;
	
	if (startX >= 0 && startX < bmapwidth && startY >= 0 && startY < bmapheight)
	{
		if ( (target = check (mo, startY*bmapwidth+startX, params)) )
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
			if ( (target = check (mo, blockIndex, params)) )
			{
				return target;
			}
		}
		// Trace the second block section (right edge)
		for (blockIndex--; blockIndex <= secondStop; blockIndex += bmapwidth)
		{
			if ( (target = check (mo, blockIndex, params)) )
			{
				return target;
			}
		}		
		// Trace the third block section (bottom edge)
		for (blockIndex -= bmapwidth; blockIndex >= thirdStop; blockIndex--)
		{
			if ( (target = check (mo, blockIndex, params)) )
			{
				return target;
			}
		}
		// Trace the final block section (left edge)
		for (blockIndex++; blockIndex > finalStop; blockIndex -= bmapwidth)
		{
			if ( (target = check (mo, blockIndex, params)) )
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

static AActor *RoughBlockCheck (AActor *mo, int index, void *param)
{
	bool onlyseekable = param != NULL;
	FBlockNode *link;

	for (link = blocklinks[index]; link != NULL; link = link->NextActor)
	{
		if (link->Me != mo)
		{
			if (onlyseekable && !mo->CanSeek(link->Me))
			{
				continue;
			}
			if (mo->IsOkayToAttack (link->Me))
			{
				return link->Me;
			}
		}
	}
	return NULL;
}

//===========================================================================
//
// P_VanillaPointOnLineSide
// P_PointOnLineSide() from the initial Doom source code release
//
//===========================================================================

int P_VanillaPointOnLineSide(fixed_t x, fixed_t y, const line_t* line)
{
	fixed_t	dx;
	fixed_t	dy;
	fixed_t	left;
	fixed_t	right;

	if (!line->dx)
	{
		if (x <= line->v1->x)
			return line->dy > 0;

		return line->dy < 0;
	}
	if (!line->dy)
	{
		if (y <= line->v1->y)
			return line->dx < 0;

		return line->dx > 0;
	}

	dx = (x - line->v1->x);
	dy = (y - line->v1->y);

	left = FixedMul ( line->dy>>FRACBITS , dx );
	right = FixedMul ( dy , line->dx>>FRACBITS );

	if (right < left)
		return 0;		// front side
	return 1;			// back side
}

//===========================================================================
//
// P_VanillaPointOnDivlineSide
// P_PointOnDivlineSide() from the initial Doom source code release
//
//===========================================================================

int P_VanillaPointOnDivlineSide(fixed_t x, fixed_t y, const divline_t* line)
{
	fixed_t	dx;
	fixed_t	dy;
	fixed_t	left;
	fixed_t	right;

	if (!line->dx)
	{
		if (x <= line->x)
			return line->dy > 0;

		return line->dy < 0;
	}
	if (!line->dy)
	{
		if (y <= line->y)
			return line->dx < 0;

		return line->dx > 0;
	}

	dx = (x - line->x);
	dy = (y - line->y);

	// try to quickly decide by looking at sign bits
	if ( (line->dy ^ line->dx ^ dx ^ dy)&0x80000000 )
	{
		if ( (line->dy ^ dx) & 0x80000000 )
			return 1;		// (left is negative)
		return 0;
	}

	left = FixedMul ( line->dy>>8, dx>>8 );
	right = FixedMul ( dy>>8 , line->dx>>8 );

	if (right < left)
		return 0;		// front side
	return 1;			// back side
}

