
#ifndef __GLTEXTURE_H
#define __GLTEXTURE_H

#ifdef LoadImage
#undef LoadImage
#endif

#define SHADED_TEXTURE -1
#define DIRECT_PALETTE -2

#include "tarray.h"
#include "gl/system/gl_interface.h"

class FCanvasTexture;
class AActor;
typedef TMap<int, bool> SpriteHits;

// For error catching while changing parameters.
enum EInvalid
{
	Invalid = 0
};

enum
{
	GLT_CLAMPX=1,
	GLT_CLAMPY=2
};

class FHardwareTexture
{
public:
	enum
	{
		MAX_TEXTURES = 16
	};

private:
	struct TranslatedTexture
	{
		unsigned int glTexID;
		int translation;
		bool mipmapped;

		void Delete();
	};

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

	TranslatedTexture glDefTex;
	TArray<TranslatedTexture> glTex_Translated;
	unsigned int glDepthID;	// only used by camera textures
	unsigned int glBufferID = 0;
	int glTextureBytes = 4;

	TranslatedTexture * GetTexID(int translation);

	int GetDepthBuffer(int w, int h);
	void Resize(int swidth, int sheight, int width, int height, unsigned char *src_data, unsigned char *dst_data);

public:
	FHardwareTexture(bool nocompress);
	~FHardwareTexture();

	static void Unbind(int texunit);
	static void UnbindAll();

	void BindToFrameBuffer(int w, int h);

	unsigned int Bind(int texunit, int translation, bool needmipmap);
	void AllocateBuffer(int w, int h, int texelsize);
	uint8_t *MapBuffer();

	unsigned int CreateTexture(unsigned char * buffer, int w, int h, int texunit, bool mipmap, int translation, const FString &name) = delete;
	unsigned int CreateTexture(unsigned char * buffer, int w, int h, int texunit, bool mipmap, int translation, const char *name);
	unsigned int GetTextureHandle(int translation);

	void Clean(bool all);
	void CleanUnused(SpriteHits &usedtranslations);
};

#endif
