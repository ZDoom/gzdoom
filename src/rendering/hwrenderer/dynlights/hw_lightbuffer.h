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
	int GetBinding(unsigned int index, size_t* pOffset, size_t* pSize);

	// OpenGL needs the buffer to mess around with the binding.
	IDataBuffer* GetBuffer() const
	{
		return mBuffer;
	}

};

int gl_SetDynModelLight(AActor *self, int dynlightindex);

#endif

