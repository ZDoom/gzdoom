
#pragma once

#include "vulkan/system/vk_objects.h"

class VulkanFrameBuffer;

class VkBufferManager
{
public:
	VkBufferManager(VulkanFrameBuffer* fb);
	~VkBufferManager();

private:
	VulkanFrameBuffer* fb = nullptr;
};
