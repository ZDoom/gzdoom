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
** hw_flatvertices.cpp
** Creates flat vertex data for hardware rendering.
**
**/

#include "doomtype.h"
#include "p_local.h"
#include "r_state.h"
#include "c_cvars.h"
#include "g_levellocals.h"
#include "flatvertices.h"


//==========================================================================
//
//
//
//==========================================================================

FFlatVertexGenerator::FFlatVertexGenerator(int width, int height)
{
	vbo_shadowdata.Resize(NUM_RESERVED);

	// the first quad is reserved for handling coordinates through uniforms.
	vbo_shadowdata[0].Set(0, 0, 0, 0, 0);
	vbo_shadowdata[1].Set(1, 0, 0, 0, 0);
	vbo_shadowdata[2].Set(2, 0, 0, 0, 0);
	vbo_shadowdata[3].Set(3, 0, 0, 0, 0);

	// and the second one for the fullscreen quad used for blend overlays.
	vbo_shadowdata[4].Set(0, 0, 0, 0, 0);
	vbo_shadowdata[5].Set(0, (float)height, 0, 0, 1);
	vbo_shadowdata[6].Set((float)width, 0, 0, 1, 0);
	vbo_shadowdata[7].Set((float)width, (float)height, 0, 1, 1);

	// and this is for the postprocessing copy operation
	vbo_shadowdata[8].Set(-1.0f, -1.0f, 0, 0.0f, 0.0f);
	vbo_shadowdata[9].Set(-1.0f, 1.0f, 0, 0.0f, 1.f);
	vbo_shadowdata[10].Set(1.0f, -1.0f, 0, 1.f, 0.0f);
	vbo_shadowdata[11].Set(1.0f, 1.0f, 0, 1.f, 1.f);

	// The next two are the stencil caps.
	vbo_shadowdata[12].Set(-32767.0f, 32767.0f, -32767.0f, 0, 0);
	vbo_shadowdata[13].Set(-32767.0f, 32767.0f, 32767.0f, 0, 0);
	vbo_shadowdata[14].Set(32767.0f, 32767.0f, 32767.0f, 0, 0);
	vbo_shadowdata[15].Set(32767.0f, 32767.0f, -32767.0f, 0, 0);

	vbo_shadowdata[16].Set(-32767.0f, -32767.0f, -32767.0f, 0, 0);
	vbo_shadowdata[17].Set(-32767.0f, -32767.0f, 32767.0f, 0, 0);
	vbo_shadowdata[18].Set(32767.0f, -32767.0f, 32767.0f, 0, 0);
	vbo_shadowdata[19].Set(32767.0f, -32767.0f, -32767.0f, 0, 0);
}

void FFlatVertexGenerator::OutputResized(int width, int height)
{
	vbo_shadowdata[4].Set(0, 0, 0, 0, 0);
	vbo_shadowdata[5].Set(0, (float)height, 0, 0, 1);
	vbo_shadowdata[6].Set((float)width, 0, 0, 1, 0);
	vbo_shadowdata[7].Set((float)width, (float)height, 0, 1, 1);
}

//==========================================================================
//
// Initialize a single vertex
//
//==========================================================================

void FFlatVertex::SetFlatVertex(vertex_t *vt, const secplane_t & plane)
{
	x = (float)vt->fX();
	y = (float)vt->fY();
	z = (float)plane.ZatPoint(vt);
	u = (float)vt->fX()/64.f;
	v = -(float)vt->fY()/64.f;
}

//==========================================================================
//
// Find a 3D floor
//
//==========================================================================

static F3DFloor *Find3DFloor(sector_t *target, sector_t *model)
{
	for(unsigned i=0; i<target->e->XFloor.ffloors.Size(); i++)
	{
		F3DFloor *ffloor = target->e->XFloor.ffloors[i];
		if (ffloor->model == model && !(ffloor->flags & FF_THISINSIDE)) return ffloor;
	}
	return NULL;
}

//==========================================================================
//
// Creates the vertices for one plane in one subsector
//
//==========================================================================

int FFlatVertexGenerator::CreateIndexedSubsectorVertices(subsector_t *sub, const secplane_t &plane, int floor, int vi, FFlatVertexGenerator::FIndexGenerationInfo &gen)
{
	if (sub->numlines < 3) return -1;

	int idx = ibo_data.Reserve((sub->numlines - 2) * 3);
	int idxc = idx;
	int firstndx = gen.GetIndex(sub->firstline[0].v1);
	int secondndx = gen.GetIndex(sub->firstline[1].v1);
	for (unsigned int k = 2; k<sub->numlines; k++)
	{
		auto ndx = gen.GetIndex(sub->firstline[k].v1);

		ibo_data[idx++] = vi + firstndx;
		ibo_data[idx++] = vi + secondndx;
		ibo_data[idx++] = vi + ndx;
		secondndx = ndx;
	}
	return idx;
}

//==========================================================================
//
// Creates the vertices for one plane in one subsector
//
//==========================================================================

int FFlatVertexGenerator::CreateIndexedSectorVertices(sector_t *sec, const secplane_t &plane, int floor, FFlatVertexGenerator::FIndexGenerationInfo &gen)
{
	int rt = ibo_data.Size();
	int vi = vbo_shadowdata.Reserve(gen.vertices.Size());
	float diff;

	// Create the actual vertices.
	if (sec->transdoor && floor) diff = -1.f;
	else diff = 0.f;
	for (unsigned i = 0; i < gen.vertices.Size(); i++)
	{
		vbo_shadowdata[vi + i].SetFlatVertex(gen.vertices[i], plane);
		vbo_shadowdata[vi + i].z += diff;
	}
	
	// Create the indices for the subsectors
	for (int j = 0; j<sec->subsectorcount; j++)
	{
		subsector_t *sub = sec->subsectors[j];
		CreateIndexedSubsectorVertices(sub, plane, floor, vi, gen);
	}
	sec->ibocount = ibo_data.Size() - rt;
	return rt;
}

//==========================================================================
//
//
//
//==========================================================================

int FFlatVertexGenerator::CreateIndexedVertices(int h, sector_t *sec, const secplane_t &plane, int floor, TArray<FFlatVertexGenerator::FIndexGenerationInfo> &gen)
{
	sec->vboindex[h] = vbo_shadowdata.Size();
	// First calculate the vertices for the sector itself
	sec->vboheight[h] = sec->GetPlaneTexZ(h);
	sec->iboindex[h] = CreateIndexedSectorVertices(sec, plane, floor, gen[sec->Index()]);

	// Next are all sectors using this one as heightsec
	TArray<sector_t *> &fakes = sec->e->FakeFloor.Sectors;
	for (unsigned g = 0; g < fakes.Size(); g++)
	{
		sector_t *fsec = fakes[g];
		fsec->iboindex[2 + h] = CreateIndexedSectorVertices(fsec, plane, false, gen[fsec->Index()]);
	}

	// and finally all attached 3D floors
	TArray<sector_t *> &xf = sec->e->XFloor.attached;
	for (unsigned g = 0; g < xf.Size(); g++)
	{
		sector_t *fsec = xf[g];
		F3DFloor *ffloor = Find3DFloor(fsec, sec);

		if (ffloor != NULL && ffloor->flags & FF_RENDERPLANES)
		{
			bool dotop = (ffloor->top.model == sec) && (ffloor->top.isceiling == h);
			bool dobottom = (ffloor->bottom.model == sec) && (ffloor->bottom.isceiling == h);

			if (dotop || dobottom)
			{
				auto ndx = CreateIndexedSectorVertices(fsec, plane, false, gen[fsec->Index()]);
				if (dotop) ffloor->top.vindex = ndx;
				if (dobottom) ffloor->bottom.vindex = ndx;
			}
		}
	}
	sec->vbocount[h] = vbo_shadowdata.Size() - sec->vboindex[h];
	return sec->iboindex[h];
}


//==========================================================================
//
//
//
//==========================================================================

void FFlatVertexGenerator::CreateIndexedFlatVertices()
{
	TArray<FIndexGenerationInfo> gen;
	gen.Resize(level.sectors.Size());
	// This must be generated up front so that the following code knows how many vertices a sector contains.
	for (unsigned i = 0; i < level.sectors.Size(); i++)
	{
		for (int j = 0; j < level.sectors[i].subsectorcount; j++)
		{
			auto sub = level.sectors[i].subsectors[j];
			for (unsigned k = 0; k < sub->numlines; k++)
			{
				auto vert = sub->firstline[k].v1;
				gen[i].AddVertex(vert);
			}
		}
	}
	for (int h = sector_t::floor; h <= sector_t::ceiling; h++)
	{
		for (auto &sec : level.sectors)
		{
			CreateIndexedVertices(h, &sec, sec.GetSecPlane(h), h == sector_t::floor, gen);
		}
	}

	// We need to do a final check for Vavoom water and FF_FIX sectors.
	// No new vertices are needed here. The planes come from the actual sector
	for (auto &sec : level.sectors)
	{
		for (auto ff : sec.e->XFloor.ffloors)
		{
			if (ff->top.model == &sec)
			{
				ff->top.vindex = sec.iboindex[ff->top.isceiling];
			}
			if (ff->bottom.model == &sec)
			{
				ff->bottom.vindex = sec.iboindex[ff->top.isceiling];
			}
		}
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FFlatVertexGenerator::UpdatePlaneVertices(sector_t *sec, int plane)
{
	int startvt = sec->vboindex[plane];
	int countvt = sec->vbocount[plane];
	secplane_t &splane = sec->GetSecPlane(plane);
	FFlatVertex *vt = &vbo_shadowdata[startvt];
	FFlatVertex *mapvt = &mMap[startvt];
	for(int i=0; i<countvt; i++, vt++, mapvt++)
	{
		vt->z = (float)splane.ZatPoint(vt->x, vt->y);
		if (plane == sector_t::floor && sec->transdoor) vt->z -= 1;
		mapvt->z = vt->z;
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FFlatVertexGenerator::CreateVertices()
{
	vbo_shadowdata.Resize(NUM_RESERVED);
	CreateIndexedFlatVertices();
}

//==========================================================================
//
//
//
//==========================================================================

void FFlatVertexGenerator::CheckPlanes(sector_t *sector)
{
	if (sector->GetPlaneTexZ(sector_t::ceiling) != sector->vboheight[sector_t::ceiling])
	{
		UpdatePlaneVertices(sector, sector_t::ceiling);
		sector->vboheight[sector_t::ceiling] = sector->GetPlaneTexZ(sector_t::ceiling);
	}
	if (sector->GetPlaneTexZ(sector_t::floor) != sector->vboheight[sector_t::floor])
	{
		UpdatePlaneVertices(sector, sector_t::floor);
		sector->vboheight[sector_t::floor] = sector->GetPlaneTexZ(sector_t::floor);
	}
}

//==========================================================================
//
// checks the validity of all planes attached to this sector
// and updates them if possible.
//
//==========================================================================

void FFlatVertexGenerator::CheckUpdate(sector_t *sector)
{
	CheckPlanes(sector);
	sector_t *hs = sector->GetHeightSec();
	if (hs != NULL) CheckPlanes(hs);
	for (unsigned i = 0; i < sector->e->XFloor.ffloors.Size(); i++)
		CheckPlanes(sector->e->XFloor.ffloors[i]->model);
}
