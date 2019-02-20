#pragma once

#include "volk/volk.h"
#include "vk_mem_alloc/vk_mem_alloc.h"
#include <mutex>
#include <vector>
#include <algorithm>

class VulkanSwapChain;

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

	VkSemaphore imageAvailableSemaphore = VK_NULL_HANDLE;
	VkSemaphore renderFinishedSemaphore = VK_NULL_HANDLE;
	VkFence renderFinishedFence = VK_NULL_HANDLE;

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

	VkDebugUtilsMessengerEXT debugMessenger;
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

#if 0

struct QueueFamilyIndices
{
	int graphicsFamily = -1;
	int presentFamily = -1;

	bool isComplete()
	{
		return graphicsFamily >= 0 && presentFamily >= 0;
	}
};

struct SwapChainSupportDetails
{
	VkSurfaceCapabilitiesKHR capabilities;
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentModes;
};

class VulkanDevice
{
public:
	VkInstance vkInstance = VK_NULL_HANDLE;
	VkSurfaceKHR vkSurface = VK_NULL_HANDLE;
	VkPhysicalDevice vkPhysicalDevice = VK_NULL_HANDLE;
	VkDevice vkDevice = VK_NULL_HANDLE;
	VkDebugReportCallbackEXT vkCallback = VK_NULL_HANDLE;
	VkQueue vkGraphicsQueue = VK_NULL_HANDLE;
	VkQueue vkTransferQueue = VK_NULL_HANDLE;	// for image transfers only.
	VkQueue vkPresentQueue = VK_NULL_HANDLE;
	VmaAllocator vkAllocator = VK_NULL_HANDLE;
	VkCommandPool vkCommandPool = VK_NULL_HANDLE;
	int numAllocatorExtensions = 0;
	VkPhysicalDeviceProperties physicalProperties = {};

private:
	void CreateInstance();
	bool CheckValidationLayerSupport();
	void SetupDebugCallback();
	void CreateSurface();
	void PickPhysicalDevice();
	bool IsDeviceSuitable(VkPhysicalDevice device);
	bool CheckDeviceExtensionSupport(VkPhysicalDevice device);
	void CreateLogicalDevice();
	void CreateAllocator();
	void CreateCommandPool();

	std::vector<const char*> GetRequiredExtensions();

	std::mutex vkSingleTimeQueueMutex;

public:
	// This cannot be set up in the constructor because it may throw exceptions when initialization fails.
	void CreateDevice();
	void DestroyDevice();
	QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device);
	SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device);
	~VulkanDevice()
	{
		DestroyDevice();
	}

	VkCommandBuffer BeginSingleTimeCommands();
	void EndSingleTimeCommands(VkCommandBuffer commandBuffer);
	VkResult TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels);

	int GetTexDimension(int d)
	{
		return std::min<int>(d, physicalProperties.limits.maxImageDimension2D);
	}

	int GetUniformBufferAlignment()
	{
		return (int)physicalProperties.limits.minUniformBufferOffsetAlignment; 
	}
};

#endif
