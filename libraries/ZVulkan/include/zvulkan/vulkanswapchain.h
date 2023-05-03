#pragma once

#include "vulkandevice.h"
#include "vulkanobjects.h"

class VulkanSemaphore;
class VulkanFence;

class VulkanSwapChain
{
public:
	VulkanSwapChain(VulkanDevice* device);
	~VulkanSwapChain();

	void Create(int width, int height, int imageCount, bool vsync, bool hdr, bool exclusivefullscreen);
	bool Lost() const { return lost; }

	int Width() const { return actualExtent.width; }
	int Height() const { return actualExtent.height; }
	VkSurfaceFormatKHR Format() const { return format; }

	int ImageCount() const { return (int)images.size(); }
	VulkanImage* GetImage(int index) { return images[index].get(); }
	VulkanImageView* GetImageView(int index) { return views[index].get(); }

	int AcquireImage(VulkanSemaphore* semaphore = nullptr, VulkanFence* fence = nullptr);
	void QueuePresent(int imageIndex, VulkanSemaphore* semaphore = nullptr);

private:
	void SelectFormat(bool hdr);
	void SelectPresentMode(bool vsync, bool exclusivefullscreen);

	bool CreateSwapchain(int width, int height, int imageCount, bool exclusivefullscreen, VkSwapchainKHR oldSwapChain = VK_NULL_HANDLE);

	std::vector<VkSurfaceFormatKHR> GetSurfaceFormats();
	std::vector<VkPresentModeKHR> GetPresentModes(bool exclusivefullscreen);

	VulkanDevice* device = nullptr;
	bool lost = true;

	VkExtent2D actualExtent = {};
	VkSwapchainKHR swapchain = VK_NULL_HANDLE;
	VkSurfaceFormatKHR format = {};
	VkPresentModeKHR presentMode;
	std::vector<std::unique_ptr<VulkanImage>> images;
	std::vector<std::unique_ptr<VulkanImageView>> views;

	VulkanSwapChain(const VulkanSwapChain&) = delete;
	VulkanSwapChain& operator=(const VulkanSwapChain&) = delete;
};
