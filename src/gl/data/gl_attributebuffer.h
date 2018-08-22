#pragma once

#include <atomic>
#include <mutex>
#include "gl_load/gl_system.h"
#include "hwrenderer/data/shaderuniforms.h"
#include "vectors.h"

struct AttributeBufferData
{
	FVector4 uLightColor;
	FVector4 uObjectColor;
	FVector4 uObjectColor2;
	FVector4 uGlowTopPlane;
	FVector4 uGlowTopColor;
	FVector4 uGlowBottomPlane;
	FVector4 uGlowBottomColor;
	FVector4 uSplitTopPlane;
	FVector4 uSplitBottomPlane;
	FVector4 uFogColor;
	FVector4 uDynLightColor;
	FVector2 uClipSplit;
	float uLightLevel;
	float uFogDensity;
	float uLightFactor;
	float uLightDist;
	float uDesaturationFactor;
	float timer;
	float uAlphaThreshold;
	int uTextureMode;
	int uFogEnabled;
	int uLightIndex;	
	int uTexMatrixIndex;
	int uNormaIsLight;
	
};


class GLAttributeBuffer
{
	bool mIndexed;
	bool mPersistent;
	unsigned int mBufferId;
	unsigned int mBufferType;
	unsigned int mBufferSize;
	unsigned int mBlockAlign;
	std::atomic<unsigned int> mUploadIndex;
	unsigned int mLastMappedIndex;
	unsigned int mByteSize;
	void * mBufferPointer;

	unsigned int mBlockSize;

	TArray<AttributeBufferData> mBufferedData;
	std::mutex mBufferMutex;

	void Allocate();
	void DoBind(unsigned index);

public:

	GLAttributeBuffer();
	~GLAttributeBuffer();
	void Clear();
	void Map();
	void Unmap();
	int Upload(AttributeBufferData *attrs);
	void CheckSize();
	int Bind(unsigned int index)
	{
		if (!mIndexed && index != mLastMappedIndex)
		{
			mLastMappedIndex = index;
			glBindBufferRange(mBufferType, AttributeBUF_BINDINGPOINT, mBufferId, index * mBlockAlign, mBlockAlign);
		}
		return index;
	}

};

