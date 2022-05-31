/*
**  Vulkan backend
**  Copyright (c) 2016-2020 Magnus Norddahl
**
**  This software is provided 'as-is', without any express or implied
**  warranty.  In no event will the authors be held liable for any damages
**  arising from the use of this software.
**
**  Permission is granted to anyone to use this software for any purpose,
**  including commercial applications, and to alter it and redistribute it
**  freely, subject to the following restrictions:
**
**  1. The origin of this software must not be misrepresented; you must not
**     claim that you wrote the original software. If you use this software
**     in a product, an acknowledgment in the product documentation would be
**     appreciated but is not required.
**  2. Altered source versions must be plainly marked as such, and must not be
**     misrepresented as being the original software.
**  3. This notice may not be removed or altered from any source distribution.
**
*/

#include "volk/volk.h"

#ifdef _WIN32
#undef max
#undef min
#endif

#include <inttypes.h>
#include <vector>
#include <array>
#include <set>
#include <string>
#include <algorithm>

#include "vk_device.h"
#include "vk_swapchain.h"
#include "vk_objects.h"
#include "c_cvars.h"
#include "c_dispatch.h"
#include "i_system.h"
#include "version.h"
#include "engineerrors.h"
#include "v_text.h"

bool I_GetVulkanPlatformExtensions(unsigned int *count, const char **names);
bool I_CreateVulkanSurface(VkInstance instance, VkSurfaceKHR *surface);

FString JitCaptureStackTrace(int framesToSkip, bool includeNativeFrames);

// Physical device info
static std::vector<VulkanPhysicalDevice> AvailableDevices;
static std::vector<VulkanCompatibleDevice> SupportedDevices;

CUSTOM_CVAR(Bool, vk_debug, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)
{
	Printf("This won't take effect until " GAMENAME " is restarted.\n");
}

CVAR(Bool, vk_debug_callstack, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)

CUSTOM_CVAR(Int, vk_device, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)
{
	Printf("This won't take effect until " GAMENAME " is restarted.\n");
}

CCMD(vk_listdevices)
{
	for (size_t i = 0; i < SupportedDevices.size(); i++)
	{
		Printf("#%d - %s\n", (int)i, SupportedDevices[i].device->Properties.deviceName);
	}
}

VulkanDevice::VulkanDevice()
{
	try
	{
		InitVolk();
		CreateInstance();
		CreateSurface();
		SelectPhysicalDevice();
		SelectFeatures();
		CreateDevice();
		CreateAllocator();
	}
	catch (...)
	{
		ReleaseResources();
		throw;
	}
}

VulkanDevice::~VulkanDevice()
{
	ReleaseResources();
}

void VulkanDevice::SelectFeatures()
{
	UsedDeviceFeatures.samplerAnisotropy = PhysicalDevice.Features.samplerAnisotropy;
	UsedDeviceFeatures.fragmentStoresAndAtomics = PhysicalDevice.Features.fragmentStoresAndAtomics;
	UsedDeviceFeatures.depthClamp = PhysicalDevice.Features.depthClamp;
	UsedDeviceFeatures.shaderClipDistance = PhysicalDevice.Features.shaderClipDistance;
}

bool VulkanDevice::CheckRequiredFeatures(const VkPhysicalDeviceFeatures &f)
{
	return
		f.samplerAnisotropy == VK_TRUE &&
		f.fragmentStoresAndAtomics == VK_TRUE;
}

void VulkanDevice::SelectPhysicalDevice()
{
	AvailableDevices = GetPhysicalDevices(instance);
	if (AvailableDevices.empty())
		VulkanError("No Vulkan devices found. Either the graphics card has no vulkan support or the driver is too old.");

	for (size_t idx = 0; idx < AvailableDevices.size(); idx++)
	{
		const auto &info = AvailableDevices[idx];

		if (!CheckRequiredFeatures(info.Features))
			continue;

		std::set<std::string> requiredExtensionSearch(EnabledDeviceExtensions.begin(), EnabledDeviceExtensions.end());
		for (const auto &ext : info.Extensions)
			requiredExtensionSearch.erase(ext.extensionName);
		if (!requiredExtensionSearch.empty())
			continue;

		VulkanCompatibleDevice dev;
		dev.device = &AvailableDevices[idx];

		// Figure out what can present
		for (int i = 0; i < (int)info.QueueFamilies.size(); i++)
		{
			VkBool32 presentSupport = false;
			VkResult result = vkGetPhysicalDeviceSurfaceSupportKHR(info.Device, i, surface, &presentSupport);
			if (result == VK_SUCCESS && info.QueueFamilies[i].queueCount > 0 && presentSupport)
			{
				dev.presentFamily = i;
				break;
			}
		}

		// The vulkan spec states that graphics and compute queues can always do transfer.
		// Furthermore the spec states that graphics queues always can do compute.
		// Last, the spec makes it OPTIONAL whether the VK_QUEUE_TRANSFER_BIT is set for such queues, but they MUST support transfer.
		//
		// In short: pick the first graphics queue family for everything.
		for (int i = 0; i < (int)info.QueueFamilies.size(); i++)
		{
			const auto &queueFamily = info.QueueFamilies[i];
			if (queueFamily.queueCount > 0 && (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT))
			{
				dev.graphicsFamily = i;
				dev.graphicsTimeQueries = queueFamily.timestampValidBits != 0;
				break;
			}
		}

		if (dev.graphicsFamily != -1 && dev.presentFamily != -1)
		{
			SupportedDevices.push_back(dev);
		}
	}

	if (SupportedDevices.empty())
		VulkanError("No Vulkan device supports the minimum requirements of this application");

	// The device order returned by Vulkan can be anything. Prefer discrete > integrated > virtual gpu > cpu > other
	std::stable_sort(SupportedDevices.begin(), SupportedDevices.end(), [&](const auto &a, const auto b) {

		// Sort by GPU type first. This will ensure the "best" device is most likely to map to vk_device 0
		static const int typeSort[] = { 4, 1, 0, 2, 3 };
		int sortA = a.device->Properties.deviceType < 5 ? typeSort[a.device->Properties.deviceType] : (int)a.device->Properties.deviceType;
		int sortB = b.device->Properties.deviceType < 5 ? typeSort[b.device->Properties.deviceType] : (int)b.device->Properties.deviceType;
		if (sortA != sortB)
			return sortA < sortB;

		// Then sort by the device's unique ID so that vk_device uses a consistent order
		int sortUUID = memcmp(a.device->Properties.pipelineCacheUUID, b.device->Properties.pipelineCacheUUID, VK_UUID_SIZE);
		return sortUUID < 0;
	});

	size_t selected = vk_device;
	if (selected >= SupportedDevices.size())
		selected = 0;

	// Enable optional extensions we are interested in, if they are available on this device
	for (const auto &ext : SupportedDevices[selected].device->Extensions)
	{
		for (const auto &opt : OptionalDeviceExtensions)
		{
			if (strcmp(ext.extensionName, opt) == 0)
			{
				EnabledDeviceExtensions.push_back(opt);
			}
		}
	}

	PhysicalDevice = *SupportedDevices[selected].device;
	graphicsFamily = SupportedDevices[selected].graphicsFamily;
	presentFamily = SupportedDevices[selected].presentFamily;
	graphicsTimeQueries = SupportedDevices[selected].graphicsTimeQueries;
}

bool VulkanDevice::SupportsDeviceExtension(const char *ext) const
{
	return std::find(EnabledDeviceExtensions.begin(), EnabledDeviceExtensions.end(), ext) != EnabledDeviceExtensions.end();
}

void VulkanDevice::CreateAllocator()
{
	VmaAllocatorCreateInfo allocinfo = {};
	if (SupportsDeviceExtension(VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME) && SupportsDeviceExtension(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME))
		allocinfo.flags = VMA_ALLOCATOR_CREATE_KHR_DEDICATED_ALLOCATION_BIT;
	allocinfo.physicalDevice = PhysicalDevice.Device;
	allocinfo.device = device;
	allocinfo.instance = instance;
	allocinfo.preferredLargeHeapBlockSize = 64 * 1024 * 1024;
	if (vmaCreateAllocator(&allocinfo, &allocator) != VK_SUCCESS)
		VulkanError("Unable to create allocator");
}

void VulkanDevice::CreateDevice()
{
	float queuePriority = 1.0f;
	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;

	std::set<int> neededFamilies;
	neededFamilies.insert(graphicsFamily);
	neededFamilies.insert(presentFamily);

	for (int index : neededFamilies)
	{
		VkDeviceQueueCreateInfo queueCreateInfo = {};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = index;
		queueCreateInfo.queueCount = 1;
		queueCreateInfo.pQueuePriorities = &queuePriority;
		queueCreateInfos.push_back(queueCreateInfo);
	}

	VkDeviceCreateInfo deviceCreateInfo = {};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.queueCreateInfoCount = (uint32_t)queueCreateInfos.size();
	deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
	deviceCreateInfo.pEnabledFeatures = &UsedDeviceFeatures;
	deviceCreateInfo.enabledExtensionCount = (uint32_t)EnabledDeviceExtensions.size();
	deviceCreateInfo.ppEnabledExtensionNames = EnabledDeviceExtensions.data();
	deviceCreateInfo.enabledLayerCount = 0;

	VkResult result = vkCreateDevice(PhysicalDevice.Device, &deviceCreateInfo, nullptr, &device);
	CheckVulkanError(result, "Could not create vulkan device");

	volkLoadDevice(device);

	vkGetDeviceQueue(device, graphicsFamily, 0, &graphicsQueue);
	vkGetDeviceQueue(device, presentFamily, 0, &presentQueue);
}

void VulkanDevice::CreateSurface()
{
	if (!I_CreateVulkanSurface(instance, &surface))
	{
		VulkanError("Could not create vulkan surface");
	}
}

void VulkanDevice::CreateInstance()
{
	AvailableLayers = GetAvailableLayers();
	Extensions = GetExtensions();
	EnabledExtensions = GetPlatformExtensions();

	std::string debugLayer = "VK_LAYER_KHRONOS_validation";
	bool wantDebugLayer = vk_debug;
	bool debugLayerFound = false;
	if (wantDebugLayer)
	{
		for (const VkLayerProperties& layer : AvailableLayers)
		{
			if (layer.layerName == debugLayer)
			{
				EnabledValidationLayers.push_back(layer.layerName);
				EnabledExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
				debugLayerFound = true;
				break;
			}
		}
	}

	// Enable optional instance extensions we are interested in
	for (const auto &ext : Extensions)
	{
		for (const auto &opt : OptionalExtensions)
		{
			if (strcmp(ext.extensionName, opt) == 0)
			{
				EnabledExtensions.push_back(opt);
			}
		}
	}

	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "GZDoom";
	appInfo.applicationVersion = VK_MAKE_VERSION(VER_MAJOR, VER_MINOR, VER_REVISION);
	appInfo.pEngineName = "GZDoom";
	appInfo.engineVersion = VK_MAKE_VERSION(ENG_MAJOR, ENG_MINOR, ENG_REVISION);
	appInfo.apiVersion = VK_API_VERSION_1_0;

	VkInstanceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;
	createInfo.enabledExtensionCount = (uint32_t)EnabledExtensions.size();
	createInfo.enabledLayerCount = (uint32_t)EnabledValidationLayers.size();
	createInfo.ppEnabledLayerNames = EnabledValidationLayers.data();
	createInfo.ppEnabledExtensionNames = EnabledExtensions.data();

	VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);
	CheckVulkanError(result, "Could not create vulkan instance");

	volkLoadInstance(instance);

	if (debugLayerFound)
	{
		VkDebugUtilsMessengerCreateInfoEXT dbgCreateInfo = {};
		dbgCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		dbgCreateInfo.messageSeverity =
			//VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
			//VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		dbgCreateInfo.messageType =
			VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		dbgCreateInfo.pfnUserCallback = DebugCallback;
		dbgCreateInfo.pUserData = this;
		result = vkCreateDebugUtilsMessengerEXT(instance, &dbgCreateInfo, nullptr, &debugMessenger);
		CheckVulkanError(result, "vkCreateDebugUtilsMessengerEXT failed");

		DebugLayerActive = true;
	}
}

VkBool32 VulkanDevice::DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* callbackData, void* userData)
{
	static std::mutex mtx;
	static std::set<FString> seenMessages;
	static int totalMessages;

	std::unique_lock<std::mutex> lock(mtx);

	FString msg = callbackData->pMessage;

	// For patent-pending reasons the validation layer apparently can't do this itself..
	for (uint32_t i = 0; i < callbackData->objectCount; i++)
	{
		if (callbackData->pObjects[i].pObjectName)
		{
			FString hexname;
			hexname.Format("0x%" PRIx64, callbackData->pObjects[i].objectHandle);
			msg.Substitute(hexname.GetChars(), callbackData->pObjects[i].pObjectName);
		}
	}

	bool found = seenMessages.find(msg) != seenMessages.end();
	if (!found)
	{
		if (totalMessages < 20)
		{
			totalMessages++;
			seenMessages.insert(msg);

			const char *typestr;
			bool showcallstack = false;
			if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
			{
				typestr = "vulkan error";
				showcallstack = true;
			}
			else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
			{
				typestr = "vulkan warning";
			}
			else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
			{
				typestr = "vulkan info";
			}
			else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
			{
				typestr = "vulkan verbose";
			}
			else
			{
				typestr = "vulkan";
			}

			if (showcallstack)
				Printf("\n");
			Printf(TEXTCOLOR_RED "[%s] ", typestr);
			Printf(TEXTCOLOR_WHITE "%s\n", msg.GetChars());

			if (vk_debug_callstack && showcallstack)
			{
				FString callstack = JitCaptureStackTrace(0, true);
				if (!callstack.IsEmpty())
					Printf("%s\n", callstack.GetChars());
			}
		}
	}

	return VK_FALSE;
}

std::vector<VkLayerProperties> VulkanDevice::GetAvailableLayers()
{
	uint32_t layerCount;
	VkResult result = vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

	std::vector<VkLayerProperties> availableLayers(layerCount);
	result = vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
	return availableLayers;
}

std::vector<VkExtensionProperties> VulkanDevice::GetExtensions()
{
	uint32_t extensionCount = 0;
	VkResult result = vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

	std::vector<VkExtensionProperties> extensions(extensionCount);
	result = vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());
	return extensions;
}

std::vector<VulkanPhysicalDevice> VulkanDevice::GetPhysicalDevices(VkInstance instance)
{
	uint32_t deviceCount = 0;
	VkResult result = vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
	if (result == VK_ERROR_INITIALIZATION_FAILED) // Some drivers return this when a card does not support vulkan
		return {};
	CheckVulkanError(result, "vkEnumeratePhysicalDevices failed");
	if (deviceCount == 0)
		return {};

	std::vector<VkPhysicalDevice> devices(deviceCount);
	result = vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
	CheckVulkanError(result, "vkEnumeratePhysicalDevices failed (2)");

	std::vector<VulkanPhysicalDevice> devinfo(deviceCount);
	for (size_t i = 0; i < devices.size(); i++)
	{
		auto &dev = devinfo[i];
		dev.Device = devices[i];

		vkGetPhysicalDeviceMemoryProperties(dev.Device, &dev.MemoryProperties);
		vkGetPhysicalDeviceProperties(dev.Device, &dev.Properties);
		vkGetPhysicalDeviceFeatures(dev.Device, &dev.Features);

		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(dev.Device, &queueFamilyCount, nullptr);
		dev.QueueFamilies.resize(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(dev.Device, &queueFamilyCount, dev.QueueFamilies.data());

		uint32_t deviceExtensionCount = 0;
		vkEnumerateDeviceExtensionProperties(dev.Device, nullptr, &deviceExtensionCount, nullptr);
		dev.Extensions.resize(deviceExtensionCount);
		vkEnumerateDeviceExtensionProperties(dev.Device, nullptr, &deviceExtensionCount, dev.Extensions.data());
	}
	return devinfo;
}

std::vector<const char *> VulkanDevice::GetPlatformExtensions()
{
	uint32_t extensionCount = 0;
	if (!I_GetVulkanPlatformExtensions(&extensionCount, nullptr))
		VulkanError("Cannot obtain number of Vulkan extensions");

	std::vector<const char *> extensions(extensionCount);
	if (!I_GetVulkanPlatformExtensions(&extensionCount, extensions.data()))
		VulkanError("Cannot obtain list of Vulkan extensions");
	return extensions;
}

void VulkanDevice::InitVolk()
{
	if (volkInitialize() != VK_SUCCESS)
	{
		VulkanError("Unable to find Vulkan");
	}
	auto iver = volkGetInstanceVersion();
	if (iver == 0)
	{
		VulkanError("Vulkan not supported");
	}
}

void VulkanDevice::ReleaseResources()
{
	if (device)
		vkDeviceWaitIdle(device);

	if (allocator)
		vmaDestroyAllocator(allocator);

	if (device)
		vkDestroyDevice(device, nullptr);
	device = nullptr;

	if (surface)
		vkDestroySurfaceKHR(instance, surface, nullptr);
	surface = 0;

	if (debugMessenger)
		vkDestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);

	if (instance)
		vkDestroyInstance(instance, nullptr);
	instance = nullptr;
}

uint32_t VulkanDevice::FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
	for (uint32_t i = 0; i < PhysicalDevice.MemoryProperties.memoryTypeCount; i++)
	{
		if ((typeFilter & (1 << i)) && (PhysicalDevice.MemoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
			return i;
	}

	VulkanError("failed to find suitable memory type!");
	return 0;
}

FString VkResultToString(VkResult result)
{
	switch (result)
	{
	case VK_SUCCESS: return "success";
	case VK_NOT_READY: return "not ready";
	case VK_TIMEOUT: return "timeout";
	case VK_EVENT_SET: return "event set";
	case VK_EVENT_RESET: return "event reset";
	case VK_INCOMPLETE: return "incomplete";
	case VK_ERROR_OUT_OF_HOST_MEMORY: return "out of host memory";
	case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "out of device memory";
	case VK_ERROR_INITIALIZATION_FAILED: return "initialization failed";
	case VK_ERROR_DEVICE_LOST: return "device lost";
	case VK_ERROR_MEMORY_MAP_FAILED: return "memory map failed";
	case VK_ERROR_LAYER_NOT_PRESENT: return "layer not present";
	case VK_ERROR_EXTENSION_NOT_PRESENT: return "extension not present";
	case VK_ERROR_FEATURE_NOT_PRESENT: return "feature not present";
	case VK_ERROR_INCOMPATIBLE_DRIVER: return "incompatible driver";
	case VK_ERROR_TOO_MANY_OBJECTS: return "too many objects";
	case VK_ERROR_FORMAT_NOT_SUPPORTED: return "format not supported";
	case VK_ERROR_FRAGMENTED_POOL: return "fragmented pool";
	case VK_ERROR_OUT_OF_POOL_MEMORY: return "out of pool memory";
	case VK_ERROR_INVALID_EXTERNAL_HANDLE: return "invalid external handle";
	case VK_ERROR_SURFACE_LOST_KHR: return "surface lost";
	case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: return "native window in use";
	case VK_SUBOPTIMAL_KHR: return "suboptimal";
	case VK_ERROR_OUT_OF_DATE_KHR: return "out of date";
	case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR: return "incompatible display";
	case VK_ERROR_VALIDATION_FAILED_EXT: return "validation failed";
	case VK_ERROR_INVALID_SHADER_NV: return "invalid shader";
	case VK_ERROR_FRAGMENTATION_EXT: return "fragmentation";
	case VK_ERROR_NOT_PERMITTED_EXT: return "not permitted";
	default: break;
	}
	FString res;
	res.Format("vkResult %d", (int)result);
	return result;
}
