// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2014-2016 Christoph Oelckers
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
** gl_lightbuffer.cpp
** Buffer data maintenance for dynamic lights
**
**/

#include "gl_load/gl_system.h"
#include "gl/shaders/gl_shader.h"
#include "gl/dynlights/gl_lightbuffer.h"
#include "hwrenderer/utility/hw_clock.h"
#include "hwrenderer/dynlights/hw_dynlightdata.h"
#include "hwrenderer/data/shaderuniforms.h"

static const int ELEMENTS_PER_LIGHT = 4;			// each light needs 4 vec4's.
static const int ELEMENT_SIZE = (4*sizeof(float));


FLightBuffer::FLightBuffer()
{
	int maxNumberOfLights = gl.lightmethod ==  LM_DIRECT? 80000 : 40000;
	
	mBufferSize = maxNumberOfLights * ELEMENTS_PER_LIGHT;
	mByteSize = mBufferSize * ELEMENT_SIZE;
	
	// Hack alert: On Intel's GL driver SSBO's perform quite worse than UBOs.
	// We only want to disable using SSBOs for lights but not disable the feature entirely.
	// Note that using an uniform buffer here will limit the number of lights per surface so it isn't done for NVidia and AMD.
	if (gl.flags & RFL_SHADER_STORAGE_BUFFER && !strstr(gl.vendorstring, "Intel"))
	{
		mBufferType = GL_SHADER_STORAGE_BUFFER;
		mBlockAlign = 0;
		mBlockSize = mBufferSize / ELEMENT_SIZE;
		mMaxUploadSize = mBlockSize;
	}
	else
	{
		mBufferType = GL_UNIFORM_BUFFER;
		mBlockSize = gl.maxuniformblock / ELEMENT_SIZE;
		mBlockAlign = gl.uniformblockalignment / ELEMENT_SIZE;
		mMaxUploadSize = (mBlockSize - mBlockAlign);
		mByteSize += gl.maxuniformblock;	// to avoid mapping beyond the end of the buffer.
	}

	glGenBuffers(1, &mBufferId);
	glBindBufferBase(mBufferType, LIGHTBUF_BINDINGPOINT, mBufferId);
	glBindBuffer(mBufferType, mBufferId);	// Note: Some older AMD drivers don't do that in glBindBufferBase, as they should.
	if (gl.lightmethod == LM_DIRECT)
	{
		glBufferStorage(mBufferType, mByteSize, NULL, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
		mBufferPointer = (float*)glMapBufferRange(mBufferType, 0, mByteSize, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
	}
	else
	{
		glBufferData(mBufferType, mByteSize, NULL, GL_DYNAMIC_DRAW);
		mBufferPointer = NULL;
	}

	Clear();
	mLastMappedIndex = UINT_MAX;
}

FLightBuffer::~FLightBuffer()
{
	glBindBuffer(mBufferType, 0);
	glDeleteBuffers(1, &mBufferId);
}

void FLightBuffer::Clear()
{
	mIndex = 0;
}

int FLightBuffer::UploadLights(FDynLightData &data)
{
	// All meaasurements here are in vec4's.
	int size0 = data.arrays[0].Size()/4;
	int size1 = data.arrays[1].Size()/4;
	int size2 = data.arrays[2].Size()/4;
	int totalsize = size0 + size1 + size2 + 1;

	if (totalsize > (int)mMaxUploadSize)
	{
		int diff = totalsize - (int)mMaxUploadSize;
		
		size2 -= diff;
		if (size2 < 0)
		{
			size1 += size2;
			size2 = 0;
		}
		if (size1 < 0)
		{
			size0 += size1;
			size1 = 0;
		}
		totalsize = size0 + size1 + size2 + 1;
	}

	assert(mBufferPointer != nullptr);
	if (mBufferPointer == nullptr) return -1;

	if (totalsize <= 4 || mIndex + totalsize > mBufferSize) return -1;
	auto thisindex = mIndex.fetch_add(totalsize);
	if (thisindex + totalsize > mBufferSize) return -1;	// must retest because another thread might have changed mIndex.

	float *copyptr = mBufferPointer + thisindex*4;

	float parmcnt[] = { 0, float(size0), float(size0 + size1), float(size0 + size1 + size2) };
	memcpy(&copyptr[0], parmcnt, ELEMENT_SIZE);
	memcpy(&copyptr[4], &data.arrays[0][0], size0 * ELEMENT_SIZE);
	memcpy(&copyptr[4 + 4*size0], &data.arrays[1][0], size1 * ELEMENT_SIZE);
	memcpy(&copyptr[4 + 4*(size0 + size1)], &data.arrays[2][0], size2 * ELEMENT_SIZE);

	return thisindex;
}

void FLightBuffer::Begin()
{
	if (gl.lightmethod == LM_DEFERRED)
	{
		glBindBuffer(mBufferType, mBufferId);
		mBufferPointer = (float*)glMapBufferRange(mBufferType, 0, mByteSize, GL_MAP_WRITE_BIT);
	}
}

void FLightBuffer::Finish()
{
	if (gl.lightmethod == LM_DEFERRED)
	{
		glBindBuffer(mBufferType, mBufferId);
		glUnmapBuffer(mBufferType);
		mBufferPointer = NULL;
	}
}

int FLightBuffer::BindUBO(unsigned int index)
{
	unsigned int offset = (index / mBlockAlign) * mBlockAlign;

	if (offset != mLastMappedIndex)
	{
		// this will only get called if a uniform buffer is used. For a shader storage buffer we only need to bind the buffer once at the start to all shader programs
		mLastMappedIndex = offset;
		glBindBufferRange(GL_UNIFORM_BUFFER, LIGHTBUF_BINDINGPOINT, mBufferId, offset * ELEMENT_SIZE, mBlockSize * ELEMENT_SIZE);
	}
	return (index - offset);
}



