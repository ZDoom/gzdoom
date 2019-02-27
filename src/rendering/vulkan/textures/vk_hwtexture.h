#pragma once

#ifdef LoadImage
#undef LoadImage
#endif

#define SHADED_TEXTURE -1
#define DIRECT_PALETTE -2

#include "tarray.h"
#include "hwrenderer/textures/hw_ihwtexture.h"
#include "volk/volk.h"

struct FMaterialState;
class VulkanDescriptorSet;
class VulkanImage;
class VulkanImageView;
class VulkanBuffer;

class VkHardwareTexture : public IHardwareTexture
{
public:
	VkHardwareTexture();
	~VkHardwareTexture();

	void Reset();

	VulkanDescriptorSet *GetDescriptorSet(const FMaterialState &state);

	// Software renderer stuff
	void AllocateBuffer(int w, int h, int texelsize) override;
	uint8_t *MapBuffer() override;
	unsigned int CreateTexture(unsigned char * buffer, int w, int h, int texunit, bool mipmap, int translation, const char *name) override;

private:
	void CreateTexture(int w, int h, int pixelsize, VkFormat format, const void *pixels);
	void GenerateMipmaps(VulkanImage *image, VulkanCommandBuffer *cmdbuffer);
	static int GetMipLevels(int w, int h);

	std::unique_ptr<VulkanDescriptorSet> mDescriptorSet;
	std::unique_ptr<VulkanImage> mImage;
	std::unique_ptr<VulkanImageView> mImageView;
	std::unique_ptr<VulkanBuffer> mStagingBuffer;
	VkImageLayout mImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	int mTexelsize = 4;
};

#if 0

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

#endif
