
#pragma once

#include "vulkan/system/vk_objects.h"
#include <list>

class VulkanFrameBuffer;
class VkHardwareTexture;
class VkMaterial;

class VkTextureManager
{
public:
	VkTextureManager(VulkanFrameBuffer* fb);
	~VkTextureManager();

	void AddTexture(VkHardwareTexture* texture);
	void RemoveTexture(VkHardwareTexture* texture);

private:
	VulkanFrameBuffer* fb = nullptr;

	std::list<VkHardwareTexture*> Textures;
};
