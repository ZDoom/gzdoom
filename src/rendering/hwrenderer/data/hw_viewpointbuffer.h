
#include "tarray.h"
#include "hwrenderer/data/buffers.h"

struct HWViewpointUniforms;
class FRenderState;

class GLViewpointBuffer
{
	IDataBuffer *mBuffer;

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

	GLViewpointBuffer();
	~GLViewpointBuffer();
	void Clear();
	int Bind(FRenderState &di, unsigned int index);
	void Set2D(FRenderState &di, int width, int height);
	int SetViewpoint(FRenderState &di, HWViewpointUniforms *vp);
	unsigned int GetBlockSize() const { return mBlockSize; }
};

