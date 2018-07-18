#pragma once

#ifdef LoadImage
#undef LoadImage
#endif

#define SHADED_TEXTURE -1
#define DIRECT_PALETTE -2

#include "tarray.h"
#include "hwrenderer/textures/hw_ihwtexture.h"
#include "volk/volk.h"

class FCanvasTexture;
class AActor;
class VkTexture;
class VkRenderTexture;
class VulkanDevice;

class VkHardwareTexture : public IHardwareTexture
{
public:
	enum
	{
		MAX_TEXTURES = 16
	};

private:
	struct TranslatedTexture
	{
		VkTexture *vkTexture;
		int translation;
		bool mipmapped;

		void Delete();
	};
	
	VulkanDevice *vDevice;

private:

	bool forcenocompression;

	TranslatedTexture vkDefTex;
	TArray<TranslatedTexture> vkTex_Translated;

	int vkTextureBytes = 4;

	TranslatedTexture * GetTexID(int translation);

public:
	VkHardwareTexture(VulkanDevice *dev, bool nocompress);
	~VkHardwareTexture();

	//static void Unbind(int texunit);
	//static void UnbindAll();

	//void BindToFrameBuffer(int w, int h);

	//unsigned int Bind(int texunit, int translation, bool needmipmap);
	//bool BindOrCreate(FTexture *tex, int texunit, int clampmode, int translation, int flags);

	//void AllocateBuffer(int w, int h, int texelsize);
	//uint8_t *MapBuffer();

	VkResult CreateTexture(unsigned char * buffer, int w, int h, bool mipmap, int translation);

	void Clean(bool all);
	void CleanUnused(SpriteHits &usedtranslations);

	VkTexture *GetVkTexture(FTexture *tex, int translation, bool needmipmap, int flags);
};

