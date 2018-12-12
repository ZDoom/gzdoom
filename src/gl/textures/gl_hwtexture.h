
#ifndef __GLTEXTURE_H
#define __GLTEXTURE_H

#ifdef LoadImage
#undef LoadImage
#endif

#define SHADED_TEXTURE -1
#define DIRECT_PALETTE -2

#include "tarray.h"
#include "gl_load/gl_interface.h"
#include "hwrenderer/textures/hw_ihwtexture.h"

class FCanvasTexture;
class AActor;

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

	bool forcenocompression;

	unsigned int glTexID = 0;
	unsigned int glDepthID = 0;	// only used by camera textures
	unsigned int glBufferID = 0;
	int glTextureBytes = 4;
	bool mipmapped = false;

	int GetDepthBuffer(int w, int h);

public:
	FHardwareTexture(bool nocompress)
	{
		forcenocompression = nocompress;
	}

	~FHardwareTexture();

	static void Unbind(int texunit);
	static void UnbindAll();

	void BindToFrameBuffer(int w, int h);

	unsigned int Bind(int texunit, bool needmipmap);
	bool BindOrCreate(FTexture *tex, int texunit, int clampmode, int translation, int flags);

	void AllocateBuffer(int w, int h, int texelsize);
	uint8_t *MapBuffer();

	unsigned int CreateTexture(unsigned char * buffer, int w, int h, int texunit, bool mipmap, int translation, const char *name);
	unsigned int GetTextureHandle(int translation);
};

}
#endif
