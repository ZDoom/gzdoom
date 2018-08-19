#pragma once

#include <stdint.h>
#include "tarray.h"
#include "hwrenderer/data/uniformbuffer.h"

typedef TMap<int, bool> SpriteHits;
class FTexture;

class IHardwareTexture
{
public:
	enum
	{
		MAX_TEXTURES = 16
	};

public:
	IUniformBuffer *mCustomUniforms = nullptr;


public:
	IHardwareTexture() {}
	virtual ~IHardwareTexture()
	{
		if (mCustomUniforms) delete mCustomUniforms;
	}

	virtual void AllocateBuffer(int w, int h, int texelsize) = 0;
	virtual uint8_t *MapBuffer() = 0;
	virtual unsigned int CreateTexture(unsigned char * buffer, int w, int h, int texunit, bool mipmap, int translation, const char *name) = 0;

	virtual void Clean(bool all) = 0;
	virtual void CleanUnused(SpriteHits &usedtranslations) = 0;
};

