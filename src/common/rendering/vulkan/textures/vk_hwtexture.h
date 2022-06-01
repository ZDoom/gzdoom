#pragma once

#ifdef LoadImage
#undef LoadImage
#endif

#define SHADED_TEXTURE -1
#define DIRECT_PALETTE -2

#include "tarray.h"
#include "hw_ihwtexture.h"
#include "volk/volk.h"
#include "vk_imagetransition.h"
#include "hw_material.h"

struct FMaterialState;
class VulkanDescriptorSet;
class VulkanImage;
class VulkanImageView;
class VulkanBuffer;
class FGameTexture;

class VkHardwareTexture : public IHardwareTexture
{
	friend class VkMaterial;
public:
	VkHardwareTexture(int numchannels);
	~VkHardwareTexture();

	static void ResetAll();
	void Reset();

	// Software renderer stuff
	void AllocateBuffer(int w, int h, int texelsize) override;
	uint8_t *MapBuffer() override;
	unsigned int CreateTexture(unsigned char * buffer, int w, int h, int texunit, bool mipmap, const char *name) override;

	// Wipe screen
	void CreateWipeTexture(int w, int h, const char *name);

	VkTextureImage *GetImage(FTexture *tex, int translation, int flags);
	VkTextureImage *GetDepthStencil(FTexture *tex);


private:
	void CreateImage(FTexture *tex, int translation, int flags);

	void CreateTexture(int w, int h, int pixelsize, VkFormat format, const void *pixels, bool mipmap);
	static int GetMipLevels(int w, int h);

	static VkHardwareTexture *First;
	VkHardwareTexture *Prev = nullptr;
	VkHardwareTexture *Next = nullptr;

	VkTextureImage mImage;
	int mTexelsize = 4;

	VkTextureImage mDepthStencil;

	uint8_t* mappedSWFB = nullptr;
};


class VkMaterial : public FMaterial
{
	static VkMaterial* First;
	VkMaterial* Prev = nullptr;
	VkMaterial* Next = nullptr;

	struct DescriptorEntry
	{
		int clampmode;
		intptr_t remap;
		std::unique_ptr<VulkanDescriptorSet> descriptor;

		DescriptorEntry(int cm, intptr_t f, std::unique_ptr<VulkanDescriptorSet>&& d)
		{
			clampmode = cm;
			remap = f;
			descriptor = std::move(d);
		}
	};

	std::vector<DescriptorEntry> mDescriptorSets;

public:
	VkMaterial(FGameTexture *tex, int scaleflags);
	~VkMaterial();
	VulkanDescriptorSet* GetDescriptorSet(const FMaterialState& state);
	void DeleteDescriptors() override;
	static void ResetAllDescriptors();

};