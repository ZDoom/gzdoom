#pragma once

#include "vulkan/system/vk_device.h"


class VkDepthBuffer
{

	VulkanDevice *vDevice;
    VkImage textureImage = VK_NULL_HANDLE;
    VmaAllocation textureImageAllocation = VK_NULL_HANDLE;
    VkImageView textureImageView = VK_NULL_HANDLE;
	
	VkResult CreateImage(uint32_t width, uint32_t height, VkImage& image, VmaAllocation& allocation);
	VkResult CreateImageView(VkImage image);
	void Destroy();

public:
	VkDepthBuffer(VulkanDevice *dev)
	{
		vDevice = dev;
	}

	~VkDepthBuffer()
	{
		Destroy();
	}

	VkResult Create(int width, int height);
	VkImageView Handle() const { return textureImageView; }

};
