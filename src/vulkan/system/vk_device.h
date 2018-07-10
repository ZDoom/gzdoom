#pragma once

#include "volk/volk.h"
#include "base_sysfb.h"

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
	VkQueue vkPresentQueue = VK_NULL_HANDLE;
private:
	void CreateInstance();
	bool CheckValidationLayerSupport();
	void SetupDebugCallback();
	void CreateSurface();
	void PickPhysicalDevice();
	bool IsDeviceSuitable(VkPhysicalDevice device);
	bool CheckDeviceExtensionSupport(VkPhysicalDevice device);
	void CreateLogicalDevice();
	std::vector<const char*> GetRequiredExtensions();

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
};
