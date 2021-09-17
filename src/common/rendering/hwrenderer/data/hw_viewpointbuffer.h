
#include "tarray.h"
#include "hwrenderer/data/buffers.h"

struct HWViewpointUniforms;
class FRenderState;

class HWViewpointBuffer
{
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
	
	int m2DWidth = -1, m2DHeight = -1;

	unsigned int mBlockSize;

	void CheckSize();

public:

	HWViewpointBuffer(int pipelineNbr = 1);
	~HWViewpointBuffer();
	void Clear();
	int Bind(FRenderState &di, unsigned int index);
	void Set2D(FRenderState &di, int width, int height, int pll = 0);
	int SetViewpoint(FRenderState &di, HWViewpointUniforms *vp);
	unsigned int GetBlockSize() const { return mBlockSize; }
};

