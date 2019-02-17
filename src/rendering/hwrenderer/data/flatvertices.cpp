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
#include "v_video.h"
#include "cmdlib.h"
#include "hwrenderer/data/buffers.h"
#include "hwrenderer/scene/hw_renderstate.h"

//==========================================================================
//
//
//
//==========================================================================

FFlatVertexBuffer::FFlatVertexBuffer(int width, int height)
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

	mVertexBuffer = screen->CreateVertexBuffer();
	mIndexBuffer = screen->CreateIndexBuffer();

	unsigned int bytesize = BUFFER_SIZE * sizeof(FFlatVertex);
	mVertexBuffer->SetData(bytesize, nullptr, false);

	static const FVertexBufferAttribute format[] = {
		{ 0, VATTR_VERTEX, VFmt_Float3, (int)myoffsetof(FFlatVertex, x) },
		{ 0, VATTR_TEXCOORD, VFmt_Float2, (int)myoffsetof(FFlatVertex, u) }
	};
	mVertexBuffer->SetFormat(1, 2, sizeof(FFlatVertex), format);

	mIndex = mCurIndex = 0;
	mNumReserved = NUM_RESERVED;
	Copy(0, NUM_RESERVED);
}

//==========================================================================
//
//
//
//==========================================================================

FFlatVertexBuffer::~FFlatVertexBuffer()
{
	delete mIndexBuffer;
	delete mVertexBuffer;
	mIndexBuffer = nullptr;
	mVertexBuffer = nullptr;
}

//==========================================================================
//
//
//
//==========================================================================

void FFlatVertexBuffer::OutputResized(int width, int height)
{
	vbo_shadowdata[4].Set(0, 0, 0, 0, 0);
	vbo_shadowdata[5].Set(0, (float)height, 0, 0, 1);
	vbo_shadowdata[6].Set((float)width, 0, 0, 1, 0);
	vbo_shadowdata[7].Set((float)width, (float)height, 0, 1, 1);
	Copy(4, 4);
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

int FFlatVertexBuffer::CreateIndexedSectorVertices(sector_t *sec, const secplane_t &plane, int floor, VertexContainer &verts)
{
	unsigned vi = vbo_shadowdata.Reserve(verts.vertices.Size());
	float diff;

	// Create the actual vertices.
	if (sec->transdoor && floor) diff = -1.f;
	else diff = 0.f;
	for (unsigned i = 0; i < verts.vertices.Size(); i++)
	{
		vbo_shadowdata[vi + i].SetFlatVertex(verts.vertices[i].vertex, plane);
		vbo_shadowdata[vi + i].z += diff;
	}

	unsigned rt = ibo_data.Reserve(verts.indices.Size());
	for (unsigned i = 0; i < verts.indices.Size(); i++)
	{
		ibo_data[rt + i] = vi + verts.indices[i];
	}
	return (int)rt;
}

//==========================================================================
//
//
//
//==========================================================================

int FFlatVertexBuffer::CreateIndexedVertices(int h, sector_t *sec, const secplane_t &plane, int floor, VertexContainers &verts)
{
	sec->vboindex[h] = vbo_shadowdata.Size();
	// First calculate the vertices for the sector itself
	sec->vboheight[h] = sec->GetPlaneTexZ(h);
    sec->ibocount = verts[sec->Index()].indices.Size();
	sec->iboindex[h] = CreateIndexedSectorVertices(sec, plane, floor, verts[sec->Index()]);

	// Next are all sectors using this one as heightsec
	TArray<sector_t *> &fakes = sec->e->FakeFloor.Sectors;
	for (unsigned g = 0; g < fakes.Size(); g++)
	{
		sector_t *fsec = fakes[g];
		fsec->iboindex[2 + h] = CreateIndexedSectorVertices(fsec, plane, false, verts[fsec->Index()]);
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
				auto ndx = CreateIndexedSectorVertices(fsec, plane, false, verts[fsec->Index()]);
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

void FFlatVertexBuffer::CreateIndexedFlatVertices(TArray<sector_t> &sectors)
{
    auto verts = BuildVertices(sectors);

	int i = 0;
	/*
	for (auto &vert : verts)
	{
		Printf(PRINT_LOG, "Sector %d\n", i);
		Printf(PRINT_LOG, "%d vertices, %d indices\n", vert.vertices.Size(), vert.indices.Size());
		int j = 0;
		for (auto &v : vert.vertices)
		{
			Printf(PRINT_LOG, "    %d: (%2.3f, %2.3f)\n", j++, v.vertex->fX(), v.vertex->fY());
		}
		for (unsigned i=0;i<vert.indices.Size();i+=3)
		{
			Printf(PRINT_LOG, "     %d, %d, %d\n", vert.indices[i], vert.indices[i + 1], vert.indices[i + 2]);
		}

		i++;
	}
	*/

    
	for (int h = sector_t::floor; h <= sector_t::ceiling; h++)
	{
		for (auto &sec : sectors)
		{
			CreateIndexedVertices(h, &sec, sec.GetSecPlane(h), h == sector_t::floor, verts);
		}
	}

	// We need to do a final check for Vavoom water and FF_FIX sectors.
	// No new vertices are needed here. The planes come from the actual sector
	for (auto &sec : sectors)
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

void FFlatVertexBuffer::UpdatePlaneVertices(sector_t *sec, int plane)
{
	int startvt = sec->vboindex[plane];
	int countvt = sec->vbocount[plane];
	secplane_t &splane = sec->GetSecPlane(plane);
	FFlatVertex *vt = &vbo_shadowdata[startvt];
	FFlatVertex *mapvt = GetBuffer(startvt);
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

void FFlatVertexBuffer::CreateVertices(TArray<sector_t> &sectors)
{
	vbo_shadowdata.Resize(NUM_RESERVED);
	CreateIndexedFlatVertices(sectors);
}

//==========================================================================
//
//
//
//==========================================================================

void FFlatVertexBuffer::CheckPlanes(sector_t *sector)
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

void FFlatVertexBuffer::CheckUpdate(sector_t *sector)
{
	CheckPlanes(sector);
	sector_t *hs = sector->GetHeightSec();
	if (hs != NULL) CheckPlanes(hs);
	for (unsigned i = 0; i < sector->e->XFloor.ffloors.Size(); i++)
		CheckPlanes(sector->e->XFloor.ffloors[i]->model);
}

//==========================================================================
//
//
//
//==========================================================================

std::pair<FFlatVertex *, unsigned int> FFlatVertexBuffer::AllocVertices(unsigned int count)
{
	FFlatVertex *p = GetBuffer();
	auto index = mCurIndex.fetch_add(count);
	auto offset = index;
	if (index + count >= BUFFER_SIZE_TO_USE)
	{
		// If a single scene needs 2'000'000 vertices there must be something very wrong. 
		I_FatalError("Out of vertex memory. Tried to allocate more than %u vertices for a single frame", index + count);
	}
	return std::make_pair(p, index);
}

//==========================================================================
//
//
//
//==========================================================================

void FFlatVertexBuffer::Copy(int start, int count)
{
	Map();
	memcpy(GetBuffer(start), &vbo_shadowdata[0], count * sizeof(FFlatVertex));
	Unmap();
}

//==========================================================================
//
//
//
//==========================================================================

void FFlatVertexBuffer::CreateVBO(TArray<sector_t> &sectors)
{
	vbo_shadowdata.Resize(mNumReserved);
	FFlatVertexBuffer::CreateVertices(sectors);
	mCurIndex = mIndex = vbo_shadowdata.Size();
	Copy(0, mIndex);
	mIndexBuffer->SetData(ibo_data.Size() * sizeof(uint32_t), &ibo_data[0]);
}
