#pragma once

#include <atomic>
#include <mutex>
#include "hwrenderer/data/shaderuniforms.h"
#include "r_data/matrix.h"

class VSMatrix;

class GLTextureMatrixBuffer
{
	bool mPersistent;
	unsigned int mBufferId;
	unsigned int mBufferType;
	unsigned int mBufferSize;
	unsigned int mBlockAlign;
	unsigned int mUploadIndex;
	unsigned int mLastMappedIndex;
	VSMatrix * mBufferPointer;

	unsigned int mBlockSize;
	unsigned int mByteSize;
	unsigned int mSectorCount;

	void Allocate();
	int DoBind(unsigned index);
	void Unload();

public:

	GLTextureMatrixBuffer();
	~GLTextureMatrixBuffer();
	void ValidateSize(int numsectors);
	int Upload(const VSMatrix &mat, int index);
	void Map();
	void Unmap();
	int Bind(unsigned int index)
	{
		if (mBlockAlign > 0) return DoBind(index);
		return index;
	}
	unsigned int GetBlockSize() const
	{
		return mBlockSize;
	}

};

