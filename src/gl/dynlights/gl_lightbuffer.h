#ifndef __GL_LIGHTBUFFER_H
#define __GL_LIGHTBUFFER_H

#include "tarray.h"
#include "hwrenderer/dynlights/hw_dynlightdata.h"
#include <atomic>
#include <mutex>

class FLightBuffer
{
	unsigned int mBufferId;
	float * mBufferPointer;

	unsigned int mBufferType;
    std::atomic<unsigned int> mIndex;
	unsigned int mLastMappedIndex;
	unsigned int mBlockAlign;
	unsigned int mBlockSize;
	unsigned int mBufferSize;
	unsigned int mByteSize;
    unsigned int mMaxUploadSize;
    
    std::mutex mBufferMutex;
    TArray<float> mBufferedData;

public:

	FLightBuffer();
	~FLightBuffer();
	void Clear();
	int UploadLights(FDynLightData &data);
	void Begin();
	void Finish();
    void CheckSize();
	int BindUBO(unsigned int index);
	unsigned int GetBlockSize() const { return mBlockSize; }
	unsigned int GetBufferType() const { return mBufferType; }
    
    int ShaderIndex(unsigned int index) const
    {
        if (mBlockAlign == 0) return index;
        // This must match the math in BindUBO.
        unsigned int offset = (index / mBlockAlign) * mBlockAlign;
        return int(index-offset);
    }

};

int gl_SetDynModelLight(AActor *self, int dynlightindex);

#endif

