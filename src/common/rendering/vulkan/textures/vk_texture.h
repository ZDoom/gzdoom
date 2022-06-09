
#pragma once

#include "vulkan/system/vk_objects.h"

class VulkanFrameBuffer;

class VkTextureManager
{
public:
	VkTextureManager(VulkanFrameBuffer* fb);
	~VkTextureManager();

private:
	VulkanFrameBuffer* fb = nullptr;
};
