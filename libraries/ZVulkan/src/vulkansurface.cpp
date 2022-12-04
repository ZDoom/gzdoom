
#include "vulkansurface.h"
#include "vulkaninstance.h"

VulkanSurface::VulkanSurface(std::shared_ptr<VulkanInstance> instance, VkSurfaceKHR surface) : Instance(std::move(instance)), Surface(surface)
{
}

VulkanSurface::~VulkanSurface()
{
	vkDestroySurfaceKHR(Instance->Instance, Surface, nullptr);
}

#ifdef VK_USE_PLATFORM_WIN32_KHR

VulkanSurface::VulkanSurface(std::shared_ptr<VulkanInstance> instance, HWND window) : Instance(std::move(instance)), Window(window)
{
	VkWin32SurfaceCreateInfoKHR createInfo = { VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR };
	createInfo.hwnd = window;
	createInfo.hinstance = GetModuleHandle(nullptr);

	VkResult result = vkCreateWin32SurfaceKHR(Instance->Instance, &createInfo, nullptr, &Surface);
	if (result != VK_SUCCESS)
		VulkanError("Could not create vulkan surface");
}

#endif
