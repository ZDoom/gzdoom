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
	PalEntry mLightAttr;		// AA: desaturation factor, the rest depends on the current light settings
								// for lightmode 2: RR: light distance, GGBB: light factor
								// for lightmode 4 or colored fog: GGBB: fog density
								// for lightmode 8: BB: sector light level
	int mGlowIndex;
	int mLightIndex;
	int mMatIndex;
};

class FAttribBuffer : public FDataBuffer
{
public:
	FAttribBuffer();
	unsigned int Reserve(unsigned int amount, AttribBufferElement **pptr);
};


#endif