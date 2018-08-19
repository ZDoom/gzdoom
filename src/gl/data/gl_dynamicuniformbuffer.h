#pragma once


class GLDynamicUniformBuffer
{
	bool mPersistent;
	unsigned int mBufferId;
	unsigned int mBufferType;
	
	unsigned int mByteSize;				// size of buffer in bytes
	unsigned int mElementSize;			// size of one element in bytes
	unsigned int mElementCount;			// number of elements in the buffer
	unsigned int mReservedCount;		// number of reserved elements (will not be used for other things.)
	unsigned int mStreamCount;			// number of elements for streaming data (used as a ring buffer.)
	unsigned int mBlockAlign;			// binding alignment in bytes
	unsigned int mBlockSize;			// how many element fit in one bindable block.		
	unsigned int mUploadIndex;			// upload index for next streamable element
	unsigned int mLastBoundIndex;		// last active binding index
	char *mBufferPointer;
	void (*mStaticInitFunc)(char *buffer);

	void Allocate();
	int DoBind(unsigned index);
	void Unload();

public:

	// Init is a function because it may have to be called again upon reallocation.
	GLDynamicUniformBuffer(int bindingpoint, unsigned int elementsize, unsigned int reserved, unsigned forstreaming, void (*staticinitfunc)(char *buffer));
	~GLDynamicUniformBuffer();
	void ValidateSize(int numelements);
	int Upload(const void *src, int index, unsigned int count);
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
	unsigned int DynamicStart() const
	{
		return mReservedCount + mStreamCount;
	}

};

