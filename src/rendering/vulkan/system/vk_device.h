#pragma once

#include "volk/volk.h"
#include "vk_mem_alloc/vk_mem_alloc.h"
#include <mutex>
#include <vector>
#include <algorithm>
#include <memory>

class VulkanSwapChain;
class VulkanSemaphore;
class VulkanFence;

class VulkanPhysicalDevice
{
public:
	VkPhysicalDevice Device = VK_NULL_HANDLE;

	std::vector<VkExtensionProperties> Extensions;
	std::vector<VkQueueFamilyProperties> QueueFamilies;
	VkPhysicalDeviceProperties Properties = {};
	VkPhysicalDeviceFeatures Features = {};
	VkPhysicalDeviceMemoryProperties MemoryProperties = {};
};

class VulkanCompatibleDevice
{
public:
	VulkanPhysicalDevice *device = nullptr;
	int graphicsFamily = -1;
	int transferFamily = -1;
	int presentFamily = -1;
};

class VulkanDevice
{
public:
	VulkanDevice();
	~VulkanDevice();

	void WindowResized();
	void WaitPresent();

	void BeginFrame();
	void PresentFrame();

	uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

	// Instance setup
	std::vector<VkLayerProperties> AvailableLayers;
	std::vector<VkExtensionProperties> Extensions;
	std::vector<const char *> EnabledExtensions;
	std::vector<const char*> EnabledValidationLayers;

	// Device setup
	VkPhysicalDeviceFeatures UsedDeviceFeatures = {};
	std::vector<const char *> EnabledDeviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
	VulkanPhysicalDevice PhysicalDevice;

	VkInstance instance = VK_NULL_HANDLE;
	VkSurfaceKHR surface = VK_NULL_HANDLE;
	VkDevice device = VK_NULL_HANDLE;
	VmaAllocator allocator = VK_NULL_HANDLE;

	VkQueue graphicsQueue = VK_NULL_HANDLE;
	VkQueue presentQueue = VK_NULL_HANDLE;
	VkQueue transferQueue = VK_NULL_HANDLE;

	int graphicsFamily = -1;
	int transferFamily = -1;
	int presentFamily = -1;

	std::unique_ptr<VulkanSwapChain> swapChain;
	uint32_t presentImageIndex = 0;

	std::unique_ptr<VulkanSemaphore> imageAvailableSemaphore;
	std::unique_ptr<VulkanSemaphore> renderFinishedSemaphore;
	std::unique_ptr<VulkanFence> renderFinishedFence;

private:
	void CreateInstance();
	void CreateSurface();
	void SelectPhysicalDevice();
	void CreateDevice();
	void CreateAllocator();
	void CreateSemaphores();
	void ReleaseResources();

	static bool CheckFeatures(const VkPhysicalDeviceFeatures &f);

	VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;

	static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);

	static void InitVolk();
	static std::vector<VkLayerProperties> GetAvailableLayers();
	static std::vector<VkExtensionProperties> GetExtensions();
	static std::vector<const char *> GetPlatformExtensions();
	static std::vector<VulkanPhysicalDevice> GetPhysicalDevices(VkInstance instance);
};
