//-----------------------------------------------------------------------------
//
// Copyright 1998-1998 Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
// Copyright 1999-2016 Randy Heit
// Copyright 2002-2016 Christoph Oelckers
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
//		Boom secnodes
//
//-----------------------------------------------------------------------------

#include "g_levellocals.h"
#include "p_maputl.h"
#include "actor.h"

//=============================================================================
// phares 3/21/98
//
// Maintain a freelist of msecnode_t's to reduce memory allocs and frees.
//=============================================================================

msecnode_t *headsecnode = nullptr;
FMemArena secnodearena;

//=============================================================================
//
// P_GetSecnode
//
// Retrieve a node from the freelist. The calling routine
// should make sure it sets all fields properly.
//
//=============================================================================

msecnode_t *P_GetSecnode()
{
	msecnode_t *node;

	if (headsecnode)
	{
		node = headsecnode;
		headsecnode = headsecnode->m_snext;
	}
	else
	{
		node = (msecnode_t *)secnodearena.Alloc(sizeof(*node));
	}
	return node;
}

//=============================================================================
//
// P_PutSecnode
//
// Returns a node to the freelist.
//
//=============================================================================

void P_PutSecnode(msecnode_t *node)
{
	node->m_snext = headsecnode;
	headsecnode = node;
}

//=============================================================================
// phares 3/16/98
//
// P_AddSecnode
//
// Searches the current list to see if this sector is
// already there. If not, it adds a sector node at the head of the list of
// sectors this object appears in. This is called when creating a list of
// nodes that will get linked in later. Returns a pointer to the new node.
//
//=============================================================================

template<class nodetype, class linktype>
nodetype *P_AddSecnode(linktype *s, AActor *thing, nodetype *nextnode, nodetype *&sec_thinglist)
{
	nodetype *node;

	if (s == 0)
	{
		I_FatalError("AddSecnode of 0 for %s\n", thing->GetClass()->TypeName.GetChars());
	}

	node = nextnode;
	while (node)
	{
		if (node->m_sector == s)	// Already have a node for this sector?
		{
			node->m_thing = thing;	// Yes. Setting m_thing says 'keep it'.
			return nextnode;
		}
		node = node->m_tnext;
	}

	// Couldn't find an existing node for this sector. Add one at the head
	// of the list.

	node = (nodetype*)P_GetSecnode();

	// killough 4/4/98, 4/7/98: mark new nodes unvisited.
	node->visited = 0;

	node->m_sector = s; 			// sector
	node->m_thing = thing; 		// mobj
	node->m_tprev = nullptr;			// prev node on Thing thread
	node->m_tnext = nextnode;		// next node on Thing thread
	if (nextnode)
		nextnode->m_tprev = node;	// set back link on Thing

	// Add new node at head of sector thread starting at s->touching_thinglist

	node->m_sprev = nullptr;			// prev node on sector thread
	node->m_snext = sec_thinglist; // next node on sector thread
	if (sec_thinglist)
		node->m_snext->m_sprev = node;
	sec_thinglist = node;
	return node;
}

template msecnode_t *P_AddSecnode<msecnode_t, sector_t>(sector_t *s, AActor *thing, msecnode_t *nextnode, msecnode_t *&sec_thinglist);
template portnode_t *P_AddSecnode<portnode_t, FLinePortal>(FLinePortal *s, AActor *thing, portnode_t *nextnode, portnode_t *&sec_thinglist);

//=============================================================================
//
// P_DelSecnode
//
// Deletes a sector node from the list of
// sectors this object appears in. Returns a pointer to the next node
// on the linked list, or nullptr.
//
//=============================================================================

template<class nodetype, class linktype>
nodetype *P_DelSecnode(nodetype *node, nodetype *linktype::*listhead)
{
	nodetype* tp;  // prev node on thing thread
	nodetype* tn;  // next node on thing thread
	nodetype* sp;  // prev node on sector thread
	nodetype* sn;  // next node on sector thread

	if (node)
	{
		// Unlink from the Thing thread. The Thing thread begins at
		// sector_list and not from AActor->touching_sectorlist.

		tp = node->m_tprev;
		tn = node->m_tnext;
		if (tp)
			tp->m_tnext = tn;
		if (tn)
			tn->m_tprev = tp;

		// Unlink from the sector thread. This thread begins at
		// sector_t->touching_thinglist.

		sp = node->m_sprev;
		sn = node->m_snext;
		if (sp)
			sp->m_snext = sn;
		else
			node->m_sector->*listhead = sn;
		if (sn)
			sn->m_sprev = sp;

		// Return this node to the freelist

		P_PutSecnode((msecnode_t*)node);
		return tn;
	}
	return nullptr;
} 														// phares 3/13/98

template msecnode_t *P_DelSecnode(msecnode_t *node, msecnode_t *sector_t::*listhead);
template portnode_t *P_DelSecnode(portnode_t *node, portnode_t *FLinePortal::*listhead);

//=============================================================================
//
// P_DelSeclist
//
// Delete an entire sector list
//
//=============================================================================

void P_DelSeclist(msecnode_t *node, msecnode_t *sector_t::*sechead)
{
	while (node)
		node = P_DelSecnode(node, sechead);
}

void P_DelSeclist(portnode_t *node, portnode_t *FLinePortal::*sechead)
{
	while (node)
		node = P_DelSecnode(node, sechead);
}


//=============================================================================
// phares 3/14/98
//
// P_CreateSecNodeList
//
// Alters/creates the sector_list that shows what sectors the object resides in
//
//=============================================================================

msecnode_t *P_CreateSecNodeList(AActor *thing, double radius, msecnode_t *sector_list, msecnode_t *sector_t::*seclisthead)
{
	msecnode_t *node;

	// First, clear out the existing m_thing fields. As each node is
	// added or verified as needed, m_thing will be set properly. When
	// finished, delete all nodes where m_thing is still nullptr. These
	// represent the sectors the Thing has vacated.

	node = sector_list;
	while (node)
	{
		node->m_thing = nullptr;
		node = node->m_tnext;
	}

	FBoundingBox box(thing->X(), thing->Y(), radius);
	FBlockLinesIterator it(thing->Level, box);
	line_t *ld;

	while ((ld = it.Next()))
	{
		if (!inRange(box, ld) || BoxOnLineSide(box, ld) != -1)
			continue;

		// This line crosses through the object.

		// Collect the sector(s) from the line and add to the
		// sector_list you're examining. If the Thing ends up being
		// allowed to move to this position, then the sector_list
		// will be attached to the Thing's AActor at touching_sectorlist.

		sector_list = P_AddSecnode(ld->frontsector, thing, sector_list, ld->frontsector->*seclisthead);

		// Don't assume all lines are 2-sided, since some Things
		// like MT_TFOG are allowed regardless of whether their radius takes
		// them beyond an impassable linedef.

		// killough 3/27/98, 4/4/98:
		// Use sidedefs instead of 2s flag to determine two-sidedness.

		if (ld->backsector)
			sector_list = P_AddSecnode(ld->backsector, thing, sector_list, ld->backsector->*seclisthead);
	}

	// Add the sector of the (x,y) point to sector_list.

	sector_list = P_AddSecnode(thing->Sector, thing, sector_list, thing->Sector->*seclisthead);

	// Now delete any nodes that won't be used. These are the ones where
	// m_thing is still nullptr.

	node = sector_list;
	while (node)
	{
		if (node->m_thing == nullptr)
		{
			if (node == sector_list)
				sector_list = node->m_tnext;
			node = P_DelSecnode(node, seclisthead);
		}
		else
		{
			node = node->m_tnext;
		}
	}
	return sector_list;
}

//=============================================================================
//
// P_DelPortalnode
//
// Same for line portal nodes
//
//=============================================================================

portnode_t *P_DelPortalnode(portnode_t *node)
{
	portnode_t* tp;  // prev node on thing thread
	portnode_t* tn;  // next node on thing thread
	portnode_t* sp;  // prev node on sector thread
	portnode_t* sn;  // next node on sector thread

	if (node)
	{
		// Unlink from the Thing thread. The Thing thread begins at
		// sector_list and not from AActor->touching_sectorlist.

		tp = node->m_tprev;
		tn = node->m_tnext;
		if (tp)
			tp->m_tnext = tn;
		if (tn)
			tn->m_tprev = tp;

		// Unlink from the sector thread. This thread begins at
		// sector_t->touching_thinglist.

		sp = node->m_sprev;
		sn = node->m_snext;
		if (sp)
			sp->m_snext = sn;
		else
			node->m_sector->lineportal_thinglist = sn;
		if (sn)
			sn->m_sprev = sp;

		// Return this node to the freelist (use the same one as for msecnodes, since both types are the same size.)
		P_PutSecnode(reinterpret_cast<msecnode_t *>(node));
		return tn;
	}
	return nullptr;
}


//=============================================================================
//
// P_AddPortalnode
//
//=============================================================================

portnode_t *P_AddPortalnode(FLinePortal *s, AActor *thing, portnode_t *nextnode)
{
	portnode_t *node;

	if (s == nullptr)
	{
		I_FatalError("AddSecnode of 0 for %s\n", thing->GetClass()->TypeName.GetChars());
	}

	node = reinterpret_cast<portnode_t*>(P_GetSecnode());

	// killough 4/4/98, 4/7/98: mark new nodes unvisited.
	node->visited = 0;

	node->m_sector = s; 			// portal
	node->m_thing = thing; 			// mobj
	node->m_tprev = nullptr;			// prev node on Thing thread
	node->m_tnext = nextnode;		// next node on Thing thread
	if (nextnode)
		nextnode->m_tprev = node;	// set back link on Thing

									// Add new node at head of portal thread starting at s->touching_thinglist

	node->m_sprev = nullptr;			// prev node on portal thread
	node->m_snext = s->lineportal_thinglist; // next node on portal thread
	if (s->lineportal_thinglist)
		node->m_snext->m_sprev = node;
	s->lineportal_thinglist = node;
	return node;
}

//==========================================================================
//
// Handle the lists used to render actors from other portal areas
//
//==========================================================================

void AActor::UpdateRenderSectorList()
{
	static const double SPRITE_SPACE = 64.;
	if (Pos() != OldRenderPos && !(flags & MF_NOSECTOR))
	{
		// Only check if the map contains line portals
		ClearRenderLineList();
		if (Level->PortalBlockmap.containsLines && Pos().XY() != OldRenderPos.XY())
		{
			int bx = Level->blockmap.GetBlockX(X());
			int by = Level->blockmap.GetBlockY(Y());
			FBoundingBox bb(X(), Y(), min(radius*1.5, 128.));	// Don't go further than 128 map units, even for large actors
			// Are there any portals near the actor's position?
			if (Level->blockmap.isValidBlock(bx, by) && Level->PortalBlockmap(bx, by).neighborContainsLines)
			{
				// Go through the entire list. In most cases this is faster than setting up a blockmap iterator
				for (auto &p : Level->linePortals)
				{
					if (p.mType == PORTT_VISUAL) continue;
					if (inRange(bb, p.mOrigin) && BoxOnLineSide(bb, p.mOrigin))
					{
						touching_lineportallist = P_AddPortalnode(&p, this, touching_lineportallist);
					}
				}
			}
		}
		sector_t *sec = Sector;
		double lasth = -FLT_MAX;
		ClearRenderSectorList();
		while (!sec->PortalBlocksMovement(sector_t::ceiling))
		{
			double planeh = sec->GetPortalPlaneZ(sector_t::ceiling);
			if (planeh <= lasth) break;	// broken setup.
			if (Top() + SPRITE_SPACE < planeh) break;
			lasth = planeh;
			DVector2 newpos = Pos().XY() + sec->GetPortalDisplacement(sector_t::ceiling);
			sec = sec->Level->PointInSector(newpos);
			touching_sectorportallist = P_AddSecnode(sec, this, touching_sectorportallist, sec->sectorportal_thinglist);
		}
		sec = Sector;
		lasth = FLT_MAX;
		while (!sec->PortalBlocksMovement(sector_t::floor))
		{
			double planeh = sec->GetPortalPlaneZ(sector_t::floor);
			if (planeh >= lasth) break;	// broken setup.
			if (Z() - SPRITE_SPACE > planeh) break;
			lasth = planeh;
			DVector2 newpos = Pos().XY() + sec->GetPortalDisplacement(sector_t::floor);
			sec = sec->Level->PointInSector(newpos);
			touching_sectorportallist = P_AddSecnode(sec, this, touching_sectorportallist, sec->sectorportal_thinglist);
		}
	}
}

void AActor::ClearRenderSectorList()
{
	P_DelSeclist(touching_sectorportallist, &sector_t::sectorportal_thinglist);
	touching_sectorportallist = nullptr;
}

void AActor::ClearRenderLineList()
{
	P_DelSeclist(touching_lineportallist, &FLinePortal::lineportal_thinglist);
	touching_lineportallist = nullptr;
}

//===========================================================================
//
// FBlockNode - allows to link actors into multiple blocks in the blockmap
//
//===========================================================================

FBlockNode *FBlockNode::FreeBlocks = nullptr;

FBlockNode *FBlockNode::Create(AActor *who, int x, int y, int group)
{
	FBlockNode *block;

	if (FreeBlocks != nullptr)
	{
		block = FreeBlocks;
		FreeBlocks = block->NextBlock;
	}
	else
	{
		block = (FBlockNode *)secnodearena.Alloc(sizeof(FBlockNode));
	}
	block->BlockIndex = x + y * who->Level->blockmap.bmapwidth;
	block->Me = who;
	block->NextActor = nullptr;
	block->PrevActor = nullptr;
	block->PrevBlock = nullptr;
	block->NextBlock = nullptr;
	return block;
}

void FBlockNode::Release()
{
	NextBlock = FreeBlocks;
	FreeBlocks = this;
}
