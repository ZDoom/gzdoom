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

#include "vk_swapchain.h"
#include "vk_objects.h"
#include "c_cvars.h"
#include "version.h"
#include "v_video.h"
#include "vk_framebuffer.h"


CVAR(Bool, vk_hdr, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG);

void I_GetVulkanDrawableSize(int *width, int *height);

VulkanSwapChain::VulkanSwapChain(VulkanDevice *device) : device(device)
{
}

VulkanSwapChain::~VulkanSwapChain()
{
	ReleaseResources();
}

uint32_t VulkanSwapChain::AcquireImage(int width, int height, bool vsync, VulkanSemaphore *semaphore, VulkanFence *fence)
{
	if (lastSwapWidth != width || lastSwapHeight != height || lastVsync != vsync || lastHdr != vk_hdr || !swapChain)
	{
		Recreate(vsync);
		lastSwapWidth = width;
		lastSwapHeight = height;
		lastVsync = vsync;
		lastHdr = vk_hdr;
	}

	uint32_t imageIndex;
	while (true)
	{
		if (!swapChain)
		{
			imageIndex = 0xffffffff;
			break;
		}

		VkResult result = vkAcquireNextImageKHR(device->device, swapChain, 1'000'000'000, semaphore ? semaphore->semaphore : VK_NULL_HANDLE, fence ? fence->fence : VK_NULL_HANDLE, &imageIndex);
		if (result == VK_SUCCESS)
		{
			break;
		}
		else if (result == VK_SUBOPTIMAL_KHR || result == VK_ERROR_SURFACE_LOST_KHR)
		{
			// Force the recreate to happen next frame.
			// The spec is not very clear about what happens to the semaphore or the acquired image if the swapchain is recreated before the image is released with a call to vkQueuePresentKHR.
			lastSwapWidth = 0;
			lastSwapHeight = 0;
			break;
		}
		else if (result == VK_ERROR_OUT_OF_DATE_KHR)
		{
			Recreate(vsync);
		}
		else if (result == VK_NOT_READY || result == VK_TIMEOUT)
		{
			imageIndex = 0xffffffff;
			break;
		}
		else if (result == VK_ERROR_OUT_OF_HOST_MEMORY || result == VK_ERROR_OUT_OF_DEVICE_MEMORY)
		{
			VulkanError("vkAcquireNextImageKHR failed: out of memory");
		}
		else if (result == VK_ERROR_DEVICE_LOST)
		{
			VulkanError("vkAcquireNextImageKHR failed: device lost");
		}
		else
		{
			VulkanError("vkAcquireNextImageKHR failed");
		}
	}
	return imageIndex;
}

void VulkanSwapChain::QueuePresent(uint32_t imageIndex, VulkanSemaphore *semaphore)
{
	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = semaphore ? 1 : 0;
	presentInfo.pWaitSemaphores = semaphore ? &semaphore->semaphore : VK_NULL_HANDLE;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &swapChain;
	presentInfo.pImageIndices = &imageIndex;
	presentInfo.pResults = nullptr;
	VkResult result = vkQueuePresentKHR(device->presentQueue, &presentInfo);
	if (result == VK_SUBOPTIMAL_KHR || result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_ERROR_SURFACE_LOST_KHR)
	{
		lastSwapWidth = 0;
		lastSwapHeight = 0;
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
	else if (result != VK_SUCCESS)
	{
		VulkanError("vkQueuePresentKHR failed");
	}
}

void VulkanSwapChain::Recreate(bool vsync)
{
	ReleaseViews();
	swapChainImages.clear();

	VkSwapchainKHR oldSwapChain = swapChain;
	CreateSwapChain(vsync, oldSwapChain);
	if (oldSwapChain)
		vkDestroySwapchainKHR(device->device, oldSwapChain, nullptr);

	if (swapChain)
	{
		GetImages();
		CreateViews();
	}
}

bool VulkanSwapChain::CreateSwapChain(bool vsync, VkSwapchainKHR oldSwapChain)
{
	SelectFormat();
	SelectPresentMode(vsync);

	int width, height;
	I_GetVulkanDrawableSize(&width, &height);

	VkSurfaceCapabilitiesKHR surfaceCapabilities = GetSurfaceCapabilities();

	actualExtent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };
	actualExtent.width = max(surfaceCapabilities.minImageExtent.width, min(surfaceCapabilities.maxImageExtent.width, actualExtent.width));
	actualExtent.height = max(surfaceCapabilities.minImageExtent.height, min(surfaceCapabilities.maxImageExtent.height, actualExtent.height));
	if (actualExtent.width == 0 || actualExtent.height == 0)
	{
		swapChain = VK_NULL_HANDLE;
		return false;
	}

	uint32_t imageCount = surfaceCapabilities.minImageCount + 1;
	if (surfaceCapabilities.maxImageCount > 0 && imageCount > surfaceCapabilities.maxImageCount)
		imageCount = surfaceCapabilities.maxImageCount;

	// When vsync is on we only want two images. This creates a slight performance penalty in exchange for reduced input latency (less mouse lag).
	// When vsync is off we want three images as it allows us to generate new images even during the vertical blanking period where one entry is being used by the presentation engine.
	if (swapChainPresentMode == VK_PRESENT_MODE_MAILBOX_KHR || swapChainPresentMode == VK_PRESENT_MODE_IMMEDIATE_KHR)
		imageCount = min(imageCount, (uint32_t)3);
	else
		imageCount = min(imageCount, (uint32_t)2);

	VkSwapchainCreateInfoKHR swapChainCreateInfo = {};
	swapChainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapChainCreateInfo.surface = device->surface;
	swapChainCreateInfo.minImageCount = imageCount;
	swapChainCreateInfo.imageFormat = swapChainFormat.format;
	swapChainCreateInfo.imageColorSpace = swapChainFormat.colorSpace;
	swapChainCreateInfo.imageExtent = actualExtent;
	swapChainCreateInfo.imageArrayLayers = 1;
	swapChainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	uint32_t queueFamilyIndices[] = { (uint32_t)device->graphicsFamily, (uint32_t)device->presentFamily };
	if (device->graphicsFamily != device->presentFamily)
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
	swapChainCreateInfo.presentMode = swapChainPresentMode;
	swapChainCreateInfo.clipped = VK_TRUE;
	swapChainCreateInfo.oldSwapchain = oldSwapChain;

	VkResult result = vkCreateSwapchainKHR(device->device, &swapChainCreateInfo, nullptr, &swapChain);
	if (result != VK_SUCCESS)
	{
		swapChain = VK_NULL_HANDLE;
		return false;
	}

	return true;
}

void VulkanSwapChain::CreateViews()
{
	framebuffers.resize(swapChainImages.size());
	swapChainImageViews.reserve(swapChainImages.size());
	for (size_t i = 0; i < swapChainImages.size(); i++)
	{
		device->SetDebugObjectName("SwapChainImage", (uint64_t)swapChainImages[i], VK_OBJECT_TYPE_IMAGE);

		VkImageViewCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		createInfo.image = swapChainImages[i];
		createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		createInfo.format = swapChainFormat.format;
		createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		createInfo.subresourceRange.baseMipLevel = 0;
		createInfo.subresourceRange.levelCount = 1;
		createInfo.subresourceRange.baseArrayLayer = 0;
		createInfo.subresourceRange.layerCount = 1;

		VkImageView view;
		VkResult result = vkCreateImageView(device->device, &createInfo, nullptr, &view);
		CheckVulkanError(result, "Could not create image view for swapchain image");

		device->SetDebugObjectName("SwapChainImageView", (uint64_t)view, VK_OBJECT_TYPE_IMAGE_VIEW);

		swapChainImageViews.push_back(view);
	}
}

bool VulkanSwapChain::IsHdrModeActive() const
{
	return swapChainFormat.colorSpace == VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT;
}

void VulkanSwapChain::SelectFormat()
{
	std::vector<VkSurfaceFormatKHR> surfaceFormats = GetSurfaceFormats();
	if (surfaceFormats.empty())
		VulkanError("No surface formats supported");

	if (surfaceFormats.size() == 1 && surfaceFormats.front().format == VK_FORMAT_UNDEFINED)
	{
		swapChainFormat.format = VK_FORMAT_B8G8R8A8_UNORM;
		swapChainFormat.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
		return;
	}

	if (vk_hdr)
	{
		for (const auto& format : surfaceFormats)
		{
			if (format.format == VK_FORMAT_R16G16B16A16_SFLOAT && format.colorSpace == VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT)
			{
				swapChainFormat = format;
				return;
			}
		}
	}

	for (const auto &format : surfaceFormats)
	{
		if (format.format == VK_FORMAT_B8G8R8A8_UNORM && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
		{
			swapChainFormat = format;
			return;
		}
	}

	swapChainFormat = surfaceFormats.front();
}

void VulkanSwapChain::SelectPresentMode(bool vsync)
{
	std::vector<VkPresentModeKHR> presentModes = GetPresentModes();

	if (presentModes.empty())
		VulkanError("No surface present modes supported");

	swapChainPresentMode = VK_PRESENT_MODE_FIFO_KHR;
	if (vsync)
	{
		bool supportsFifoRelaxed = std::find(presentModes.begin(), presentModes.end(), VK_PRESENT_MODE_FIFO_RELAXED_KHR) != presentModes.end();
		if (supportsFifoRelaxed)
			swapChainPresentMode = VK_PRESENT_MODE_FIFO_RELAXED_KHR;
	}
	else
	{
		bool supportsMailbox = std::find(presentModes.begin(), presentModes.end(), VK_PRESENT_MODE_MAILBOX_KHR) != presentModes.end();
		bool supportsImmediate = std::find(presentModes.begin(), presentModes.end(), VK_PRESENT_MODE_IMMEDIATE_KHR) != presentModes.end();
		if (supportsMailbox)
			swapChainPresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
		else if (supportsImmediate)
			swapChainPresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
	}
}

void VulkanSwapChain::GetImages()
{
	uint32_t imageCount;
	VkResult result = vkGetSwapchainImagesKHR(device->device, swapChain, &imageCount, nullptr);
	CheckVulkanError(result, "vkGetSwapchainImagesKHR failed");

	swapChainImages.resize(imageCount);
	result = vkGetSwapchainImagesKHR(device->device, swapChain, &imageCount, swapChainImages.data());
	CheckVulkanError(result, "vkGetSwapchainImagesKHR failed (2)");
}

void VulkanSwapChain::ReleaseViews()
{
	framebuffers.clear();
	for (auto &view : swapChainImageViews)
	{
		vkDestroyImageView(device->device, view, nullptr);
	}
	swapChainImageViews.clear();
}

void VulkanSwapChain::ReleaseResources()
{
	ReleaseViews();
	if (swapChain)
		vkDestroySwapchainKHR(device->device, swapChain, nullptr);
}

VkSurfaceCapabilitiesKHR VulkanSwapChain::GetSurfaceCapabilities()
{
	VkSurfaceCapabilitiesKHR surfaceCapabilities;
	VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device->PhysicalDevice.Device, device->surface, &surfaceCapabilities);
	CheckVulkanError(result, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR failed");
	return surfaceCapabilities;
}

std::vector<VkSurfaceFormatKHR> VulkanSwapChain::GetSurfaceFormats()
{
	uint32_t surfaceFormatCount = 0;
	VkResult result = vkGetPhysicalDeviceSurfaceFormatsKHR(device->PhysicalDevice.Device, device->surface, &surfaceFormatCount, nullptr);
	CheckVulkanError(result, "vkGetPhysicalDeviceSurfaceFormatsKHR failed");
	if (surfaceFormatCount == 0)
		return {};

	std::vector<VkSurfaceFormatKHR> surfaceFormats(surfaceFormatCount);
	result = vkGetPhysicalDeviceSurfaceFormatsKHR(device->PhysicalDevice.Device, device->surface, &surfaceFormatCount, surfaceFormats.data());
	CheckVulkanError(result, "vkGetPhysicalDeviceSurfaceFormatsKHR failed");
	return surfaceFormats;
}

std::vector<VkPresentModeKHR> VulkanSwapChain::GetPresentModes()
{
	uint32_t presentModeCount = 0;
	VkResult result = vkGetPhysicalDeviceSurfacePresentModesKHR(device->PhysicalDevice.Device, device->surface, &presentModeCount, nullptr);
	CheckVulkanError(result, "vkGetPhysicalDeviceSurfacePresentModesKHR failed");
	if (presentModeCount == 0)
		return {};

	std::vector<VkPresentModeKHR> presentModes(presentModeCount);
	vkGetPhysicalDeviceSurfacePresentModesKHR(device->PhysicalDevice.Device, device->surface, &presentModeCount, presentModes.data());
	CheckVulkanError(result, "vkGetPhysicalDeviceSurfacePresentModesKHR failed");
	return presentModes;
}
