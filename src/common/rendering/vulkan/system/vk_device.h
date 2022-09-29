#pragma once

#include "volk/volk.h"
#include "vk_mem_alloc/vk_mem_alloc.h"
#include "engineerrors.h"
#include <mutex>
#include <vector>
#include <algorithm>
#include <memory>
#include "zstring.h"

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
	int presentFamily = -1;
	bool graphicsTimeQueries = false;
};

class VulkanDevice
{
public:
	VulkanDevice();
	~VulkanDevice();

	void SetDebugObjectName(const char *name, uint64_t handle, VkObjectType type)
	{
		if (!DebugLayerActive) return;

		VkDebugUtilsObjectNameInfoEXT info = {};
		info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
		info.objectHandle = handle;
		info.objectType = type;
		info.pObjectName = name;
		vkSetDebugUtilsObjectNameEXT(device, &info);
	}

	uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

	// Instance setup
	std::vector<VkLayerProperties> AvailableLayers;
	std::vector<VkExtensionProperties> Extensions;
	std::vector<const char *> EnabledExtensions;
	std::vector<const char *> OptionalExtensions = { VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME };
	std::vector<const char*> EnabledValidationLayers;
	uint32_t ApiVersion = {};

	// Device setup
	VkPhysicalDeviceFeatures UsedDeviceFeatures = {};
	std::vector<const char *> EnabledDeviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
	std::vector<const char *> OptionalDeviceExtensions =
	{
		VK_EXT_HDR_METADATA_EXTENSION_NAME,
		VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME,
		VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,
		VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
		VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
		VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
		VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
		VK_KHR_RAY_QUERY_EXTENSION_NAME
	};
	VulkanPhysicalDevice PhysicalDevice;
	bool DebugLayerActive = false;

	VkInstance instance = VK_NULL_HANDLE;
	VkSurfaceKHR surface = VK_NULL_HANDLE;
	VkDevice device = VK_NULL_HANDLE;
	VmaAllocator allocator = VK_NULL_HANDLE;

	VkQueue graphicsQueue = VK_NULL_HANDLE;
	VkQueue presentQueue = VK_NULL_HANDLE;

	int graphicsFamily = -1;
	int presentFamily = -1;
	bool graphicsTimeQueries = false;

	bool SupportsDeviceExtension(const char* ext) const;

private:
	void CreateInstance();
	void CreateSurface();
	void SelectPhysicalDevice();
	void SelectFeatures();
	void CreateDevice();
	void CreateAllocator();
	void ReleaseResources();

	static bool CheckRequiredFeatures(const VkPhysicalDeviceFeatures &f);

	VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;

	static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);

	static void InitVolk();
	static std::vector<VkLayerProperties> GetAvailableLayers();
	static std::vector<VkExtensionProperties> GetExtensions();
	static std::vector<const char *> GetPlatformExtensions();
	static std::vector<VulkanPhysicalDevice> GetPhysicalDevices(VkInstance instance);
};

FString VkResultToString(VkResult result);

class CVulkanError : public CEngineError
{
public:
	CVulkanError() : CEngineError() {}
	CVulkanError(const char* message) : CEngineError(message) {}
};


inline void VulkanError(const char *text)
{
	throw CVulkanError(text);
}

inline void CheckVulkanError(VkResult result, const char *text)
{
	if (result >= VK_SUCCESS)
		return;

	FString msg;
	msg.Format("%s: %s", text, VkResultToString(result).GetChars());
	throw CVulkanError(msg.GetChars());
}
