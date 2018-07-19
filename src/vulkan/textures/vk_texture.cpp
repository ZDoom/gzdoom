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
// Texture creation - mainly based on www.vulkan-tutorial.com's demo code.

#include "vk_texture.h"
#include <algorithm>

//==========================================================================
//
//
//
//==========================================================================

void VkTexture::Destroy()
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
// Allocate the transfer buffer
//
//==========================================================================

VkResult VkTexture::CreateBuffer(VkDeviceSize size, VkBuffer& buffer, VmaAllocation& allocation) 
{
	VkBufferCreateInfo bufferInfo = {};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = size;
	bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	
	VmaAllocationCreateInfo allocInfo = {};
	allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
	return vmaCreateBuffer(vDevice->vkAllocator, &bufferInfo, &allocInfo, &buffer, &allocation, nullptr);
}

//==========================================================================
//
// Allocate the texture image
//
//==========================================================================

VkResult VkTexture::CreateImage(uint32_t texWidth, uint32_t texHeight, int miplevels, VkImage& image, VmaAllocation& allocation)
 {
	VkImageCreateInfo imageInfo = {};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = texWidth;
	imageInfo.extent.height = texHeight;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = miplevels;
	imageInfo.arrayLayers = 1;
	imageInfo.format = textureFormat;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
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

void VkTexture::CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) 
{
	VkCommandBuffer commandBuffer = vDevice->BeginSingleTimeCommands();

	VkBufferImageCopy region = {};
	region.bufferOffset = 0;
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 1;
	region.imageOffset = { 0, 0, 0 };
	region.imageExtent = {
		width,
		height,
		1
	};

	vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

	vDevice->EndSingleTimeCommands(commandBuffer);
}

//==========================================================================
//
//
//
//==========================================================================

void VkTexture::GenerateMipmaps(VkImage image, int32_t texWidth, int32_t texHeight, uint32_t mipLevels) 
{
	if (mipLevels == 1) return;

	VkCommandBuffer commandBuffer = vDevice->BeginSingleTimeCommands();

	VkImageMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.image = image;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;
	barrier.subresourceRange.levelCount = 1;

	int32_t mipWidth = texWidth;
	int32_t mipHeight = texHeight;

	for (uint32_t i = 1; i < mipLevels; i++) 
	{
		barrier.subresourceRange.baseMipLevel = i - 1;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

		vkCmdPipelineBarrier(commandBuffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
			0, nullptr,
			0, nullptr,
			1, &barrier);

		VkImageBlit blit = {};
		blit.srcOffsets[0] = { 0, 0, 0 };
		blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
		blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.srcSubresource.mipLevel = i - 1;
		blit.srcSubresource.baseArrayLayer = 0;
		blit.srcSubresource.layerCount = 1;
		blit.dstOffsets[0] = { 0, 0, 0 };
		if (mipWidth > 1) mipWidth /= 2;
		if (mipHeight > 1) mipHeight /= 2;
		blit.dstOffsets[1] = { mipWidth, mipHeight, 1 };
		blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.dstSubresource.mipLevel = i;
		blit.dstSubresource.baseArrayLayer = 0;
		blit.dstSubresource.layerCount = 1;

		vkCmdBlitImage(commandBuffer,
			image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1, &blit,
			VK_FILTER_LINEAR);

		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		vkCmdPipelineBarrier(commandBuffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
			0, nullptr,
			0, nullptr,
			1, &barrier);

	}

	barrier.subresourceRange.baseMipLevel = mipLevels - 1;
	barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	vkCmdPipelineBarrier(commandBuffer,
		VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
		0, nullptr,
		0, nullptr,
		1, &barrier);

	vDevice->EndSingleTimeCommands(commandBuffer);
}

//==========================================================================
//
//
//
//==========================================================================

VkResult VkTexture::CreateImageView(VkImage image, uint32_t mipLevels) 
{
	VkImageViewCreateInfo viewInfo = {};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = image;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = textureFormat;
	viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = mipLevels;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;
	return vkCreateImageView(vDevice->vkDevice, &viewInfo, nullptr, &textureImageView);
}

//==========================================================================
//
// Create the texture image.
// The input must have been validated by the caller to be proper texture data.
//
//==========================================================================

VkResult VkTexture::Create(const uint8_t *pixels, int texWidth, int texHeight, bool mipmapped, int numchannels)
{
	static const VkFormat FormatForChannel[] = {VK_FORMAT_R8_UNORM, VK_FORMAT_R8G8_UNORM, VK_FORMAT_B8G8R8_UNORM, VK_FORMAT_B8G8R8A8_UNORM};
	if (numchannels < 1 || numchannels > 4) return VK_ERROR_FORMAT_NOT_SUPPORTED;
	textureFormat = FormatForChannel[numchannels-1];

	VkDeviceSize imageSize = texWidth * texHeight * numchannels;
	int mipLevels = mipmapped ? static_cast<uint32_t>(floor(log2(std::max(texWidth, texHeight)))) + 1 : 1;
	this->mipmapped = mipmapped;

	VkBuffer stagingBuffer = VK_NULL_HANDLE;
	VmaAllocation stagingBufferAllocation = VK_NULL_HANDLE;
	VkResult res = CreateBuffer(imageSize, stagingBuffer, stagingBufferAllocation);
	if (res == VK_SUCCESS)
	{
		void* data;
		res = vmaMapMemory(vDevice->vkAllocator, stagingBufferAllocation, &data);
		if (res == VK_SUCCESS)
		{
			memcpy(data, pixels, static_cast<size_t>(imageSize));
			vmaUnmapMemory(vDevice->vkAllocator, stagingBufferAllocation);

			res = CreateImage(texWidth, texHeight, mipLevels, textureImage, textureImageAllocation);
			if (res == VK_SUCCESS)
			{
				vDevice->TransitionImageLayout(textureImage, textureFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, mipLevels);
				CopyBufferToImage(stagingBuffer, textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
				//transitioned to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL while generating mipmaps

				vmaDestroyBuffer(vDevice->vkAllocator, stagingBuffer, stagingBufferAllocation);
				stagingBuffer = VK_NULL_HANDLE;

				GenerateMipmaps(textureImage, texWidth, texHeight, mipLevels);
				res = CreateImageView(textureImage, mipLevels);
				if (res == VK_SUCCESS) return VK_SUCCESS;
			}
		}
		if (stagingBuffer != VK_NULL_HANDLE)
		{
			vmaDestroyBuffer(vDevice->vkAllocator, stagingBuffer, stagingBufferAllocation);
		}
	}
	Destroy();
	return res;
}
