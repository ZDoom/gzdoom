#pragma once

#include "vk_device.h"

class VulkanSwapChain
{
public:
	VulkanSwapChain(VulkanDevice *device);
	~VulkanSwapChain();

	bool vsync;
	VkSwapchainKHR swapChain = VK_NULL_HANDLE;
	VkSurfaceFormatKHR swapChainFormat;
	VkPresentModeKHR swapChainPresentMode;

	std::vector<VkImage> swapChainImages;
	std::vector<VkImageView> swapChainImageViews;

	VkExtent2D actualExtent;

private:
	void SelectFormat();
	void SelectPresentMode();
	void CreateSwapChain();
	void CreateViews();
	void SetHdrMetadata();
	void GetImages();
	void ReleaseResources();

	VkSurfaceCapabilitiesKHR GetSurfaceCapabilities();
	std::vector<VkSurfaceFormatKHR> GetSurfaceFormats();
	std::vector<VkPresentModeKHR> GetPresentModes();

	VulkanDevice *device = nullptr;

	VulkanSwapChain(const VulkanSwapChain &) = delete;
	VulkanSwapChain &operator=(const VulkanSwapChain &) = delete;
};
