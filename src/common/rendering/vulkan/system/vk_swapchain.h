#pragma once

#include "vk_device.h"

class VulkanSemaphore;
class VulkanFence;
class VulkanFramebuffer;

class VulkanSwapChain
{
public:
	VulkanSwapChain(VulkanDevice *device);
	~VulkanSwapChain();

	uint32_t AcquireImage(int width, int height, bool vsync, VulkanSemaphore *semaphore = nullptr, VulkanFence *fence = nullptr);
	void QueuePresent(uint32_t imageIndex, VulkanSemaphore *semaphore = nullptr);

	bool IsHdrModeActive() const;

	VkSwapchainKHR swapChain = VK_NULL_HANDLE;
	VkSurfaceFormatKHR swapChainFormat;
	VkPresentModeKHR swapChainPresentMode;

	std::vector<VkImage> swapChainImages;
	std::vector<VkImageView> swapChainImageViews;
	std::vector<std::unique_ptr<VulkanFramebuffer>> framebuffers;

	VkExtent2D actualExtent;

private:
	void Recreate(bool vsync);
	void SelectFormat();
	void SelectPresentMode(bool vsync);
	bool CreateSwapChain(bool vsync, VkSwapchainKHR oldSwapChain = VK_NULL_HANDLE);
	void CreateViews();
	void GetImages();
	void ReleaseResources();
	void ReleaseViews();

	VkSurfaceCapabilitiesKHR GetSurfaceCapabilities();
	std::vector<VkSurfaceFormatKHR> GetSurfaceFormats();
	std::vector<VkPresentModeKHR> GetPresentModes();

	VulkanDevice *device = nullptr;

	int lastSwapWidth = 0;
	int lastSwapHeight = 0;
	bool lastVsync = false;
	bool lastHdr = false;

	VulkanSwapChain(const VulkanSwapChain &) = delete;
	VulkanSwapChain &operator=(const VulkanSwapChain &) = delete;
};
