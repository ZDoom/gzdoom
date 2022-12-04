
#include "vulkanswapchain.h"
#include "vulkanobjects.h"
#include "vulkansurface.h"
#include "vulkanbuilders.h"

VulkanSwapChain::VulkanSwapChain(VulkanDevice* device) : device(device)
{
}

VulkanSwapChain::~VulkanSwapChain()
{
	views.clear();
	images.clear();
	if (swapchain)
		vkDestroySwapchainKHR(device->device, swapchain, nullptr);
}

void VulkanSwapChain::Create(int width, int height, int imageCount, bool vsync, bool hdr, bool exclusivefullscreen)
{
	views.clear();
	images.clear();

	SelectFormat(hdr);
	SelectPresentMode(vsync, exclusivefullscreen);

	VkSwapchainKHR oldSwapchain = swapchain;
	CreateSwapchain(width, height, imageCount, exclusivefullscreen, oldSwapchain);
	if (oldSwapchain)
		vkDestroySwapchainKHR(device->device, oldSwapchain, nullptr);

	if (exclusivefullscreen && lost)
	{
		// We could not acquire exclusive fullscreen. Fall back to normal fullsceen instead.
		exclusivefullscreen = false;

		SelectFormat(hdr);
		SelectPresentMode(vsync, exclusivefullscreen);

		oldSwapchain = swapchain;
		CreateSwapchain(width, height, imageCount, exclusivefullscreen, oldSwapchain);
		if (oldSwapchain)
			vkDestroySwapchainKHR(device->device, oldSwapchain, nullptr);
	}

	if (swapchain)
	{
		uint32_t imageCount;
		VkResult result = vkGetSwapchainImagesKHR(device->device, swapchain, &imageCount, nullptr);
		if (result != VK_SUCCESS)
			throw std::runtime_error("vkGetSwapchainImagesKHR failed");

		std::vector<VkImage> swapchainImages;
		swapchainImages.resize(imageCount);
		result = vkGetSwapchainImagesKHR(device->device, swapchain, &imageCount, swapchainImages.data());
		if (result != VK_SUCCESS)
			throw std::runtime_error("vkGetSwapchainImagesKHR failed (2)");

		for (VkImage vkimage : swapchainImages)
		{
			auto image = std::make_unique<VulkanImage>(device, vkimage, nullptr, actualExtent.width, actualExtent.height, 1, 1);
			auto view = ImageViewBuilder()
				.Type(VK_IMAGE_VIEW_TYPE_2D)
				.Image(image.get(), format.format)
				.DebugName("SwapchainImageView")
				.Create(device);
			images.push_back(std::move(image));
			views.push_back(std::move(view));
		}
	}
}

void VulkanSwapChain::SelectFormat(bool hdr)
{
	std::vector<VkSurfaceFormatKHR> surfaceFormats = GetSurfaceFormats();
	if (surfaceFormats.empty())
		throw std::runtime_error("No surface formats supported");

	if (surfaceFormats.size() == 1 && surfaceFormats.front().format == VK_FORMAT_UNDEFINED)
	{
		format.format = VK_FORMAT_B8G8R8A8_UNORM;
		format.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
		return;
	}

	if (hdr)
	{
		for (const auto& f : surfaceFormats)
		{
			if (f.format == VK_FORMAT_R16G16B16A16_SFLOAT && f.colorSpace == VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT)
			{
				format = f;
				return;
			}
		}
	}

	for (const auto& f : surfaceFormats)
	{
		if (f.format == VK_FORMAT_B8G8R8A8_UNORM && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
		{
			format = f;
			return;
		}
	}

	format = surfaceFormats.front();
}

void VulkanSwapChain::SelectPresentMode(bool vsync, bool exclusivefullscreen)
{
	std::vector<VkPresentModeKHR> presentModes = GetPresentModes(exclusivefullscreen);
	if (presentModes.empty())
		throw std::runtime_error("No surface present modes supported");

	presentMode = VK_PRESENT_MODE_FIFO_KHR;
	if (vsync)
	{
		bool supportsFifoRelaxed = std::find(presentModes.begin(), presentModes.end(), VK_PRESENT_MODE_FIFO_RELAXED_KHR) != presentModes.end();
		if (supportsFifoRelaxed)
			presentMode = VK_PRESENT_MODE_FIFO_RELAXED_KHR;
	}
	else
	{
		bool supportsMailbox = std::find(presentModes.begin(), presentModes.end(), VK_PRESENT_MODE_MAILBOX_KHR) != presentModes.end();
		bool supportsImmediate = std::find(presentModes.begin(), presentModes.end(), VK_PRESENT_MODE_IMMEDIATE_KHR) != presentModes.end();
		if (supportsMailbox)
			presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
		else if (supportsImmediate)
			presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
	}
}

bool VulkanSwapChain::CreateSwapchain(int width, int height, int imageCount, bool exclusivefullscreen, VkSwapchainKHR oldSwapChain)
{
	lost = false;

	VkResult result;
	VkSurfaceCapabilitiesKHR surfaceCapabilities;
#ifdef WIN32
	if (exclusivefullscreen && device->SupportsDeviceExtension(VK_EXT_FULL_SCREEN_EXCLUSIVE_EXTENSION_NAME))
	{
		VkPhysicalDeviceSurfaceInfo2KHR info = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR };
		VkSurfaceFullScreenExclusiveInfoEXT exclusiveInfo = { VK_STRUCTURE_TYPE_SURFACE_FULL_SCREEN_EXCLUSIVE_INFO_EXT };
		VkSurfaceFullScreenExclusiveWin32InfoEXT exclusiveWin32Info = { VK_STRUCTURE_TYPE_SURFACE_FULL_SCREEN_EXCLUSIVE_WIN32_INFO_EXT };
		info.surface = device->Surface->Surface;
		info.pNext = &exclusiveInfo;
		exclusiveInfo.fullScreenExclusive = VK_FULL_SCREEN_EXCLUSIVE_APPLICATION_CONTROLLED_EXT;
		exclusiveInfo.pNext = &exclusiveWin32Info;
		exclusiveWin32Info.hmonitor = MonitorFromWindow(device->Surface->Window, MONITOR_DEFAULTTONEAREST);

		VkSurfaceCapabilities2KHR capabilites = { VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR };
		VkSurfaceCapabilitiesFullScreenExclusiveEXT exclusiveCapabilities = { VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_FULL_SCREEN_EXCLUSIVE_EXT };
		capabilites.pNext = &exclusiveCapabilities;

		result = vkGetPhysicalDeviceSurfaceCapabilities2KHR(device->PhysicalDevice.Device, &info, &capabilites);
		if (result != VK_SUCCESS)
			throw std::runtime_error("vkGetPhysicalDeviceSurfaceCapabilities2EXT failed");

		surfaceCapabilities = capabilites.surfaceCapabilities;
		exclusivefullscreen = exclusiveCapabilities.fullScreenExclusiveSupported == VK_TRUE;
	}
	else
	{
		result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device->PhysicalDevice.Device, device->Surface->Surface, &surfaceCapabilities);
		if (result != VK_SUCCESS)
			throw std::runtime_error("vkGetPhysicalDeviceSurfaceCapabilitiesKHR failed");
		exclusivefullscreen = false;
	}
#else
	result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device->PhysicalDevice.Device, device->Surface->Surface, &surfaceCapabilities);
	if (result != VK_SUCCESS)
		throw std::runtime_error("vkGetPhysicalDeviceSurfaceCapabilitiesKHR failed");
	exclusivefullscreen = false;
#endif

	actualExtent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };
	actualExtent.width = std::max(surfaceCapabilities.minImageExtent.width, std::min(surfaceCapabilities.maxImageExtent.width, actualExtent.width));
	actualExtent.height = std::max(surfaceCapabilities.minImageExtent.height, std::min(surfaceCapabilities.maxImageExtent.height, actualExtent.height));
	if (actualExtent.width == 0 || actualExtent.height == 0)
	{
		swapchain = VK_NULL_HANDLE;
		lost = true;
		return false;
	}

	imageCount = std::max(surfaceCapabilities.minImageCount, std::min(surfaceCapabilities.maxImageCount, (uint32_t)imageCount));

	VkSwapchainCreateInfoKHR swapChainCreateInfo = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
	swapChainCreateInfo.surface = device->Surface->Surface;
	swapChainCreateInfo.minImageCount = imageCount;
	swapChainCreateInfo.imageFormat = format.format;
	swapChainCreateInfo.imageColorSpace = format.colorSpace;
	swapChainCreateInfo.imageExtent = actualExtent;
	swapChainCreateInfo.imageArrayLayers = 1;
	swapChainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	uint32_t queueFamilyIndices[] = { (uint32_t)device->GraphicsFamily, (uint32_t)device->PresentFamily };
	if (device->GraphicsFamily != device->PresentFamily)
	{
		swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		swapChainCreateInfo.queueFamilyIndexCount = 2;
		swapChainCreateInfo.pQueueFamilyIndices = queueFamilyIndices;
	}
	else
	{
		swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		swapChainCreateInfo.queueFamilyIndexCount = 0;
		swapChainCreateInfo.pQueueFamilyIndices = nullptr;
	}

	swapChainCreateInfo.preTransform = surfaceCapabilities.currentTransform;
	swapChainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; // If alpha channel is passed on to the DWM or not
	swapChainCreateInfo.presentMode = presentMode;
	swapChainCreateInfo.clipped = VK_FALSE;// VK_TRUE;
	swapChainCreateInfo.oldSwapchain = oldSwapChain;

#ifdef WIN32
	VkSurfaceFullScreenExclusiveInfoEXT exclusiveInfo = { VK_STRUCTURE_TYPE_SURFACE_FULL_SCREEN_EXCLUSIVE_INFO_EXT };
	VkSurfaceFullScreenExclusiveWin32InfoEXT exclusiveWin32Info = { VK_STRUCTURE_TYPE_SURFACE_FULL_SCREEN_EXCLUSIVE_WIN32_INFO_EXT };
	if (exclusivefullscreen && device->SupportsDeviceExtension(VK_EXT_FULL_SCREEN_EXCLUSIVE_EXTENSION_NAME))
	{
		swapChainCreateInfo.pNext = &exclusiveInfo;
		exclusiveInfo.fullScreenExclusive = VK_FULL_SCREEN_EXCLUSIVE_APPLICATION_CONTROLLED_EXT;
		exclusiveInfo.pNext = &exclusiveWin32Info;
		exclusiveWin32Info.hmonitor = MonitorFromWindow(device->Surface->Window, MONITOR_DEFAULTTONEAREST);
	}
#endif

	result = vkCreateSwapchainKHR(device->device, &swapChainCreateInfo, nullptr, &swapchain);
	if (result != VK_SUCCESS)
	{
		swapchain = VK_NULL_HANDLE;
		lost = true;
		return false;
	}

#ifdef WIN32
	if (exclusivefullscreen && device->SupportsDeviceExtension(VK_EXT_FULL_SCREEN_EXCLUSIVE_EXTENSION_NAME))
	{
		result = vkAcquireFullScreenExclusiveModeEXT(device->device, swapchain);
		if (result != VK_SUCCESS)
		{
			lost = true;
		}
	}
#endif

	return true;
}

int VulkanSwapChain::AcquireImage(VulkanSemaphore* semaphore, VulkanFence* fence)
{
	if (lost)
		return -1;

	uint32_t imageIndex;
	VkResult result = vkAcquireNextImageKHR(device->device, swapchain, 1'000'000'000, semaphore ? semaphore->semaphore : VK_NULL_HANDLE, fence ? fence->fence : VK_NULL_HANDLE, &imageIndex);
	if (result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR)
	{
		return imageIndex;
	}
	else if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT)
	{
		lost = true;
		return -1;
	}
	else if (result == VK_NOT_READY || result == VK_TIMEOUT)
	{
		return -1;
	}
	else
	{
		throw std::runtime_error("Failed to acquire next image!");
	}
}

void VulkanSwapChain::QueuePresent(int imageIndex, VulkanSemaphore* semaphore)
{
	uint32_t index = imageIndex;
	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = semaphore ? 1 : 0;
	presentInfo.pWaitSemaphores = semaphore ? &semaphore->semaphore : VK_NULL_HANDLE;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &swapchain;
	presentInfo.pImageIndices = &index;
	presentInfo.pResults = nullptr;
	VkResult result = vkQueuePresentKHR(device->PresentQueue, &presentInfo);
	if (result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR)
	{
		return;
	}
	else if (result == VK_ERROR_OUT_OF_DATE_KHR  || result == VK_ERROR_SURFACE_LOST_KHR || result == VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT)
	{
		lost = true;
	}
	else if (result == VK_ERROR_OUT_OF_HOST_MEMORY || result == VK_ERROR_OUT_OF_DEVICE_MEMORY)
	{
		// The spec says we can recover from this.
		// However, if we are out of memory it is better to crash now than in some other weird place further away from the source of the problem.

		throw std::runtime_error("vkQueuePresentKHR failed: out of memory");
	}
	else if (result == VK_ERROR_DEVICE_LOST)
	{
		throw std::runtime_error("vkQueuePresentKHR failed: device lost");
	}
	else
	{
		throw std::runtime_error("vkQueuePresentKHR failed");
	}
}

std::vector<VkSurfaceFormatKHR> VulkanSwapChain::GetSurfaceFormats()
{
	uint32_t surfaceFormatCount = 0;
	VkResult result = vkGetPhysicalDeviceSurfaceFormatsKHR(device->PhysicalDevice.Device, device->Surface->Surface, &surfaceFormatCount, nullptr);
	if (result != VK_SUCCESS)
		throw std::runtime_error("vkGetPhysicalDeviceSurfaceFormatsKHR failed");
	else if (surfaceFormatCount == 0)
		return {};

	std::vector<VkSurfaceFormatKHR> surfaceFormats(surfaceFormatCount);
	result = vkGetPhysicalDeviceSurfaceFormatsKHR(device->PhysicalDevice.Device, device->Surface->Surface, &surfaceFormatCount, surfaceFormats.data());
	if (result != VK_SUCCESS)
		throw std::runtime_error("vkGetPhysicalDeviceSurfaceFormatsKHR failed");
	return surfaceFormats;
}

std::vector<VkPresentModeKHR> VulkanSwapChain::GetPresentModes(bool exclusivefullscreen)
{
#ifdef WIN32
	if (exclusivefullscreen && device->SupportsDeviceExtension(VK_EXT_FULL_SCREEN_EXCLUSIVE_EXTENSION_NAME))
	{
		VkPhysicalDeviceSurfaceInfo2KHR surfaceInfo = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR };
		VkSurfaceFullScreenExclusiveInfoEXT exclusiveInfo = { VK_STRUCTURE_TYPE_SURFACE_FULL_SCREEN_EXCLUSIVE_INFO_EXT };
		surfaceInfo.surface = device->Surface->Surface;
		surfaceInfo.pNext = &exclusiveInfo;
		exclusiveInfo.fullScreenExclusive = VK_FULL_SCREEN_EXCLUSIVE_APPLICATION_CONTROLLED_EXT;

		uint32_t presentModeCount = 0;
		VkResult result = vkGetPhysicalDeviceSurfacePresentModes2EXT(device->PhysicalDevice.Device, &surfaceInfo, &presentModeCount, nullptr);
		if (result != VK_SUCCESS)
			throw std::runtime_error("vkGetPhysicalDeviceSurfacePresentModes2EXT failed");
		else if (presentModeCount == 0)
			return {};

		std::vector<VkPresentModeKHR> presentModes(presentModeCount);
		result = vkGetPhysicalDeviceSurfacePresentModes2EXT(device->PhysicalDevice.Device, &surfaceInfo, &presentModeCount, presentModes.data());
		if (result != VK_SUCCESS)
			throw std::runtime_error("vkGetPhysicalDeviceSurfacePresentModes2EXT failed");
		return presentModes;
	}
	else
#endif
	{
		uint32_t presentModeCount = 0;
		VkResult result = vkGetPhysicalDeviceSurfacePresentModesKHR(device->PhysicalDevice.Device, device->Surface->Surface, &presentModeCount, nullptr);
		if (result != VK_SUCCESS)
			throw std::runtime_error("vkGetPhysicalDeviceSurfacePresentModesKHR failed");
		else if (presentModeCount == 0)
			return {};

		std::vector<VkPresentModeKHR> presentModes(presentModeCount);
		result = vkGetPhysicalDeviceSurfacePresentModesKHR(device->PhysicalDevice.Device, device->Surface->Surface, &presentModeCount, presentModes.data());
		if (result != VK_SUCCESS)
			throw std::runtime_error("vkGetPhysicalDeviceSurfacePresentModesKHR failed");
		return presentModes;
	}
}
