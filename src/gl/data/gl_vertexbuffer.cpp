/*
** glc_vertexbuffer.cpp
** Vertex buffer handling.
**
**---------------------------------------------------------------------------
** Copyright 2005 Christoph Oelckers
** All rights reserved.
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
** 4. When not used as part of GZDoom or a GZDoom derivative, this code will be
**    covered by the terms of the GNU Lesser General Public License as published
**    by the Free Software Foundation; either version 2.1 of the License, or (at
**    your option) any later version.
** 5. Full disclosure of the entire project's source code, except for third
**    party libraries is mandatory. (NOTE: This clause is non-negotiable!)
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

#include "gl/system/gl_system.h"
#include "doomtype.h"
#include "p_local.h"
#include "r_state.h"
#include "m_argv.h"
#include "c_cvars.h"
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
	if (gl.glslversion > 0)
	{
		glVertexAttribPointer(VATTR_VERTEX, 3, GL_FLOAT, false, sizeof(FSimpleVertex), &VSiO->x);
		glVertexAttribPointer(VATTR_TEXCOORD, 2, GL_FLOAT, false, sizeof(FSimpleVertex), &VSiO->u);
		glVertexAttribPointer(VATTR_COLOR, 4, GL_UNSIGNED_BYTE, true, sizeof(FSimpleVertex), &VSiO->color);
		glEnableVertexAttribArray(VATTR_VERTEX);
		glEnableVertexAttribArray(VATTR_TEXCOORD);
		glEnableVertexAttribArray(VATTR_COLOR);
		glDisableVertexAttribArray(VATTR_VERTEX2);
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
		if (gl.glslversion > 0)
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
		if (gl.glslversion > 0)
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
	gl_RenderState.SetVertexBuffer(this);
	glBufferData(GL_ARRAY_BUFFER, count * sizeof(*verts), verts, GL_STREAM_DRAW);
}

//==========================================================================
//
//
//
//==========================================================================

FFlatVertexBuffer::FFlatVertexBuffer(int width, int height)
: FVertexBuffer(gl.buffermethod == BM_PERSISTENT)
{
	if (gl.buffermethod == BM_PERSISTENT)
	{
		unsigned int bytesize = BUFFER_SIZE * sizeof(FFlatVertex);
		glBindBuffer(GL_ARRAY_BUFFER, vbo_id);
		glBufferStorage(GL_ARRAY_BUFFER, bytesize, NULL, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
		map = (FFlatVertex*)glMapBufferRange(GL_ARRAY_BUFFER, 0, bytesize, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
	}
	else
	{
		// The fallback path uses immediate mode rendering and does not set up an actual vertex buffer
		vbo_shadowdata.Reserve(BUFFER_SIZE);
		map = new FFlatVertex[BUFFER_SIZE];
	}
	mIndex = mCurIndex = 0;
	mNumReserved = 12;
	vbo_shadowdata.Resize(mNumReserved);

	// the first quad is reserved for handling coordinates through uniforms.
	vbo_shadowdata[0].Set(1, 0, 0, 0, 0);
	vbo_shadowdata[1].Set(2, 0, 0, 0, 0);
	vbo_shadowdata[2].Set(3, 0, 0, 0, 0);
	vbo_shadowdata[3].Set(4, 0, 0, 0, 0);

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

}

FFlatVertexBuffer::~FFlatVertexBuffer()
{
	if (vbo_id != 0)
	{
		glBindBuffer(GL_ARRAY_BUFFER, vbo_id);
		glUnmapBuffer(GL_ARRAY_BUFFER);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}
	else
	{
		delete[] map;
	}
	map = nullptr;
}


void FFlatVertexBuffer::BindVBO()
{
	glBindBuffer(GL_ARRAY_BUFFER, vbo_id);
	if (gl.glslversion > 0)
	{
		if (vbo_id != 0)	// set this up only if there is an actual buffer.
		{
			glVertexAttribPointer(VATTR_VERTEX, 3, GL_FLOAT, false, sizeof(FFlatVertex), &VTO->x);
			glVertexAttribPointer(VATTR_TEXCOORD, 2, GL_FLOAT, false, sizeof(FFlatVertex), &VTO->u);
		}
		else
		{
			// If we cannot use a hardware buffer, use an old-style client array.
			glVertexAttribPointer(VATTR_VERTEX, 3, GL_FLOAT, false, sizeof(FFlatVertex), &map->x);
			glVertexAttribPointer(VATTR_TEXCOORD, 2, GL_FLOAT, false, sizeof(FFlatVertex), &map->u);
		}
		glEnableVertexAttribArray(VATTR_VERTEX);
		glEnableVertexAttribArray(VATTR_TEXCOORD);
		glDisableVertexAttribArray(VATTR_COLOR);
		glDisableVertexAttribArray(VATTR_VERTEX2);
	}
	else
	{
		glDisableClientState(GL_VERTEX_ARRAY);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		glDisableClientState(GL_COLOR_ARRAY);
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
		for(int i=0; i<numsectors;i++)
		{
			CreateVertices(h, &sectors[i], sectors[i].GetSecPlane(h), h == sector_t::floor);
		}
	}

	// We need to do a final check for Vavoom water and FF_FIX sectors.
	// No new vertices are needed here. The planes come from the actual sector
	for(int i=0; i<numsectors;i++)
	{
		for(unsigned j=0;j<sectors[i].e->XFloor.ffloors.Size(); j++)
		{
			F3DFloor *ff = sectors[i].e->XFloor.ffloors[j];

			if (ff->top.model == &sectors[i])
			{
				ff->top.vindex = sectors[i].vboindex[ff->top.isceiling];
			}
			if (ff->bottom.model == &sectors[i])
			{
				ff->bottom.vindex = sectors[i].vboindex[ff->top.isceiling];
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
	memcpy(map, &vbo_shadowdata[0], vbo_shadowdata.Size() * sizeof(FFlatVertex));
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