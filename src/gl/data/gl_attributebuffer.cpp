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
** gl_viewpointbuffer.cpp
** Buffer data maintenance for per viewpoint uniform data
**
**/

#include "gl_load/gl_system.h"
#include "gl_load/gl_interface.h"
#include "hwrenderer/data/shaderuniforms.h"
#include "gl_attributebuffer.h"
#include "r_data/matrix.h"
#include "gl/renderer//gl_renderer.h"
#include "gl/dynlights/gl_lightbuffer.h"
#include "gl/data/gl_dynamicuniformbuffer.h"

static const int INITIAL_BUFFER_SIZE = 20000;	// sufficient for Frozen Time


GLAttributeBuffer::GLAttributeBuffer()
{
	mBufferSize = INITIAL_BUFFER_SIZE;
	mBufferType = GL_UNIFORM_BUFFER;	// may be changed later.
	mIndexed = false;					// set to true when being used as an array.
	
	mBlockAlign = mBufferType == GL_UNIFORM_BUFFER? ((sizeof(AttributeBufferData) / gl.uniformblockalignment) + 1) * gl.uniformblockalignment : sizeof(AttributeBufferData);
	mByteSize = mBufferSize * mBlockAlign;
	mReserved = 0;
	Allocate();
	Clear();
	mLastMappedIndex = UINT_MAX;
	
	// Set up the default attribute settings.
	Map();
	AttributeBufferData attr;
	attr.CreateDefaultEntries([=](AttributeBufferData &attr)
	{
		mReserved = this->Upload(&attr) + 1;
	});
	Unmap();
}

GLAttributeBuffer::~GLAttributeBuffer()
{
	glBindBuffer(GL_UNIFORM_BUFFER, mBufferId);
	glUnmapBuffer(GL_UNIFORM_BUFFER);
	glBindBuffer(mBufferType, 0);
	glDeleteBuffers(1, &mBufferId);
}

void GLAttributeBuffer::Allocate()
{
	glGenBuffers(1, &mBufferId);
	glBindBufferBase(mBufferType, ATTRIBUTE_BINDINGPOINT, mBufferId);
	glBindBuffer(mBufferType, mBufferId);	// Note: Some older AMD drivers don't do that in glBindBufferBase, as they should.
	if (gl.buffermethod == BM_PERSISTENT)
	{
		glBufferStorage(mBufferType, mByteSize, NULL, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
		mBufferPointer = glMapBufferRange(mBufferType, 0, mByteSize, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
		mPersistent = true;
	}
	else
	{
		glBufferData(mBufferType, mByteSize, NULL, GL_DYNAMIC_DRAW);
		mBufferPointer = nullptr;
		mPersistent = false;
	}
}

void GLAttributeBuffer::CheckSize()
{
	assert (mBufferedData.Size() == (mUploadIndex - mBufferSize));
	// reallocate the buffer with twice the size
	unsigned int oldbuffer = mBufferId;
	unsigned int oldsize = mBufferSize;
	
	if (mUploadIndex < mBufferSize * 2)
	{
		mBufferSize *= 2;
	}
	else
	{
		mBufferSize = mUploadIndex + 1;
	}
	mByteSize = mBufferSize * mBlockAlign;
	
	// first unmap the old buffer
	glBindBuffer(mBufferType, mBufferId);
	glUnmapBuffer(mBufferType);
	
	Allocate();
	glBindBuffer(GL_COPY_READ_BUFFER, oldbuffer);
	
	// copy contents and delete the old buffer.
	glCopyBufferSubData(GL_COPY_READ_BUFFER, mBufferType, 0, 0, mByteSize / 2);	// old size is half of the current one.
	glBindBuffer(GL_COPY_READ_BUFFER, 0);
	glDeleteBuffers(1, &oldbuffer);
	
	Map();
	for(auto &md : mBufferedData)
	{
		auto writeptr = (AttributeBufferData *) (((char*)mBufferPointer) + mBlockAlign * oldsize);
		*writeptr = md;
		oldsize++;
	}
	mBufferedData.Clear();
	Unmap();
	
}

void GLAttributeBuffer::Map()
{
	if (!mPersistent)
	{
		glBindBuffer(mBufferType, mBufferId);
		mBufferPointer = glMapBufferRange(mBufferType, 0, mByteSize, GL_MAP_WRITE_BIT);
	}
}

void GLAttributeBuffer::Unmap()
{
	if (!mPersistent && mBufferPointer)
	{
		glBindBuffer(mBufferType, mBufferId);
		glUnmapBuffer(mBufferType);
		mBufferPointer = nullptr;
	}
	if (mUploadIndex >= mBufferSize) CheckSize();
}

int GLAttributeBuffer::Upload(AttributeBufferData *attr)
{
	assert(mBufferPointer != nullptr);	// May only be called when the buffer is mapped.
	if (mBufferPointer == nullptr) return 0;
	
	auto ui = mUploadIndex.fetch_add(1);
	// In a multithreaded context this cannot reallocate the buffer, so we have to store the data in an intermediate buffer,
	// until the CheckSize function can do the actual reallocation on the main thread.
	if (ui >= mBufferSize)
	{
		// This code is not thread safe and must be mutexed.
		std::lock_guard<std::mutex> lock(mBufferMutex);
		auto ndx = mBufferedData.Size();
		mBufferedData.Push(*attr);
		return mBufferSize + ndx;
	}
	
	auto writeptr = (AttributeBufferData *) (((char*)mBufferPointer) + mBlockAlign * ui);
	*writeptr = *attr;

	// Fix indices for uniform buffer mode.
	if (GLRenderer->mLights->GetBufferType() == GL_UNIFORM_BUFFER)
	{
		if (attr->uLightIndex > -1) writeptr->uLightIndex = GLRenderer->mLights->ShaderIndex(attr->uLightIndex);
		writeptr->uTexMatrixIndex = GLRenderer->mTextureMatrices->ShaderIndex(attr->uTexMatrixIndex);
	}
	
	
	return ui;
}

void GLAttributeBuffer::Clear()
{
	mUploadIndex = mReserved;
}


