#pragma once

#include "vulkaninstance.h"

class VulkanSurface
{
public:
	VulkanSurface(std::shared_ptr<VulkanInstance> instance, VkSurfaceKHR surface);
	~VulkanSurface();

	std::shared_ptr<VulkanInstance> Instance;
	VkSurfaceKHR Surface = VK_NULL_HANDLE;

#ifdef VK_USE_PLATFORM_WIN32_KHR

	VulkanSurface(std::shared_ptr<VulkanInstance> instance, HWND window);
	HWND Window = 0;

#endif
};
