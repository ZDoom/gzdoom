#pragma once

#include "gl_load/gl_system.h"
#include "hwrenderer/data/shaderuniforms.h"

class VSMatrix;

class GLModelBuffer
{
	bool mIndexed;
	unsigned int mBufferId;
	unsigned int mBufferType;
	unsigned int mBufferSize;
	unsigned int mBlockAlign;
	unsigned int mUploadIndex;
	unsigned int mLastMappedIndex;
	unsigned int mByteSize;
	void * mBufferPointer;
	
	unsigned int mBlockSize;

	void CheckSize();
	void Allocate();

public:

	GLModelBuffer();
	~GLModelBuffer();
	void Clear();
	void Map();
	void Unmap();
	int Upload(VSMatrix *modelmat, float interpolationfactor);
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

