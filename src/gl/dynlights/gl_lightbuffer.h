#ifndef __GL_LIGHTBUFFER_H
#define __GL_LIGHTBUFFER_H

#include "tarray.h"
#include "hwrenderer/dynlights/hw_dynlightdata.h"

class FLightBuffer
{
	TArray<int> mIndices;
	unsigned int mBufferId;
	float * mBufferPointer;

	unsigned int mBufferType;
	unsigned int mIndex;
	unsigned int mUploadIndex;
	unsigned int mLastMappedIndex;
	unsigned int mBlockAlign;
	unsigned int mBlockSize;
	unsigned int mBufferSize;
	unsigned int mByteSize;

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
	unsigned int GetIndexPtr() const { return mIndices.Size();	}
	void StoreIndex(int index) { mIndices.Push(index); }
	int GetIndex(int i) const { return mIndices[i];	}
};

int gl_SetDynModelLight(AActor *self, int dynlightindex);

#endif

