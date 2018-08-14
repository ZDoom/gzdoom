#ifndef __GL_LIGHTBUFFER_H
#define __GL_LIGHTBUFFER_H

#include "tarray.h"
#include "hwrenderer/dynlights/hw_dynlightdata.h"
#include <atomic>

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

public:

	FLightBuffer();
	~FLightBuffer();
	void Clear();
	int UploadLights(FDynLightData &data);
	void Begin();
	void Finish();
	int BindUBO(unsigned int index);
	unsigned int GetBlockSize() const { return mBlockSize; }
	unsigned int GetBufferType() const { return mBufferType; }
};

int gl_SetDynModelLight(AActor *self, int dynlightindex);

#endif

