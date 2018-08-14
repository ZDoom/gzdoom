#pragma once

#include <atomic>
#include <mutex>
#include "gl_load/gl_system.h"
#include "hwrenderer/data/shaderuniforms.h"
#include "r_data/matrix.h"

class VSMatrix;

struct ModelBufferData
{
    VSMatrix modelMatrix;
    VSMatrix normalModelMatrix;
    // normalModelMatrix[3][3] contains the interpolation factor, because this element does not affect anything relevant when multiplying with it.
};


class GLModelBuffer
{
	bool mIndexed;
	unsigned int mBufferId;
	unsigned int mBufferType;
	unsigned int mBufferSize;
	unsigned int mBlockAlign;
	std::atomic<unsigned int> mUploadIndex;
	unsigned int mLastMappedIndex;
	unsigned int mByteSize;
	void * mBufferPointer;

	unsigned int mBlockSize;

	TArray<ModelBufferData> mBufferedData;
	std::mutex mBufferMutex;

	void Allocate();
	void DoBind(unsigned index);

public:

	GLModelBuffer();
	~GLModelBuffer();
	void Clear();
	void Map();
	void Unmap();
	int Upload(VSMatrix *modelmat, float interpolationfactor);
	void CheckSize();
	int Bind(unsigned int index)
	{
		if (!mIndexed && index != mLastMappedIndex)
		{
			mLastMappedIndex = index;
			glBindBufferRange(mBufferType, MODELBUF_BINDINGPOINT, mBufferId, index * mBlockAlign, mBlockAlign);
		}
		return index;
	}

};

