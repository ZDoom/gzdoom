// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2018 Christoph Oelckers
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
/*
** vk_device.cpp
** Implementation of the Vulkan device creation code
** Most of the contents in this file were lifted from the demo project for www.vulkan-tutorial.com
**
*/

#ifdef _WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#endif

#include "volk/volk.h"

#ifdef _WIN32
#undef max
#undef min

extern HWND Window;
#endif

#include <vector>
#include <array>
#include <set>

#include "vk_device.h"
#include "c_cvars.h"
#include "version.h"

#ifdef NDEBUG
CVAR(Bool, vk_debug, false, 0);
#else
CVAR(Bool, vk_debug, true, 0);
#endif

const std::array<const char*, 1> validationLayers = {
	"VK_LAYER_LUNARG_standard_validation"
};

const std::array<const char*, 1> deviceExtensions = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};


//==========================================================================
//
// From http://vulkan-tutorial.com
//
//==========================================================================

bool VulkanDevice::CheckValidationLayerSupport() 
{
	uint32_t layerCount;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

	std::vector<VkLayerProperties> availableLayers(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

	for (const char* layerName : validationLayers) {
		bool layerFound = false;

		for (const auto& layerProperties : availableLayers) {
			if (strcmp(layerName, layerProperties.layerName) == 0) {
				layerFound = true;
				break;
			}
		}

		if (!layerFound) {
			return false;
		}
	}

	return true;
}

//==========================================================================
//
//
//
//==========================================================================

std::vector<const char*> VulkanDevice::GetRequiredExtensions()
{
	std::vector<const char*> extensions = { "VK_KHR_surface" };

#ifdef _WIN32
	extensions.push_back("VK_KHR_win32_surface");
#elif defined __APPLE__
	extensions.push_back("VK_MVK_macos_surface");
#else
	// Todo.
#endif

	if (vk_debug) 
	{
		extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
	}
	return extensions;
}


//==========================================================================
//
// From http://vulkan-tutorial.com
//
//==========================================================================

void VulkanDevice::CreateInstance()
{
	if (vk_debug && !CheckValidationLayerSupport()) 
	{
		// Only print a development warning.
		DPrintf(DMSG_NOTIFY, "validation layers requested, but not available!");
		vk_debug = false;
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

	auto extensions = GetRequiredExtensions();
	createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
	createInfo.ppEnabledExtensionNames = extensions.data();

	if (vk_debug) 
	{
		createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
		createInfo.ppEnabledLayerNames = validationLayers.data();
	}
	else 
	{
		createInfo.enabledLayerCount = 0;
	}

	if (vkCreateInstance(&createInfo, nullptr, &vkInstance) != VK_SUCCESS) 
	{
		I_Error("failed to create instance!");
	}
	volkLoadInstance(vkInstance);
}

//==========================================================================
//
// From http://vulkan-tutorial.com
//
//==========================================================================

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objType, uint64_t obj, size_t location, int32_t code, const char* layerPrefix, const char* msg, void* userData)
{
	const char* prefix;

	if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT)
	{
		prefix = "error";
	}
	else if (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT)
	{
		prefix = "warning";
	}
	Printf("Vulkan validation layer %s: %s\n", prefix, msg);
	return VK_FALSE;
}

void VulkanDevice::SetupDebugCallback()
{
	if (!vk_debug) return;

	VkDebugReportCallbackCreateInfoEXT createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
	createInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
	createInfo.pfnCallback = debugCallback;

	if (vkCreateDebugReportCallbackEXT(vkInstance, &createInfo, nullptr, &vkCallback) != VK_SUCCESS) 
	{
		// This should not be fatal.
		DPrintf(DMSG_NOTIFY, "failed to set up debug callback!");
	}
}

//==========================================================================
//
// From http://vulkan-tutorial.com
//
//==========================================================================

void VulkanDevice::CreateSurface()
{
#ifdef _WIN32
	VkWin32SurfaceCreateInfoKHR createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	createInfo.hwnd = Window;
	createInfo.hinstance = GetModuleHandle(nullptr);
	auto success = vkCreateWin32SurfaceKHR(vkInstance, &createInfo, nullptr, &vkSurface) == VK_SUCCESS;
#elif defined __APPLE__
	// todo
#else
	// todo
#endif

	if (!success) 
	{
		I_Error("failed to create window surface!");
	}
}

//==========================================================================
//
// From http://vulkan-tutorial.com
//
//==========================================================================

QueueFamilyIndices VulkanDevice::FindQueueFamilies(VkPhysicalDevice device)
{
	QueueFamilyIndices indices;

	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

	int i = 0;
	for (const auto& queueFamily : queueFamilies) 
	{
		if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) 
		{
			indices.graphicsFamily = i;
		}

		VkBool32 presentSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(device, i, vkSurface, &presentSupport);

		if (queueFamily.queueCount > 0 && presentSupport) 
		{
			indices.presentFamily = i;
		}

		if (indices.isComplete()) 
		{
			break;
		}

		i++;
	}

	return indices;
}


//==========================================================================
//
// From http://vulkan-tutorial.com
//
//==========================================================================

bool VulkanDevice::CheckDeviceExtensionSupport(VkPhysicalDevice device)
{
	uint32_t extensionCount;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

	std::vector<VkExtensionProperties> availableExtensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

	std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

	for (const auto& extension : availableExtensions) 
	{
		requiredExtensions.erase(extension.extensionName);
	}

	return requiredExtensions.empty();
}

//==========================================================================
//
// From http://vulkan-tutorial.com
//
//==========================================================================

SwapChainSupportDetails VulkanDevice::QuerySwapChainSupport(VkPhysicalDevice device)
{
	SwapChainSupportDetails details;

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, vkSurface, &details.capabilities);

	uint32_t formatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, vkSurface, &formatCount, nullptr);

	if (formatCount != 0) 
	{
		details.formats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, vkSurface, &formatCount, details.formats.data());
	}

	uint32_t presentModeCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, vkSurface, &presentModeCount, nullptr);

	if (presentModeCount != 0) 
	{
		details.presentModes.resize(presentModeCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, vkSurface, &presentModeCount, details.presentModes.data());
	}

	return details;
}


//==========================================================================
//
// From http://vulkan-tutorial.com
//
//==========================================================================

bool VulkanDevice::IsDeviceSuitable(VkPhysicalDevice device)
{
	QueueFamilyIndices indices = FindQueueFamilies(device);

	bool extensionsSupported = CheckDeviceExtensionSupport(device);

	bool swapChainAdequate = false;
	if (extensionsSupported) 
	{
		SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(device);
		swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
	}

	VkPhysicalDeviceFeatures supportedFeatures;
	vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

	return indices.isComplete() && extensionsSupported && swapChainAdequate  && supportedFeatures.samplerAnisotropy;
}


//==========================================================================
//
// From http://vulkan-tutorial.com
//
//==========================================================================

void VulkanDevice::PickPhysicalDevice()
{
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(vkInstance, &deviceCount, nullptr);

	if (deviceCount == 0) 
	{
		I_Error("failed to find GPUs with Vulkan support!");
	}

	std::vector<VkPhysicalDevice> devices(deviceCount);
	vkEnumeratePhysicalDevices(vkInstance, &deviceCount, devices.data());

	for (const auto& device : devices) 
	{
		if (IsDeviceSuitable(device)) 
		{
			vkPhysicalDevice = device;
			break;
		}
	}

	if (vkPhysicalDevice == VK_NULL_HANDLE) 
	{
		I_Error("failed to find a suitable GPU!");
	}
}

//==========================================================================
//
// From http://vulkan-tutorial.com
//
//==========================================================================

void VulkanDevice::CreateLogicalDevice()
{
	QueueFamilyIndices indices = FindQueueFamilies(vkPhysicalDevice);

	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
	std::set<int> uniqueQueueFamilies = { indices.graphicsFamily, indices.presentFamily };

	float queuePriority = 1.0f;
	for (int queueFamily : uniqueQueueFamilies) 
	{
		VkDeviceQueueCreateInfo queueCreateInfo = {};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = queueFamily;
		queueCreateInfo.queueCount = 1;
		queueCreateInfo.pQueuePriorities = &queuePriority;
		queueCreateInfos.push_back(queueCreateInfo);
	}

	VkPhysicalDeviceFeatures deviceFeatures = {};
	deviceFeatures.samplerAnisotropy = VK_TRUE;

	VkDeviceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

	createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
	createInfo.pQueueCreateInfos = queueCreateInfos.data();

	createInfo.pEnabledFeatures = &deviceFeatures;

	createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
	createInfo.ppEnabledExtensionNames = deviceExtensions.data();

	if (vk_debug) 
	{
		createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
		createInfo.ppEnabledLayerNames = validationLayers.data();
	}
	else {
		createInfo.enabledLayerCount = 0;
	}

	if (vkCreateDevice(vkPhysicalDevice, &createInfo, nullptr, &vkDevice) != VK_SUCCESS)
	{
		I_Error("failed to create logical device!");
	}
	volkLoadDevice(vkDevice);

	vkGetDeviceQueue(vkDevice, indices.graphicsFamily, 0, &vkGraphicsQueue);
	vkGetDeviceQueue(vkDevice, indices.presentFamily, 0, &vkPresentQueue);
}


//==========================================================================
//
//
//
//==========================================================================

void VulkanDevice::CreateDevice()
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

	CreateInstance();
	SetupDebugCallback();
	CreateSurface();
	PickPhysicalDevice();
	CreateLogicalDevice();
}

//==========================================================================
//
//
//
//==========================================================================

void VulkanDevice::DestroyDevice()
{
	if (vkDevice != VK_NULL_HANDLE)
		vkDestroyDevice(vkDevice, nullptr);

	if (vkCallback != VK_NULL_HANDLE)
		vkDestroyDebugReportCallbackEXT(vkInstance, vkCallback, nullptr);
	
	if (vkSurface != VK_NULL_HANDLE)
		vkDestroySurfaceKHR(vkInstance, vkSurface, nullptr);

	if (vkInstance != VK_NULL_HANDLE)
		vkDestroyInstance(vkInstance, nullptr);
}

