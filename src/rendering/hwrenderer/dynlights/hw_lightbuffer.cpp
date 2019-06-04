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

#include "hw_lightbuffer.h"
#include "hwrenderer/utility/hw_clock.h"
#include "hwrenderer/dynlights/hw_dynlightdata.h"
#include "hwrenderer/data/shaderuniforms.h"

static const int ELEMENTS_PER_LIGHT = 4;			// each light needs 4 vec4's.
static const int ELEMENT_SIZE = (4*sizeof(float));


FLightBuffer::FLightBuffer()
{
	int maxNumberOfLights = 80000;
	
	mBufferSize = maxNumberOfLights * ELEMENTS_PER_LIGHT;
	mByteSize = mBufferSize * ELEMENT_SIZE;
	
	// Hack alert: On Intel's GL driver SSBO's perform quite worse than UBOs.
	// We only want to disable using SSBOs for lights but not disable the feature entirely.
	// Note that using an uniform buffer here will limit the number of lights per surface so it isn't done for NVidia and AMD.
	if (screen->IsVulkan() || ((screen->hwcaps & RFL_SHADER_STORAGE_BUFFER) && !strstr(screen->vendorstring, "Intel")))
	{
		mBufferType = true;
		mBlockAlign = 0;
		mBlockSize = mBufferSize;
		mMaxUploadSize = mBlockSize;
	}
	else
	{
		mBufferType = false;
		mBlockSize = screen->maxuniformblock / ELEMENT_SIZE;
		mBlockAlign = screen->uniformblockalignment / ELEMENT_SIZE;
		mMaxUploadSize = (mBlockSize - mBlockAlign);
		mByteSize += screen->maxuniformblock;	// to avoid mapping beyond the end of the buffer.
	}

	mBuffer = screen->CreateDataBuffer(LIGHTBUF_BINDINGPOINT, mBufferType, false);
	mBuffer->SetData(mByteSize, nullptr, false);

	Clear();
}

FLightBuffer::~FLightBuffer()
{
	delete mBuffer;
}

void FLightBuffer::Clear()
{
	mIndex = 0;
	mLastMappedIndex = UINT_MAX;
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

	float *mBufferPointer = (float*)mBuffer->Memory();
	assert(mBufferPointer != nullptr);
	if (mBufferPointer == nullptr) return -1;
	if (totalsize <= 1) return -1;	// there are no lights
	
	unsigned thisindex = mIndex.fetch_add(totalsize);
	float parmcnt[] = { 0, float(size0), float(size0 + size1), float(size0 + size1 + size2) };

	if (thisindex + totalsize <= mBufferSize)
	{
		float *copyptr = mBufferPointer + thisindex*4;
		
		memcpy(&copyptr[0], parmcnt, ELEMENT_SIZE);
		memcpy(&copyptr[4], &data.arrays[0][0], size0 * ELEMENT_SIZE);
		memcpy(&copyptr[4 + 4*size0], &data.arrays[1][0], size1 * ELEMENT_SIZE);
		memcpy(&copyptr[4 + 4*(size0 + size1)], &data.arrays[2][0], size2 * ELEMENT_SIZE);
		return thisindex;
	}
	else
	{
		return -1;	// Buffer is full. Since it is being used live at the point of the upload we cannot do much here but to abort.
	}
}

int FLightBuffer::DoBindUBO(unsigned int index)
{
	// this function will only get called if a uniform buffer is used. For a shader storage buffer we only need to bind the buffer once at the start.
	unsigned int offset = (index / mBlockAlign) * mBlockAlign;

	if (offset != mLastMappedIndex)
	{
		mLastMappedIndex = offset;
		mBuffer->BindRange(offset * ELEMENT_SIZE, mBlockSize * ELEMENT_SIZE);
	}
	return (index - offset);
}



