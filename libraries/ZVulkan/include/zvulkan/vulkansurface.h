#pragma once

#include "vulkaninstance.h"

#if defined(VK_USE_PLATFORM_XLIB_KHR)
#include <X11/Xlib.h>
#endif

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

#elif defined(VK_USE_PLATFORM_XLIB_KHR)

	VulkanSurface(std::shared_ptr<VulkanInstance> instance, Display* disp, Window wind);
	Display* disp = nullptr;
	Window wind;

#endif
};
