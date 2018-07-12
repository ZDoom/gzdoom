#pragma once

#include <stdint.h>
#include "tarray.h"

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


public:
	IHardwareTexture() {}
	virtual ~IHardwareTexture() {}

	virtual void BindToFrameBuffer(int w, int h) = 0;
	virtual bool BindOrCreate(FTexture *tex, int texunit, int clampmode, int translation, int flags) = 0;
	virtual void AllocateBuffer(int w, int h, int texelsize) = 0;
	virtual uint8_t *MapBuffer() = 0;
	virtual unsigned int CreateTexture(unsigned char * buffer, int w, int h, int texunit, bool mipmap, int translation, const char *name) = 0;

	virtual void Clean(bool all) = 0;
	virtual void CleanUnused(SpriteHits &usedtranslations) = 0;
};

