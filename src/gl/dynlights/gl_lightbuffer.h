#ifndef __GL_LIGHTBUFFER_H
#define __GL_LIGHTBUFFER_H

#include "tarray.h"
struct FDynLightData;

class FLightBuffer
{
	TArray<float> mBufferArray;
	unsigned int mBufferId;
	float * mBufferPointer;

	unsigned int mBufferType;
	unsigned int mIndex;
	unsigned int mLastMappedIndex;
	unsigned int mBlockAlign;
	unsigned int mBlockSize;

public:

	FLightBuffer();
	~FLightBuffer();
	void Clear();
	int UploadLights(FDynLightData &data);
	void Finish();
	int BindUBO(unsigned int index);
	unsigned int GetBlockSize() const { return mBlockSize; }
	unsigned int GetBufferType() const { return mBufferType; }
};

#endif

