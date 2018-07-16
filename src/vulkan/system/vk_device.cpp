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
CVAR(Bool, vk_debug, true, 0);	// this should be false, once the oversized model can be removed.
#else
CVAR(Bool, vk_debug, true, 0);
#endif

static const std::array<const char*, 1> validationLayers = {
	"VK_LAYER_LUNARG_standard_validation"
};

static std::vector<const char*> deviceExtensions = {
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

		if (!strcmp(extension.extensionName, "VK_KHR_get_memory_requirements2")) numAllocatorExtensions++;
		else if (!strcmp(extension.extensionName, "VK_KHR_dedicated_allocation")) numAllocatorExtensions++;
	}

	if (requiredExtensions.empty())
	{
		if (numAllocatorExtensions == 2)
		{
			deviceExtensions.push_back("VK_KHR_get_memory_requirements2");
			deviceExtensions.push_back("VK_KHR_dedicated_allocation");
		}
		return true;
	}
	return false;
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
		queueCreateInfo.queueCount = queueFamily == indices.graphicsFamily? 2 : 1;
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
	vkGetDeviceQueue(vkDevice, indices.graphicsFamily, 1, &vkTransferQueue);
	vkGetDeviceQueue(vkDevice, indices.presentFamily, 0, &vkPresentQueue);
}

//==========================================================================
//
//
//
//==========================================================================

void VulkanDevice::CreateAllocator()
{
	VmaAllocatorCreateInfo allocinfo = {};

	allocinfo.flags = VMA_ALLOCATOR_CREATE_KHR_DEDICATED_ALLOCATION_BIT;
	allocinfo.physicalDevice = vkPhysicalDevice;
	allocinfo.device = vkDevice;
	allocinfo.preferredLargeHeapBlockSize = 64 * 1024 * 1024;	// 64 MiB.
	if (vmaCreateAllocator(&allocinfo, &vkAllocator) != VK_SUCCESS)
	{
		I_Error("Unable to create allocator");
	}
}

//==========================================================================
//
//
//
//==========================================================================

void VulkanDevice::CreateCommandPool() 
{
	QueueFamilyIndices queueFamilyIndices = FindQueueFamilies(vkPhysicalDevice);

	VkCommandPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily;

	if (vkCreateCommandPool(vkDevice, &poolInfo, nullptr, &vkCommandPool) != VK_SUCCESS) 
	{
		I_Error("failed to create graphics command pool!");
	}
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
	CreateCommandPool();
	CreateAllocator();
}

//==========================================================================
//
//
//
//==========================================================================

void VulkanDevice::DestroyDevice()
{
	if (vkAllocator != VK_NULL_HANDLE)
		vmaDestroyAllocator(vkAllocator);

	if (vkCommandPool != VK_NULL_HANDLE)
		vkDestroyCommandPool(vkDevice, vkCommandPool, nullptr);

	if (vkDevice != VK_NULL_HANDLE)
		vkDestroyDevice(vkDevice, nullptr);

	if (vkCallback != VK_NULL_HANDLE)
		vkDestroyDebugReportCallbackEXT(vkInstance, vkCallback, nullptr);
	
	if (vkSurface != VK_NULL_HANDLE)
		vkDestroySurfaceKHR(vkInstance, vkSurface, nullptr);

	if (vkInstance != VK_NULL_HANDLE)
		vkDestroyInstance(vkInstance, nullptr);
}

//==========================================================================
//
// Utilities
//
//==========================================================================

static bool hasStencilComponent(VkFormat format) 
{
	return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

//==========================================================================
//
// helpers for image/texture uploads
//
//==========================================================================

VkCommandBuffer VulkanDevice::BeginSingleTimeCommands() 
{
	VkCommandBufferAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool = vkCommandPool;
	allocInfo.commandBufferCount = 1;

	VkCommandBuffer commandBuffer;
	vkAllocateCommandBuffers(vkDevice, &allocInfo, &commandBuffer);

	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(commandBuffer, &beginInfo);

	return commandBuffer;
}

//==========================================================================
//
//
//
//==========================================================================

void VulkanDevice::EndSingleTimeCommands(VkCommandBuffer commandBuffer)
{
	vkEndCommandBuffer(commandBuffer);

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	{
		std::lock_guard<std::mutex> lock(vkSingleTimeQueueMutex);
		vkQueueSubmit(vkTransferQueue, 1, &submitInfo, VK_NULL_HANDLE);
		vkQueueWaitIdle(vkTransferQueue);
	}

	vkFreeCommandBuffers(vkDevice, vkCommandPool, 1, &commandBuffer);
}


//==========================================================================
//
// This function gets used from multiple places
//
//==========================================================================

VkResult VulkanDevice::TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels)
{
	VkImageMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;

	if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

		if (hasStencilComponent(format)) {
			barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
		}
	}
	else {
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	}

	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = mipLevels;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;

	VkPipelineStageFlags sourceStage;
	VkPipelineStageFlags destinationStage;

	if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
	{
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
	{
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) 
	{
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	}
	else 
	{
		return VK_ERROR_FORMAT_NOT_SUPPORTED;
	}
	VkCommandBuffer commandBuffer = BeginSingleTimeCommands();

	vkCmdPipelineBarrier(
		commandBuffer,
		sourceStage, destinationStage,
		0,
		0, nullptr,
		0, nullptr,
		1, &barrier
	);

	EndSingleTimeCommands(commandBuffer);
	return VK_SUCCESS;
}

