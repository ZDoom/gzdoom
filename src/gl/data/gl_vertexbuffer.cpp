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

#include "gl_load/gl_system.h"
#include "doomtype.h"
#include "p_local.h"
#include "r_state.h"
#include "gl_load/gl_interface.h"
#include "gl/renderer/gl_renderer.h"
#include "gl/shaders/gl_shader.h"
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
: FVertexBuffer(!gl.legacyMode), FFlatVertexGenerator(width, height)
{
	ibo_id = 0;
	if (gl.buffermethod != BM_LEGACY) glGenBuffers(1, &ibo_id);
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

	mMap = map;
	Map();
	memcpy(map, &vbo_shadowdata[0], mNumReserved * sizeof(FFlatVertex));
	Unmap();
}

FFlatVertexBuffer::~FFlatVertexBuffer()
{
	if (vbo_id != 0)
	{
		glBindBuffer(GL_ARRAY_BUFFER, vbo_id);
		glUnmapBuffer(GL_ARRAY_BUFFER);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}
	if (ibo_id != 0)
	{
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
		glDeleteBuffers(1, &ibo_id);
	}
	if (gl.legacyMode)
	{
		delete[] map;
	}
	map = nullptr;
}

void FFlatVertexBuffer::OutputResized(int width, int height)
{
	FFlatVertexGenerator::OutputResized(width, height);
	Map();
	memcpy(&map[4], &vbo_shadowdata[4], 4 * sizeof(FFlatVertex));
	Unmap();
}

void FFlatVertexBuffer::BindVBO()
{
	glBindBuffer(GL_ARRAY_BUFFER, vbo_id);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_id);
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
		mMap = map = (FFlatVertex*)glMapBufferRange(GL_ARRAY_BUFFER, 0, bytesize, GL_MAP_WRITE_BIT|GL_MAP_UNSYNCHRONIZED_BIT);
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
		mMap = map = nullptr;
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
	FFlatVertexGenerator::CreateVertices();
	mCurIndex = mIndex = vbo_shadowdata.Size();
	Map();
	memcpy(map, &vbo_shadowdata[0], vbo_shadowdata.Size() * sizeof(FFlatVertex));
	Unmap();
	if (ibo_id > 0)
	{
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_id);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, ibo_data.Size() * sizeof(uint32_t), &ibo_data[0], GL_STATIC_DRAW);
	}
}
