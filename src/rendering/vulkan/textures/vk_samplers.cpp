// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2018 Christoph Oelckers
// All rights reserved.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//--------------------------------------------------------------------------
//

#include "volk/volk.h"
#include "c_cvars.h"
#include "v_video.h"

#include "hwrenderer/utility/hw_cvars.h"
#include "vulkan/system/vk_device.h"
#include "vulkan/system/vk_builders.h"
#include "vk_samplers.h"
#include "hwrenderer/textures/hw_material.h"

struct VkTexFilter
{
	VkFilter minFilter;
	VkFilter magFilter;
	VkSamplerMipmapMode mipfilter;
	bool mipmapping;
} ;

VkTexFilter TexFilter[]={
	{VK_FILTER_NEAREST,		VK_FILTER_NEAREST,		VK_SAMPLER_MIPMAP_MODE_NEAREST, false},
	{VK_FILTER_NEAREST,		VK_FILTER_NEAREST,		VK_SAMPLER_MIPMAP_MODE_NEAREST, true},
	{VK_FILTER_LINEAR,		VK_FILTER_LINEAR,		VK_SAMPLER_MIPMAP_MODE_NEAREST, false},
	{VK_FILTER_LINEAR,		VK_FILTER_LINEAR,		VK_SAMPLER_MIPMAP_MODE_NEAREST, true},
	{VK_FILTER_LINEAR,		VK_FILTER_LINEAR,		VK_SAMPLER_MIPMAP_MODE_LINEAR, true},
	{VK_FILTER_NEAREST,		VK_FILTER_NEAREST,		VK_SAMPLER_MIPMAP_MODE_LINEAR, true},
	{VK_FILTER_LINEAR,		VK_FILTER_NEAREST,		VK_SAMPLER_MIPMAP_MODE_LINEAR, true},
};

struct VkTexClamp
{
	VkSamplerAddressMode clamp_u;
	VkSamplerAddressMode clamp_v;
};

VkTexClamp TexClamp[] ={
	{ VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_REPEAT },
	{ VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_REPEAT },
	{ VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE },
	{ VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE },
	{ VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE },
	{ VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_REPEAT },
	{ VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_REPEAT }
};

VkSamplerManager::VkSamplerManager(VulkanDevice *dev)
{
	vDevice = dev;
	Create();
}

VkSamplerManager::~VkSamplerManager()
{
	Destroy();
}

void VkSamplerManager::SetTextureFilterMode()
{
	Destroy();
	Create();
}

void VkSamplerManager::Create()
{
	int filter = V_IsHardwareRenderer() ? gl_texture_filter : 0;
	
	for(int i = 0; i < 7; i++)
	{
		SamplerBuilder builder;
		builder.setMagFilter(TexFilter[filter].magFilter);
		builder.setMinFilter(TexFilter[filter].minFilter);
		builder.setAddressMode(TexClamp[i].clamp_u, TexClamp[i].clamp_v, VK_SAMPLER_ADDRESS_MODE_REPEAT);
		builder.setMipmapMode(TexFilter[filter].mipfilter);
		if (i <= CLAMP_XY && TexFilter[filter].mipmapping)
		{
			builder.setAnisotropy(gl_texture_filter_anisotropic);
			builder.setMaxLod(100.0f); // According to the spec this value is clamped so something high makes it usable for all textures.
		}
		else
		{
			builder.setMaxLod(0.25f);
		}
		mSamplers[i] = builder.create(vDevice);
		mSamplers[i]->SetDebugName("VkSamplerManager.mSamplers");
    }
}

void VkSamplerManager::Destroy()
{
	for (int i = 0; i < 7; i++)
		mSamplers[i].reset();
}
