
#include "tarray.h"

struct HWViewpointUniforms;

class GLViewpointBuffer
{
	unsigned int mBufferId;
	unsigned int mBufferSize;
	unsigned int mBlockAlign;
	unsigned int mUploadIndex;
	unsigned int mLastMappedIndex;
	unsigned int mByteSize;
	void * mBufferPointer;
	TArray<bool> mClipPlaneInfo;
	
	unsigned int m2DWidth = ~0u, m2DHeight = ~0u;

	unsigned int mBlockSize;

	void CheckSize();
	void Allocate();

public:

	GLViewpointBuffer();
	~GLViewpointBuffer();
	void Clear();
	void Map();
	void Unmap();
	int Bind(unsigned int index);
	void Set2D(int width, int height);
	int SetViewpoint(HWViewpointUniforms *vp);
	unsigned int GetBlockSize() const { return mBlockSize; }
};

