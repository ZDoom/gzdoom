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

int FFlatVertexGenerator::CreateSubsectorVertices(subsector_t *sub, const secplane_t &plane, int floor)
{
	int idx = vbo_shadowdata.Reserve(sub->numlines);
	for(unsigned int k=0; k<sub->numlines; k++, idx++)
	{
		vbo_shadowdata[idx].SetFlatVertex(sub->firstline[k].v1, plane);
		if (sub->sector->transdoor && floor) vbo_shadowdata[idx].z -= 1.f;
	}
	return idx;
}

//==========================================================================
//
// Creates the vertices for one plane in one subsector
//
//==========================================================================

int FFlatVertexGenerator::CreateSectorVertices(sector_t *sec, const secplane_t &plane, int floor)
{
	int rt = vbo_shadowdata.Size();
	// First calculate the vertices for the sector itself
	for(int j=0; j<sec->subsectorcount; j++)
	{
		subsector_t *sub = sec->subsectors[j];
		CreateSubsectorVertices(sub, plane, floor);
	}
	return rt;
}

//==========================================================================
//
//
//
//==========================================================================

int FFlatVertexGenerator::CreateVertices(int h, sector_t *sec, const secplane_t &plane, int floor)
{
	// First calculate the vertices for the sector itself
	sec->vboheight[h] = sec->GetPlaneTexZ(h);
	sec->vboindex[h] = CreateSectorVertices(sec, plane, floor);

	// Next are all sectors using this one as heightsec
	TArray<sector_t *> &fakes = sec->e->FakeFloor.Sectors;
	for (unsigned g=0; g<fakes.Size(); g++)
	{
		sector_t *fsec = fakes[g];
		fsec->vboindex[2+h] = CreateSectorVertices(fsec, plane, false);
	}

	// and finally all attached 3D floors
	TArray<sector_t *> &xf = sec->e->XFloor.attached;
	for (unsigned g=0; g<xf.Size(); g++)
	{
		sector_t *fsec = xf[g];
		F3DFloor *ffloor = Find3DFloor(fsec, sec);

		if (ffloor != NULL && ffloor->flags & FF_RENDERPLANES)
		{
			bool dotop = (ffloor->top.model == sec) && (ffloor->top.isceiling == h);
			bool dobottom = (ffloor->bottom.model == sec) && (ffloor->bottom.isceiling == h);

			if (dotop || dobottom)
			{
				if (dotop) ffloor->top.vindex = vbo_shadowdata.Size();
				if (dobottom) ffloor->bottom.vindex = vbo_shadowdata.Size();
	
				CreateSectorVertices(fsec, plane, false);
			}
		}
	}
	sec->vbocount[h] = vbo_shadowdata.Size() - sec->vboindex[h];
	return sec->vboindex[h];
}


//==========================================================================
//
//
//
//==========================================================================

void FFlatVertexGenerator::CreateFlatVertices()
{
	for (int h = sector_t::floor; h <= sector_t::ceiling; h++)
	{
		for(auto &sec : level.sectors)
		{
			CreateVertices(h, &sec, sec.GetSecPlane(h), h == sector_t::floor);
		}
	}

	// We need to do a final check for Vavoom water and FF_FIX sectors.
	// No new vertices are needed here. The planes come from the actual sector
	for (auto &sec : level.sectors)
	{
		for(auto ff : sec.e->XFloor.ffloors)
		{
			if (ff->top.model == &sec)
			{
				ff->top.vindex = sec.vboindex[ff->top.isceiling];
			}
			if (ff->bottom.model == &sec)
			{
				ff->bottom.vindex = sec.vboindex[ff->top.isceiling];
			}
		}
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FFlatVertexGenerator::UpdatePlaneVertices(sector_t *sec, int plane, FFlatVertex *map)
{
	int startvt = sec->vboindex[plane];
	int countvt = sec->vbocount[plane];
	secplane_t &splane = sec->GetSecPlane(plane);
	FFlatVertex *vt = &vbo_shadowdata[startvt];
	FFlatVertex *mapvt = &map[startvt];
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
	CreateFlatVertices();
}

//==========================================================================
//
//
//
//==========================================================================

void FFlatVertexGenerator::CheckPlanes(sector_t *sector, FFlatVertex *map)
{
	if (sector->GetPlaneTexZ(sector_t::ceiling) != sector->vboheight[sector_t::ceiling])
	{
		UpdatePlaneVertices(sector, sector_t::ceiling, map);
		sector->vboheight[sector_t::ceiling] = sector->GetPlaneTexZ(sector_t::ceiling);
	}
	if (sector->GetPlaneTexZ(sector_t::floor) != sector->vboheight[sector_t::floor])
	{
		UpdatePlaneVertices(sector, sector_t::floor, map);
		sector->vboheight[sector_t::floor] = sector->GetPlaneTexZ(sector_t::floor);
	}
}

//==========================================================================
//
// checks the validity of all planes attached to this sector
// and updates them if possible.
//
//==========================================================================

void FFlatVertexGenerator::CheckUpdate(sector_t *sector, FFlatVertex *map)
{
	CheckPlanes(sector, map);
	sector_t *hs = sector->GetHeightSec();
	if (hs != NULL) CheckPlanes(hs, map);
	for (unsigned i = 0; i < sector->e->XFloor.ffloors.Size(); i++)
		CheckPlanes(sector->e->XFloor.ffloors[i]->model, map);
}
