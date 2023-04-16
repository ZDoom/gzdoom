#pragma once
#include "tarray.h"
#include "hwrenderer/data/buffers.h"

struct HWViewpointUniforms;
class DFrameBuffer;
class FRenderState;

class HWViewpointBuffer
{
	DFrameBuffer* fb = nullptr;
	IDataBuffer *mBuffer;
	IDataBuffer* mBufferPipeline[HW_MAX_PIPELINE_BUFFERS];
	int mPipelineNbr;
	int mPipelinePos = 0;

	unsigned int mBufferSize;
	unsigned int mBlockAlign;
	unsigned int mUploadIndex;
	unsigned int mLastMappedIndex;
	unsigned int mByteSize;
	TArray<bool> mClipPlaneInfo;

	unsigned int mBlockSize;

	void CheckSize();

public:

	HWViewpointBuffer(DFrameBuffer* fb, int pipelineNbr = 1);
	~HWViewpointBuffer();
	void Clear();
	int Bind(FRenderState &di, unsigned int index);
	void Set2D(FRenderState &di, int width, int height, int pll = 0);
	int SetViewpoint(FRenderState &di, HWViewpointUniforms *vp);
	unsigned int GetBlockSize() const { return mBlockSize; }
};

