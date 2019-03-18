#ifndef __GL_LIGHTBUFFER_H
#define __GL_LIGHTBUFFER_H

#include "tarray.h"
#include "hwrenderer/dynlights/hw_dynlightdata.h"
#include "hwrenderer/data/buffers.h"
#include <atomic>
#include <mutex>

class FRenderState;

class FLightBuffer
{
	IDataBuffer *mBuffer;

	bool mBufferType;
    std::atomic<unsigned int> mIndex;
	unsigned int mLastMappedIndex;
	unsigned int mBlockAlign;
	unsigned int mBlockSize;
	unsigned int mBufferSize;
	unsigned int mByteSize;
    unsigned int mMaxUploadSize;
    
	void CheckSize();

public:

	FLightBuffer();
	~FLightBuffer();
	void Clear();
	int UploadLights(FDynLightData &data);
	void Map() { mBuffer->Map(); }
	void Unmap() { mBuffer->Unmap(); }
	unsigned int GetBlockSize() const { return mBlockSize; }
	bool GetBufferType() const { return mBufferType; }

	int DoBindUBO(unsigned int index);

    int ShaderIndex(unsigned int index) const
    {
        if (mBlockAlign == 0) return index;
        // This must match the math in BindUBO.
        unsigned int offset = (index / mBlockAlign) * mBlockAlign;
        return int(index-offset);
    }

	// Only relevant for OpenGL, so this does not need access to the render state.
	int BindUBO(int index)
	{
		if (!mBufferType && index > -1)
		{
			index = DoBindUBO(index);
		}
		return index;
	}

	// The parameter is a reminder for Vulkan.
	void BindBase()
	{
		mBuffer->BindBase();
		mLastMappedIndex = UINT_MAX;
	}

};

int gl_SetDynModelLight(AActor *self, int dynlightindex);

#endif

