#pragma once

#include "tarray.h"
#include "hwrenderer/data/buffers.h"
#include "common/utility/matrix.h"
#include <atomic>
#include <mutex>

class DFrameBuffer;
class FRenderState;

class BoneBuffer
{
	DFrameBuffer* fb = nullptr;
	IBuffer *mBuffer;
	IBuffer* mBufferPipeline[HW_MAX_PIPELINE_BUFFERS];
	int mPipelineNbr;
	int mPipelinePos = 0;

	bool mBufferType;
    std::atomic<unsigned int> mIndex;
	unsigned int mBlockAlign;
	unsigned int mBlockSize;
	unsigned int mBufferSize;
	unsigned int mByteSize;
    unsigned int mMaxUploadSize;

public:
	BoneBuffer(DFrameBuffer* fb, int pipelineNbr = 1);
	~BoneBuffer();

	void Clear();
	int UploadBones(const TArray<VSMatrix> &bones);
	void Map() { mBuffer->Map(); }
	void Unmap() { mBuffer->Unmap(); }
	unsigned int GetBlockSize() const { return mBlockSize; }
	bool GetBufferType() const { return mBufferType; }
	int GetBinding(unsigned int index, size_t* pOffset, size_t* pSize);

	// Only for GLES to determin how much data is in the buffer
	int GetCurrentIndex() { return mIndex; };

	// OpenGL needs the buffer to mess around with the binding.
	IBuffer* GetBuffer() const
	{
		return mBuffer;
	}
};
