
#pragma once

#include "vulkan/system/vk_objects.h"
#include <list>

class VulkanFrameBuffer;
class VkHardwareTexture;
class VkMaterial;
class VkPPTexture;

class VkTextureManager
{
public:
	VkTextureManager(VulkanFrameBuffer* fb);
	~VkTextureManager();

	void AddTexture(VkHardwareTexture* texture);
	void RemoveTexture(VkHardwareTexture* texture);

	void AddPPTexture(VkPPTexture* texture);
	void RemovePPTexture(VkPPTexture* texture);

private:
	VulkanFrameBuffer* fb = nullptr;

	std::list<VkHardwareTexture*> Textures;
	std::list<VkPPTexture*> PPTextures;
};
