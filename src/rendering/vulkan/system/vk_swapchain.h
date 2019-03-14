#pragma once

#include "vk_device.h"

class VulkanSwapChain
{
public:
	VulkanSwapChain(VulkanDevice *device);
	~VulkanSwapChain();

	bool vsync;
	VkSwapchainKHR swapChain;
	VkSurfaceFormatKHR swapChainFormat;

	std::vector<VkImage> swapChainImages;
	std::vector<VkImageView> swapChainImageViews;

	VkExtent2D actualExtent;

private:
	VulkanDevice *device = nullptr;

	VulkanSwapChain(const VulkanSwapChain &) = delete;
	VulkanSwapChain &operator=(const VulkanSwapChain &) = delete;
};
