#ifndef __PARAMBUFFER_H
#define __PARAMBUFFER_H

struct ParameterBufferElement
{
	float vec[4];
};

class FParameterBuffer
{
	unsigned int mBufferId;
	unsigned int mSize;
	unsigned int mPointer;
	unsigned int mStartIndex;
	ParameterBufferElement *mMappedBuffer;
	
public:
	FParameterBuffer();
	~FParameterBuffer();
	void StartFrame();
	unsigned int Reserve(unsigned int amount, ParameterBufferElement **pptr);
};

	
#endif