#pragma once
class FBitmap;
class FTexture;

#include "tarray.h"
#include "hw_ihwtexture.h"


#ifdef LoadImage
#undef LoadImage
#endif

#define SHADED_TEXTURE -1
#define DIRECT_PALETTE -2

#include "tarray.h"
#include "gl_interface.h"
#include "hw_ihwtexture.h"

class FCanvasTexture;

namespace OpenGLRenderer
{

class FHardwareTexture : public IHardwareTexture
{
public:

	static unsigned int lastbound[MAX_TEXTURES];

	static int GetTexDimension(int value)
	{
		if (value > gl.max_texturesize) return gl.max_texturesize;
		return value;
	}

	static void InitGlobalState() { for (int i = 0; i < MAX_TEXTURES; i++) lastbound[i] = 0; }

private:

	bool forcenofilter;

	unsigned int glTexID = 0;
	unsigned int glDepthID = 0;	// only used by camera textures
	unsigned int glBufferID = 0;
	int glTextureBytes;
	bool mipmapped = false;

	int GetDepthBuffer(int w, int h);

public:
	FHardwareTexture(int numchannels = 4, bool disablefilter = false)
	{
		forcenofilter = disablefilter;
		glTextureBytes = numchannels;
	}

	~FHardwareTexture();

	static void Unbind(int texunit);
	static void UnbindAll();

	void BindToFrameBuffer(int w, int h);

	unsigned int Bind(int texunit, bool needmipmap);
	bool BindOrCreate(FTexture* tex, int texunit, int clampmode, int translation, int flags);

	void AllocateBuffer(int w, int h, int texelsize);
	uint8_t* MapBuffer();

	unsigned int CreateTexture(unsigned char* buffer, int w, int h, int texunit, bool mipmap, const char* name);
	unsigned int GetTextureHandle()
	{
		return glTexID;
	}

	int numChannels() { return glTextureBytes; }
};

}
