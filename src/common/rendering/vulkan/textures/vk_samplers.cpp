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

#include "volk/volk.h"
#include "c_cvars.h"
#include "v_video.h"
#include "hw_cvars.h"
#include "vulkan/system/vk_device.h"
#include "vulkan/system/vk_builders.h"
#include "vulkan/system/vk_framebuffer.h"
#include "vulkan/system/vk_commandbuffer.h"
#include "vk_samplers.h"
#include "hw_material.h"
#include "i_interface.h"
#include "hwrenderer/postprocessing/hw_postprocess.h"

struct VkTexFilter
{
	VkFilter minFilter;
	VkFilter magFilter;
	VkSamplerMipmapMode mipfilter;
	bool mipmapping;
};

static VkTexFilter TexFilter[] =
{
	{VK_FILTER_NEAREST,		VK_FILTER_NEAREST,		VK_SAMPLER_MIPMAP_MODE_NEAREST, false},
	{VK_FILTER_NEAREST,		VK_FILTER_NEAREST,		VK_SAMPLER_MIPMAP_MODE_NEAREST, true},
	{VK_FILTER_LINEAR,		VK_FILTER_LINEAR,		VK_SAMPLER_MIPMAP_MODE_NEAREST, false},
	{VK_FILTER_LINEAR,		VK_FILTER_LINEAR,		VK_SAMPLER_MIPMAP_MODE_NEAREST, true},
	{VK_FILTER_LINEAR,		VK_FILTER_LINEAR,		VK_SAMPLER_MIPMAP_MODE_LINEAR, true},
	{VK_FILTER_NEAREST,		VK_FILTER_NEAREST,		VK_SAMPLER_MIPMAP_MODE_LINEAR, true},
	{VK_FILTER_LINEAR,		VK_FILTER_NEAREST,		VK_SAMPLER_MIPMAP_MODE_LINEAR, true}
};

struct VkTexClamp
{
	VkSamplerAddressMode clamp_u;
	VkSamplerAddressMode clamp_v;
};

static VkTexClamp TexClamp[] =
{
	{ VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_REPEAT },
	{ VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_REPEAT },
	{ VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE },
	{ VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE },
};

VkSamplerManager::VkSamplerManager(VulkanFrameBuffer* fb) : fb(fb)
{
	CreateHWSamplers();
}

VkSamplerManager::~VkSamplerManager()
{
}

void VkSamplerManager::FilterModeChanged()
{
	DeleteHWSamplers();
	CreateHWSamplers();
}

void VkSamplerManager::CreateHWSamplers()
{
	int filter = sysCallbacks.DisableTextureFilter && sysCallbacks.DisableTextureFilter()? 0 : gl_texture_filter;

	for (int i = CLAMP_NONE; i <= CLAMP_XY; i++)
	{
		SamplerBuilder builder;
		builder.setMagFilter(TexFilter[filter].magFilter);
		builder.setMinFilter(TexFilter[filter].minFilter);
		builder.setAddressMode(TexClamp[i].clamp_u, TexClamp[i].clamp_v, VK_SAMPLER_ADDRESS_MODE_REPEAT);
		builder.setMipmapMode(TexFilter[filter].mipfilter);
		if (TexFilter[filter].mipmapping)
		{
			builder.setAnisotropy(gl_texture_filter_anisotropic);
			builder.setMaxLod(100.0f); // According to the spec this value is clamped so something high makes it usable for all textures.
		}
		else
		{
			builder.setMaxLod(0.25f);
		}
		mSamplers[i] = builder.create(fb->device);
		mSamplers[i]->SetDebugName("VkSamplerManager.mSamplers");
	}
	{
		SamplerBuilder builder;
		builder.setMagFilter(TexFilter[filter].magFilter);
		builder.setMinFilter(TexFilter[filter].magFilter);
		builder.setAddressMode(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_REPEAT);
		builder.setMipmapMode(VK_SAMPLER_MIPMAP_MODE_NEAREST);
		builder.setMaxLod(0.25f);
		mSamplers[CLAMP_XY_NOMIP] = builder.create(fb->device);
		mSamplers[CLAMP_XY_NOMIP]->SetDebugName("VkSamplerManager.mSamplers");
	}
	for (int i = CLAMP_NOFILTER; i <= CLAMP_NOFILTER_XY; i++)
	{
		SamplerBuilder builder;
		builder.setMagFilter(VK_FILTER_NEAREST);
		builder.setMinFilter(VK_FILTER_NEAREST);
		builder.setAddressMode(TexClamp[i - CLAMP_NOFILTER].clamp_u, TexClamp[i - CLAMP_NOFILTER].clamp_v, VK_SAMPLER_ADDRESS_MODE_REPEAT);
		builder.setMipmapMode(VK_SAMPLER_MIPMAP_MODE_NEAREST);
		builder.setMaxLod(0.25f);
		mSamplers[i] = builder.create(fb->device);
		mSamplers[i]->SetDebugName("VkSamplerManager.mSamplers");
	}
	// CAMTEX is repeating with texture filter and no mipmap
	{
		SamplerBuilder builder;
		builder.setMagFilter(TexFilter[filter].magFilter);
		builder.setMinFilter(TexFilter[filter].magFilter);
		builder.setAddressMode(VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_REPEAT);
		builder.setMipmapMode(VK_SAMPLER_MIPMAP_MODE_NEAREST);
		builder.setMaxLod(0.25f);
		mSamplers[CLAMP_CAMTEX] = builder.create(fb->device);
		mSamplers[CLAMP_CAMTEX]->SetDebugName("VkSamplerManager.mSamplers");
	}
}

void VkSamplerManager::DeleteHWSamplers()
{
	for (auto& sampler : mSamplers)
	{
		if (sampler)
			fb->GetCommands()->FrameDeleteList.Samplers.push_back(std::move(sampler));
	}
}

VulkanSampler* VkSamplerManager::Get(PPFilterMode filter, PPWrapMode wrap)
{
	int index = (((int)filter) << 1) | (int)wrap;
	auto& sampler = mPPSamplers[index];
	if (sampler)
		return sampler.get();

	SamplerBuilder builder;
	builder.setMipmapMode(VK_SAMPLER_MIPMAP_MODE_NEAREST);
	builder.setMinFilter(filter == PPFilterMode::Nearest ? VK_FILTER_NEAREST : VK_FILTER_LINEAR);
	builder.setMagFilter(filter == PPFilterMode::Nearest ? VK_FILTER_NEAREST : VK_FILTER_LINEAR);
	builder.setAddressMode(wrap == PPWrapMode::Clamp ? VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE : VK_SAMPLER_ADDRESS_MODE_REPEAT);
	sampler = builder.create(fb->device);
	sampler->SetDebugName("VkPostprocess.mSamplers");
	return sampler.get();
}
