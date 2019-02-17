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
#include "gl_buffers.h"
#include "gl/renderer/gl_renderstate.h"
#include "v_video.h"

namespace OpenGLRenderer
{

//==========================================================================
//
// basic buffer implementation
//
//==========================================================================

static inline void InvalidateBufferState()
{
	gl_RenderState.ResetVertexBuffer();	// force rebinding of buffers on next Apply call.
}

GLBuffer::GLBuffer(int usetype)
	: mUseType(usetype)
{
	glGenBuffers(1, &mBufferId);
}

GLBuffer::~GLBuffer()
{
	if (mBufferId != 0)
	{
		glBindBuffer(mUseType, mBufferId);
		glUnmapBuffer(mUseType);
		glBindBuffer(mUseType, 0);
		glDeleteBuffers(1, &mBufferId);
	}
}

void GLBuffer::Bind()
{
	glBindBuffer(mUseType, mBufferId);
}


void GLBuffer::SetData(size_t size, const void *data, bool staticdata)
{
	Bind();
	if (data != nullptr)
	{
		glBufferData(mUseType, size, data, staticdata? GL_STATIC_DRAW : GL_STREAM_DRAW);
	}
	else
	{
		mPersistent = screen->BuffersArePersistent() && !staticdata;
		if (mPersistent)
		{
			glBufferStorage(mUseType, size, nullptr, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
			map = glMapBufferRange(mUseType, 0, size, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
		}
		else
		{
			glBufferData(mUseType, size, nullptr, staticdata ? GL_STATIC_DRAW : GL_STREAM_DRAW);
			map = nullptr;
		}
		if (!staticdata) nomap = false;
	}
	buffersize = size;
	InvalidateBufferState();
}

void GLBuffer::SetSubData(size_t offset, size_t size, const void *data)
{
	Bind();
	glBufferSubData(mUseType, offset, size, data);
}

void GLBuffer::Map()
{
	assert(nomap == false);	// do not allow mapping of static buffers. Vulkan cannot do that so it should be blocked in OpenGL, too.
	if (!mPersistent && !nomap)
	{
		Bind();
		map = (FFlatVertex*)glMapBufferRange(mUseType, 0, buffersize, GL_MAP_WRITE_BIT|GL_MAP_UNSYNCHRONIZED_BIT);
		InvalidateBufferState();
	}
}

void GLBuffer::Unmap()
{
	assert(nomap == false);
	if (!mPersistent && map != nullptr)
	{
		Bind();
		glUnmapBuffer(mUseType);
		InvalidateBufferState();
		map = nullptr;
	}
}

void *GLBuffer::Lock(unsigned int size)
{
	// This initializes this buffer as a static object with no data.
	SetData(size, nullptr, true);
	return glMapBufferRange(mUseType, 0, size, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT | GL_MAP_UNSYNCHRONIZED_BIT);
}

void GLBuffer::Unlock()
{
	Bind();
	glUnmapBuffer(mUseType);
	InvalidateBufferState();
}

void GLBuffer::Resize(size_t newsize)
{
	assert(!nomap);	// only mappable buffers can be resized. 
	if (newsize > buffersize && !nomap)
	{
		// reallocate the buffer with twice the size
		unsigned int oldbuffer = mBufferId;

		// first unmap the old buffer
		Bind();
		glUnmapBuffer(mUseType);

		glGenBuffers(1, &mBufferId);
		SetData(newsize, nullptr, false);
		glBindBuffer(GL_COPY_READ_BUFFER, oldbuffer);

		// copy contents and delete the old buffer.
		glCopyBufferSubData(GL_COPY_READ_BUFFER, mUseType, 0, 0, buffersize);
		glBindBuffer(GL_COPY_READ_BUFFER, 0);
		glDeleteBuffers(1, &oldbuffer);
		buffersize = newsize;
		InvalidateBufferState();
	}
}


//===========================================================================
//
// Vertex buffer implementation
//
//===========================================================================

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
			attrinf.bindingpoint = attrs[i].binding;
		}
	}
}

void GLVertexBuffer::Bind(int *offsets)
{
	int i = 0;

	// This is what gets called from RenderState.Apply. It shouldn't be called anywhere else if the render state is in use
	GLBuffer::Bind();
	for(auto &attrinf : mAttributeInfo)
	{
		if (attrinf.size == 0)
		{
			glDisableVertexAttribArray(i);
		}
		else
		{
			glEnableVertexAttribArray(i);
			size_t ofs = offsets == nullptr ? attrinf.offset : attrinf.offset + mStride * offsets[attrinf.bindingpoint];
			glVertexAttribPointer(i, attrinf.size, attrinf.format, attrinf.format != GL_FLOAT, (GLsizei)mStride, (void*)(intptr_t)ofs);
		}
		i++;
	}
}

void GLDataBuffer::BindRange(size_t start, size_t length)
{
	if (mUseType == GL_UNIFORM_BUFFER)	// SSBO's cannot be rebound.
		glBindBufferRange(mUseType, mBindingPoint, mBufferId, start, length);
}

void GLDataBuffer::BindBase()
{
	glBindBufferBase(mUseType, mBindingPoint, mBufferId);
}

}