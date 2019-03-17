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
	void ResetDescriptors();

	void Precache(FMaterial *mat, int translation, int flags);

	VulkanDescriptorSet *GetDescriptorSet(const FMaterialState &state);

	// Software renderer stuff
	void AllocateBuffer(int w, int h, int texelsize) override;
	uint8_t *MapBuffer() override;
	unsigned int CreateTexture(unsigned char * buffer, int w, int h, int texunit, bool mipmap, int translation, const char *name) override;

	// Wipe screen
	void CreateWipeTexture(int w, int h, const char *name);

	static VkHardwareTexture *First;
	VkHardwareTexture *Prev = nullptr;
	VkHardwareTexture *Next = nullptr;

	VulkanImage *GetImage(FTexture *tex, int translation, int flags);
	VulkanImageView *GetImageView(FTexture *tex, int translation, int flags);

private:
	void CreateImage(FTexture *tex, int translation, int flags);

	void CreateTexture(int w, int h, int pixelsize, VkFormat format, const void *pixels);
	void GenerateMipmaps(VulkanImage *image, VulkanCommandBuffer *cmdbuffer);
	static int GetMipLevels(int w, int h);

	struct DescriptorKey
	{
		int clampmode;
		int flags;

		bool operator<(const DescriptorKey &other) const { return memcmp(this, &other, sizeof(DescriptorKey)) < 0; }
	};

	std::map<DescriptorKey, std::unique_ptr<VulkanDescriptorSet>> mDescriptorSets;
	std::unique_ptr<VulkanImage> mImage;
	std::unique_ptr<VulkanImageView> mImageView;
	std::unique_ptr<VulkanBuffer> mStagingBuffer;
	VkImageLayout mImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	int mTexelsize = 4;
};
