// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2018 Christoph Oelckers
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
** Low level vertex buffer class
**
**/

#include "gl_load/gl_system.h"
#include "glsys_vertexbuffer.h"
#include "gl/renderer/gl_renderstate.h"

//==========================================================================
//
// Vertex buffer implementation
//
//==========================================================================

GLVertexBuffer::GLVertexBuffer()
{
	glGenBuffers(1, &vbo_id);
}

GLVertexBuffer::~GLVertexBuffer()
{
	if (vbo_id != 0)
	{
		glBindBuffer(GL_ARRAY_BUFFER, vbo_id);
		glUnmapBuffer(GL_ARRAY_BUFFER);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glDeleteBuffers(1, &vbo_id);
		gl_RenderState.ResetVertexBuffer();
	}
}

void GLVertexBuffer::SetData(size_t size, void *data, bool staticdata)
{
	if (data != nullptr)
	{
		glBindBuffer(GL_ARRAY_BUFFER, vbo_id);
		glBufferData(GL_ARRAY_BUFFER, size, data, staticdata? GL_STATIC_DRAW : GL_STREAM_DRAW);
	}
	else
	{
		mPersistent = screen->BuffersArePersistent() && !staticdata;
		if (mPersistent)
		{
			glBindBuffer(GL_ARRAY_BUFFER, vbo_id);
			glBufferStorage(GL_ARRAY_BUFFER, size, NULL, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
			map = glMapBufferRange(GL_ARRAY_BUFFER, 0, size, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
		}
		else
		{
			glBindBuffer(GL_ARRAY_BUFFER, vbo_id);
			glBufferData(GL_ARRAY_BUFFER, size, NULL, staticdata ? GL_STATIC_DRAW : GL_STREAM_DRAW);
			map = nullptr;
		}
		nomap = false;
	}
	buffersize = size;
	gl_RenderState.ResetVertexBuffer();	// This is needed because glBindBuffer overwrites the setting stored in the render state.
}

void GLVertexBuffer::Map()
{
	assert(nomap == false);
	if (!mPersistent)
	{
		glBindBuffer(GL_ARRAY_BUFFER, vbo_id);
		gl_RenderState.ResetVertexBuffer();
		map = (FFlatVertex*)glMapBufferRange(GL_ARRAY_BUFFER, 0, buffersize, GL_MAP_WRITE_BIT|GL_MAP_UNSYNCHRONIZED_BIT);
	}
}

void GLVertexBuffer::Unmap()
{
	assert(nomap == false);
	if (!mPersistent)
	{
		glBindBuffer(GL_ARRAY_BUFFER, vbo_id);
		gl_RenderState.ResetVertexBuffer();
		glUnmapBuffer(GL_ARRAY_BUFFER);
		map = nullptr;
	}
}

void GLVertexBuffer::SetFormat(int numBindingPoints, int numAttributes, size_t stride, const FVertexBufferAttribute *attrs)
{
	static int VFmtToGLFmt[] = { GL_FLOAT, GL_FLOAT, GL_FLOAT, GL_FLOAT, GL_UNSIGNED_BYTE, GL_INT_2_10_10_10_REV };
	static uint8_t VFmtToSize[] = {4, 3, 2, 1, 4, 4};
	
	mStride = stride;
	mNumBindingPoints = numBindingPoints;
	
	for(int i = 0; i < numAttributes; i++)
	{
		if (attrs[i].location >= 0 && attrs[i].location < VATTR_MAX)
		{
			auto & attrinf = mAttributeInfo[attrs[i].location];
			attrinf.format = VFmtToGLFmt[attrs[i].format];
			attrinf.size = VFmtToSize[attrs[i].format];
			attrinf.offset = attrs[i].offset;
		}
	}
}

void GLVertexBuffer::Bind(int *offsets)
{
	int i = 0;

	glBindBuffer(GL_ARRAY_BUFFER, vbo_id);
	for(auto &attrinf : mAttributeInfo)
	{
		if (attrinf.size == 0)
		{
			glDisableVertexAttribArray(i);
		}
		else
		{
			glEnableVertexAttribArray(i);
			size_t ofs = offsets == nullptr? attrinf.offset : attrinf.offset + mStride * offsets[attrinf.bindingpoint];
			glVertexAttribPointer(i, attrinf.size, attrinf.format, attrinf.format != GL_FLOAT, (GLsizei)mStride, (void*)(intptr_t)ofs);
		}
		i++;
	}
}

//===========================================================================
//
//
//
//===========================================================================

void *GLVertexBuffer::Lock(unsigned int size)
{
	SetData(size, nullptr, true);
	return glMapBufferRange(GL_ARRAY_BUFFER, 0, size, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT | GL_MAP_UNSYNCHRONIZED_BIT);
}

//===========================================================================
//
//
//
//===========================================================================

void GLVertexBuffer::Unlock()
{
	glBindBuffer(GL_ARRAY_BUFFER, vbo_id);
	glUnmapBuffer(GL_ARRAY_BUFFER);
	gl_RenderState.ResetVertexBuffer();
}

//==========================================================================
//
// Index buffer implementation
//
//==========================================================================

GLIndexBuffer::GLIndexBuffer()
{
	glGenBuffers(1, &ibo_id);
}

GLIndexBuffer::~GLIndexBuffer()
{
	if (ibo_id != 0)
	{
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_id);
		glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
		glDeleteBuffers(1, &ibo_id);
	}
}

void GLIndexBuffer::SetData(size_t size, void *data, bool staticdata)
{
	if (data != nullptr)
	{
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_id);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, size, data, staticdata? GL_STATIC_DRAW : GL_STREAM_DRAW);
	}
	buffersize = size;
	gl_RenderState.ResetVertexBuffer();	// This is needed because glBindBuffer overwrites the setting stored in the render state.
}

void GLIndexBuffer::Bind()
{
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_id);
}

//===========================================================================
//
//
//
//===========================================================================

void *GLIndexBuffer::Lock(unsigned int size)
{
	SetData(size, nullptr, true);
	return glMapBufferRange(GL_ELEMENT_ARRAY_BUFFER, 0, size, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT | GL_MAP_UNSYNCHRONIZED_BIT);
}

//===========================================================================
//
//
//
//===========================================================================

void GLIndexBuffer::Unlock()
{
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_id);
	glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);
	gl_RenderState.ResetVertexBuffer();
}

