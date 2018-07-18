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
// depth buffer creation - mainly based on www.vulkan-tutorial.com's demo code.

#include "vk_depthbuffer.h"

//==========================================================================
//
//
//
//==========================================================================

void VkDepthBuffer::Destroy()
{
	if (textureImageView != VK_NULL_HANDLE)
		vkDestroyImageView(vDevice->vkDevice, textureImageView, nullptr);

	if (textureImage != VK_NULL_HANDLE)
		vmaDestroyImage(vDevice->vkAllocator, textureImage, textureImageAllocation);

	textureImageView = VK_NULL_HANDLE;
	textureImage = VK_NULL_HANDLE;
	textureImageAllocation = VK_NULL_HANDLE;
}

//==========================================================================
//
// Allocate the texture image
//
//==========================================================================

VkResult VkDepthBuffer::CreateImage(uint32_t texWidth, uint32_t texHeight, VkImage& image, VmaAllocation& allocation)
 {
	VkImageCreateInfo imageInfo = {};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = texWidth;
	imageInfo.extent.height = texHeight;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.format = VK_FORMAT_D24_UNORM_S8_UINT;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	
	VmaAllocationCreateInfo allocInfo = {};
	allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	return vmaCreateImage(vDevice->vkAllocator, &imageInfo, &allocInfo, &image, &allocation, nullptr);
}

//==========================================================================
//
//
//
//==========================================================================

VkResult VkDepthBuffer::CreateImageView(VkImage image) 
{
	VkImageViewCreateInfo viewInfo = {};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = image;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = VK_FORMAT_D24_UNORM_S8_UINT;
	viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;
	return vkCreateImageView(vDevice->vkDevice, &viewInfo, nullptr, &textureImageView);
}

//==========================================================================
//
//
//
//==========================================================================

VkResult VkDepthBuffer::Create(int texWidth, int texHeight)
{
	VkDeviceSize imageSize = texWidth * texHeight * 4;

	auto res = CreateImage(texWidth, texHeight, textureImage, textureImageAllocation);
	if (res == VK_SUCCESS)
	{
		vDevice->TransitionImageLayout(textureImage, VK_FORMAT_D24_UNORM_S8_UINT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 1);
		res = CreateImageView(textureImage);
		if (res == VK_SUCCESS) return VK_SUCCESS;
	}
	Destroy();
	return res;
}

