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

#include <zvulkan/vulkanobjects.h>
#include <zvulkan/vulkandevice.h>
#include <zvulkan/vulkanbuilders.h>
#include "c_cvars.h"
#include "v_video.h"
#include "hw_cvars.h"
#include "vulkan/vk_renderdevice.h"
#include "vulkan/commands/vk_commandbuffer.h"
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

VkSamplerManager::VkSamplerManager(VulkanRenderDevice* fb) : fb(fb)
{
	CreateHWSamplers();
	CreateShadowmapSampler();
	CreateLightmapSampler();
}

VkSamplerManager::~VkSamplerManager()
{
}

void VkSamplerManager::ResetHWSamplers()
{
	DeleteHWSamplers();
	CreateHWSamplers();
}

void VkSamplerManager::CreateHWSamplers()
{
	int filter = sysCallbacks.DisableTextureFilter && sysCallbacks.DisableTextureFilter() ? 0 : gl_texture_filter;

	for (int i = CLAMP_NONE; i <= CLAMP_XY; i++)
	{
		SamplerBuilder builder;
		builder.MagFilter(TexFilter[filter].magFilter);
		builder.MinFilter(TexFilter[filter].minFilter);
		builder.AddressMode(TexClamp[i].clamp_u, TexClamp[i].clamp_v, VK_SAMPLER_ADDRESS_MODE_REPEAT);
		builder.MipmapMode(TexFilter[filter].mipfilter);
		if (TexFilter[filter].mipmapping)
		{
			builder.Anisotropy(gl_texture_filter_anisotropic);
			builder.MaxLod(100.0f); // According to the spec this value is clamped so something high makes it usable for all textures.
		}
		else
		{
			builder.MaxLod(0.25f);
		}
		builder.DebugName("VkSamplerManager.mSamplers");
		mSamplers[i] = builder.Create(fb->GetDevice());
	}

	mSamplers[CLAMP_XY_NOMIP] = SamplerBuilder()
		.MagFilter(TexFilter[filter].magFilter)
		.MinFilter(TexFilter[filter].magFilter)
		.AddressMode(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_REPEAT)
		.MipmapMode(VK_SAMPLER_MIPMAP_MODE_NEAREST)
		.MaxLod(0.25f)
		.DebugName("VkSamplerManager.mSamplers")
		.Create(fb->GetDevice());

	for (int i = CLAMP_NOFILTER; i <= CLAMP_NOFILTER_XY; i++)
	{
		mSamplers[i] = SamplerBuilder()
			.MagFilter(VK_FILTER_NEAREST)
			.MinFilter(VK_FILTER_NEAREST)
			.AddressMode(TexClamp[i - CLAMP_NOFILTER].clamp_u, TexClamp[i - CLAMP_NOFILTER].clamp_v, VK_SAMPLER_ADDRESS_MODE_REPEAT)
			.MipmapMode(VK_SAMPLER_MIPMAP_MODE_NEAREST)
			.MaxLod(0.25f)
			.DebugName("VkSamplerManager.mSamplers")
			.Create(fb->GetDevice());
	}

	// CAMTEX is repeating with texture filter and no mipmap
	mSamplers[CLAMP_CAMTEX] = SamplerBuilder()
		.MagFilter(TexFilter[filter].magFilter)
		.MinFilter(TexFilter[filter].magFilter)
		.AddressMode(VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_REPEAT)
		.MipmapMode(VK_SAMPLER_MIPMAP_MODE_NEAREST)
		.MaxLod(0.25f)
		.DebugName("VkSamplerManager.mSamplers")
		.Create(fb->GetDevice());


	mOverrideSamplers[int(MaterialLayerSampling::NearestMipLinear)] = SamplerBuilder()
		.MagFilter(VK_FILTER_NEAREST)
		.MinFilter(VK_FILTER_LINEAR)
		.AddressMode(VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_REPEAT)
		.MipmapMode(VK_SAMPLER_MIPMAP_MODE_LINEAR)
		.MaxLod(100.0f)
		.Anisotropy(gl_texture_filter_anisotropic)
		.Create(fb->GetDevice());

	mOverrideSamplers[int(MaterialLayerSampling::LinearMipLinear)] = SamplerBuilder()
		.MagFilter(VK_FILTER_LINEAR)
		.MinFilter(VK_FILTER_LINEAR)
		.AddressMode(VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_REPEAT)
		.MipmapMode(VK_SAMPLER_MIPMAP_MODE_LINEAR)
		.MaxLod(100.0f)
		.Anisotropy(gl_texture_filter_anisotropic)
		.Create(fb->GetDevice());
}

void VkSamplerManager::DeleteHWSamplers()
{
	auto deleteSamplers = [&](auto& samplers)
	{
		for (auto& sampler : samplers)
		{
			if (sampler)
				fb->GetCommands()->DrawDeleteList->Add(std::move(sampler));
		}
	};

	deleteSamplers(mSamplers);
	deleteSamplers(mOverrideSamplers);
}

VulkanSampler* VkSamplerManager::Get(PPFilterMode filter, PPWrapMode wrap)
{
	int index = (((int)filter) << 1) | (int)wrap;
	auto& sampler = mPPSamplers[index];
	if (sampler)
		return sampler.get();

	sampler = SamplerBuilder()
		.MipmapMode(VK_SAMPLER_MIPMAP_MODE_NEAREST)
		.MinFilter(filter == PPFilterMode::Nearest ? VK_FILTER_NEAREST : VK_FILTER_LINEAR)
		.MagFilter(filter == PPFilterMode::Nearest ? VK_FILTER_NEAREST : VK_FILTER_LINEAR)
		.AddressMode(wrap == PPWrapMode::Clamp ? VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE : VK_SAMPLER_ADDRESS_MODE_REPEAT)
		.DebugName("VkPostprocess.mSamplers")
		.Create(fb->GetDevice());

	return sampler.get();
}

void VkSamplerManager::CreateShadowmapSampler()
{
	ShadowmapSampler = SamplerBuilder()
		.MipmapMode(VK_SAMPLER_MIPMAP_MODE_NEAREST)
		.MinFilter(VK_FILTER_NEAREST)
		.MagFilter(VK_FILTER_NEAREST)
		.AddressMode(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE)
		.DebugName("VkRenderBuffers.ShadowmapSampler")
		.Create(fb->GetDevice());
}

void VkSamplerManager::CreateLightmapSampler()
{
	LightmapSampler = SamplerBuilder()
		.MipmapMode(VK_SAMPLER_MIPMAP_MODE_LINEAR)
		.MinFilter(VK_FILTER_LINEAR)
		.MagFilter(VK_FILTER_LINEAR)
		.AddressMode(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE)
		.DebugName("VkRenderBuffers.LightmapSampler")
		.Create(fb->GetDevice());
}
