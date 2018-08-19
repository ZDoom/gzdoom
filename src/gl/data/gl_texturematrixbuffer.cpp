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
#include "gl_texturematrixbuffer.h"
#include "templates.h"
#include "g_levellocals.h"
#include "r_data/matrix.h"

static const int INITIAL_SECTOR_COUNT = 1024;
static const int ADDITIONAL_ENTRIES = 50;	// for skyboxes

GLTextureMatrixBuffer::GLTextureMatrixBuffer()
{
	mSectorCount = INITIAL_SECTOR_COUNT;
	
	if (gl.flags & RFL_SHADER_STORAGE_BUFFER && !strstr(gl.vendorstring, "Intel"))
	{
		mBufferType = GL_SHADER_STORAGE_BUFFER;
		mBlockAlign = 0;
		mBlockSize = 0;
	}
	else
	{
		mBufferType = GL_UNIFORM_BUFFER;
		// A matrix is always 64 bytes.
		if (gl.maxuniformblock % sizeof(VSMatrix) == 0)
		{
			mBlockAlign = gl.maxuniformblock;
		}
		else 
		{
			mBlockAlign = gl.uniformblockalignment;	// this should really never happen.
		}
		mBlockSize = mBlockAlign / sizeof(VSMatrix);
	}
	
	mBufferId = 0;
	mUploadIndex = 2;
	Allocate();
	mLastMappedIndex = UINT_MAX;
}

GLTextureMatrixBuffer::~GLTextureMatrixBuffer()
{
	Unload();
}

void GLTextureMatrixBuffer::Unload()
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

void GLTextureMatrixBuffer::Allocate()
{
	Unload();
	
	mBufferSize = mSectorCount * 2 + ADDITIONAL_ENTRIES;
	mByteSize = mBufferSize * sizeof(VSMatrix);
	glGenBuffers(1, &mBufferId);
	glBindBufferBase(mBufferType, TEXMATRIX_BINDINGPOINT, mBufferId);
	glBindBuffer(mBufferType, mBufferId);
	if (gl.buffermethod == BM_PERSISTENT)
	{
		glBufferStorage(mBufferType, mByteSize, NULL, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
		mBufferPointer = (VSMatrix*)glMapBufferRange(mBufferType, 0, mByteSize, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
		mPersistent = true;
	}
	else
	{
		glBufferData(mBufferType, mByteSize, NULL, GL_DYNAMIC_DRAW);
		mBufferPointer = nullptr;
		mPersistent = false;
	}
	Map();
	// Index #0 is the identity matrix.
	// Index #1 is a y-flip matrix for camera textures.
	VSMatrix mat;
	mat.loadIdentity();
	mBufferPointer[0] = mat;
	mat.scale(1.f, -1.f, 1.f);
	mat.translate(0.f, 1.f, 0.0f);
	mBufferPointer[1] = mat;
	Unmap();
}

void GLTextureMatrixBuffer::ValidateSize(int numsectors)
{
	if (mSectorCount < numsectors)
	{
		mSectorCount = MAX<unsigned>(mSectorCount*2, numsectors);
		Allocate();
	}
	// Mark all indices as uninitialized.
	int tmindex = -ADDITIONAL_ENTRIES;
	for (auto &sec : level.sectors)
	{
		sec.planes[sector_t::floor].ubIndexMatrix = tmindex--;
		sec.planes[sector_t::ceiling].ubIndexMatrix = tmindex--;
	}
}

void GLTextureMatrixBuffer::Map()
{
	if (!mPersistent)
	{
		glBindBuffer(mBufferType, mBufferId);
		mBufferPointer = (VSMatrix*)glMapBufferRange(mBufferType, 0, mByteSize, GL_MAP_WRITE_BIT);
	}
}

void GLTextureMatrixBuffer::Unmap()
{
	if (!mPersistent && mBufferPointer)
	{
		glBindBuffer(mBufferType, mBufferId);
		glUnmapBuffer(mBufferType);
		mBufferPointer = nullptr;
	}
}

int GLTextureMatrixBuffer::Upload(const VSMatrix &mat, int index)
{
	assert(mBufferPointer != nullptr);	// May only be called when the buffer is mapped.
	if (mBufferPointer == nullptr) return 0;
	
	if (index == -1) // dynamic case for sky domes.
	{
		// The dynamic case for sky domes uses a simple circular buffer. 48 sky domes in a single map are highly unlikely and due to the serial nature of portal rendering
		// even then has little chance on stomping on data still in flight.
		// This can only happen in non-multithreaded parts right now so no synchronization is needed here.
		index = mUploadIndex++;
		if (index >= 50)
		{
			index = 2;
		}
	}
	
	mBufferPointer[index] = mat;
	return index;
}

// When this gets called we already have ruled out the case where rebinding is not needed.
int  GLTextureMatrixBuffer::DoBind(unsigned int index)
{
	unsigned int offset = (index*sizeof(VSMatrix) / mBlockAlign) * mBlockAlign;

	if (offset != mLastMappedIndex)
	{
		mLastMappedIndex = offset;
		glBindBufferRange(GL_UNIFORM_BUFFER, TEXMATRIX_BINDINGPOINT, mBufferId, offset, MIN(mBlockAlign, mByteSize - offset));
	}
	return index - (offset/sizeof(VSMatrix));
}


