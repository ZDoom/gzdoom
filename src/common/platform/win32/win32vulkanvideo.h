#pragma once

#include "win32basevideo.h"
#include "c_cvars.h"
#include "vulkan/vk_renderdevice.h"
#include <zvulkan/vulkansurface.h>
#include <zvulkan/vulkanbuilders.h>

void I_GetVulkanDrawableSize(int* width, int* height);
bool I_CreateVulkanSurface(VkInstance instance, VkSurfaceKHR* surface);
bool I_GetVulkanPlatformExtensions(unsigned int* count, const char** names);

EXTERN_CVAR(Bool, vid_fullscreen)
EXTERN_CVAR(Bool, vk_debug)
EXTERN_CVAR(Int, vk_device)

//==========================================================================
//
// 
//
//==========================================================================

class Win32VulkanVideo : public Win32BaseVideo
{
	std::shared_ptr<VulkanSurface> surface;
public:
	Win32VulkanVideo() 
	{
		unsigned int count = 64;
		const char* names[64];
		if (!I_GetVulkanPlatformExtensions(&count, names))
			VulkanError("I_GetVulkanPlatformExtensions failed");

		VulkanInstanceBuilder builder;
		builder.DebugLayer(vk_debug);
		for (unsigned int i = 0; i < count; i++)
			builder.RequireExtension(names[i]);
		auto instance = builder.Create();

		VkSurfaceKHR surfacehandle = nullptr;
		if (!I_CreateVulkanSurface(instance->Instance, &surfacehandle))
			VulkanError("I_CreateVulkanSurface failed");

		surface = std::make_shared<VulkanSurface>(instance, surfacehandle);
	}

	~Win32VulkanVideo()
	{
	}

	void Shutdown() override
	{
		surface.reset();
	}

	DFrameBuffer *CreateFrameBuffer() override
	{
		auto fb = new VulkanRenderDevice(m_hMonitor, vid_fullscreen, surface);
		return fb;
	}

protected:
};
