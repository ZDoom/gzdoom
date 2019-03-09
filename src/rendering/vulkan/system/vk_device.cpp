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

#include "vk_device.h"
#include "vk_swapchain.h"
#include "vk_objects.h"
#include "c_cvars.h"
#include "c_dispatch.h"
#include "i_system.h"
#include "version.h"
#include "doomerrors.h"
#include "gamedata/fonts/v_text.h"

void I_GetVulkanDrawableSize(int *width, int *height);
bool I_GetVulkanPlatformExtensions(unsigned int *count, const char **names);
bool I_CreateVulkanSurface(VkInstance instance, VkSurfaceKHR *surface);

// Physical device info
static std::vector<VulkanPhysicalDevice> AvailableDevices;
static std::vector<VulkanCompatibleDevice> SupportedDevices;

EXTERN_CVAR(Bool, vid_vsync);

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

		UsedDeviceFeatures.samplerAnisotropy = VK_TRUE;
		UsedDeviceFeatures.shaderClipDistance = VK_TRUE;
		UsedDeviceFeatures.fragmentStoresAndAtomics = VK_TRUE;
		UsedDeviceFeatures.depthClamp = VK_TRUE;
		UsedDeviceFeatures.shaderClipDistance = VK_TRUE;

		SelectPhysicalDevice();
		CreateDevice();
		CreateAllocator();

		int width, height;
		I_GetVulkanDrawableSize(&width, &height);
		swapChain = std::make_unique<VulkanSwapChain>(this, width, height, vid_vsync);

		CreateSemaphores();
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

bool VulkanDevice::CheckFeatures(const VkPhysicalDeviceFeatures &f)
{
	return
		f.samplerAnisotropy == VK_TRUE &&
		f.shaderClipDistance == VK_TRUE &&
		f.fragmentStoresAndAtomics == VK_TRUE &&
		f.depthClamp == VK_TRUE &&
		f.shaderClipDistance == VK_TRUE;
}

void VulkanDevice::SelectPhysicalDevice()
{
	AvailableDevices = GetPhysicalDevices(instance);

	for (size_t idx = 0; idx < AvailableDevices.size(); idx++)
	{
		const auto &info = AvailableDevices[idx];

		if (!CheckFeatures(info.Features))
			continue;

		VulkanCompatibleDevice dev;
		dev.device = &AvailableDevices[idx];

		int i = 0;
		for (const auto& queueFamily : info.QueueFamilies)
		{
			// Only accept a decent GPU for now..
			VkQueueFlags gpuFlags = (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_TRANSFER_BIT);
			if (queueFamily.queueCount > 0 && (queueFamily.queueFlags & gpuFlags) == gpuFlags)
			{
				dev.graphicsFamily = i;
				dev.transferFamily = i;
			}

			VkBool32 presentSupport = false;
			VkResult result = vkGetPhysicalDeviceSurfaceSupportKHR(info.Device, i, surface, &presentSupport);
			if (result == VK_SUCCESS && queueFamily.queueCount > 0 && presentSupport)
				dev.presentFamily = i;

			i++;
		}

		std::set<std::string> requiredExtensionSearch(EnabledDeviceExtensions.begin(), EnabledDeviceExtensions.end());
		for (const auto &ext : info.Extensions)
			requiredExtensionSearch.erase(ext.extensionName);
		if (!requiredExtensionSearch.empty())
			continue;

		if (dev.graphicsFamily != -1 && dev.presentFamily != -1 && dev.transferFamily != -1)
		{
			SupportedDevices.push_back(dev);
		}
	}

	if (SupportedDevices.empty())
		throw std::runtime_error("No Vulkan device supports the minimum requirements of this application");

	// The device order returned by Vulkan can be anything. Prefer discrete > integrated > virtual gpu > cpu > other
	std::stable_sort(SupportedDevices.begin(), SupportedDevices.end(), [&](const auto &a, const auto b) {

		// Sort by GPU type first. This will ensure the "best" device is most likely to map to vk_device 0
		static const int typeSort[] = { 4, 1, 0, 2, 3 };
		int sortA = a.device->Properties.deviceType < 5 ? typeSort[a.device->Properties.deviceType] : (int)a.device->Properties.deviceType;
		int sortB = b.device->Properties.deviceType < 5 ? typeSort[b.device->Properties.deviceType] : (int)b.device->Properties.deviceType;
		if (sortA < sortB)
			return true;

		// Then sort by the device's unique ID so that vk_device uses a consistent order
		int sortUUID = memcmp(a.device->Properties.pipelineCacheUUID, b.device->Properties.pipelineCacheUUID, VK_UUID_SIZE);
		return sortUUID < 0;
	});

	size_t selected = vk_device;
	if (selected >= SupportedDevices.size())
		selected = 0;

	PhysicalDevice = *SupportedDevices[selected].device;
	graphicsFamily = SupportedDevices[selected].graphicsFamily;
	presentFamily = SupportedDevices[selected].presentFamily;
	transferFamily = SupportedDevices[selected].transferFamily;
}

void VulkanDevice::WindowResized()
{
	int width, height;
	I_GetVulkanDrawableSize(&width, &height);

	swapChain.reset();
	swapChain = std::make_unique<VulkanSwapChain>(this, width, height, vid_vsync);
}

void VulkanDevice::WaitPresent()
{
	vkWaitForFences(device, 1, &renderFinishedFence->fence, VK_TRUE, std::numeric_limits<uint64_t>::max());
	vkResetFences(device, 1, &renderFinishedFence->fence);
}

void VulkanDevice::BeginFrame()
{
	VkResult result = vkAcquireNextImageKHR(device, swapChain->swapChain, std::numeric_limits<uint64_t>::max(), imageAvailableSemaphore->semaphore, VK_NULL_HANDLE, &presentImageIndex);
	if (result != VK_SUCCESS)
		throw std::runtime_error("Failed to acquire next image!");
}

void VulkanDevice::PresentFrame()
{
	VkSemaphore waitSemaphores[] = { renderFinishedSemaphore->semaphore };
	VkSwapchainKHR swapChains[] = { swapChain->swapChain };

	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = waitSemaphores;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapChains;
	presentInfo.pImageIndices = &presentImageIndex;
	presentInfo.pResults = nullptr;
	vkQueuePresentKHR(presentQueue, &presentInfo);
}

void VulkanDevice::CreateAllocator()
{
	VmaAllocatorCreateInfo allocinfo = {};
	// allocinfo.flags = VMA_ALLOCATOR_CREATE_KHR_DEDICATED_ALLOCATION_BIT; // To do: enable this for better performance
	allocinfo.physicalDevice = PhysicalDevice.Device;
	allocinfo.device = device;
	allocinfo.preferredLargeHeapBlockSize = 64 * 1024 * 1024;
	if (vmaCreateAllocator(&allocinfo, &allocator) != VK_SUCCESS)
		throw std::runtime_error("Unable to create allocator");
}

void VulkanDevice::CreateSemaphores()
{
	imageAvailableSemaphore.reset(new VulkanSemaphore(this));
	renderFinishedSemaphore.reset(new VulkanSemaphore(this));
	renderFinishedFence.reset(new VulkanFence(this));
}

void VulkanDevice::CreateDevice()
{
	float queuePriority = 1.0f;
	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;

	std::set<int> neededFamilies;
	neededFamilies.insert(graphicsFamily);
	neededFamilies.insert(presentFamily);
	neededFamilies.insert(transferFamily);

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
		throw std::runtime_error("Could not create vulkan device");

	volkLoadDevice(device);

	vkGetDeviceQueue(device, graphicsFamily, 0, &graphicsQueue);
	vkGetDeviceQueue(device, presentFamily, 0, &presentQueue);
	vkGetDeviceQueue(device, transferFamily, 0, &transferQueue);
}

void VulkanDevice::CreateSurface()
{
	if (!I_CreateVulkanSurface(instance, &surface))
	{
		throw std::runtime_error("Could not create vulkan surface");
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
		throw std::runtime_error("Could not create vulkan instance");

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
			throw std::runtime_error("vkCreateDebugUtilsMessengerEXT failed");
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
	bool found = seenMessages.find(msg) != seenMessages.end();
	if (!found)
	{
		if (totalMessages < 100)
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
			Printf(TEXTCOLOR_WHITE "%s\n", callbackData->pMessage);
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
	if (result != VK_SUCCESS)
		throw std::runtime_error("vkEnumeratePhysicalDevices failed");
	if (deviceCount == 0)
		return {};

	std::vector<VkPhysicalDevice> devices(deviceCount);
	result = vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
	if (result != VK_SUCCESS)
		throw std::runtime_error("vkEnumeratePhysicalDevices failed (2)");

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
		throw std::runtime_error("Cannot obtain number of Vulkan extensions");

	std::vector<const char *> extensions(extensionCount);
	if (!I_GetVulkanPlatformExtensions(&extensionCount, extensions.data()))
		throw std::runtime_error("Cannot obtain list of Vulkan extensions");
	return extensions;
}

void VulkanDevice::InitVolk()
{
	if (volkInitialize() != VK_SUCCESS)
	{
		throw std::runtime_error("Unable to find Vulkan");
	}
	auto iver = volkGetInstanceVersion();
	if (iver == 0)
	{
		throw std::runtime_error("Vulkan not supported");
	}
}

void VulkanDevice::ReleaseResources()
{
	if (device)
		vkDeviceWaitIdle(device);

	imageAvailableSemaphore.reset();
	renderFinishedSemaphore.reset();
	renderFinishedFence.reset();
	swapChain.reset();

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

	throw std::runtime_error("failed to find suitable memory type!");
}
