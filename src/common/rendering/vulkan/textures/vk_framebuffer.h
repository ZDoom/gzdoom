
#pragma once

#include "zvulkan/vulkanobjects.h"
#include <array>
#include <map>
#include <vector>

class VulkanRenderDevice;
enum class PPFilterMode;
enum class PPWrapMode;

class VkFramebufferManager
{
public:
	VkFramebufferManager(VulkanRenderDevice* fb);
	~VkFramebufferManager();

	void AcquireImage();
	void QueuePresent();

	std::map<int, std::unique_ptr<VulkanFramebuffer>> Framebuffers;

	std::shared_ptr<VulkanSwapChain> SwapChain;
	int PresentImageIndex = -1;

	std::unique_ptr<VulkanSemaphore> SwapChainImageAvailableSemaphore;
	std::vector<std::unique_ptr<VulkanSemaphore>> RenderFinishedSemaphores;

private:
	VulkanRenderDevice* fb = nullptr;
	int CurrentWidth = 0;
	int CurrentHeight = 0;
	bool CurrentVSync = false;
	bool CurrentHdr = false;
	bool CurrentExclusiveFullscreen = false;
};
