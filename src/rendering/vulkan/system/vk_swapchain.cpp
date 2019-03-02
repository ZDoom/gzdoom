
#include "vk_swapchain.h"

VulkanSwapChain::VulkanSwapChain(VulkanDevice *device, int width, int height, bool vsync) : vsync(vsync), device(device)
{
	VkSurfaceCapabilitiesKHR surfaceCapabilities;
	VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device->physicalDevice, device->surface, &surfaceCapabilities);
	if (result != VK_SUCCESS)
		throw std::runtime_error("vkGetPhysicalDeviceSurfaceCapabilitiesKHR failed");

	uint32_t surfaceFormatCount = 0;
	result = vkGetPhysicalDeviceSurfaceFormatsKHR(device->physicalDevice, device->surface, &surfaceFormatCount, nullptr);
	if (result != VK_SUCCESS)
		throw std::runtime_error("vkGetPhysicalDeviceSurfaceFormatsKHR failed");
	else if (surfaceFormatCount == 0)
		throw std::runtime_error("No surface formats supported");

	std::vector<VkSurfaceFormatKHR> surfaceFormats(surfaceFormatCount);
	result = vkGetPhysicalDeviceSurfaceFormatsKHR(device->physicalDevice, device->surface, &surfaceFormatCount, surfaceFormats.data());
	if (result != VK_SUCCESS)
		throw std::runtime_error("vkGetPhysicalDeviceSurfaceFormatsKHR failed");

	uint32_t presentModeCount = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device->physicalDevice, device->surface, &presentModeCount, nullptr);
	if (result != VK_SUCCESS)
		throw std::runtime_error("vkGetPhysicalDeviceSurfacePresentModesKHR failed");
	else if (presentModeCount == 0)
		throw std::runtime_error("No surface present modes supported");

	std::vector<VkPresentModeKHR> presentModes(presentModeCount);
	vkGetPhysicalDeviceSurfacePresentModesKHR(device->physicalDevice, device->surface, &presentModeCount, presentModes.data());
	if (result != VK_SUCCESS)
		throw std::runtime_error("vkGetPhysicalDeviceSurfacePresentModesKHR failed");

	if (surfaceFormats.size() == 1 && surfaceFormats.front().format == VK_FORMAT_UNDEFINED)
	{
		swapChainFormat.format = VK_FORMAT_B8G8R8A8_UNORM;
		swapChainFormat.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
	}
	else
	{
		swapChainFormat = surfaceFormats.front();
		for (const auto &format : surfaceFormats)
		{
			if (format.format == VK_FORMAT_B8G8R8A8_UNORM && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			{
				swapChainFormat = format;
				break;
			}
		}
	}

	VkPresentModeKHR swapChainPresentMode = VK_PRESENT_MODE_FIFO_KHR;
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

	actualExtent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };
	actualExtent.width = std::max(surfaceCapabilities.minImageExtent.width, std::min(surfaceCapabilities.maxImageExtent.width, actualExtent.width));
	actualExtent.height = std::max(surfaceCapabilities.minImageExtent.height, std::min(surfaceCapabilities.maxImageExtent.height, actualExtent.height));

	uint32_t imageCount = surfaceCapabilities.minImageCount + 1;
	if (surfaceCapabilities.maxImageCount > 0 && imageCount > surfaceCapabilities.maxImageCount)
		imageCount = surfaceCapabilities.maxImageCount;

	imageCount = std::min(imageCount, (uint32_t)2); // Only use two buffers (triple buffering sucks! good for benchmarks, bad for mouse input)

	VkSwapchainCreateInfoKHR swapChainCreateInfo = {};
	swapChainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapChainCreateInfo.surface = device->surface;
	swapChainCreateInfo.minImageCount = imageCount;
	swapChainCreateInfo.imageFormat = swapChainFormat.format;
	swapChainCreateInfo.imageColorSpace = swapChainFormat.colorSpace;
	swapChainCreateInfo.imageExtent = actualExtent;
	swapChainCreateInfo.imageArrayLayers = 1;
	swapChainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

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
	swapChainCreateInfo.oldSwapchain = VK_NULL_HANDLE;

	result = vkCreateSwapchainKHR(device->device, &swapChainCreateInfo, nullptr, &swapChain);
	if (result != VK_SUCCESS)
		throw std::runtime_error("Could not create vulkan swapchain");

	try
	{
		result = vkGetSwapchainImagesKHR(device->device, swapChain, &imageCount, nullptr);
		if (result != VK_SUCCESS)
			throw std::runtime_error("vkGetSwapchainImagesKHR failed");

		swapChainImages.resize(imageCount);
		result = vkGetSwapchainImagesKHR(device->device, swapChain, &imageCount, swapChainImages.data());
		if (result != VK_SUCCESS)
			throw std::runtime_error("vkGetSwapchainImagesKHR failed (2)");

		swapChainImageViews.resize(swapChainImages.size());
		for (size_t i = 0; i < swapChainImages.size(); i++)
		{
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
			result = vkCreateImageView(device->device, &createInfo, nullptr, &swapChainImageViews[i]);
			if (result != VK_SUCCESS)
				throw std::runtime_error("Could not create image view for swapchain image");
		}
	}
	catch (...)
	{
		for (auto &view : swapChainImageViews)
		{
			if (view)
				vkDestroyImageView(device->device, view, nullptr);
		}
		vkDestroySwapchainKHR(device->device, swapChain, nullptr);
		throw;
	}
}

VulkanSwapChain::~VulkanSwapChain()
{
	for (auto &view : swapChainImageViews)
	{
		if (view)
			vkDestroyImageView(device->device, view, nullptr);
	}
	vkDestroySwapchainKHR(device->device, swapChain, nullptr);
}
