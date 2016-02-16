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

FVertexBuffer::FVertexBuffer()
{
	vao_id = vbo_id = 0;
	glGenBuffers(1, &vbo_id);
	glGenVertexArrays(1, &vao_id);

}
	
FVertexBuffer::~FVertexBuffer()
{
	if (vbo_id != 0)
	{
		glDeleteBuffers(1, &vbo_id);
	}
	if (vao_id != 0)
	{
		glDeleteVertexArrays(1, &vao_id);
	}
}

void FVertexBuffer::BindVBO()
{
	glBindVertexArray(vao_id);
}

//==========================================================================
//
//
//
//==========================================================================

FFlatVertexBuffer::FFlatVertexBuffer()
: FVertexBuffer()
{
	if (gl.flags & RFL_BUFFER_STORAGE)
	{
		unsigned int bytesize = BUFFER_SIZE * sizeof(FFlatVertex);
		glBindBuffer(GL_ARRAY_BUFFER, vbo_id);
		glBufferStorage(GL_ARRAY_BUFFER, bytesize, NULL, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
		map = (FFlatVertex*)glMapBufferRange(GL_ARRAY_BUFFER, 0, bytesize, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);

		glBindVertexArray(vao_id);
		glBindBuffer(GL_ARRAY_BUFFER, vbo_id);
		glVertexAttribPointer(VATTR_VERTEX, 3,GL_FLOAT, false, sizeof(FFlatVertex), &VTO->x);
		glVertexAttribPointer(VATTR_TEXCOORD, 2,GL_FLOAT, false, sizeof(FFlatVertex), &VTO->u);
		glEnableVertexAttribArray(VATTR_VERTEX);
		glEnableVertexAttribArray(VATTR_TEXCOORD);
		glBindVertexArray(0);
	}
	else
	{
		vbo_shadowdata.Reserve(BUFFER_SIZE);
		map = &vbo_shadowdata[0];
	}
	mNumReserved = mIndex = mCurIndex = 0;
}

FFlatVertexBuffer::~FFlatVertexBuffer()
{
	if (gl.flags & RFL_BUFFER_STORAGE)
	{
		glBindBuffer(GL_ARRAY_BUFFER, vbo_id);
		glUnmapBuffer(GL_ARRAY_BUFFER);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}
}

//==========================================================================
//
// immediate mode fallback for drivers without GL_ARB_buffer_storage
//
// No single core method is performant enough  to handle this adequately
// so we have to resort to immediate mode instead...
//
//==========================================================================

void FFlatVertexBuffer::ImmRenderBuffer(unsigned int primtype, unsigned int offset, unsigned int count)
{
	// this will only get called if we can't acquire a persistently mapped buffer.
#ifndef CORE_PROFILE
	glBegin(primtype);
	for (unsigned int i = 0; i < count; i++)
	{
		glVertexAttrib2fv(VATTR_TEXCOORD, &map[offset + i].u);
		glVertexAttrib3fv(VATTR_VERTEX, &map[offset + i].x);
	}
	glEnd();
#endif
}

//==========================================================================
//
// Initialize a single vertex
//
//==========================================================================

void FFlatVertex::SetFlatVertex(vertex_t *vt, const secplane_t & plane)
{
	x = vt->fx;
	y = vt->fy;
	z = plane.ZatPoint(vt->fx, vt->fy);
	u = vt->fx/64.f;
	v = -vt->fy/64.f;
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
	if (gl.flags & RFL_BUFFER_STORAGE)
	{
		vbo_shadowdata.Resize(mNumReserved);
		CreateFlatVBO();
		mCurIndex = mIndex = vbo_shadowdata.Size();
		memcpy(map, &vbo_shadowdata[0], vbo_shadowdata.Size() * sizeof(FFlatVertex));
	}
	else if (sectors)
	{
		// set all VBO info to invalid values so that we can save some checks in the rendering code
		for(int i=0;i<numsectors;i++)
		{
			sectors[i].vboindex[3] = sectors[i].vboindex[2] = 
			sectors[i].vboindex[1] = sectors[i].vboindex[0] = -1;
			sectors[i].vboheight[1] = sectors[i].vboheight[0] = FIXED_MIN;
		}
	}

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
	if (gl.flags & RFL_BUFFER_STORAGE)
	{
		CheckPlanes(sector);
		sector_t *hs = sector->GetHeightSec();
		if (hs != NULL) CheckPlanes(hs);
		for (unsigned i = 0; i < sector->e->XFloor.ffloors.Size(); i++)
			CheckPlanes(sector->e->XFloor.ffloors[i]->model);
	}
}