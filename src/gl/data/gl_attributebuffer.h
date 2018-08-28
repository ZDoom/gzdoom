#pragma once

#include <atomic>
#include <mutex>
#include "gl_load/gl_system.h"
#include "hwrenderer/data/shaderuniforms.h"
#include "hwrenderer/scene/hw_drawstructs.h"
#include "vectors.h"



class GLAttributeBuffer
{
	bool mIndexed;
	bool mPersistent;
	unsigned int mBufferId;
	unsigned int mBufferType;
	unsigned int mBufferSize;
	unsigned int mBlockAlign;
	std::atomic<unsigned int> mUploadIndex;
	unsigned int mLastMappedIndex;
	unsigned int mByteSize;
	void * mBufferPointer;

	unsigned int mBlockSize;
    unsigned int mReserved;

	TArray<AttributeBufferData> mBufferedData;
	std::mutex mBufferMutex;

	void Allocate();
	void DoBind(unsigned index);

public:

	GLAttributeBuffer();
	~GLAttributeBuffer();
	void Clear();
	void Map();
	void Unmap();
	int Upload(AttributeBufferData *attrs);
	void CheckSize();
	int Bind(unsigned int index)
	{
		glBindBufferRange(mBufferType, ATTRIBUTE_BINDINGPOINT, mBufferId, index * mBlockAlign, mBlockAlign);
		return index;
	}

};

