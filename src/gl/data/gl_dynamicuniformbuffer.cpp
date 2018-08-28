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
** gl_dynamicuniformbuffer.cpp
** Buffer class for semi-static map data (i.e. the size is constant, but content may change occasionally)
**
**/

#include "gl_load/gl_system.h"
#include "gl_load/gl_interface.h"
#include "hwrenderer/data/shaderuniforms.h"
#include "gl_dynamicuniformbuffer.h"
#include "templates.h"

GLDynamicUniformBuffer::GLDynamicUniformBuffer(int bindingpoint, unsigned int elementsize, unsigned int reserved, unsigned forstreaming, void (*staticinitfunc)(char *buffer))
{
	mElementCount = reserved + forstreaming;
	mReservedCount = reserved;
	mStreamCount = forstreaming;
	mElementSize = elementsize;
	mLastBoundIndex = UINT_MAX;
	mBufferId = 0;
	mUploadIndex = reserved;
	mStaticInitFunc = staticinitfunc;
	
	if (gl.flags & RFL_SHADER_STORAGE_BUFFER && !strstr(gl.vendorstring, "Intel"))
	{
		mBufferType = GL_SHADER_STORAGE_BUFFER;
		mBlockAlign = 0;
		mBlockSize = 0;
	}
	else
	{
		mBufferType = GL_UNIFORM_BUFFER;
		if (gl.maxuniformblock % elementsize == 0)
		{
			mBlockAlign = gl.maxuniformblock;
		}
		else 
		{
			mBlockAlign = gl.uniformblockalignment;
		}
		mBlockSize = mBlockAlign / elementsize;
	}
	
	Allocate();
}

GLDynamicUniformBuffer::~GLDynamicUniformBuffer()
{
	Unload();
}

void GLDynamicUniformBuffer::Unload()
{
	if (mBufferId != 0)
	{
		glBindBuffer(mBufferType, mBufferId);
		glUnmapBuffer(mBufferType);
		glBindBuffer(mBufferType, 0);
		glDeleteBuffers(1, &mBufferId);
	}
	mBufferId = 0;
}

void GLDynamicUniformBuffer::Allocate()
{
	Unload();
	
	mByteSize = mElementCount * mElementSize;
	glGenBuffers(1, &mBufferId);
	glBindBufferBase(mBufferType, TEXMATRIX_BINDINGPOINT, mBufferId);
	glBindBuffer(mBufferType, mBufferId);
	if (gl.buffermethod == BM_PERSISTENT)
	{
		glBufferStorage(mBufferType, mByteSize, NULL, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
		mBufferPointer = (char*)glMapBufferRange(mBufferType, 0, mByteSize, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
		mPersistent = true;
	}
	else
	{
		glBufferData(mBufferType, mByteSize, NULL, GL_DYNAMIC_DRAW);
		mBufferPointer = nullptr;
		mPersistent = false;
	}
	if (mStaticInitFunc != nullptr)
	{
		Map();
		mStaticInitFunc(mBufferPointer);
		Unmap();
	}
}

void GLDynamicUniformBuffer::ValidateSize(int newnumelements)	// that is 'new elements plus the reserved items'
{
	newnumelements += DynamicStart();
	if (mElementCount < (unsigned)newnumelements)
	{
		mElementCount = newnumelements;
		Allocate();
	}
}

void GLDynamicUniformBuffer::Map()
{
	if (!mPersistent)
	{
		glBindBuffer(mBufferType, mBufferId);
		mBufferPointer = (char*)glMapBufferRange(mBufferType, 0, mByteSize, GL_MAP_WRITE_BIT);
	}
}

void GLDynamicUniformBuffer::Unmap()
{
	if (!mPersistent && mBufferPointer)
	{
		glBindBuffer(mBufferType, mBufferId);
		glUnmapBuffer(mBufferType);
		mBufferPointer = nullptr;
	}
}

int GLDynamicUniformBuffer::Upload(const void *data, int index, unsigned int count)
{
	assert(mBufferPointer != nullptr);	// May only be called when the buffer is mapped.
	if (mBufferPointer == nullptr) return 0;
	
	if (index == -1) // dynamic case. Currently only used by the texture matrix for the sky domes.
	{
		if (mStreamCount == 0) return 0;	// don't do this!
		index = mUploadIndex++;
		if ((unsigned)index >= mReservedCount + mStreamCount)
		{
			index = mReservedCount;
		}
	}
	
	memcpy(mBufferPointer + index * mElementSize, data, count * mElementSize);
	return index;
}

// When this gets called we already have ruled out the case where rebinding is not needed.
int  GLDynamicUniformBuffer::DoBind(unsigned int index)
{
	unsigned int offset = ((index*mElementSize) / mBlockAlign) * mBlockAlign;

	if (offset != mLastBoundIndex)
	{
		mLastBoundIndex = offset;
		glBindBufferRange(GL_UNIFORM_BUFFER, TEXMATRIX_BINDINGPOINT, mBufferId, offset, MIN(mBlockAlign, mByteSize - offset));
	}
	return index - offset / mElementSize;
}


