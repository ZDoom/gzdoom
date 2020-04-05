#pragma once

#include "win32basevideo.h"
#include "c_cvars.h"
#include "rendering/vulkan/system/vk_framebuffer.h"


EXTERN_CVAR(Bool, fullscreen)

//==========================================================================
//
// 
//
//==========================================================================

class Win32VulkanVideo : public Win32BaseVideo
{
	VulkanDevice *device = nullptr;
public:
	Win32VulkanVideo() 
	{
		device = new VulkanDevice();
	}
	
	~Win32VulkanVideo()
	{
		delete device;
	}

	void Shutdown() override
	{
		delete device;
		device = nullptr;
	}

	DFrameBuffer *CreateFrameBuffer() override
	{
		auto fb = new VulkanFrameBuffer(m_hMonitor, fullscreen, device);
		return fb;
	}

protected:
};
