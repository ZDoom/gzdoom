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

#include "vk_imagetransition.h"

void VkImageTransition::addImage(VkTextureImage *image, VkImageLayout targetLayout, bool undefinedSrcLayout)
{
	if (image->Layout == targetLayout)
		return;

	VkAccessFlags srcAccess = 0;
	VkAccessFlags dstAccess = 0;
	VkImageAspectFlags aspectMask = image->AspectMask;

	switch (image->Layout)
	{
	case VK_IMAGE_LAYOUT_UNDEFINED:
		srcAccess = 0;
		srcStageMask |= VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		break;
	case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
		srcAccess = VK_ACCESS_TRANSFER_READ_BIT;
		srcStageMask |= VK_PIPELINE_STAGE_TRANSFER_BIT;
		break;
	case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
		srcAccess = VK_ACCESS_SHADER_READ_BIT;
		srcStageMask |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		break;
	case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
		srcAccess = VK_ACCESS_TRANSFER_WRITE_BIT;
		srcStageMask |= VK_PIPELINE_STAGE_TRANSFER_BIT;
		break;
	case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
		srcAccess = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		srcStageMask |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		break;
	case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
		srcAccess = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		srcStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		break;
	default:
		I_FatalError("Unimplemented src image layout transition\n");
	}

	switch (targetLayout)
	{
	case VK_IMAGE_LAYOUT_GENERAL:
		dstAccess = 0;
		dstStageMask |= VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		break;
	case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
		dstAccess = VK_ACCESS_TRANSFER_READ_BIT;
		dstStageMask |= VK_PIPELINE_STAGE_TRANSFER_BIT;
		break;
	case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
		dstAccess = VK_ACCESS_SHADER_READ_BIT;
		dstStageMask |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		break;
	case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
		dstAccess = VK_ACCESS_TRANSFER_WRITE_BIT;
		dstStageMask |= VK_PIPELINE_STAGE_TRANSFER_BIT;
		break;
	case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
		dstAccess = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dstStageMask |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		break;
	case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
		srcAccess = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		srcStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		break;
	default:
		I_FatalError("Unimplemented dst image layout transition\n");
	}

	barrier.addImage(image->Image.get(), undefinedSrcLayout ? VK_IMAGE_LAYOUT_UNDEFINED : image->Layout, targetLayout, srcAccess, dstAccess, aspectMask);
	needbarrier = true;
	image->Layout = targetLayout;
}

void VkImageTransition::execute(VulkanCommandBuffer *cmdbuffer)
{
	if (needbarrier)
		barrier.execute(cmdbuffer, srcStageMask, dstStageMask);
}

/////////////////////////////////////////////////////////////////////////////

void VkTextureImage::GenerateMipmaps(VulkanCommandBuffer *cmdbuffer)
{
	int mipWidth = Image->width;
	int mipHeight = Image->height;
	int i;
	for (i = 1; mipWidth > 1 || mipHeight > 1; i++)
	{
		PipelineBarrier barrier0;
		barrier0.addImage(Image.get(), Layout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_ASPECT_COLOR_BIT, i - 1);
		barrier0.addImage(Image.get(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_ASPECT_COLOR_BIT, i);
		barrier0.execute(cmdbuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
		Layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

		int nextWidth = std::max(mipWidth >> 1, 1);
		int nextHeight = std::max(mipHeight >> 1, 1);

		VkImageBlit blit = {};
		blit.srcOffsets[0] = { 0, 0, 0 };
		blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
		blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.srcSubresource.mipLevel = i - 1;
		blit.srcSubresource.baseArrayLayer = 0;
		blit.srcSubresource.layerCount = 1;
		blit.dstOffsets[0] = { 0, 0, 0 };
		blit.dstOffsets[1] = { nextWidth, nextHeight, 1 };
		blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.dstSubresource.mipLevel = i;
		blit.dstSubresource.baseArrayLayer = 0;
		blit.dstSubresource.layerCount = 1;
		cmdbuffer->blitImage(Image->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, Image->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

		PipelineBarrier barrier1;
		barrier1.addImage(Image.get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_ASPECT_COLOR_BIT, i - 1);
		barrier1.execute(cmdbuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

		mipWidth = nextWidth;
		mipHeight = nextHeight;
	}

	PipelineBarrier barrier2;
	barrier2.addImage(Image.get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_ASPECT_COLOR_BIT, i - 1);
	barrier2.execute(cmdbuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

	Layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}
