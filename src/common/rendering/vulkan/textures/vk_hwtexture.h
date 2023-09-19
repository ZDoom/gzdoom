#pragma once

#ifdef LoadImage
#undef LoadImage
#endif

#define SHADED_TEXTURE -1
#define DIRECT_PALETTE -2

#include "tarray.h"
#include "hw_ihwtexture.h"
#include <zvulkan/vulkanobjects.h>
#include "vk_imagetransition.h"
#include "hw_material.h"
#include <list>

struct FMaterialState;
class VulkanDescriptorSet;
class VulkanImage;
class VulkanImageView;
class VulkanBuffer;
class VulkanRenderDevice;
class FGameTexture;

class VkHardwareTexture : public IHardwareTexture
{
	friend class VkMaterial;
public:
	VkHardwareTexture(VulkanRenderDevice* fb, int numchannels);
	~VkHardwareTexture();

	void Reset();

	// Software renderer stuff
	void AllocateBuffer(int w, int h, int texelsize) override;
	uint8_t *MapBuffer() override;
	unsigned int CreateTexture(unsigned char * buffer, int w, int h, int texunit, bool mipmap, const char *name) override;

	// Wipe screen
	void CreateWipeTexture(int w, int h, const char *name);

	VkTextureImage *GetImage(FTexture *tex, int translation, int flags);
	VkTextureImage *GetDepthStencil(FTexture *tex);

	VulkanRenderDevice* fb = nullptr;
	std::list<VkHardwareTexture*>::iterator it;

private:
	void CreateImage(FTexture *tex, int translation, int flags);

	void CreateTexture(int w, int h, int pixelsize, VkFormat format, const void *pixels, bool mipmap);
	static int GetMipLevels(int w, int h);

	VkTextureImage mImage;
	int mTexelsize = 4;

	VkTextureImage mDepthStencil;

	uint8_t* mappedSWFB = nullptr;
};

class VkMaterial : public FMaterial
{
public:
	VkMaterial(VulkanRenderDevice* fb, FGameTexture* tex, int scaleflags);
	~VkMaterial();

	void DeleteDescriptors() override;

	VulkanRenderDevice* fb = nullptr;
	std::list<VkMaterial*>::iterator it;

	VulkanDescriptorSet* GetDescriptorSet(int threadIndex, const FMaterialState& state);
	int GetBindlessIndex(const FMaterialState& state);

private:
	struct DescriptorEntry
	{
		int clampmode;
		intptr_t remap;
		std::unique_ptr<VulkanDescriptorSet> descriptor;
		int bindlessIndex;

		DescriptorEntry(int cm, intptr_t f, std::unique_ptr<VulkanDescriptorSet>&& d, int index)
		{
			clampmode = cm;
			remap = f;
			descriptor = std::move(d);
			bindlessIndex = index;
		}
	};

	struct ThreadDescriptorEntry
	{
		int clampmode;
		intptr_t remap;
		VulkanDescriptorSet* descriptor;

		ThreadDescriptorEntry(int cm, intptr_t f, VulkanDescriptorSet* d) : clampmode(cm), remap(f), descriptor(d) { }
	};

	VulkanDescriptorSet* GetDescriptorSet(const FMaterialState& state);
	DescriptorEntry& GetDescriptorEntry(const FMaterialState& state);

	std::vector<DescriptorEntry> mDescriptorSets;
	std::vector<std::vector<ThreadDescriptorEntry>> mThreadDescriptorSets;
};
