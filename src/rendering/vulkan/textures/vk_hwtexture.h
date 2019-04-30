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

	static void ResetAll();
	void Reset();

	void Precache(FMaterial *mat, int translation, int flags);

	VulkanDescriptorSet *GetDescriptorSet(const FMaterialState &state);

	// Software renderer stuff
	void AllocateBuffer(int w, int h, int texelsize) override;
	uint8_t *MapBuffer() override;
	unsigned int CreateTexture(unsigned char * buffer, int w, int h, int texunit, bool mipmap, int translation, const char *name) override;

	// Wipe screen
	void CreateWipeTexture(int w, int h, const char *name);

	VulkanImage *GetImage(FTexture *tex, int translation, int flags);
	VulkanImageView *GetImageView(FTexture *tex, int translation, int flags);

	static void ResetAllDescriptors();

private:
	void CreateImage(FTexture *tex, int translation, int flags);

	void CreateTexture(int w, int h, int pixelsize, VkFormat format, const void *pixels);
	void GenerateMipmaps(VulkanImage *image, VulkanCommandBuffer *cmdbuffer);
	static int GetMipLevels(int w, int h);

	void ResetDescriptors();

	static VkHardwareTexture *First;
	VkHardwareTexture *Prev = nullptr;
	VkHardwareTexture *Next = nullptr;

	struct DescriptorEntry
	{
		int clampmode;
		int flags;
		std::unique_ptr<VulkanDescriptorSet> descriptor;

		DescriptorEntry(int cm, int f, std::unique_ptr<VulkanDescriptorSet> &&d)
		{
			clampmode = cm;
			flags = f;
			descriptor = std::move(d);
		}
	};

	std::vector<DescriptorEntry> mDescriptorSets;
	std::unique_ptr<VulkanImage> mImage;
	std::unique_ptr<VulkanImageView> mImageView;
	VkImageLayout mImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	int mTexelsize = 4;
};
