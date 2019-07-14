
#include <assert.h>

#ifdef _WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#endif

#include "volk/volk.h"


extern HWND Window;

void I_GetVulkanDrawableSize(int *width, int *height)
{
	assert(Window);

	RECT clientRect = { 0 };
	GetClientRect(Window, &clientRect);
	
	if (width != nullptr)
	{
		*width = clientRect.right;
	}

	if (height != nullptr)
	{
		*height = clientRect.bottom;
	}
}

bool I_GetVulkanPlatformExtensions(unsigned int *count, const char **names)
{
	static const char* extensions[] = 
	{
		VK_KHR_SURFACE_EXTENSION_NAME,
		VK_KHR_WIN32_SURFACE_EXTENSION_NAME
	};
	static const unsigned int extensionCount = static_cast<unsigned int>(sizeof extensions / sizeof extensions[0]);

	if (count == nullptr && names == nullptr)
	{
		return false;
	}
	else if (names == nullptr)
	{
		*count = extensionCount;
		return true;
	}
	else
	{
		const bool result = *count >= extensionCount;
		*count = min(*count, extensionCount);

		for (unsigned int i = 0; i < *count; ++i)
		{
			names[i] = extensions[i];
		}

		return result;
	}
}

bool I_CreateVulkanSurface(VkInstance instance, VkSurfaceKHR *surface)
{
	VkWin32SurfaceCreateInfoKHR windowCreateInfo;
	windowCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	windowCreateInfo.pNext = nullptr;
	windowCreateInfo.flags = 0;
	windowCreateInfo.hwnd = Window;
	windowCreateInfo.hinstance = GetModuleHandle(nullptr);

	const VkResult result = vkCreateWin32SurfaceKHR(instance, &windowCreateInfo, nullptr, surface);
	return result == VK_SUCCESS;
}
