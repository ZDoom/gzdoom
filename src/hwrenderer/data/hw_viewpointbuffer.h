
#include "tarray.h"
#include "hwrenderer/data/buffers.h"

struct HWViewpointUniforms;
struct HWDrawInfo;

class GLViewpointBuffer
{
	IDataBuffer *mBuffer;

	unsigned int mBufferSize;
	unsigned int mBlockAlign;
	unsigned int mUploadIndex;
	unsigned int mLastMappedIndex;
	unsigned int mByteSize;
	TArray<bool> mClipPlaneInfo;
	
	unsigned int m2DWidth = ~0u, m2DHeight = ~0u;

	unsigned int mBlockSize;

	void CheckSize();

public:

	GLViewpointBuffer();
	~GLViewpointBuffer();
	void Clear();
	int Bind(HWDrawInfo *di, unsigned int index);
	void Set2D(HWDrawInfo *di, int width, int height);
	int SetViewpoint(HWDrawInfo *di, HWViewpointUniforms *vp);
	unsigned int GetBlockSize() const { return mBlockSize; }
};

