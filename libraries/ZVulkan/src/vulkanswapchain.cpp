
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

	CreateSwapchain(width, height, imageCount, vsync, hdr, exclusivefullscreen);

	if (exclusivefullscreen && lost)
	{
		// We could not acquire exclusive fullscreen. Fall back to normal fullsceen instead.
		CreateSwapchain(width, height, imageCount, vsync, hdr, false);
	}

	if (swapchain)
	{
		uint32_t imageCount;
		VkResult result = vkGetSwapchainImagesKHR(device->device, swapchain, &imageCount, nullptr);
		if (result != VK_SUCCESS)
			VulkanError("vkGetSwapchainImagesKHR failed");

		std::vector<VkImage> swapchainImages;
		swapchainImages.resize(imageCount);
		result = vkGetSwapchainImagesKHR(device->device, swapchain, &imageCount, swapchainImages.data());
		if (result != VK_SUCCESS)
			VulkanError("vkGetSwapchainImagesKHR failed (2)");

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

void VulkanSwapChain::SelectFormat(const VulkanSurfaceCapabilities& caps, bool hdr)
{
	if (caps.Formats.size() == 1 && caps.Formats.front().format == VK_FORMAT_UNDEFINED)
	{
		format.format = VK_FORMAT_B8G8R8A8_UNORM;
		format.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
		return;
	}

	if (hdr)
	{
		for (const auto& f : caps.Formats)
		{
			if (f.format == VK_FORMAT_R16G16B16A16_SFLOAT && f.colorSpace == VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT)
			{
				format = f;
				return;
			}
		}
	}

	for (const auto& f : caps.Formats)
	{
		if (f.format == VK_FORMAT_B8G8R8A8_UNORM && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
		{
			format = f;
			return;
		}
	}

	format = caps.Formats.front();
}

bool VulkanSwapChain::CreateSwapchain(int width, int height, int imageCount, bool vsync, bool hdr, bool exclusivefullscreen)
{
	lost = false;

	VulkanSurfaceCapabilities caps = GetSurfaceCapabilities(exclusivefullscreen);

	if (exclusivefullscreen && (caps.PresentModes.empty() || !caps.FullScreenExclusive.fullScreenExclusiveSupported))
	{
		// Try again without exclusive full screen.
		exclusivefullscreen = false;
		caps = GetSurfaceCapabilities(exclusivefullscreen);
	}

	if (caps.PresentModes.empty())
		VulkanError("No surface present modes supported");

	bool supportsFifoRelaxed = std::find(caps.PresentModes.begin(), caps.PresentModes.end(), VK_PRESENT_MODE_FIFO_RELAXED_KHR) != caps.PresentModes.end();
	bool supportsMailbox = std::find(caps.PresentModes.begin(), caps.PresentModes.end(), VK_PRESENT_MODE_MAILBOX_KHR) != caps.PresentModes.end();
	bool supportsImmediate = std::find(caps.PresentModes.begin(), caps.PresentModes.end(), VK_PRESENT_MODE_IMMEDIATE_KHR) != caps.PresentModes.end();

	presentMode = VK_PRESENT_MODE_FIFO_KHR;
	if (vsync)
	{
		if (supportsFifoRelaxed)
			presentMode = VK_PRESENT_MODE_FIFO_RELAXED_KHR;
	}
	else
	{
		if (exclusivefullscreen) // Exclusive full screen doesn't seem to support mailbox for some reason, even if it is advertised
		{
			if (supportsImmediate)
				presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
			else if (supportsMailbox)
				presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
		}
		else
		{
			/*if (supportsMailbox)
				presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
			else*/ if (supportsImmediate)
				presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
		}
	}

	SelectFormat(caps, hdr);

	actualExtent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };
	actualExtent.width = std::max(caps.Capabilites.minImageExtent.width, std::min(caps.Capabilites.maxImageExtent.width, actualExtent.width));
	actualExtent.height = std::max(caps.Capabilites.minImageExtent.height, std::min(caps.Capabilites.maxImageExtent.height, actualExtent.height));
	if (actualExtent.width == 0 || actualExtent.height == 0)
	{
		if (swapchain)
			vkDestroySwapchainKHR(device->device, swapchain, nullptr);
		swapchain = VK_NULL_HANDLE;
		lost = true;
		return false;
	}

	if (caps.Capabilites.maxImageCount != 0)
		imageCount = std::min(caps.Capabilites.maxImageCount, (uint32_t)imageCount);
	imageCount = std::max(caps.Capabilites.minImageCount, (uint32_t)imageCount);

	VkSwapchainCreateInfoKHR swapChainCreateInfo = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
#ifdef WIN32
	VkSurfaceFullScreenExclusiveInfoEXT exclusiveInfo = { VK_STRUCTURE_TYPE_SURFACE_FULL_SCREEN_EXCLUSIVE_INFO_EXT };
	VkSurfaceFullScreenExclusiveWin32InfoEXT exclusiveWin32Info = { VK_STRUCTURE_TYPE_SURFACE_FULL_SCREEN_EXCLUSIVE_WIN32_INFO_EXT };
#endif

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

	swapChainCreateInfo.preTransform = caps.Capabilites.currentTransform;
	swapChainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; // If alpha channel is passed on to the DWM or not
	swapChainCreateInfo.presentMode = presentMode;
	swapChainCreateInfo.clipped = VK_TRUE; // Applications SHOULD set this value to VK_TRUE if they do not expect to read back the content of presentable images before presenting them or after reacquiring them, and if their fragment shaders do not have any side effects that require them to run for all pixels in the presentable image
	swapChainCreateInfo.oldSwapchain = swapchain;

#ifdef WIN32
	if (exclusivefullscreen)
	{
		swapChainCreateInfo.pNext = &exclusiveInfo;
		exclusiveInfo.pNext = &exclusiveWin32Info;
		exclusiveInfo.fullScreenExclusive = VK_FULL_SCREEN_EXCLUSIVE_APPLICATION_CONTROLLED_EXT;
		exclusiveWin32Info.hmonitor = MonitorFromWindow(device->Surface->Window, MONITOR_DEFAULTTONEAREST);
	}
#endif

	VkResult result = vkCreateSwapchainKHR(device->device, &swapChainCreateInfo, nullptr, &swapchain);

	if (swapChainCreateInfo.oldSwapchain)
		vkDestroySwapchainKHR(device->device, swapChainCreateInfo.oldSwapchain, nullptr);

	if (result != VK_SUCCESS)
	{
		swapchain = VK_NULL_HANDLE;
		lost = true;
		return false;
	}

#ifdef WIN32
	if (exclusivefullscreen)
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
		VulkanError("Failed to acquire next image!");
		return -1;
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

		VulkanError("vkQueuePresentKHR failed: out of memory");
	}
	else if (result == VK_ERROR_DEVICE_LOST)
	{
		VulkanError("vkQueuePresentKHR failed: device lost");
	}
	else
	{
		VulkanError("vkQueuePresentKHR failed");
	}
}

VulkanSurfaceCapabilities VulkanSwapChain::GetSurfaceCapabilities(bool exclusivefullscreen)
{
	// They sure made it easy to query something that isn't even time critical. Good job guys!

	VulkanSurfaceCapabilities caps;

	VkPhysicalDeviceSurfaceInfo2KHR surfaceInfo = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR };
#ifdef WIN32
	VkSurfaceFullScreenExclusiveInfoEXT exclusiveInfo = { VK_STRUCTURE_TYPE_SURFACE_FULL_SCREEN_EXCLUSIVE_INFO_EXT };
	VkSurfaceFullScreenExclusiveWin32InfoEXT exclusiveWin32Info = { VK_STRUCTURE_TYPE_SURFACE_FULL_SCREEN_EXCLUSIVE_WIN32_INFO_EXT };
#endif

#ifdef WIN32
	if (exclusivefullscreen)
	{
		exclusiveInfo.fullScreenExclusive = VK_FULL_SCREEN_EXCLUSIVE_APPLICATION_CONTROLLED_EXT;
		exclusiveWin32Info.hmonitor = MonitorFromWindow(device->Surface->Window, MONITOR_DEFAULTTONEAREST);
	}
#endif

	surfaceInfo.surface = device->Surface->Surface;

	if (device->SupportsExtension(VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME))
	{
		const void** next = &surfaceInfo.pNext;

#ifdef WIN32
		if (exclusivefullscreen && device->SupportsExtension(VK_EXT_FULL_SCREEN_EXCLUSIVE_EXTENSION_NAME))
		{
			*next = &exclusiveInfo;
			next = const_cast<const void**>(&exclusiveInfo.pNext);

			*next = &exclusiveWin32Info;
			next = &exclusiveWin32Info.pNext;
		}
#endif

		VkSurfaceCapabilities2KHR caps2 = { VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR };
		next = const_cast<const void**>(&caps2.pNext);

#ifdef WIN32
		if (exclusivefullscreen && device->SupportsExtension(VK_EXT_FULL_SCREEN_EXCLUSIVE_EXTENSION_NAME))
		{
			*next = &caps.FullScreenExclusive;
			next = const_cast<const void**>(&caps.FullScreenExclusive.pNext);
		}
#endif

		VkResult result = vkGetPhysicalDeviceSurfaceCapabilities2KHR(device->PhysicalDevice.Device, &surfaceInfo, &caps2);
		if (result != VK_SUCCESS)
			VulkanError("vkGetPhysicalDeviceSurfaceCapabilities2KHR failed");

		caps.Capabilites = caps2.surfaceCapabilities;
	}
	else
	{
		VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device->PhysicalDevice.Device, device->Surface->Surface, &caps.Capabilites);
		if (result != VK_SUCCESS)
			VulkanError("vkGetPhysicalDeviceSurfaceCapabilitiesKHR failed");
	}

#ifdef WIN32
	if (exclusivefullscreen && device->SupportsExtension(VK_EXT_FULL_SCREEN_EXCLUSIVE_EXTENSION_NAME))
	{
		const void** next = &surfaceInfo.pNext;

		uint32_t presentModeCount = 0;
		VkResult result = vkGetPhysicalDeviceSurfacePresentModes2EXT(device->PhysicalDevice.Device, &surfaceInfo, &presentModeCount, nullptr);
		if (result != VK_SUCCESS)
			VulkanError("vkGetPhysicalDeviceSurfacePresentModes2EXT failed");

		if (presentModeCount > 0)
		{
			caps.PresentModes.resize(presentModeCount);
			result = vkGetPhysicalDeviceSurfacePresentModes2EXT(device->PhysicalDevice.Device, &surfaceInfo, &presentModeCount, caps.PresentModes.data());
			if (result != VK_SUCCESS)
				VulkanError("vkGetPhysicalDeviceSurfacePresentModes2EXT failed");
		}
	}
	else
#endif
	{
		uint32_t presentModeCount = 0;
		VkResult result = vkGetPhysicalDeviceSurfacePresentModesKHR(device->PhysicalDevice.Device, device->Surface->Surface, &presentModeCount, nullptr);
		if (result != VK_SUCCESS)
			VulkanError("vkGetPhysicalDeviceSurfacePresentModesKHR failed");

		if (presentModeCount > 0)
		{
			caps.PresentModes.resize(presentModeCount);
			result = vkGetPhysicalDeviceSurfacePresentModesKHR(device->PhysicalDevice.Device, device->Surface->Surface, &presentModeCount, caps.PresentModes.data());
			if (result != VK_SUCCESS)
				VulkanError("vkGetPhysicalDeviceSurfacePresentModesKHR failed");
		}
	}

	if (device->SupportsExtension(VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME))
	{
		uint32_t surfaceFormatCount = 0;
		VkResult result = vkGetPhysicalDeviceSurfaceFormats2KHR(device->PhysicalDevice.Device, &surfaceInfo, &surfaceFormatCount, nullptr);
		if (result != VK_SUCCESS)
			VulkanError("vkGetPhysicalDeviceSurfaceFormats2KHR failed");

		if (surfaceFormatCount > 0)
		{
			std::vector<VkSurfaceFormat2KHR> formats(surfaceFormatCount, { VK_STRUCTURE_TYPE_SURFACE_FORMAT_2_KHR });
			result = vkGetPhysicalDeviceSurfaceFormats2KHR(device->PhysicalDevice.Device, &surfaceInfo, &surfaceFormatCount, formats.data());
			if (result != VK_SUCCESS)
				VulkanError("vkGetPhysicalDeviceSurfaceFormats2KHR failed");

			for (VkSurfaceFormat2KHR& fmt : formats)
				caps.Formats.push_back(fmt.surfaceFormat);
		}
	}
	else
	{
		uint32_t surfaceFormatCount = 0;
		VkResult result = vkGetPhysicalDeviceSurfaceFormatsKHR(device->PhysicalDevice.Device, device->Surface->Surface, &surfaceFormatCount, nullptr);
		if (result != VK_SUCCESS)
			VulkanError("vkGetPhysicalDeviceSurfaceFormatsKHR failed");
		
		if (surfaceFormatCount > 0)
		{
			caps.Formats.resize(surfaceFormatCount);
			result = vkGetPhysicalDeviceSurfaceFormatsKHR(device->PhysicalDevice.Device, device->Surface->Surface, &surfaceFormatCount, caps.Formats.data());
			if (result != VK_SUCCESS)
				VulkanError("vkGetPhysicalDeviceSurfaceFormatsKHR failed");
		}
	}

	caps.FullScreenExclusive.pNext = nullptr;

	return caps;
}
