#pragma once

#include "volk/volk.h"
#include "vk_mem_alloc/vk_mem_alloc.h"
#include <mutex>
#include <vector>
#include <algorithm>

class VulkanSwapChain;
class VulkanSemaphore;
class VulkanFence;

class VulkanDevice
{
public:
	VulkanDevice();
	~VulkanDevice();

	void windowResized();
	void waitPresent();

	void beginFrame();
	void presentFrame();

	VkDevice device = nullptr;

	int graphicsFamily = -1;
	int computeFamily = -1;
	int transferFamily = -1;
	int sparseBindingFamily = -1;
	int presentFamily = -1;

	std::unique_ptr<VulkanSwapChain> swapChain;
	uint32_t presentImageIndex = 0;

	VkQueue graphicsQueue = nullptr;

	std::unique_ptr<VulkanSemaphore> imageAvailableSemaphore;
	std::unique_ptr<VulkanSemaphore> renderFinishedSemaphore;
	std::unique_ptr<VulkanFence> renderFinishedFence;

	VmaAllocator allocator = VK_NULL_HANDLE;

	std::vector<VkLayerProperties> availableLayers;
	std::vector<VkExtensionProperties> extensions;
	std::vector<VkExtensionProperties> availableDeviceExtensions;

	uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

private:
	void createInstance();
	void createSurface();
	void selectPhysicalDevice();
	void createDevice();
	void createAllocator();
	void createSemaphores();
	void releaseResources();

	VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);

	std::vector<const char *> requiredExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

	VkInstance instance = nullptr;
	VkSurfaceKHR surface = 0;

	VkPhysicalDevice physicalDevice = {};

	VkQueue presentQueue = nullptr;

	VulkanDevice(const VulkanDevice &) = delete;
	VulkanDevice &operator=(const VulkanDevice &) = delete;

	friend class VulkanSwapChain;
};
