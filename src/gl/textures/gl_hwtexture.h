
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

	short texwidth, texheight;
	bool forcenocompression;

	TranslatedTexture glDefTex;
	TArray<TranslatedTexture> glTex_Translated;
	unsigned int glDepthID;	// only used by camera textures

	TranslatedTexture * GetTexID(int translation);

	int GetDepthBuffer();
	void Resize(int width, int height, unsigned char *src_data, unsigned char *dst_data);

public:
	FHardwareTexture(int w, int h, bool nocompress);
	~FHardwareTexture();

	static void Unbind(int texunit);
	static void UnbindAll();

	void BindToFrameBuffer();

	unsigned int Bind(int texunit, int translation, bool needmipmap);
	unsigned int CreateTexture(unsigned char * buffer, int w, int h, int texunit, bool mipmap, int translation, const FString &name);
	unsigned int GetTextureHandle(int translation);

	void Clean(bool all);
	void CleanUnused(SpriteHits &usedtranslations);
};

#endif
