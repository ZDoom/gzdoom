//-----------------------------------------------------------------------------
//
// Copyright 1994-1996 Raven Software
// Copyright 1999-2016 Randy Heit
// Copyright 2002-2018 Christoph Oelckers
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

// HEADER FILES ------------------------------------------------------------

#include "doomdef.h"
#include "p_local.h"
#include "m_bbox.h"
#include "a_sharedglobal.h"
#include "p_3dmidtex.h"
#include "p_lnspec.h"
#include "po_man.h"
#include "p_setup.h"
#include "p_blockmap.h"
#include "p_maputl.h"
#include "r_utility.h"
#include "g_levellocals.h"
#include "actorinlines.h"
#include "v_text.h"

#include "maploader/maploader.h"

//==========================================================================
//
// InitBlockMap
//
//==========================================================================

void MapLoader::InitPolyBlockMap ()
{
	int bmapwidth = Level->blockmap.bmapwidth;
	int bmapheight = Level->blockmap.bmapheight;

	Level->PolyBlockMap.Resize(bmapwidth*bmapheight);
	memset (Level->PolyBlockMap.Data(), 0, bmapwidth*bmapheight*sizeof(polyblock_t *));

	for(auto &poly : Level->Polyobjects)
	{
		poly.LinkPolyobj();
	}
}

//==========================================================================
//
// InitSideLists [RH]
//
// Group sides by vertex and collect side that are known to belong to a
// polyobject so that they can be initialized fast.
//==========================================================================

void MapLoader::InitSideLists ()
{
	auto &sides = Level->sides;
	for (unsigned i = 0; i < sides.Size(); ++i)
	{
		if (sides[i].linedef != NULL &&
			(sides[i].linedef->special == Polyobj_StartLine ||
				sides[i].linedef->special == Polyobj_ExplicitLine))
		{
			KnownPolySides.Push (i);
		}
	}
}

//==========================================================================
//
// AddPolyVert
//
// Helper function for IterFindPolySides()
//
//==========================================================================

static void AddPolyVert(TArray<uint32_t> &vnum, uint32_t vert)
{
	for (unsigned int i = vnum.Size() - 1; i-- != 0; )
	{
		if (vnum[i] == vert)
		{ // Already in the set. No need to add it.
			return;
		}
	}
	vnum.Push(vert);
}

//==========================================================================
//
// IterFindPolySides
//
// Beginning with the first vertex of the starting side, for each vertex
// in vnum, add all the sides that use it as a first vertex to the polyobj,
// and add all their second vertices to vnum. This continues until there
// are no new vertices in vnum.
//
//==========================================================================

void MapLoader::IterFindPolySides (FPolyObj *po, side_t *side)
{
	static TArray<uint32_t> vnum;
	unsigned int vnumat;

	assert(sidetemp.Size() > 0);

	vnum.Clear();
	vnum.Push(uint32_t(Index(side->V1())));
	vnumat = 0;

	while (vnum.Size() != vnumat)
	{
		uint32_t sidenum = sidetemp[vnum[vnumat++]].b.first;
		while (sidenum != NO_SIDE)
		{
			po->Sidedefs.Push(&Level->sides[sidenum]);
			AddPolyVert(vnum, uint32_t(Index(Level->sides[sidenum].V2())));
			sidenum = sidetemp[sidenum].b.next;
		}
	}
}


//==========================================================================
//
// SpawnPolyobj
//
//==========================================================================

static int posicmp(const void *a, const void *b)
{
	return (*(const side_t **)a)->linedef->args[1] - (*(const side_t **)b)->linedef->args[1];
}

void MapLoader::SpawnPolyobj (int index, int tag, int type)
{
	unsigned int ii;
	int i;
	FPolyObj *po = &Level->Polyobjects[index];
	po->Level = Level;

	for (ii = 0; ii < KnownPolySides.Size(); ++ii)
	{
		i = KnownPolySides[ii];
		if (i < 0)
		{
			continue;
		}
		po->bBlocked = false;
		po->bHasPortals = 0;

		side_t *sd = &Level->sides[i];
		
		if (sd->linedef->special == Polyobj_StartLine &&
			sd->linedef->args[0] == tag)
		{
			if (po->Sidedefs.Size() > 0)
			{
				Printf (TEXTCOLOR_RED "SpawnPolyobj: Polyobj %d already spawned.\n", tag);
				return;
			}
			else
			{
				sd->linedef->special = 0;
				sd->linedef->args[0] = 0;
				IterFindPolySides(&Level->Polyobjects[index], sd);
				po->MirrorNum = sd->linedef->args[1];
				po->crush = (type != SMT_PolySpawn) ? 3 : 0;
				po->bHurtOnTouch = (type == SMT_PolySpawnHurt);
				po->tag = tag;
				po->seqType = sd->linedef->args[2];
				if (po->seqType < 0 || po->seqType > 63)
				{
					po->seqType = 0;
				}
			}
			break;
		}
	}
	if (po->Sidedefs.Size() == 0)
	{ 
		// didn't find a polyobj through PO_LINE_START
		TArray<side_t *> polySideList;
		unsigned int psIndexOld;
		psIndexOld = po->Sidedefs.Size();

		for (ii = 0; ii < KnownPolySides.Size(); ++ii)
		{
			i = KnownPolySides[ii];

			if (i >= 0 &&
				Level->sides[i].linedef->special == Polyobj_ExplicitLine &&
				Level->sides[i].linedef->args[0] == tag)
			{
				if (!Level->sides[i].linedef->args[1])
				{
					Printf(TEXTCOLOR_RED "SpawnPolyobj: Explicit line missing order number in poly %d, linedef %d.\n", tag, Index(Level->sides[i].linedef));
					return;
				}
				else
				{
					po->Sidedefs.Push(&Level->sides[i]);
				}
			}
		}
		qsort(&po->Sidedefs[0], po->Sidedefs.Size(), sizeof(po->Sidedefs[0]), posicmp);
		if (po->Sidedefs.Size() > 0)
		{
			po->crush = (type != SMT_PolySpawn) ? 3 : 0;
			po->bHurtOnTouch = (type == SMT_PolySpawnHurt);
			po->tag = tag;
			po->seqType = po->Sidedefs[0]->linedef->args[3];
			po->MirrorNum = po->Sidedefs[0]->linedef->args[2];
		}
		else
		{
			Printf(TEXTCOLOR_RED "SpawnPolyobj: Poly %d does not exist\n", tag);
			return;
		}
	}

	validcount++;	
	for(unsigned int i=0; i<po->Sidedefs.Size(); i++)
	{
		line_t *l = po->Sidedefs[i]->linedef;

		if (l->validcount != validcount)
		{
			FLinePortal *port = l->getPortal();
			if (port && (port->mDefFlags & PORTF_PASSABLE))
			{
				int type = port->mType == PORTT_LINKED ? 2 : 1;
				if (po->bHasPortals < type) po->bHasPortals = (uint8_t)type;
			}
			l->validcount = validcount;
			po->Linedefs.Push(l);

			vertex_t *v = l->v1;
			int j;
			for(j = po->Vertices.Size() - 1; j >= 0; j--)
			{
				if (po->Vertices[j] == v) break;
			}
			if (j < 0) po->Vertices.Push(v);

			v = l->v2;
			for(j = po->Vertices.Size() - 1; j >= 0; j--)
			{
				if (po->Vertices[j] == v) break;
			}
			if (j < 0) po->Vertices.Push(v);

		}
	}
	po->Sidedefs.ShrinkToFit();
	po->Linedefs.ShrinkToFit();
	po->Vertices.ShrinkToFit();
}

//==========================================================================
//
// TranslateToStartSpot
//
//==========================================================================

void MapLoader::TranslateToStartSpot (int tag, const DVector2 &origin)
{
	FPolyObj *po;
	DVector2 delta;

	po = Level->GetPolyobj(tag);
	if (po == nullptr)
	{ // didn't match the tag with a polyobj tag
		Printf(TEXTCOLOR_RED "TranslateToStartSpot: Unable to match polyobj tag: %d\n", tag);
		return;
	}
	if (po->Sidedefs.Size() == 0)
	{
		Printf(TEXTCOLOR_RED "TranslateToStartSpot: Anchor point located without a StartSpot point: %d\n", tag);
		return;
	}
	po->OriginalPts.Resize(po->Sidedefs.Size());
	po->PrevPts.Resize(po->Sidedefs.Size());
	delta = origin - po->StartSpot.pos;

	for (unsigned i = 0; i < po->Sidedefs.Size(); i++)
	{
		po->Sidedefs[i]->Flags |= WALLF_POLYOBJ;
	}
	for (unsigned i = 0; i < po->Linedefs.Size(); i++)
	{
		po->Linedefs[i]->bbox[BOXTOP] -= delta.Y;
		po->Linedefs[i]->bbox[BOXBOTTOM] -= delta.Y;
		po->Linedefs[i]->bbox[BOXLEFT] -= delta.X;
		po->Linedefs[i]->bbox[BOXRIGHT] -= delta.X;
	}
	for (unsigned i = 0; i < po->Vertices.Size(); i++)
	{
		po->Vertices[i]->set(po->Vertices[i]->fX() - delta.X, po->Vertices[i]->fY() - delta.Y);
		po->OriginalPts[i].pos = po->Vertices[i]->fPos() - po->StartSpot.pos;
	}
	po->CalcCenter();
	// For compatibility purposes
	po->CenterSubsector = Level->PointInRenderSubsector(po->CenterSpot.pos);
}

//==========================================================================
//
// PO_Init
//
//==========================================================================

void MapLoader::PO_Init (void)
{
	int NumPolyobjs = 0;
	TArray<FMapThing *> polythings;
	for (auto &mthing : MapThingsConverted)
	{
		if (mthing.EdNum == 0 || mthing.EdNum == -1 || mthing.info == nullptr || mthing.info->Type != nullptr) continue;

		FDoomEdEntry *mentry = mthing.info;
		switch (mentry->Special)
		{
		case SMT_PolyAnchor:
		case SMT_PolySpawn:
		case SMT_PolySpawnCrush:
		case SMT_PolySpawnHurt:
			polythings.Push(&mthing);
			if (mentry->Special != SMT_PolyAnchor)
				NumPolyobjs++;
		}
	}

	int polyIndex;

	// [RH] Make this faster
	InitSideLists ();

	Level->Polyobjects.Resize(NumPolyobjs);
	for (auto &po : Level->Polyobjects) po.Level = Level;	// must be done before the init loop below.

	polyIndex = 0; // index polyobj number
	// Find the startSpot points, and spawn each polyobj
	for (int i=polythings.Size()-1; i >= 0; i--)
	{
		// 9301 (3001) = no crush, 9302 (3002) = crushing, 9303 = hurting touch
		int type = polythings[i]->info->Special;
		if (type >= SMT_PolySpawn && type <= SMT_PolySpawnHurt)
		{ 
			// Polyobj StartSpot Pt.
			Level->Polyobjects[polyIndex].StartSpot.pos = polythings[i]->pos;
			SpawnPolyobj(polyIndex, polythings[i]->angle, type);
			polyIndex++;
		} 
	}
	for (int i = polythings.Size() - 1; i >= 0; i--)
	{
		int type = polythings[i]->info->Special;
		if (type == SMT_PolyAnchor)
		{ 
			// Polyobj Anchor Pt.
			TranslateToStartSpot (polythings[i]->angle, polythings[i]->pos);
		}
	}

	// check for a startspot without an anchor point
	for (auto &poly : Level->Polyobjects)
	{
		if (poly.OriginalPts.Size() == 0)
		{
			Printf (TEXTCOLOR_RED "PO_Init: StartSpot located without an Anchor point: %d\n", poly.tag);
		}
	}
	InitPolyBlockMap();

	// [RH] Don't need the side lists anymore
	KnownPolySides.Reset();

	// mark all subsectors which have a seg belonging to a polyobj
	// These ones should not be rendered on the textured automap.
	for (auto &ss : Level->subsectors)
	{
		for(uint32_t j=0;j<ss.numlines; j++)
		{
			if (ss.firstline[j].sidedef != NULL &&
				ss.firstline[j].sidedef->Flags & WALLF_POLYOBJ)
			{
				ss.flags |= SSECF_POLYORG;
				break;
			}
		}
	}
	// clear all polyobj specials so that they do not obstruct using other lines.
	for (auto &line : Level->lines)
	{
		if (line.special == Polyobj_ExplicitLine || line.special == Polyobj_StartLine)
		{
			line.special = 0;
		}
	}
}

