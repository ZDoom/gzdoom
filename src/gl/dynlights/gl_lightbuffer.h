#ifndef __GL_LIGHTBUFFER_H
#define __GL_LIGHTBUFFER_H

#include "tarray.h"
struct FDynLightData;

class FLightBuffer
{
	TArray<float> mBufferArray;
	TArray<unsigned int> mBufferIds;
	TArray<unsigned int> mBufferStart;
	TArray<float *> mBufferPointers;
	unsigned int mBufferType;
	unsigned int mBufferSize;
	unsigned int mIndex;
	unsigned int mBufferNum;

	void AddBuffer();

public:

	FLightBuffer();
	~FLightBuffer();
	void Clear();
	void UploadLights(FDynLightData &data, unsigned int &buffernum, unsigned int &bufferindex);
	void Finish();
};

#endif

