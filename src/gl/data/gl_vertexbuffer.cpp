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
#include "cmdlib.h"
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
	glVertexAttribPointer(VATTR_VERTEX, 3, GL_FLOAT, false, sizeof(FSimpleVertex), &VSiO->x);
	glVertexAttribPointer(VATTR_TEXCOORD, 2, GL_FLOAT, false, sizeof(FSimpleVertex), &VSiO->u);
	glVertexAttribPointer(VATTR_COLOR, 4, GL_UNSIGNED_BYTE, true, sizeof(FSimpleVertex), &VSiO->color);
	glEnableVertexAttribArray(VATTR_VERTEX);
	glEnableVertexAttribArray(VATTR_TEXCOORD);
	glEnableVertexAttribArray(VATTR_COLOR);
	glDisableVertexAttribArray(VATTR_VERTEX2);
	glDisableVertexAttribArray(VATTR_NORMAL);
}

void FSimpleVertexBuffer::EnableColorArray(bool on)
{
	if (on)
	{
		glEnableVertexAttribArray(VATTR_COLOR);
	}
	else
	{
		glDisableVertexAttribArray(VATTR_COLOR);
	}
}


void FSimpleVertexBuffer::set(FSimpleVertex *verts, int count)
{
	glBindBuffer(GL_ARRAY_BUFFER, vbo_id);
	gl_RenderState.ResetVertexBuffer();
	gl_RenderState.SetVertexBuffer(this);
	glBufferData(GL_ARRAY_BUFFER, count * sizeof(*verts), verts, GL_STREAM_DRAW);
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

FSkyVertexBuffer::FSkyVertexBuffer()
{
	glBindBuffer(GL_ARRAY_BUFFER, vbo_id);
	glBufferData(GL_ARRAY_BUFFER, mVertices.Size() * sizeof(FSkyVertex), &mVertices[0], GL_STATIC_DRAW);
}

void FSkyVertexBuffer::BindVBO()
{
	glBindBuffer(GL_ARRAY_BUFFER, vbo_id);
	glVertexAttribPointer(VATTR_VERTEX, 3, GL_FLOAT, false, sizeof(FSkyVertex), &VSO->x);
	glVertexAttribPointer(VATTR_TEXCOORD, 2, GL_FLOAT, false, sizeof(FSkyVertex), &VSO->u);
	glVertexAttribPointer(VATTR_COLOR, 4, GL_UNSIGNED_BYTE, true, sizeof(FSkyVertex), &VSO->color);
	glEnableVertexAttribArray(VATTR_VERTEX);
	glEnableVertexAttribArray(VATTR_TEXCOORD);
	glEnableVertexAttribArray(VATTR_COLOR);
	glDisableVertexAttribArray(VATTR_VERTEX2);
	glDisableVertexAttribArray(VATTR_NORMAL);
}

//==========================================================================
//
//
//
//==========================================================================

FFlatVertexBuffer::FFlatVertexBuffer(int width, int height)
: FFlatVertexGenerator(width, height)
{
	mVertexBuffer = screen->CreateVertexBuffer();
	mIndexBuffer = screen->CreateIndexBuffer();

	unsigned int bytesize = BUFFER_SIZE * sizeof(FFlatVertex);
	mVertexBuffer->SetData(bytesize, nullptr, false);

	static const FVertexBufferAttribute format[] = {
		{ 0, VATTR_VERTEX, VFmt_Float3, myoffsetof(FFlatVertex, x) },
		{ 0, VATTR_TEXCOORD, VFmt_Float2, myoffsetof(FFlatVertex, u) }
	};
	mVertexBuffer->SetFormat(1, 2, sizeof(FFlatVertex), format);

	mIndex = mCurIndex = 0;
	mNumReserved = NUM_RESERVED;
	Copy(0, NUM_RESERVED);
}

FFlatVertexBuffer::~FFlatVertexBuffer()
{
	delete mIndexBuffer;
	delete mVertexBuffer;
	mIndexBuffer = nullptr;
	mVertexBuffer = nullptr;
}

void FFlatVertexBuffer::Copy(int start, int count)
{
	Map();
	memcpy(GetBuffer(start), &vbo_shadowdata[0], count * sizeof(FFlatVertex));
	Unmap();
}

void FFlatVertexBuffer::OutputResized(int width, int height)
{
	FFlatVertexGenerator::OutputResized(width, height);
	Copy(4, 4);
}

void FFlatVertexBuffer::Bind(FRenderState &state)
{
	state.SetVertexBuffer(mVertexBuffer, 0, 0);
	state.SetIndexBuffer(mIndexBuffer);
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
	Copy(0, mIndex);
	mIndexBuffer->SetData(ibo_data.Size() * sizeof(uint32_t), &ibo_data[0]);
}
