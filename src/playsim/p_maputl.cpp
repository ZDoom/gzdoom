//-----------------------------------------------------------------------------
//
// Copyright 1993-1996 id Software
// Copyright 1994-1996 Raven Software
// Copyright 1998-1998 Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
// Copyright 1999-2016 Randy Heit
// Copyright 2002-2017 Christoph Oelckers
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//		Movement/collision utility functions,
//		as used by function in p_map.c. 
//		BLOCKMAP Iterator functions,
//		and some PIT_* functions to use for iteration.
//
//-----------------------------------------------------------------------------

/* For code that originates from ZDoom the following applies:
**
**---------------------------------------------------------------------------
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


#include <stdlib.h>


#include "m_bbox.h"

#include "doomdef.h"
#include "doomdata.h"
#include "doomstat.h"
#include "p_local.h"
#include "p_maputl.h"
#include "p_3dmidtex.h"
#include "p_blockmap.h"
#include "r_utility.h"
#include "actor.h"
#include "actorinlines.h"

// State.
#include "po_man.h"
#include "vm.h"

int P_VanillaPointOnDivlineSide(double x, double y, const divline_t* line);


//==========================================================================
//
// P_AproxDistance
//
// Gives an estimation of distance (not exact)
// 
//==========================================================================

int P_AproxDistance (int dx, int dy)
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

double P_InterceptVector(const divline_t *v2, const divline_t *v1)
{
	double num;
	double den;

	double v1x = v1->x;
	double v1y = v1->y;
	double v1dx = v1->dx;
	double v1dy = v1->dy;
	double v2x = v2->x;
	double v2y = v2->y;
	double v2dx = v2->dx;
	double v2dy = v2->dy;

	den = v1dy*v2dx - v1dx*v2dy;

	if (den == 0)
		return 0;		// parallel

	num = (v1x - v2x)*v1dy + (v2y - v1y)*v1dx;
	return num / den;
}

//==========================================================================
//
// P_LineOpening
//
// Sets opentop and openbottom to the window
// through a two sided line.
//
//==========================================================================

void P_LineOpening (FLineOpening &open, AActor *actor, const line_t *linedef, const DVector2 &pos, const DVector2 *ref, int flags)
{
	if (!(flags & FFCF_ONLY3DFLOORS))
	{
		sector_t *front, *back;
		double fc = 0, ff = 0, bc = 0, bf = 0;

		if (linedef->backsector == NULL)
		{
			// single sided line
			open.range = 0;
			return;
		}

		front = linedef->frontsector;
		back = linedef->backsector;

		if (!(flags & FFCF_NOPORTALS) && (linedef->flags & ML_PORTALCONNECT))
		{
			if (!linedef->frontsector->PortalBlocksMovement(sector_t::ceiling)) fc = LINEOPEN_MAX;
			if (!linedef->backsector->PortalBlocksMovement(sector_t::ceiling)) bc = LINEOPEN_MAX;
			if (!linedef->frontsector->PortalBlocksMovement(sector_t::floor)) ff = LINEOPEN_MIN;
			if (!linedef->backsector->PortalBlocksMovement(sector_t::floor)) bf = LINEOPEN_MIN;
		}

		if (fc == 0) fc = front->ceilingplane.ZatPoint(pos);
		if (bc == 0) bc = back->ceilingplane.ZatPoint(pos);
		if (ff == 0) ff = front->floorplane.ZatPoint(pos);
		if (bf == 0) bf = back->floorplane.ZatPoint(pos);


		/*Printf ("]]]]]] %d %d\n", ff, bf);*/

		open.topsec = fc < bc? front : back;
		open.ceilingpic = open.topsec->GetTexture(sector_t::ceiling);
		open.top = fc < bc ? fc : bc;

		bool usefront;

		// Store portal state to avoid registering false dropoffs.
		open.lowfloorthroughportal = 0;
		if (ff == LINEOPEN_MIN) open.lowfloorthroughportal |= 1;
		if (bf == LINEOPEN_MIN) open.lowfloorthroughportal |= 2;

		// [RH] fudge a bit for actors that are moving across lines
		// bordering a slope/non-slope that meet on the floor. Note
		// that imprecisions in the plane equation mean there is a
		// good chance that even if a slope and non-slope look like
		// they line up, they won't be perfectly aligned.
		if (ff == LINEOPEN_MIN || bf == LINEOPEN_MIN || ref == NULL || fabs (ff-bf) > 1./256)
		{
			usefront = (ff > bf);
		}
		else
		{
			if (!front->floorplane.isSlope())
				usefront = true;
			else if (!back->floorplane.isSlope())
				usefront = false;
			else
				usefront = !P_PointOnLineSide (*ref, linedef);
		}

		open.lowfloorthroughportal = false;
		if (usefront)
		{
			open.bottom = ff;
			open.bottomsec = front;
			open.floorpic = front->GetTexture(sector_t::floor);
			open.floorterrain = front->GetTerrain(sector_t::floor);
			if (bf != -FLT_MIN) open.lowfloor = bf;
			else if (!(flags & FFCF_NODROPOFF))
			{
				// We must check through the portal for the actual dropoff.
				// If there's no lines in the lower sections we'd never get a usable value otherwise.
				open.lowfloor = NextLowestFloorAt(back, pos.X, pos.Y, back->GetPortalPlaneZ(sector_t::floor) - EQUAL_EPSILON);
			}
		}
		else
		{
			open.bottom = bf;
			open.bottomsec = back;
			open.floorpic = back->GetTexture(sector_t::floor);
			open.floorterrain = back->GetTerrain(sector_t::floor);
			if (ff != -FLT_MIN) open.lowfloor = ff;
			else if (!(flags & FFCF_NODROPOFF))
			{
				// We must check through the portal for the actual dropoff.
				// If there's no lines in the lower sections we'd never get a usable value otherwise.
				open.lowfloor = NextLowestFloorAt(front, pos.X, pos.Y, front->GetPortalPlaneZ(sector_t::floor) - EQUAL_EPSILON);
			}
		}
		open.frontfloorplane = front->floorplane;
		open.backfloorplane = back->floorplane;
	}
	else
	{ // Dummy stuff to have some sort of opening for the 3D checks to modify
		open.topsec = NULL;
		open.ceilingpic.SetInvalid();
		open.top = LINEOPEN_MAX;
		open.bottomsec = NULL;
		open.floorpic.SetInvalid();
		open.floorterrain = -1;
		open.bottom = LINEOPEN_MIN;
		open.lowfloor = LINEOPEN_MAX;
		open.frontfloorplane.SetAtHeight(LINEOPEN_MIN, sector_t::floor);
		open.backfloorplane.SetAtHeight(LINEOPEN_MIN, sector_t::floor);
	}

	open.topffloor = open.bottomffloor = nullptr;
	// Check 3D floors
	if (actor != NULL)
	{
		P_LineOpening_XFloors(open, actor, linedef, pos.X, pos.Y, !!(flags & FFCF_3DRESTRICT));
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

	// avoid overflows in the opening.
	open.range = clamp(open.top - open.bottom, LINEOPEN_MIN, LINEOPEN_MAX);
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

void AActor::UnlinkFromWorld (FLinkContext *ctx)
{
	if (ctx != nullptr) ctx->sector_list = nullptr;
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

			if (ctx != nullptr)
			{
				ctx->sector_list = touching_sectorlist;
				ctx->render_list = touching_rendersectors;
			}
			else
			{
				P_DelSeclist(touching_sectorlist, &sector_t::touching_thinglist);
				P_DelSeclist(touching_rendersectors, &sector_t::touching_renderthings);
			}
			touching_sectorlist = nullptr; //to be restored by P_SetThingPosition
			touching_rendersectors = nullptr;
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
	ClearRenderSectorList();
	ClearRenderLineList();
}

//==========================================================================
//
// If the thing is exactly on a line, move it into the sector
// slightly in order to resolve clipping issues in the renderer.
//
//==========================================================================

bool AActor::FixMapthingPos()
{
	sector_t *secstart = Level->PointInSectorBuggy(X(), Y());

	int blockx = Level->blockmap.GetBlockX(X());
	int blocky = Level->blockmap.GetBlockY(Y());
	bool success = false;

	if (Level->blockmap.isValidBlock(blockx, blocky))
	{
		int *list;

		for (list = Level->blockmap.GetLines(blockx, blocky); *list != -1; ++list)
		{
			line_t *ldef = &Level->lines[*list];

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
				|| Y() - radius >= ldef->bbox[BOXTOP])
				continue;

			// Get the exact distance to the line
			divline_t dll, dlv;
			double linelen = ldef->Delta().Length();

			P_MakeDivline(ldef, &dll);

			dlv.x = X();
			dlv.y = Y();
			dlv.dx = dll.dy / linelen;
			dlv.dy = -dll.dx / linelen;

			double distance = fabs(P_InterceptVector(&dlv, &dll));

			if (distance < radius)
			{
				DPrintf(DMSG_NOTIFY, "%s at (%f,%f) lies on %s line %d, distance = %f\n",
					this->GetClass()->TypeName.GetChars(), X(), Y(),
					ldef->Delta().X == 0 ? "vertical" : ldef->Delta().Y == 0 ? "horizontal" : "diagonal",
					ldef->Index(), distance);
				DAngle ang = ldef->Delta().Angle();
				if (ldef->backsector != NULL && ldef->backsector == secstart)
				{
					ang += DAngle::fromDeg(90.);
				}
				else
				{
					ang -= DAngle::fromDeg(90.);
				}

				// Get the distance we have to move the object away from the wall
				distance = radius - distance;
				SetXY(Pos().XY() + ang.ToVector(distance));
				ClearInterpolation();
				success = true;
			}
		}
	}
	return success;
}

//==========================================================================
//
// P_SetThingPosition
// Links a thing into both a block and a subsector based on its x y.
// Sets thing->sector properly
//
//==========================================================================

void AActor::LinkToWorld(FLinkContext *ctx, bool spawningmapthing, sector_t *sector)
{
	bool spawning = spawningmapthing;

	if (spawning)
	{
		if ((flags4 & MF4_FIXMAPTHINGPOS) && sector == NULL)
		{
			if (FixMapthingPos()) spawning = false;
		}
	}

	if (sector == NULL)
	{
		if (!spawning)
		{
			sector = Level->PointInSector(Pos());
		}
		else
		{
			sector = Level->PointInSectorBuggy(X(), Y());
		}
	}

	Sector = sector;
	subsector = Level->PointInRenderSubsector(Pos());	// this is from the rendering nodes, not the gameplay nodes!
	section = subsector->section;

	if (!(flags & MF_NOSECTOR))
	{
		// invisible things don't go into the sector links
		// killough 8/11/98: simpler scheme using pointer-to-pointer prev
		// pointers, allows head nodes to be treated like everything else

		AActor **link = &sector->thinglist;
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
		touching_sectorlist = P_CreateSecNodeList(this, radius, ctx != nullptr? ctx->sector_list : nullptr, &sector_t::touching_thinglist);	// Attach to thing
		if (renderradius >= 0) touching_rendersectors = P_CreateSecNodeList(this, RenderRadius(), ctx != nullptr ? ctx->render_list : nullptr, &sector_t::touching_renderthings);
		else
		{
			touching_rendersectors = nullptr;
			if (ctx != nullptr) P_DelSeclist(ctx->render_list, &sector_t::touching_renderthings);
		}
	}


	// link into blockmap (inert things don't need to be in the blockmap)
	if (!(flags & MF_NOBLOCKMAP))
	{
		FPortalGroupArray check;

		Level->CollectConnectedGroups(Sector->PortalGroup, Pos(), Top(), radius, check);

		BlockNode = NULL;
		FBlockNode **alink = &this->BlockNode;
		for (int i = -1; i < (int)check.Size(); i++)
		{
			DVector3 pos = i==-1? Pos() : PosRelative(check[i] & ~FPortalGroupArray::FLAT);

			int x1 = Level->blockmap.GetBlockX(pos.X - radius);
			int x2 = Level->blockmap.GetBlockX(pos.X + radius);
			int y1 = Level->blockmap.GetBlockY(pos.Y - radius);
			int y2 = Level->blockmap.GetBlockY(pos.Y + radius);

			if (x1 >= Level->blockmap.bmapwidth || x2 < 0 || y1 >= Level->blockmap.bmapheight || y2 < 0)
			{ // thing is off the map
			}
			else
			{ // [RH] Link into every block this actor touches, not just the center one
				x1 = max(0, x1);
				y1 = max(0, y1);
				x2 = min(Level->blockmap.bmapwidth - 1, x2);
				y2 = min(Level->blockmap.bmapheight - 1, y2);
				for (int y = y1; y <= y2; ++y)
				{
					for (int x = x1; x <= x2; ++x)
					{
						FBlockNode **link = &Level->blockmap.blocklinks[y*Level->blockmap.bmapwidth + x];
						FBlockNode *node = FBlockNode::Create(this, x, y, this->Sector->PortalGroup);

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
	// Portal links cannot be done unless the level is fully initialized.
	if (!spawningmapthing) UpdateRenderSectorList();
}

void AActor::SetOrigin(double x, double y, double z, bool moving)
{
	FLinkContext ctx;
	UnlinkFromWorld (&ctx);
	SetXYZ(x, y, z);
	LinkToWorld (&ctx);
	P_FindFloorCeiling(this, FFCF_ONLYSPAWNPOS);
	if (!moving) ClearInterpolation();
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

FBlockLinesIterator::FBlockLinesIterator(FLevelLocals *l, int _minx, int _miny, int _maxx, int _maxy, bool keepvalidcount)
{
	if (!keepvalidcount) validcount++;
	Level = l;
	minx = _minx;
	maxx = _maxx;
	miny = _miny;
	maxy = _maxy;
	Reset();
}

void FBlockLinesIterator::init(const FBoundingBox &box)
{
	validcount++;
	maxy = Level->blockmap.GetBlockY(box.Top());
	miny = Level->blockmap.GetBlockY(box.Bottom());
	maxx = Level->blockmap.GetBlockX(box.Right());
	minx = Level->blockmap.GetBlockX(box.Left());
	Reset();
}

FBlockLinesIterator::FBlockLinesIterator(FLevelLocals *l, const FBoundingBox &box)
{
	Level = l;
	init(box);
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
	if (Level->blockmap.isValidBlock(x, y))
	{
		unsigned offset = y*Level->blockmap.bmapwidth + x;
		polyLink = Level->PolyBlockMap.Size() > offset? Level->PolyBlockMap[offset] : nullptr;
		polyIndex = 0;

		list = Level->blockmap.GetLines(x, y);
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
				line_t *ld = &Level->lines[*list];

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
// FMultiBlockLinesIterator :: FMultiBlockLinesIterator
//
// An iterator that can check multiple portal groups.
//
//===========================================================================

FMultiBlockLinesIterator::FMultiBlockLinesIterator(FPortalGroupArray &check, AActor *origin, double checkradius)
	: checklist(check), blockIterator(origin->Level)
{
	checkpoint = origin->Pos();
	if (!check.inited) origin->Level->CollectConnectedGroups(origin->Sector->PortalGroup, checkpoint, origin->Top(), checkradius, checklist);
	checkpoint.Z = checkradius == -1? origin->radius : checkradius;
	basegroup = origin->Sector->PortalGroup;
	startsector = origin->Sector;
	Reset();
}

FMultiBlockLinesIterator::FMultiBlockLinesIterator(FPortalGroupArray &check, FLevelLocals *Level, double checkx, double checky, double checkz, double checkh, double checkradius, sector_t *newsec)
	: checklist(check), blockIterator(Level)
{
	checkpoint = { checkx, checky, checkz };
	if (newsec == NULL)	newsec = Level->PointInSector(checkx, checky);
	startsector = newsec;
	basegroup = newsec->PortalGroup;
	if (!check.inited) Level->CollectConnectedGroups(basegroup, checkpoint, checkz + checkh, checkradius, checklist);
	checkpoint.Z = checkradius;
	Reset();
}

//===========================================================================
//
// Go up a ceiling portal
//
//===========================================================================

bool FMultiBlockLinesIterator::GoUp(double x, double y)
{
	if (continueup)
	{
		if (!cursector->PortalBlocksMovement(sector_t::ceiling))
		{
			if (startIteratorForGroup(cursector->GetOppositePortalGroup(sector_t::ceiling)))
			{
				portalflags = FFCF_NOFLOOR;
				return true;
			}
		}
		continueup = false;
	}
	return false;
}

//===========================================================================
//
// Go down a floor portal
//
//===========================================================================

bool FMultiBlockLinesIterator::GoDown(double x, double y)
{
	if (continuedown)
	{
		if (!cursector->PortalBlocksMovement(sector_t::floor))
		{
			if (startIteratorForGroup(cursector->GetOppositePortalGroup(sector_t::floor)))
			{
				portalflags = FFCF_NOCEILING;
				return true;
			}
		}
		continuedown = false;
	}
	return false;
}

//===========================================================================
//
// Gets the next line - also manages switching between portal groups 
//
//===========================================================================

bool FMultiBlockLinesIterator::Next(FMultiBlockLinesIterator::CheckResult *item)
{
	line_t *line = blockIterator.Next();
	if (line != NULL)
	{
		item->line = line;
		item->Position.X = offset.X;
		item->Position.Y = offset.Y;
		item->portalflags = portalflags;
		return true;
	}
	bool onlast = unsigned(index + 1) >= checklist.Size();
	int nextflags = onlast ? 0 : checklist[index + 1] & FPortalGroupArray::FLAT;

	if (portalflags == FFCF_NOFLOOR && nextflags != FPortalGroupArray::UPPER)
	{
		// if this is the last upper portal in the list, check if we need to go further up to find the real ceiling.
		if (GoUp(offset.X, offset.Y)) return Next(item);
	}
	else if (portalflags == FFCF_NOCEILING && nextflags != FPortalGroupArray::LOWER)
	{
		// if this is the last lower portal in the list, check if we need to go further down to find the real floor.
		if (GoDown(offset.X, offset.Y)) return Next(item);
	}
	if (onlast)
	{
		cursector = startsector;
		// We reached the end of the list. Check if we still need to check up- and downwards.
		if (GoUp(checkpoint.X, checkpoint.Y) ||
			GoDown(checkpoint.X, checkpoint.Y))
		{
			return Next(item);
		}
		return false;
	}

	index++;
	startIteratorForGroup(checklist[index] & ~FPortalGroupArray::FLAT);
	switch (nextflags)
	{
	case FPortalGroupArray::UPPER:
		portalflags = FFCF_NOFLOOR;
		break;

	case FPortalGroupArray::LOWER:
		portalflags = FFCF_NOCEILING;
		break;

	default:
		portalflags = 0;
	}

	return Next(item);
}

//===========================================================================
//
// start iterating a new group
//
//===========================================================================

bool FMultiBlockLinesIterator::startIteratorForGroup(int group)
{
	offset = blockIterator.Level->Displacements.getOffset(basegroup, group);
	offset.X += checkpoint.X;
	offset.Y += checkpoint.Y;
	cursector = group == startsector->PortalGroup ? startsector : blockIterator.Level->PointInSector(offset);
	// If we ended up in a different group, 
	// presumably because the spot to be checked is too far outside the actual portal group,
	// the search needs to abort.
	if (cursector->PortalGroup != group) return false;
	bbox.setBox(offset.X, offset.Y, checkpoint.Z);
	blockIterator.init(bbox);
	return true;
}

//===========================================================================
//
// Resets the iterator
//
//===========================================================================

void FMultiBlockLinesIterator::Reset()
{
	continueup = continuedown = true;
	index = -1;
	portalflags = 0;
	startIteratorForGroup(basegroup);
}

//===========================================================================
//
// FBlockThingsIterator :: FBlockThingsIterator
//
//===========================================================================

FBlockThingsIterator::FBlockThingsIterator(FLevelLocals *l)
: DynHash()
{
	Level = l;
	minx = maxx = 0;
	miny = maxy = 0;
	ClearHash();
	block = NULL;
}

FBlockThingsIterator::FBlockThingsIterator(FLevelLocals *l, int _minx, int _miny, int _maxx, int _maxy)
: DynHash()
{
	Level = l;
	minx = _minx;
	maxx = _maxx;
	miny = _miny;
	maxy = _maxy;
	ClearHash();
	Reset();
}

void FBlockThingsIterator::init(const FBoundingBox &box, bool clearhash)
{
	maxy = Level->blockmap.GetBlockY(box.Top());
	miny = Level->blockmap.GetBlockY(box.Bottom());
	maxx = Level->blockmap.GetBlockX(box.Right());
	minx = Level->blockmap.GetBlockX(box.Left());
	if (clearhash) ClearHash();
	Reset();
}

//===========================================================================
//
// FBlockThingsIterator :: ClearHash
//
//===========================================================================

void FBlockThingsIterator::ClearHash()
{
	memset(Buckets, -1, sizeof(Buckets));
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
	if (Level->blockmap.isValidBlock(x, y))
	{
		block = Level->blockmap.blocklinks[y*Level->blockmap.bmapwidth + x];
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
				double blockleft = (curx * FBlockmap::MAPBLOCKUNITS) + Level->blockmap.bmaporgx;
				double blockright = blockleft + FBlockmap::MAPBLOCKUNITS;
				double blockbottom = (cury * FBlockmap::MAPBLOCKUNITS) + Level->blockmap.bmaporgy;
				double blocktop = blockbottom + FBlockmap::MAPBLOCKUNITS;

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
// FMultiBlockThingsIterator :: FMultiBlockThingsIterator
//
// An iterator that can check multiple portal groups.
//
//===========================================================================

FMultiBlockThingsIterator::FMultiBlockThingsIterator(FPortalGroupArray &check, AActor *origin, double checkradius, bool ignorerestricted)
	: checklist(check), blockIterator(origin->Level)
{
	checkpoint = origin->Pos();
	if (!check.inited) origin->Level->CollectConnectedGroups(origin->Sector->PortalGroup, checkpoint, origin->Top(), checkradius, checklist);
	checkpoint.Z = checkradius == -1? origin->radius : checkradius;
	basegroup = origin->Sector->PortalGroup;
	Reset();
}

FMultiBlockThingsIterator::FMultiBlockThingsIterator(FPortalGroupArray &check, FLevelLocals *Level, double checkx, double checky, double checkz, double checkh, double checkradius, bool ignorerestricted, sector_t *newsec)
	: checklist(check), blockIterator(Level)
{
	checkpoint.X = checkx;
	checkpoint.Y = checky;
	checkpoint.Z = checkz;
	if (newsec == NULL) newsec = Level->PointInSector(checkx, checky);
	basegroup = newsec->PortalGroup;
	if (!check.inited) Level->CollectConnectedGroups(basegroup, checkpoint, checkz + checkh, checkradius, checklist);
	checkpoint.Z = checkradius;
	Reset();
}

//===========================================================================
//
// Gets the next line - also manages switching between portal groups 
//
//===========================================================================

bool FMultiBlockThingsIterator::Next(FMultiBlockThingsIterator::CheckResult *item)
{
	AActor *thing = blockIterator.Next();
	if (thing != NULL)
	{
		item->thing = thing;
		item->Position = checkpoint + blockIterator.Level->Displacements.getOffset(basegroup, thing->Sector->PortalGroup);
		item->portalflags = portalflags;
		return true;
	}
	bool onlast = unsigned(index + 1) >= checklist.Size();
	int nextflags = onlast ? 0 : checklist[index + 1] & FPortalGroupArray::FLAT;

	if (onlast)
	{
		return false;
	}

	index++;
	startIteratorForGroup(checklist[index] & ~FPortalGroupArray::FLAT);
	switch (nextflags)
	{
	case FPortalGroupArray::UPPER:
		portalflags = FFCF_NOFLOOR;
		break;

	case FPortalGroupArray::LOWER:
		portalflags = FFCF_NOCEILING;
		break;

	default:
		portalflags = 0;
	}

	return Next(item);
}

//===========================================================================
//
// start iterating a new group
//
//===========================================================================

void FMultiBlockThingsIterator::startIteratorForGroup(int group)
{
	DVector2 offset = blockIterator.Level->Displacements.getOffset(basegroup, group);
	offset.X += checkpoint.X;
	offset.Y += checkpoint.Y;
	bbox.setBox(offset.X, offset.Y, checkpoint.Z);
	blockIterator.init(bbox, false);
}

//===========================================================================
//
// Resets the iterator
//
//===========================================================================

void FMultiBlockThingsIterator::Reset()
{
	index = -1;
	portalflags = 0;
	startIteratorForGroup(basegroup);
	blockIterator.ClearHash();
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
	FBlockLinesIterator it(Level, bx, by, bx, by, true);
	line_t *ld;

	while ((ld = it.Next()))
	{
		int 				s1;
		int 				s2;
		double 				frac;
		divline_t			dl;

		s1 = P_PointOnDivlineSide (ld->v1->fX(), ld->v1->fY(), &trace);
		s2 = P_PointOnDivlineSide (ld->v2->fX(), ld->v2->fY(), &trace);
		
		if (s1 == s2) continue;	// line isn't crossed
		
		// hit the line
		P_MakeDivline (ld, &dl);
		frac = P_InterceptVector (&trace, &dl);

		if (frac < Startfrac || frac > 1.) continue;	// behind source or beyond end point
			
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
					line.y = thing->Y() + thing->radius;
					if (trace.y < line.y) continue;
					line.x = thing->X() + thing->radius;
					line.dx = -thing->radius * 2;
					line.dy = 0;
					break;

				case 1:		// Right edge
					line.x = thing->X() + thing->radius;
					if (trace.x < line.x) continue;
					line.y = thing->Y() - thing->radius;
					line.dx = 0;
					line.dy = thing->radius * 2;
					break;

				case 2:		// Bottom edge
					line.y = thing->Y() - thing->radius;
					if (trace.y > line.y) continue;
					line.x = thing->X() - thing->radius;
					line.dx = thing->radius * 2;
					line.dy = 0;
					break;

				case 3:		// Left edge
					line.x = thing->X() - thing->radius;
					if (trace.x > line.x) continue;
					line.y = thing->Y() + thing->radius;
					line.dx = 0;
					line.dy = thing->radius * -2;
					break;
				}
				// Check if this side is facing the trace origin
				numfronts++;

				// If it is, see if the trace crosses it
				if (P_PointOnDivlineSide (line.x, line.y, &trace) !=
					P_PointOnDivlineSide (line.x + line.dx, line.y + line.dy, &trace))
				{
					// It's a hit
					double frac = P_InterceptVector (&trace, &line);
					if (frac < Startfrac)
					{ // behind source
						if (Startfrac > 0)
						{
							// check if the trace starts within this actor
							switch (i)
							{
							case 0:
								line.y -= 2 * thing->radius;
								break;

							case 1:
								line.x -= 2 * thing->radius;
								break;

							case 2:
								line.y += 2 * thing->radius;
								break;

							case 3:
								line.x += 2 * thing->radius;
								break;
							}
							double frac2 = P_InterceptVector(&trace, &line);
							if (frac2 >= Startfrac) goto addit;
						}
						continue;
					}
				addit:
					intercept_t newintercept;
					newintercept.frac = frac;
					newintercept.isaline = false;
					newintercept.done = false;
					newintercept.d.thing = thing;
					intercepts.Push (newintercept);
					break;
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
			double 		x1, y1, x2, y2;
			int 			s1, s2;
			divline_t		dl;
			double 		frac;
				
			bool tracepositive = (trace.dx * trace.dy)>0;
						
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

				if (frac >= Startfrac)
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

	double dist = FLT_MAX;
	for (unsigned scanpos = intercept_index; scanpos < intercepts.Size (); scanpos++)
	{
		intercept_t *scan = &intercepts[scanpos];
		if (scan->frac < dist && !scan->done)
		{
			dist = scan->frac;
			in = scan;
		}
	}
	
	if (dist > 1. || in == NULL) return NULL;	// checked everything in range			
	in->done = true;
	return in;
}

//===========================================================================
//
// FPathTraverse
// Traces a line from x1,y1 to x2,y2,
//
//===========================================================================

void FPathTraverse::init(double x1, double y1, double x2, double y2, int flags, double startfrac) 
{
	double xt1, yt1, xt2, yt2;
	double xstep, ystep;
	double partialx, partialy;
	double xintercept, yintercept;
	
	int 		mapx;
	int 		mapy;
	
	int 		mapxstep;
	int 		mapystep;

	int 		count;

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
	if (startfrac > 0)
	{
		double startdx = trace.dx * startfrac;
		double startdy = trace.dy * startfrac;

		x1 += startdx;
		y1 += startdy;
		x2 = trace.dx - startdx;
		y2 = trace.dy - startdy;
		flags |= PT_DELTA;
	}

	validcount++;
	intercept_index = intercepts.Size();
	Startfrac = startfrac;

	if (flags & PT_DELTA)
	{
		x2 += x1;
		y2 += y1;
	}

	x1 -= Level->blockmap.bmaporgx;
	y1 -= Level->blockmap.bmaporgy;
	xt1 = x1 / FBlockmap::MAPBLOCKUNITS;
	yt1 = y1 / FBlockmap::MAPBLOCKUNITS;

	x2 -= Level->blockmap.bmaporgx;
	y2 -= Level->blockmap.bmaporgy;
	xt2 = x2 / FBlockmap::MAPBLOCKUNITS;
	yt2 = y2 / FBlockmap::MAPBLOCKUNITS;

	mapx = xs_FloorToInt(xt1);
	mapy = xs_FloorToInt(yt1);
	int mapex = xs_FloorToInt(xt2);
	int mapey = xs_FloorToInt(yt2);


	if (mapex > mapx)
	{
		mapxstep = 1;
		partialx = 1. - xt1 + xs_FloorToInt(xt1);
		ystep = (y2 - y1) / fabs(x2 - x1);
	}
	else if (mapex < mapx)
	{
		mapxstep = -1;
		partialx = xt1 - xs_FloorToInt(xt1);
		ystep = (y2 - y1) / fabs(x2 - x1);
	}
	else
	{
		mapxstep = 0;
		partialx = 1.;
		ystep = 256;
	}
	yintercept = yt1 + partialx * ystep;

	if (mapey > mapy)
	{
		mapystep = 1;
		partialy = 1. - yt1 + xs_FloorToInt(yt1);
		xstep = (x2 - x1) / fabs(y2 - y1);
	}
	else if (mapey < mapy)
	{
		mapystep = -1;
		partialy = yt1 - xs_FloorToInt(yt1);
		xstep = (x2 - x1) / fabs(y2 - y1);
	}
	else
	{
		mapystep = 0;
		partialy = 1;
		xstep = 256;
	}
	xintercept = xt1 + partialy * xstep;

	// [RH] Fix for traces that pass only through blockmap corners. In that case,
	// xintercept and yintercept can both be set ahead of mapx and mapy, so the
	// for loop would never advance anywhere.

	if (fabs(xstep) == 1. && fabs(ystep) == 1.)
	{
		if (ystep < 0)
		{
			partialx = 1. - partialx;
		}
		if (xstep < 0)
		{
			partialy = 1. - partialy;
		}
		if (partialx == partialy)
		{
			xintercept = xt1;
			yintercept = yt1;
		}
	}

	// Step through map blocks.
	// Count is present to prevent a round off error
	// from skipping the break statement.

	bool compatible = (flags & PT_COMPATIBLE) && (Level->i_compatflags & COMPATF_HITSCAN);
		
	// we want to use one list of checked actors for the entire operation
	FBlockThingsIterator btit(Level);
	for (count = 0 ; count < 1000 ; count++)
	{
		if (flags & PT_ADDLINES)
		{
			AddLineIntercepts(mapx, mapy);
		}
		
		if (flags & PT_ADDTHINGS)
		{
			AddThingIntercepts(mapx, mapy, btit, compatible);
		}
				
		// both coordinates reached the end, so end the traversing.
		if ((mapxstep | mapystep) == 0)
			break;


		// [RH] Handle corner cases properly instead of pretending they don't exist.
		switch (((xs_FloorToInt(yintercept) == mapy) << 1) | (xs_FloorToInt(xintercept) == mapx))
		{
		case 0:		// neither xintercept nor yintercept match!
			count = 1000;	// Stop traversing, because somebody screwed up.
			break;

		case 1:		// xintercept matches
			xintercept += xstep;
			mapy += mapystep;
			if (mapy == mapey)
				mapystep = 0;
			break;

		case 2:		// yintercept matches
			yintercept += ystep;
			mapx += mapxstep;
			if (mapx == mapex)
				mapxstep = 0;
			break;

		case 3:		// xintercept and yintercept both match
			// The trace is exiting a block through its corner. Not only does the block
			// being entered need to be checked (which will happen when this loop
			// continues), but the other two blocks adjacent to the corner also need to
			// be checked.
			// Since Doom.exe did not do this, this code won't either if run in compatibility mode.
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
				if (mapx == mapex)
					mapxstep = 0;
				if (mapy == mapey)
					mapystep = 0;
			}
			else
			{
				count = 1000; //	Doom originally did not handle this case so do the same in compatibility mode.
			}
			break;
		}
	}
}

//===========================================================================
//
// Relocates the trace when going through a line portal
//
//===========================================================================

int FPathTraverse::PortalRelocate(intercept_t *in, int flags, DVector3 *optpos)
{
	if (!in->isaline || !in->d.line->isLinePortal()) return false;
	if (P_PointOnLineSidePrecise(trace.x, trace.y, in->d.line) == 1) return false;

	double hitx = trace.x;
	double hity = trace.y;
	double endx = trace.x + trace.dx;
	double endy = trace.y + trace.dy;
	
	P_TranslatePortalXY(in->d.line, hitx, hity);
	P_TranslatePortalXY(in->d.line, endx, endy);
	if (optpos != NULL)
	{
		P_TranslatePortalXY(in->d.line, optpos->X, optpos->Y);
		P_TranslatePortalZ(in->d.line, optpos->Z);
	}
	line_t *saved = in->d.line;	// this gets overwritten by the init call.
	intercepts.Resize(intercept_index);
	init(hitx, hity, endx, endy, flags, in->frac + EQUAL_EPSILON);
	return saved->getPortal()->mType == PORTT_LINKED? 1:-1;
}

void FPathTraverse::PortalRelocate(const DVector2 &displacement, int flags, double hitfrac)
{
	double hitx = trace.x + displacement.X;
	double hity = trace.y + displacement.Y;
	double endx = hitx + trace.dx;
	double endy = hity + trace.dy;
	intercepts.Resize(intercept_index);
	init(hitx, hity, endx, endy, flags, hitfrac);
}

//===========================================================================
//
//
//
//===========================================================================

FPathTraverse::~FPathTraverse()
{
	intercepts.Resize(intercept_index);
}


//
// P_CheckFov
// Returns true if t2 is within t1's field of view.
//
int P_CheckFov(AActor* t1, AActor* t2, double fov)
{
	return absangle(t1->AngleTo(t2), t1->Angles.Yaw) <= DAngle::fromDeg(fov);
}

DEFINE_ACTION_FUNCTION_NATIVE(AActor, CheckFov, P_CheckFov)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_POINTER(t, AActor);
	PARAM_FLOAT(fov);
	ACTION_RETURN_BOOL(P_CheckFov(self, t, fov));
}

//===========================================================================
//
// P_RoughMonsterSearch
//
// Searches though the surrounding mapblocks for monsters/players
//		distance is in FBlockmap::MAPBLOCKUNITS
//===========================================================================

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
	auto Level = mo->Level;
	int bmapwidth = Level->blockmap.bmapwidth;
	int bmapheight = Level->blockmap.bmapheight;

	startX = Level->blockmap.GetBlockX(mo->X());
	startY = Level->blockmap.GetBlockY(mo->Y());
	validcount++;
	
	if (Level->blockmap.isValidBlock(startX, startY))
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

struct BlockCheckInfo
{
	bool onlyseekable;
	bool frontonly;
	divline_t frontline;
	double fov;
};

//===========================================================================
//
// RoughBlockCheck
//
//===========================================================================

static AActor *RoughBlockCheck (AActor *mo, int index, void *param)
{
	BlockCheckInfo *info = (BlockCheckInfo *)param;

	FBlockNode *link;

	for (link = mo->Level->blockmap.blocklinks[index]; link != NULL; link = link->NextActor)
	{
		if (link->Me != mo)
		{
			if (info->onlyseekable && !mo->CanSeek(link->Me))
			{
				continue;
			}
			if (info->frontonly && P_PointOnDivlineSide(link->Me->X(), link->Me->Y(), &info->frontline) != 0)
			{
				continue;
			}
			// skip actors outside of specified FOV
			if (info->fov > 0 && !P_CheckFov(mo, link->Me, info->fov))
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

AActor *P_RoughMonsterSearch(AActor *mo, int distance, bool onlyseekable, bool frontonly, double fov)
{
	BlockCheckInfo info;
	info.onlyseekable = onlyseekable;
	info.fov = fov;
	if ((info.frontonly = frontonly))
	{
		info.frontline.x = mo->X();
		info.frontline.y = mo->Y();
		info.frontline.dx = -mo->Angles.Yaw.Sin();
		info.frontline.dy = -mo->Angles.Yaw.Cos();
	}

	return P_BlockmapSearch(mo, distance, RoughBlockCheck, (void *)&info);
}

//==========================================================================
//
// [RH] LinkToWorldForMapThing
//
// Emulate buggy PointOnLineSide and fix actors that lie on
// lines to compensate for some IWAD maps.
//
//==========================================================================

static int R_PointOnSideSlow(double xx, double yy, node_t *node)
{
	// [RH] This might have been faster than two multiplies and an
	// add on a 386/486, but it certainly isn't on anything newer than that.
	auto x = FloatToFixed(xx);
	auto y = FloatToFixed(yy);
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

	auto dx = (x - node->x);
	auto dy = (y - node->y);

	// Try to quickly decide by looking at sign bits.
	if ((node->dy ^ node->dx ^ dx ^ dy) & 0x80000000)
	{
		if ((node->dy ^ dx) & 0x80000000)
		{
			// (left is negative)
			return 1;
		}
		return 0;
	}

	// we must use doubles here because the fixed point code will produce errors due to loss of precision for extremely short linedefs.
	// Note that this function is used for all map spawned actors and not just a compatibility fallback!
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


//===========================================================================
//
// P_VanillaPointOnLineSide
// P_PointOnLineSide() from the initial Doom source code release
//
//===========================================================================

int P_VanillaPointOnLineSide(double x, double y, const line_t* line)
{
	DVector2 delta = line->Delta();

	if (delta.X == 0)
	{
		if (x <= line->v1->fX())
			return delta.Y > 0;

		return delta.Y < 0;
	}
	if (delta.Y == 0)
	{
		if (y <= line->v1->fY())
			return delta.X < 0;

		return delta.X > 0;
	}

	// Note: This cannot really be converted to floating point 
	// without breaking the intended use of this function
	// (i.e. to emulate the horrible imprecision of the entire method)

	auto dx = FloatToFixed(x - line->v1->fX());
	auto dy = FloatToFixed(y - line->v1->fY());

	auto left = MulScale( int(delta.Y * 256) , dx, 16 );
	auto right = MulScale( dy , int(delta.X * 256), 16 );

	if (right < left)
		return 0;		// front side
	return 1;			// back side
}

//==========================================================================
//
// P_PointInSubsector
//
//==========================================================================

subsector_t *FLevelLocals::PointInSubsector(double x, double y)
{
	int side;

	auto node = HeadGamenode();
	if (node == nullptr) return &subsectors[0];

	fixed_t xx = FloatToFixed(x);
	fixed_t yy = FloatToFixed(y);
	do
	{
		side = R_PointOnSide(xx, yy, node);
		node = (node_t *)node->children[side];
	} while (!((size_t)node & 1));

	return (subsector_t *)((uint8_t *)node - 1);
}

//==========================================================================
//
// Use buggy PointOnSide and fix actors that lie on
// lines to compensate for some IWAD maps.
//
//==========================================================================

sector_t *FLevelLocals::PointInSectorBuggy(double x, double y)
{
	// single subsector is a special case
	auto node = HeadGamenode();
	if (node == nullptr) return subsectors[0].sector;
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
		node = (node_t *)node->children[R_PointOnSideSlow(x, y, node)];
	} while (!((size_t)node & 1));

	subsector_t *ssec = (subsector_t *)((uint8_t *)node - 1);
	return ssec->sector;
}

//==========================================================================
//
// RPointInRenderSubsector
//
//==========================================================================

subsector_t *FLevelLocals::PointInRenderSubsector (fixed_t x, fixed_t y)
{
	node_t *node;
	int side;
	
	// single subsector is a special case
	if (nodes.Size() == 0)
		return &subsectors[0];
	
	node = HeadNode();
	
	do
	{
		side = R_PointOnSide (x, y, node);
		node = (node_t *)node->children[side];
	}
	while (!((size_t)node & 1));
	
	return (subsector_t *)((uint8_t *)node - 1);
}


//==========================================================================
//
// FBoundingBox :: BoxOnLineSide
//
// Considers the line to be infinite
// Returns side 0 or 1, -1 if box crosses the line.
//
//==========================================================================

int BoxOnLineSide(const FBoundingBox &box, const line_t* ld)
{
	int p1;
	int p2;

	if (ld->Delta().X == 0)
	{ // ST_VERTICAL
		p1 = box.Right() < ld->v1->fX();
		p2 = box.Left() < ld->v1->fX();
		if (ld->Delta().Y < 0)
		{
			p1 ^= 1;
			p2 ^= 1;
		}
	}
	else if (ld->Delta().Y == 0)
	{ // ST_HORIZONTAL:
		p1 = box.Top() > ld->v1->fY();
		p2 = box.Bottom() > ld->v1->fY();
		if (ld->Delta().X < 0)
		{
			p1 ^= 1;
			p2 ^= 1;
		}
	}
	else if ((ld->Delta().X * ld->Delta().Y) >= 0)
	{ // ST_POSITIVE:
		p1 = P_PointOnLineSide(box.Left(), box.Top(), ld);
		p2 = P_PointOnLineSide(box.Right(), box.Bottom(), ld);
	}
	else
	{ // ST_NEGATIVE:
		p1 = P_PointOnLineSide(box.Right(), box.Top(), ld);
		p2 = P_PointOnLineSide(box.Left(), box.Bottom(), ld);
	}

	return (p1 == p2) ? p1 : -1;
}

DEFINE_ACTION_FUNCTION(FLevelLocals, PointOnLineSide)
{
	PARAM_SELF_STRUCT_PROLOGUE(FLevelLocals);
	PARAM_FLOAT(x);
	PARAM_FLOAT(y);
	PARAM_POINTER(l, line_t);
	PARAM_BOOL(precise);

	int res;
	if (precise) // allow forceful overriding of compat flag
		res = P_PointOnLineSidePrecise(x, y, l);
	else
		res = P_PointOnLineSide(x, y, l);

	ACTION_RETURN_INT(res);
}

DEFINE_ACTION_FUNCTION(FLevelLocals, ActorOnLineSide)
{
	PARAM_SELF_STRUCT_PROLOGUE(FLevelLocals);
	PARAM_OBJECT(mo, AActor);
	PARAM_POINTER(l, line_t);

	FBoundingBox box(mo->X(), mo->Y(), mo->radius);
	ACTION_RETURN_INT(BoxOnLineSide(box, l));
}

DEFINE_ACTION_FUNCTION(FLevelLocals, BoxOnLineSide)
{
	PARAM_SELF_STRUCT_PROLOGUE(FLevelLocals);
	PARAM_FLOAT(x);
	PARAM_FLOAT(y);
	PARAM_FLOAT(radius);
	PARAM_POINTER(l, line_t);

	FBoundingBox box(x, y, radius);
	ACTION_RETURN_INT(BoxOnLineSide(box, l));
}

