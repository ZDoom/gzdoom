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
** gl_vertexbuffer.cpp
** Vertex buffer handling.
**
**/

#include "gl/system/gl_system.h"
#include "doomtype.h"
#include "p_local.h"
#include "r_state.h"
#include "m_argv.h"
#include "c_cvars.h"
#include "g_levellocals.h"
#include "gl/system/gl_interface.h"
#include "gl/renderer/gl_renderer.h"
#include "gl/shaders/gl_shader.h"
#include "gl/data/gl_data.h"
#include "gl/data/gl_vertexbuffer.h"


//==========================================================================
//
// Create / destroy the VBO
//
//==========================================================================

FVertexBuffer::FVertexBuffer(bool wantbuffer)
{
	vbo_id = 0;
	if (wantbuffer) glGenBuffers(1, &vbo_id);
}
	
FVertexBuffer::~FVertexBuffer()
{
	if (vbo_id != 0)
	{
		glDeleteBuffers(1, &vbo_id);
	}
}


void FSimpleVertexBuffer::BindVBO()
{
	glBindBuffer(GL_ARRAY_BUFFER, vbo_id);
	if (!gl.legacyMode)
	{
		glVertexAttribPointer(VATTR_VERTEX, 3, GL_FLOAT, false, sizeof(FSimpleVertex), &VSiO->x);
		glVertexAttribPointer(VATTR_TEXCOORD, 2, GL_FLOAT, false, sizeof(FSimpleVertex), &VSiO->u);
		glVertexAttribPointer(VATTR_COLOR, 4, GL_UNSIGNED_BYTE, true, sizeof(FSimpleVertex), &VSiO->color);
		glEnableVertexAttribArray(VATTR_VERTEX);
		glEnableVertexAttribArray(VATTR_TEXCOORD);
		glEnableVertexAttribArray(VATTR_COLOR);
		glDisableVertexAttribArray(VATTR_VERTEX2);
		glDisableVertexAttribArray(VATTR_NORMAL);
	}
	else
	{
		glVertexPointer(3, GL_FLOAT, sizeof(FSimpleVertex), &VSiO->x);
		glTexCoordPointer(2, GL_FLOAT, sizeof(FSimpleVertex), &VSiO->u);
		glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(FSimpleVertex), &VSiO->color);
		glEnableClientState(GL_VERTEX_ARRAY);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		glEnableClientState(GL_COLOR_ARRAY);
	}
}

void FSimpleVertexBuffer::EnableColorArray(bool on)
{
	if (on)
	{
		if (!gl.legacyMode)
		{
			glEnableVertexAttribArray(VATTR_COLOR);
		}
		else
		{
			glEnableClientState(GL_COLOR_ARRAY);
		}
	}
	else
	{
		if (!gl.legacyMode)
		{
			glDisableVertexAttribArray(VATTR_COLOR);
		}
		else
		{
			glDisableClientState(GL_COLOR_ARRAY);
		}
	}
}


void FSimpleVertexBuffer::set(FSimpleVertex *verts, int count)
{
	glBindBuffer(GL_ARRAY_BUFFER, vbo_id);
	gl_RenderState.ResetVertexBuffer();
	gl_RenderState.SetVertexBuffer(this);
	glBufferData(GL_ARRAY_BUFFER, count * sizeof(*verts), verts, GL_STREAM_DRAW);
}

//==========================================================================
//
//
//
//==========================================================================

FFlatVertexBuffer::FFlatVertexBuffer(int width, int height)
: FVertexBuffer(!gl.legacyMode)
{
	switch (gl.buffermethod)
	{
	case BM_PERSISTENT:
	{
		unsigned int bytesize = BUFFER_SIZE * sizeof(FFlatVertex);
		glBindBuffer(GL_ARRAY_BUFFER, vbo_id);
		glBufferStorage(GL_ARRAY_BUFFER, bytesize, NULL, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
		map = (FFlatVertex*)glMapBufferRange(GL_ARRAY_BUFFER, 0, bytesize, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
		DPrintf(DMSG_NOTIFY, "Using persistent buffer\n");
		break;
	}

	case BM_DEFERRED:
	{
		unsigned int bytesize = BUFFER_SIZE * sizeof(FFlatVertex);
		glBindBuffer(GL_ARRAY_BUFFER, vbo_id);
		glBufferData(GL_ARRAY_BUFFER, bytesize, NULL, GL_STREAM_DRAW);
		map = nullptr;
		DPrintf(DMSG_NOTIFY, "Using deferred buffer\n");
		break;
	}

	default:
	{
		map = new FFlatVertex[BUFFER_SIZE];
		DPrintf(DMSG_NOTIFY, "Using client array buffer\n");
		break;
	}
	}
	mIndex = mCurIndex = 0;
	mNumReserved = NUM_RESERVED;
	vbo_shadowdata.Resize(mNumReserved);

	// the first quad is reserved for handling coordinates through uniforms.
	vbo_shadowdata[0].Set(0, 0, 0, 0, 0);
	vbo_shadowdata[1].Set(1, 0, 0, 0, 0);
	vbo_shadowdata[2].Set(2, 0, 0, 0, 0);
	vbo_shadowdata[3].Set(3, 0, 0, 0, 0);

	// and the second one for the fullscreen quad used for blend overlays.
	vbo_shadowdata[4].Set(0, 0, 0, 0, 0);
	vbo_shadowdata[5].Set(0, (float)height, 0, 0, 0);
	vbo_shadowdata[6].Set((float)width, 0, 0, 0, 0);
	vbo_shadowdata[7].Set((float)width, (float)height, 0, 0, 0);

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

	if (gl.buffermethod == BM_DEFERRED)
	{
		Map();
		memcpy(map, &vbo_shadowdata[0], mNumReserved * sizeof(FFlatVertex));
		Unmap();
	}

}

FFlatVertexBuffer::~FFlatVertexBuffer()
{
	if (vbo_id != 0)
	{
		glBindBuffer(GL_ARRAY_BUFFER, vbo_id);
		glUnmapBuffer(GL_ARRAY_BUFFER);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}
	if (gl.legacyMode)
	{
		delete[] map;
	}
	map = nullptr;
}

void FFlatVertexBuffer::OutputResized(int width, int height)
{
	vbo_shadowdata[4].Set(0, 0, 0, 0, 0);
	vbo_shadowdata[5].Set(0, (float)height, 0, 0, 0);
	vbo_shadowdata[6].Set((float)width, 0, 0, 0, 0);
	vbo_shadowdata[7].Set((float)width, (float)height, 0, 0, 0);
	
	Map();
	memcpy(&map[4], &vbo_shadowdata[4], 4 * sizeof(FFlatVertex));
	Unmap();
}

void FFlatVertexBuffer::BindVBO()
{
	glBindBuffer(GL_ARRAY_BUFFER, vbo_id);
	if (!gl.legacyMode)
	{
		glVertexAttribPointer(VATTR_VERTEX, 3, GL_FLOAT, false, sizeof(FFlatVertex), &VTO->x);
		glVertexAttribPointer(VATTR_TEXCOORD, 2, GL_FLOAT, false, sizeof(FFlatVertex), &VTO->u);
		glEnableVertexAttribArray(VATTR_VERTEX);
		glEnableVertexAttribArray(VATTR_TEXCOORD);
		glDisableVertexAttribArray(VATTR_COLOR);
		glDisableVertexAttribArray(VATTR_VERTEX2);
		glDisableVertexAttribArray(VATTR_NORMAL);
	}
	else
	{
		glVertexPointer(3, GL_FLOAT, sizeof(FFlatVertex), &map->x);
		glTexCoordPointer(2, GL_FLOAT, sizeof(FFlatVertex), &map->u);
		glEnableClientState(GL_VERTEX_ARRAY);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		glDisableClientState(GL_COLOR_ARRAY);
	}
}

void FFlatVertexBuffer::Map()
{
	if (gl.buffermethod == BM_DEFERRED)
	{
		unsigned int bytesize = BUFFER_SIZE * sizeof(FFlatVertex);
		glBindBuffer(GL_ARRAY_BUFFER, vbo_id);
		gl_RenderState.ResetVertexBuffer();
		map = (FFlatVertex*)glMapBufferRange(GL_ARRAY_BUFFER, 0, bytesize, GL_MAP_WRITE_BIT|GL_MAP_UNSYNCHRONIZED_BIT);
	}
}

void FFlatVertexBuffer::Unmap()
{
	if (gl.buffermethod == BM_DEFERRED)
	{
		unsigned int bytesize = BUFFER_SIZE * sizeof(FFlatVertex);
		glBindBuffer(GL_ARRAY_BUFFER, vbo_id);
		gl_RenderState.ResetVertexBuffer();
		glUnmapBuffer(GL_ARRAY_BUFFER);
		map = nullptr;
	}
}

//==========================================================================
//
// Initialize a single vertex
//
//==========================================================================

void FFlatVertex::SetFlatVertex(vertex_t *vt, const secplane_t & plane)
{
	x = vt->fX();
	y = vt->fY();
	z = plane.ZatPoint(vt);
	u = vt->fX()/64.f;
	v = -vt->fY()/64.f;
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
		if (ffloor->model == model) return ffloor;
	}
	return NULL;
}

//==========================================================================
//
// Creates the vertices for one plane in one subsector
//
//==========================================================================

int FFlatVertexBuffer::CreateSubsectorVertices(subsector_t *sub, const secplane_t &plane, int floor)
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

int FFlatVertexBuffer::CreateSectorVertices(sector_t *sec, const secplane_t &plane, int floor)
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

int FFlatVertexBuffer::CreateVertices(int h, sector_t *sec, const secplane_t &plane, int floor)
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

void FFlatVertexBuffer::CreateFlatVBO()
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

void FFlatVertexBuffer::UpdatePlaneVertices(sector_t *sec, int plane)
{
	int startvt = sec->vboindex[plane];
	int countvt = sec->vbocount[plane];
	secplane_t &splane = sec->GetSecPlane(plane);
	FFlatVertex *vt = &vbo_shadowdata[startvt];
	FFlatVertex *mapvt = &map[startvt];
	for(int i=0; i<countvt; i++, vt++, mapvt++)
	{
		vt->z = splane.ZatPoint(vt->x, vt->y);
		if (plane == sector_t::floor && sec->transdoor) vt->z -= 1;
		mapvt->z = vt->z;
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FFlatVertexBuffer::CreateVBO()
{
	vbo_shadowdata.Resize(mNumReserved);
	CreateFlatVBO();
	mCurIndex = mIndex = vbo_shadowdata.Size();
	Map();
	memcpy(map, &vbo_shadowdata[0], vbo_shadowdata.Size() * sizeof(FFlatVertex));
	Unmap();
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