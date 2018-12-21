// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2005-2016 Christoph Oelckers
// All rights reserved.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//--------------------------------------------------------------------------
//
/*
** gl_setup.cpp
** Initializes the data structures required by the hardware renderer to handle
** render hacks and optimization.
**
**/

#include "doomtype.h"
#include "p_local.h"
#include "p_spec.h"
#include "p_lnspec.h"
#include "c_dispatch.h"
#include "r_sky.h"
#include "p_setup.h"
#include "g_levellocals.h"

//==========================================================================
//
// Map section generation
// Map sections are physically separated parts of the map.
// If the player is in section A, any map part in other sections can
// often be quickly discarded to improve performance.
//
//==========================================================================

struct MapSectionGenerator
{
	struct cvertex_t
	{
		double X, Y;

		operator int() const { return xs_FloorToInt(X) + 65536 * xs_FloorToInt(Y); }
		bool operator!= (const cvertex_t &other) const { return fabs(X - other.X) >= EQUAL_EPSILON || fabs(Y - other.Y) >= EQUAL_EPSILON; }
		cvertex_t& operator =(const vertex_t *v) { X = v->fX(); Y = v->fY(); return *this; }
	};

	typedef TMap<cvertex_t, int> FSectionVertexMap;
	TArray<subsector_t *> MapSectionCollector;

	//==========================================================================
	//
	// 
	//
	//==========================================================================

	void DoSetMapSection(subsector_t *sub, int num)
	{
		MapSectionCollector.Resize(1);
		MapSectionCollector[0] = sub;
		sub->mapsection = num;
		for (unsigned a = 0; a < MapSectionCollector.Size(); a++)
		{
			sub = MapSectionCollector[a];
			for (uint32_t i = 0; i < sub->numlines; i++)
			{
				seg_t * seg = sub->firstline + i;

				if (seg->PartnerSeg)
				{
					subsector_t * sub2 = seg->PartnerSeg->Subsector;

					if (sub2->mapsection != num)
					{
						assert(sub2->mapsection == 0);
						sub2->mapsection = num;
						MapSectionCollector.Push(sub2);
					}
				}
			}
		}
		MapSectionCollector.Clear();
	}

	//==========================================================================
	//
	// Merge sections. This is needed in case the map contains errors
	// like overlapping lines resulting in abnormal subsectors.
	//
	// This function ensures that any vertex position can only be in one section.
	//
	//==========================================================================


	int MergeMapSections(int num)
	{
		FSectionVertexMap vmap;
		FSectionVertexMap::Pair *pair;
		TArray<int> sectmap;
		TArray<bool> sectvalid;
		sectmap.Resize(num);
		sectvalid.Resize(num);
		for (int i = 0; i < num; i++)
		{
			sectmap[i] = -1;
			sectvalid[i] = true;
		}
		int mergecount = 1;


		cvertex_t vt;

		// first step: Set mapsection for all vertex positions.
		for (auto &seg : level.segs)
		{
			int section = seg.Subsector->mapsection;
			for (int j = 0; j < 2; j++)
			{
				vt = j == 0 ? seg.v1 : seg.v2;
				vmap[vt] = section;
			}
		}

		// second step: Check if any seg references more than one mapsection, either by subsector or by vertex
		for (auto &seg : level.segs)
		{
			int section = seg.Subsector->mapsection;
			for (int j = 0; j < 2; j++)
			{
				vt = j == 0 ? seg.v1 : seg.v2;
				int vsection = vmap[vt];

				if (vsection != section)
				{
					// These 2 sections should be merged
					for (auto &sub : level.subsectors)
					{
						if (sub.mapsection == vsection) sub.mapsection = section;
					}
					FSectionVertexMap::Iterator it(vmap);
					while (it.NextPair(pair))
					{
						if (pair->Value == vsection) pair->Value = section;
					}
					sectvalid[vsection - 1] = false;
				}
			}
		}
		for (int i = 0; i < num; i++)
		{
			if (sectvalid[i]) sectmap[i] = mergecount++;
		}
		for (auto &sub : level.subsectors)
		{
			sub.mapsection = sectmap[sub.mapsection - 1];
			assert(sub.mapsection != -1);
		}
		return mergecount - 1;
	}

	//==========================================================================
	//
	// 
	//
	//==========================================================================

	void SetMapSections()
	{
		bool set;
		int num = 0;
		do
		{
			set = false;
			for (auto &sub : level.subsectors)
			{
				if (sub.mapsection == 0)
				{
					num++;
					DoSetMapSection(&sub, num);
					set = true;
					break;
				}
			}
		}
		while (set);
		num = MergeMapSections(num);
		level.NumMapSections = num;
	#ifdef DEBUG
		Printf("%d map sections found\n", num);
	#endif
	}
};

//==========================================================================
//
// prepare subsectors for GL rendering
// - analyze rendering hacks using open sectors
// - assign a render sector (for self referencing sectors)
// - calculate a bounding box
//
//==========================================================================

static void SpreadHackedFlag(subsector_t * sub)
{
	// The subsector pointer hasn't been set yet!
	for(uint32_t i=0;i<sub->numlines;i++)
	{
		seg_t * seg = sub->firstline + i;

		if (seg->PartnerSeg)
		{
			subsector_t * sub2 = seg->PartnerSeg->Subsector;

			if (!(sub2->hacked&1) && sub2->render_sector == sub->render_sector)
			{
				sub2->hacked|=1;
				sub->hacked &= ~4;
				SpreadHackedFlag (sub2);
			}
		}
	}
}


//==========================================================================
//
// 
//
//==========================================================================

static void PrepareSectorData()
{
	TArray<subsector_t *> undetermined;

	// now group the subsectors by sector
	subsector_t ** subsectorbuffer = new subsector_t * [level.subsectors.Size()];

	for (auto &sub : level.subsectors)
	{
		sub.render_sector->subsectorcount++;
	}

	for (auto &sec : level.sectors) 
	{
		sec.subsectors = subsectorbuffer;
		subsectorbuffer += sec.subsectorcount;
		sec.subsectorcount = 0;
	}
	
	for (auto &sub : level.subsectors)
	{
		sub.render_sector->subsectors[sub.render_sector->subsectorcount++] = &sub;
	}

	// marks all malformed subsectors so rendering tricks using them can be handled more easily
	for (auto &sub : level.subsectors)
	{
		if (sub.sector == sub.render_sector)
		{
			seg_t * seg = sub.firstline;
			for(uint32_t j=0;j<sub.numlines;j++)
			{
				if (!(sub.hacked&1) && seg[j].linedef==0 && 
						seg[j].PartnerSeg!=NULL && 
						sub.render_sector != seg[j].PartnerSeg->Subsector->render_sector)
				{
					DPrintf(DMSG_NOTIFY, "Found hack: (%f,%f) (%f,%f)\n", seg[j].v1->fX(), seg[j].v1->fY(), seg[j].v2->fX(), seg[j].v2->fY());
					sub.hacked|=5;
					SpreadHackedFlag(&sub);
				}
				if (seg[j].PartnerSeg==NULL) sub.hacked|=2;	// used for quick termination checks
			}
		}
	}
	MapSectionGenerator msg;
	msg.SetMapSections();
}

//==========================================================================
//
// Some processing for transparent door hacks using a floor raised by 1 map unit
// - This will be used to lower the floor of such sectors by one map unit
//
//==========================================================================

static void PrepareTransparentDoors(sector_t * sector)
{
	bool solidwall=false;
	unsigned int notextures=0;
	unsigned int nobtextures=0;
	unsigned int selfref=0;
	sector_t * nextsec=NULL;

	P_Recalculate3DFloors(sector);
	if (sector->subsectorcount==0) return;

	sector->transdoorheight=sector->GetPlaneTexZ(sector_t::floor);
	sector->transdoor= !(sector->e->XFloor.ffloors.Size() || sector->heightsec || sector->floorplane.isSlope());

	if (sector->transdoor)
	{
		for (auto ln : sector->Lines)
		{
			if (ln->frontsector == ln->backsector)
			{
				selfref++;
				continue;
			}

			sector_t * sec=getNextSector(ln, sector);
			if (sec==NULL) 
			{
				solidwall=true;
				continue;
			}
			else
			{
				nextsec=sec;

				int side = ln->sidedef[0]->sector == sec;

				if (sector->GetPlaneTexZ(sector_t::floor)!=sec->GetPlaneTexZ(sector_t::floor)+1. || sec->floorplane.isSlope())
				{
					sector->transdoor=false;
					return;
				}
				if (!ln->sidedef[1-side]->GetTexture(side_t::top).isValid()) notextures++;
				if (!ln->sidedef[1-side]->GetTexture(side_t::bottom).isValid()) nobtextures++;
			}
		}
		if (sector->GetTexture(sector_t::ceiling)==skyflatnum)
		{
			sector->transdoor=false;
			return;
		}

		if (selfref+nobtextures!=sector->Lines.Size())
		{
			sector->transdoor=false;
		}

		if (selfref+notextures!=sector->Lines.Size())
		{
			// This is a crude attempt to fix an incorrect transparent door effect I found in some
			// WolfenDoom maps but considering the amount of code required to handle it I left it in.
			// Do this only if the sector only contains one-sided walls or ones with no lower texture.
			if (solidwall)
			{
				if (solidwall+nobtextures+selfref==sector->Lines.Size() && nextsec)
				{
					sector->heightsec=nextsec;
					sector->heightsec->MoreFlags=0;
				}
				sector->transdoor=false;
			}
		}
	}
}

//==========================================================================
//
// 
//
//==========================================================================

static void AddToVertex(const sector_t * sec, TArray<int> & list)
{
	int secno = sec->Index();

	for(unsigned i=0;i<list.Size();i++)
	{
		if (list[i]==secno) return;
	}
	list.Push(secno);
}

//==========================================================================
//
// Attach sectors to vertices - used to generate vertex height lists
//
//==========================================================================

static void InitVertexData()
{
	TArray<TArray<int>> vt_sectorlists(level.vertexes.Size(), true);

	for(auto &line : level.lines)
	{
		for(int j = 0; j < 2; ++j)
		{
			vertex_t * v = j==0? line.v1 : line.v2;

			for(int k = 0; k < 2; ++k)
			{
				sector_t * sec = k==0? line.frontsector : line.backsector;

				if (sec)
				{
					extsector_t::xfloor &x = sec->e->XFloor;

					AddToVertex(sec, vt_sectorlists[v->Index()]);
					if (sec->heightsec) AddToVertex(sec->heightsec, vt_sectorlists[v->Index()]);
				}
			}
		}
	}

	for(unsigned i = 0; i < level.vertexes.Size(); ++i)
	{
		auto &vert = level.vertexes[i];
		int cnt = vt_sectorlists[i].Size();

		vert.dirty = true;
		vert.numheights=0;
		if (cnt>1)
		{
			vert.numsectors= cnt;
			vert.sectors=new sector_t*[cnt];
			vert.heightlist = new float[cnt*2];
			for(int j=0;j<cnt;j++)
			{
				vert.sectors[j] = &level.sectors[vt_sectorlists[i][j]];
			}
		}
		else
		{
			vert.numsectors=0;
		}
	}
}

//==========================================================================
//
//
//
//==========================================================================

static void GetSideVertices(int sdnum, DVector2 *v1, DVector2 *v2)
{
	line_t *ln = level.sides[sdnum].linedef;
	if (ln->sidedef[0] == &level.sides[sdnum])
	{
		*v1 = ln->v1->fPos();
		*v2 = ln->v2->fPos();
	}
	else
	{
		*v2 = ln->v1->fPos();
		*v1 = ln->v2->fPos();
	}
}

static int segcmp(const void *a, const void *b)
{
	seg_t *A = *(seg_t**)a;
	seg_t *B = *(seg_t**)b;
	return xs_RoundToInt(FRACUNIT*(A->sidefrac - B->sidefrac));
}

//==========================================================================
//
// Group segs to sidedefs
//
//==========================================================================

static void PrepareSegs()
{
	auto numsides = level.sides.Size();
	TArray<int> segcount(numsides, true);
	int realsegs = 0;

	// count the segs
	memset(segcount.Data(), 0, numsides * sizeof(int));

	for(auto &seg : level.segs)
	{
		if (seg.sidedef == nullptr) continue;	// miniseg
		int sidenum = seg.sidedef->Index();

		realsegs++;
		segcount[sidenum]++;
		DVector2 sidestart, sideend, segend = seg.v2->fPos();
		GetSideVertices(sidenum, &sidestart, &sideend);

		sideend -=sidestart;
		segend -= sidestart;

		seg.sidefrac = float(segend.Length() / sideend.Length());
	}

	// allocate memory
	level.sides[0].segs = new seg_t*[realsegs];
	level.sides[0].numsegs = 0;

	for(unsigned i = 1; i < numsides; i++)
	{
		level.sides[i].segs = level.sides[i-1].segs + segcount[i-1];
		level.sides[i].numsegs = 0;
	}

	// assign the segs
	for (auto &seg : level.segs)
	{
		if (seg.sidedef != NULL) seg.sidedef->segs[seg.sidedef->numsegs++] = &seg;
	}

	// sort the segs
	for(unsigned i = 0; i < numsides; i++)
	{
		if (level.sides[i].numsegs > 1) qsort(level.sides[i].segs, level.sides[i].numsegs, sizeof(seg_t*), segcmp);
	}
}

//==========================================================================
//
// Initialize the level data for the GL renderer
//
//==========================================================================

void InitRenderInfo()
{
	PrepareSegs();
	PrepareSectorData();
	InitVertexData();
	TArray<int> checkmap(level.vertexes.Size());
	memset(checkmap.Data(), -1, sizeof(int)*level.vertexes.Size());
	for(auto &sec : level.sectors) 
	{
		int i = sec.sectornum;
		PrepareTransparentDoors(&sec);

		// This ignores vertices only used for seg splitting because those aren't needed here
		for(auto l : sec.Lines)
		{
			if (l->sidedef[0]->Flags & WALLF_POLYOBJ) continue;	// don't bother with polyobjects

			int vtnum1 = l->v1->Index();
			int vtnum2 = l->v2->Index();

			if (checkmap[vtnum1] < i)
			{
				checkmap[vtnum1] = i;
				sec.e->vertices.Push(&level.vertexes[vtnum1]);
				level.vertexes[vtnum1].dirty = true;
			}

			if (checkmap[vtnum2] < i)
			{
				checkmap[vtnum2] = i;
				sec.e->vertices.Push(&level.vertexes[vtnum2]);
				level.vertexes[vtnum2].dirty = true;
			}
		}
	}
}


//==========================================================================
//
// FixMinisegReferences
//
// Sometimes it can happen that two matching minisegs do not have their partner set.
// Fix that here.
//
//==========================================================================

void FixMinisegReferences()
{
	TArray<seg_t *> bogussegs;

	for (unsigned i = 0; i < level.segs.Size(); i++)
	{
		if (level.segs[i].sidedef == nullptr && level.segs[i].PartnerSeg == nullptr)
		{
			bogussegs.Push(&level.segs[i]);
		}
	}

	for (unsigned i = 0; i < bogussegs.Size(); i++)
	{
		auto seg1 = bogussegs[i];
		seg_t *pick = nullptr;
		unsigned int picki = -1;

		// Try to fix the reference: If there's exactly one other seg in the set which matches as a partner link those two segs together.
		for (unsigned j = i + 1; j < bogussegs.Size(); j++)
		{
			auto seg2 = bogussegs[j];
			if (seg1->v1 == seg2->v2 && seg2->v1 == seg1->v2 && seg1->Subsector->render_sector == seg2->Subsector->render_sector)
			{
				pick = seg2;
				picki = j;
				break;
			}
		}
		if (pick)
		{
			DPrintf(DMSG_NOTIFY, "Linking miniseg pair from (%2.3f, %2.3f) -> (%2.3f, %2.3f) in sector %d\n", pick->v2->fX(), pick->v2->fY(), pick->v1->fX(), pick->v1->fY(), pick->frontsector->Index());
			pick->PartnerSeg = seg1;
			seg1->PartnerSeg = pick;
			assert(seg1->v1 == pick->v2 && pick->v1 == seg1->v2);
			bogussegs.Delete(picki);
			bogussegs.Delete(i);
			i--;
		}
	}
}

//==========================================================================
//
// FixHoles
//
// ZDBSP can leave holes in the node tree on extremely detailed maps.
// To help out the triangulator these are filled with dummy subsectors
// so that it can process the area correctly.
//
//==========================================================================

void FixHoles()
{
	TArray<seg_t *> bogussegs;
	TArray<TArray<seg_t *>> segloops;

	for (unsigned i = 0; i < level.segs.Size(); i++)
	{
		if (level.segs[i].sidedef == nullptr && level.segs[i].PartnerSeg == nullptr)
		{
			bogussegs.Push(&level.segs[i]);
		}
	}

	while (bogussegs.Size() > 0)
	{
		segloops.Reserve(1);
		auto *segloop = &segloops.Last();

		seg_t *startseg;
		seg_t *checkseg;
		while (bogussegs.Size() > 0)
		{
			bool foundsome = false;
			if (segloop->Size() == 0)
			{
				bogussegs.Pop(startseg);
				segloop->Push(startseg);
				checkseg = startseg;
			}
			for (unsigned i = 0; i < bogussegs.Size(); i++)
			{
				auto seg1 = bogussegs[i];

				if (seg1->v1 == checkseg->v2 && seg1->Subsector->render_sector == checkseg->Subsector->render_sector)
				{
					foundsome = true;
					segloop->Push(seg1);
					bogussegs.Delete(i);
					i--;
					checkseg = seg1;

					if (seg1->v2 == startseg->v1)
					{
						// The loop is complete. Start a new one
						segloops.Reserve(1);
						segloop = &segloops.Last();
					}
				}
			}
			if (!foundsome)
			{
				if ((*segloop)[0]->v1 != segloop->Last()->v2)
				{
					// There was no connected seg, leaving an unclosed loop. 
					// Clear this and continue looking.
					segloop->Clear();
				}
			}
		}
		for (unsigned i = 0; i < segloops.Size(); i++)
		{
			if (segloops[i].Size() == 0)
			{
				segloops.Delete(i);
				i--;
			}
		}

		// Add dummy entries to the level's seg and subsector arrays
		if (segloops.Size() > 0)
		{
			// cound the number of segs to add.
			unsigned segcount = 0;
			for (auto &segloop : segloops)
				segcount += segloop.Size();

			seg_t *oldsegstartptr = &level.segs[0];
			subsector_t *oldssstartptr = &level.subsectors[0];

			unsigned newsegstart = level.segs.Reserve(segcount);
			unsigned newssstart = level.subsectors.Reserve(segloops.Size());

			seg_t *newsegstartptr = &level.segs[0];
			subsector_t *newssstartptr = &level.subsectors[0];

			// Now fix all references to these in the level data.
			// Note that the Index() method does not work here due to the reallocation.
			for (auto &seg : level.segs)
			{
				if (seg.PartnerSeg) seg.PartnerSeg = newsegstartptr + (seg.PartnerSeg - oldsegstartptr);
				seg.Subsector = newssstartptr + (seg.Subsector - oldssstartptr);
			}
			for (auto &sub : level.subsectors)
			{
				sub.firstline = newsegstartptr + (sub.firstline - oldsegstartptr);
			}
			for (auto &node : level.nodes)
			{
				// How hideous... :(
				for (auto & p : node.children)
				{
					auto intp = (intptr_t)p;
					if (intp & 1)
					{
						subsector_t *sp = (subsector_t*)(intp - 1);
						sp = newssstartptr + (sp - oldssstartptr);
						intp = intptr_t(sp) + 1;
						p = (void*)intp;
					}
				}
			}
			for (auto &segloop : segloops)
			{
				for (auto &seg : segloop)
				{
					seg = newsegstartptr + (seg - oldsegstartptr);
				}
			}

			// The seg lists in the sidedefs and the subsector lists in the sectors are not set yet when this gets called.

			// Add the new data. This doesn't care about convexity. It is never directly used to generate a primitive.
			for (auto &segloop : segloops)
			{
				DPrintf(DMSG_NOTIFY, "Adding dummy subsector for sector %d\n", segloop[0]->Subsector->render_sector->Index());

				subsector_t &sub = level.subsectors[newssstart++];
				memset(&sub, 0, sizeof(sub));
				sub.sector = segloop[0]->frontsector;
				sub.render_sector = segloop[0]->Subsector->render_sector;
				sub.numlines = segloop.Size();
				sub.firstline = &level.segs[newsegstart];
				sub.flags = SSECF_HOLE;

				for (auto otherseg : segloop)
				{
					DPrintf(DMSG_NOTIFY, "   Adding seg from (%2.3f, %2.3f) -> (%2.3f, %2.3f)\n", otherseg->v2->fX(), otherseg->v2->fY(), otherseg->v1->fX(), otherseg->v1->fY());
					seg_t &seg = level.segs[newsegstart++];
					memset(&seg, 0, sizeof(seg));
					seg.v1 = otherseg->v2;
					seg.v2 = otherseg->v1;
					seg.frontsector = seg.backsector = otherseg->backsector = otherseg->frontsector;
					seg.PartnerSeg = otherseg;
					otherseg->PartnerSeg = &seg;
					seg.Subsector = &sub;
				}
			}
		}
	}
}

//==========================================================================
//
// ReportUnpairedMinisegs
//
// Debug routine
// reports all unpaired minisegs that couldn't be fixed by either 
// explicitly pairing them or combining them to a dummy subsector
//
//==========================================================================

void ReportUnpairedMinisegs()
{
	int bogus = 0;
	for (unsigned i = 0; i < level.segs.Size(); i++)
	{
		if (level.segs[i].sidedef == nullptr && level.segs[i].PartnerSeg == nullptr)
		{
			Printf("Unpaired miniseg %d, sector %d, (%d: %2.6f, %2.6f) -> (%d: %2.6f, %2.6f)\n",
				i, level.segs[i].Subsector->render_sector->Index(),
				level.segs[i].v1->Index(), level.segs[i].v1->fX(), level.segs[i].v1->fY(),
				level.segs[i].v2->Index(), level.segs[i].v2->fX(), level.segs[i].v2->fY());
			bogus++;
		}
	}
	if (bogus > 0) Printf("%d unpaired minisegs found\n", bogus);
}
//==========================================================================
//
//
//
//==========================================================================

CCMD(listmapsections)
{
	for (int i = 0; i < 100; i++)
	{
		for (auto &sub : level.subsectors)
		{
			if (sub.mapsection == i)
			{
				Printf("Mapsection %d, sector %d, line %d\n", i, sub.render_sector->Index(), sub.firstline->linedef->Index());
				break;
			}
		}
	}
}

CCMD(listbadminisegs)
{
	ReportUnpairedMinisegs();
}
