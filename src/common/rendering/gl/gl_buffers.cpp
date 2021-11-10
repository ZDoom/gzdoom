/*
** gl_buffers.cpp
** Low level vertex buffer class
**
**---------------------------------------------------------------------------
** Copyright 2018-2020 Christoph Oelckers
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
**/

#include <algorithm>
#include "gl_load.h"
#include "gl_buffers.h"
#include "gl_renderstate.h"
#include "v_video.h"
#include "flatvertices.h"

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


void GLBuffer::SetData(size_t size, const void *data, BufferUsageType usage)
{
	Bind();
	if (usage == BufferUsageType::Static)
	{
		glBufferData(mUseType, size, data, GL_STATIC_DRAW);
	}
	else if (usage == BufferUsageType::Stream)
	{
		glBufferData(mUseType, size, data, GL_STREAM_DRAW);
	}
	else if (usage == BufferUsageType::Persistent)
	{
		mPersistent = screen->BuffersArePersistent();
		if (mPersistent)
		{
			glBufferStorage(mUseType, size, nullptr, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
			map = glMapBufferRange(mUseType, 0, size, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
		}
		else
		{
			glBufferData(mUseType, size, nullptr, GL_STREAM_DRAW);
			map = nullptr;
		}
		nomap = false;
	}
	else if (usage == BufferUsageType::Mappable)
	{
		glBufferData(mUseType, size, nullptr, GL_STATIC_DRAW);
		map = nullptr;
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
	SetData(size, nullptr, BufferUsageType::Mappable);
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
		SetData(newsize, nullptr, BufferUsageType::Persistent);
		glBindBuffer(GL_COPY_READ_BUFFER, oldbuffer);

		// copy contents and delete the old buffer.
		glCopyBufferSubData(GL_COPY_READ_BUFFER, mUseType, 0, 0, buffersize);
		glBindBuffer(GL_COPY_READ_BUFFER, 0);
		glDeleteBuffers(1, &oldbuffer);
		buffersize = newsize;
		InvalidateBufferState();
	}
}

void GLBuffer::GPUDropSync()
{
	if (mGLSync != NULL)
	{
		glDeleteSync(mGLSync);
	}

	mGLSync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
}

void GLBuffer::GPUWaitSync()
{
	GLenum status = glClientWaitSync(mGLSync, GL_SYNC_FLUSH_COMMANDS_BIT, 1000 * 1000 * 50); // Wait for a max of 50ms...
	
	if (status != GL_ALREADY_SIGNALED && status != GL_CONDITION_SATISFIED)
	{
		//Printf("Error on glClientWaitSync: %d\n", status);
	}
	
	glDeleteSync(mGLSync);

	mGLSync = NULL;
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

void GLDataBuffer::BindRange(FRenderState *state, size_t start, size_t length)
{
	glBindBufferRange(mUseType, mBindingPoint, mBufferId, start, length);
}

void GLDataBuffer::BindBase()
{
	glBindBufferBase(mUseType, mBindingPoint, mBufferId);
}


GLVertexBuffer::GLVertexBuffer() : GLBuffer(GL_ARRAY_BUFFER) {}
GLIndexBuffer::GLIndexBuffer() : GLBuffer(GL_ELEMENT_ARRAY_BUFFER) {}
GLDataBuffer::GLDataBuffer(int bindingpoint, bool is_ssbo) : GLBuffer(is_ssbo ? GL_SHADER_STORAGE_BUFFER : GL_UNIFORM_BUFFER), mBindingPoint(bindingpoint) {}

}