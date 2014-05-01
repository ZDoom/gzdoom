#ifndef __PARAMBUFFER_H
#define __PARAMBUFFER_H

#include "doomtype.h"

struct ParameterBufferElement
{
	float vec[4];
};

class FDataBuffer
{
protected:
	unsigned int mBufferId;
	unsigned int mSize;
	unsigned int mPointer;
	unsigned int mStartIndex;
	void *mMappedBuffer;

public:

	FDataBuffer(unsigned int bytesize, int bindingpoint);
	~FDataBuffer();
	void StartFrame();
};

class FParameterBuffer : public FDataBuffer
{
public:
	FParameterBuffer();
	unsigned int Reserve(unsigned int amount, ParameterBufferElement **pptr);
};

struct AttribBufferElement
{
	PalEntry mColor;			// AARRGGBB: light color
	PalEntry mFogColor;			// RRGGBB: fog color, AA: fog enable flag
	PalEntry mLightAttr;		// AA: desaturation factor, RR light factor * 32, GG: light distance, BB: sector light level (RRGG only for lightmode 2, BB only for lightmode 8, otherwise 0)
	float mFogDensity;			// sadly this cannot be folded into the other values so we have to sacrifice a full 4 byte float for it...
	int mGlowIndex;
	int mLightIndex;
	int mMatIndex;
};

class FAttribBuffer : public FDataBuffer
{
	int mFirstFree;

public:
	FAttribBuffer();
	unsigned int Reserve(unsigned int amount, AttribBufferElement **pptr);
	unsigned int Allocate();
};


#endif