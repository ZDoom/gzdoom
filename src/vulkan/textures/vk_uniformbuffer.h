#pragma once

#include "vulkan/system/vk_device.h"

class VkUniformBuffer
{
	VulkanDevice *vDevice;
    VkBuffer uniformBuffer = VK_NULL_HANDLE;
	VmaAllocation uniformBufferAllocation = VK_NULL_HANDLE;
	void *map = nullptr;
	int mapcnt = 0;
	
public:
	void Destroy();
	VkResult Create(size_t size);
	void *Map();
	void Unmap();

	VkUniformBuffer(VulkanDevice *dev)
	{
		vDevice = dev;
	}
	
	~VkUniformBuffer()
	{
		Destroy();
	}
	
	VkBuffer Handle()
	{
		return uniformBuffer;
	}
	

};
	