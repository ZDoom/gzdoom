// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2018 Christoph Oelckers
// Copyright(C) 2019 Magnus Norddahl
// All rights reserved.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//--------------------------------------------------------------------------
//

#include "volk/volk.h"

#ifdef _WIN32
#undef max
#undef min
#endif

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
#include "doomerrors.h"
#include "gamedata/fonts/v_text.h"

bool I_GetVulkanPlatformExtensions(unsigned int *count, const char **names);
bool I_CreateVulkanSurface(VkInstance instance, VkSurfaceKHR *surface);

// Physical device info
static std::vector<VulkanPhysicalDevice> AvailableDevices;
static std::vector<VulkanCompatibleDevice> SupportedDevices;

CUSTOM_CVAR(Bool, vk_debug, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)
{
	Printf("This won't take effect until " GAMENAME " is restarted.\n");
}

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
		f.fragmentStoresAndAtomics == VK_TRUE &&
		f.depthClamp == VK_TRUE;
}

void VulkanDevice::SelectPhysicalDevice()
{
	AvailableDevices = GetPhysicalDevices(instance);
	if (AvailableDevices.empty())
		I_Error("No Vulkan devices found. Either the graphics card has no vulkan support or the driver is too old.");

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
				break;
			}
		}

		if (dev.graphicsFamily != -1 && dev.presentFamily != -1)
		{
			SupportedDevices.push_back(dev);
		}
	}

	if (SupportedDevices.empty())
		I_Error("No Vulkan device supports the minimum requirements of this application");

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
	allocinfo.preferredLargeHeapBlockSize = 64 * 1024 * 1024;
	if (vmaCreateAllocator(&allocinfo, &allocator) != VK_SUCCESS)
		I_Error("Unable to create allocator");
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
	if (result != VK_SUCCESS)
		I_Error("Could not create vulkan device");

	volkLoadDevice(device);

	vkGetDeviceQueue(device, graphicsFamily, 0, &graphicsQueue);
	vkGetDeviceQueue(device, presentFamily, 0, &presentQueue);
}

void VulkanDevice::CreateSurface()
{
	if (!I_CreateVulkanSurface(instance, &surface))
	{
		I_Error("Could not create vulkan surface");
	}
}

void VulkanDevice::CreateInstance()
{
	AvailableLayers = GetAvailableLayers();
	Extensions = GetExtensions();
	EnabledExtensions = GetPlatformExtensions();

	std::string debugLayer = "VK_LAYER_LUNARG_standard_validation";
	bool wantDebugLayer = vk_debug;
	bool debugLayerFound = false;
	for (const VkLayerProperties &layer : AvailableLayers)
	{
		if (layer.layerName == debugLayer && wantDebugLayer)
		{
			EnabledValidationLayers.push_back(debugLayer.c_str());
			EnabledExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
			debugLayerFound = true;
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
	if (result != VK_SUCCESS)
		I_Error("Could not create vulkan instance");

	volkLoadInstance(instance);

	if (debugLayerFound)
	{
		VkDebugUtilsMessengerCreateInfoEXT createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		createInfo.messageSeverity =
			//VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
			//VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		createInfo.messageType =
			VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		createInfo.pfnUserCallback = DebugCallback;
		createInfo.pUserData = this;
		result = vkCreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger);
		if (result != VK_SUCCESS)
			I_Error("vkCreateDebugUtilsMessengerEXT failed");

		DebugLayerActive = true;
	}
}

VkBool32 VulkanDevice::DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* callbackData, void* userData)
{
	VulkanDevice *device = (VulkanDevice*)userData;

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
			hexname.Format("0x%llx", callbackData->pObjects[i].objectHandle);
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
			if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
			{
				typestr = "vulkan error";
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

			Printf("\n");
			Printf(TEXTCOLOR_RED "[%s] ", typestr);
			Printf(TEXTCOLOR_WHITE "%s\n", msg.GetChars());
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
	if (result != VK_SUCCESS)
		I_Error("vkEnumeratePhysicalDevices failed");
	if (deviceCount == 0)
		return {};

	std::vector<VkPhysicalDevice> devices(deviceCount);
	result = vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
	if (result != VK_SUCCESS)
		I_Error("vkEnumeratePhysicalDevices failed (2)");

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
		I_Error("Cannot obtain number of Vulkan extensions");

	std::vector<const char *> extensions(extensionCount);
	if (!I_GetVulkanPlatformExtensions(&extensionCount, extensions.data()))
		I_Error("Cannot obtain list of Vulkan extensions");
	return extensions;
}

void VulkanDevice::InitVolk()
{
	if (volkInitialize() != VK_SUCCESS)
	{
		I_Error("Unable to find Vulkan");
	}
	auto iver = volkGetInstanceVersion();
	if (iver == 0)
	{
		I_Error("Vulkan not supported");
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

	I_FatalError("failed to find suitable memory type!");
	return 0;
}
