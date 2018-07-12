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
		VkSamplerCreateInfo samplerInfo = {};
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerInfo.magFilter = TexFilter[filter].magFilter;
		samplerInfo.minFilter = TexFilter[filter].minFilter;
		samplerInfo.addressModeU = TexClamp[i].clamp_u;
		samplerInfo.addressModeV = TexClamp[i].clamp_v;
		samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.anisotropyEnable = TexFilter[filter].mipmapping;
		samplerInfo.maxAnisotropy = TexFilter[filter].mipmapping? *gl_texture_filter_anisotropic : 1.f;
		samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
		samplerInfo.unnormalizedCoordinates = VK_FALSE;
		samplerInfo.compareEnable = VK_FALSE;
		samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
		samplerInfo.mipmapMode = TexFilter[filter].mipfilter;
		samplerInfo.minLod = 0;
		samplerInfo.maxLod = TexFilter[filter].mipmapping? 100 : 0.25f;	// According to the spec this value is clamped so something high makes it usable for all textures.
		samplerInfo.mipLodBias = 0;

		mSamplers[i] = VK_NULL_HANDLE;
        if (vkCreateSampler(vDevice->vkDevice, &samplerInfo, nullptr, &mSamplers[i]) != VK_SUCCESS) 
		{
            //throw std::runtime_error("failed to create texture sampler!");
        }
    }
}

void VkSamplerManager::Destroy()
{
	for(int i = 0; i < 7; i++)
	{
		if (mSamplers[i] != VK_NULL_HANDLE) vkDestroySampler(vDevice->vkDevice, mSamplers[i], nullptr);
		mSamplers[i] = VK_NULL_HANDLE;
	}
}
	