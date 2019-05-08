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
/*
** Container class for the various translations a texture can have.
**
*/

#include "templates.h"
#include "c_cvars.h"
#include "r_data/colormaps.h"
#include "hwrenderer/textures/hw_material.h"
#include "hwrenderer/utility/hw_cvars.h"
#include "hwrenderer/scene/hw_renderstate.h"
#include "vulkan/system/vk_objects.h"
#include "vulkan/system/vk_builders.h"
#include "vulkan/system/vk_framebuffer.h"
#include "vulkan/textures/vk_samplers.h"
#include "vulkan/renderer/vk_renderpass.h"
#include "vulkan/renderer/vk_postprocess.h"
#include "vulkan/renderer/vk_renderbuffers.h"
#include "vk_hwtexture.h"

VkHardwareTexture *VkHardwareTexture::First = nullptr;

VkHardwareTexture::VkHardwareTexture()
{
	Next = First;
	First = this;
	if (Next) Next->Prev = this;
}

VkHardwareTexture::~VkHardwareTexture()
{
	if (Next) Next->Prev = Prev;
	if (Prev) Prev->Next = Next;
	else First = Next;

	Reset();
}

void VkHardwareTexture::ResetAll()
{
	for (VkHardwareTexture *cur = VkHardwareTexture::First; cur; cur = cur->Next)
		cur->Reset();
}

void VkHardwareTexture::Reset()
{
	if (auto fb = GetVulkanFrameBuffer())
	{
		ResetDescriptors();

		auto &deleteList = fb->FrameDeleteList;
		if (mImage) deleteList.Images.push_back(std::move(mImage));
		if (mImageView) deleteList.ImageViews.push_back(std::move(mImageView));
		if (mDepthStencil) deleteList.Images.push_back(std::move(mDepthStencil));
		if (mDepthStencilView) deleteList.ImageViews.push_back(std::move(mDepthStencilView));
	}
}

void VkHardwareTexture::ResetDescriptors()
{
	if (auto fb = GetVulkanFrameBuffer())
	{
		auto &deleteList = fb->FrameDeleteList;

		for (auto &it : mDescriptorSets)
		{
			deleteList.Descriptors.push_back(std::move(it.descriptor));
		}
	}

	mDescriptorSets.clear();
}

void VkHardwareTexture::ResetAllDescriptors()
{
	for (VkHardwareTexture *cur = First; cur; cur = cur->Next)
		cur->ResetDescriptors();

	auto fb = GetVulkanFrameBuffer();
	if (fb)
		fb->GetRenderPassManager()->TextureSetPoolReset();
}

void VkHardwareTexture::Precache(FMaterial *mat, int translation, int flags)
{
	int numLayers = mat->GetLayers();
	GetImageView(mat->tex, translation, flags);
	for (int i = 1; i < numLayers; i++)
	{
		FTexture *layer;
		auto systex = static_cast<VkHardwareTexture*>(mat->GetLayer(i, 0, &layer));
		systex->GetImageView(layer, 0, mat->isExpanded() ? CTF_Expand : 0);
	}
}

VulkanDescriptorSet *VkHardwareTexture::GetDescriptorSet(const FMaterialState &state)
{
	FMaterial *mat = state.mMaterial;
	FTexture *tex = state.mMaterial->tex;
	int clampmode = state.mClampMode;
	int translation = state.mTranslation;

	if (tex->UseType == ETextureType::SWCanvas) clampmode = CLAMP_NOFILTER;
	if (tex->isHardwareCanvas()) clampmode = CLAMP_CAMTEX;
	else if ((tex->isWarped() || tex->shaderindex >= FIRST_USER_SHADER) && clampmode <= CLAMP_XY) clampmode = CLAMP_NONE;

	// Textures that are already scaled in the texture lump will not get replaced by hires textures.
	int flags = state.mMaterial->isExpanded() ? CTF_Expand : (gl_texture_usehires && !tex->isScaled() && clampmode <= CLAMP_XY) ? CTF_CheckHires : 0;

	if (tex->isHardwareCanvas()) static_cast<FCanvasTexture*>(tex)->NeedUpdate();

	for (auto &set : mDescriptorSets)
	{
		if (set.descriptor && set.clampmode == clampmode && set.flags == flags) return set.descriptor.get();
	}

	int numLayers = mat->GetLayers();

	auto fb = GetVulkanFrameBuffer();
	auto descriptor = fb->GetRenderPassManager()->AllocateTextureDescriptorSet(numLayers);

	descriptor->SetDebugName("VkHardwareTexture.mDescriptorSets");

	VulkanSampler *sampler = fb->GetSamplerManager()->Get(clampmode);

	auto baseView = GetImageView(tex, translation, flags);

	WriteDescriptors update;
	update.addCombinedImageSampler(descriptor.get(), 0, baseView, sampler, mImageLayout);
	for (int i = 1; i < numLayers; i++)
	{
		FTexture *layer;
		auto systex = static_cast<VkHardwareTexture*>(mat->GetLayer(i, 0, &layer));
		update.addCombinedImageSampler(descriptor.get(), i, systex->GetImageView(layer, 0, mat->isExpanded() ? CTF_Expand : 0), sampler, systex->mImageLayout);
	}
	update.updateSets(fb->device);
	mDescriptorSets.emplace_back(clampmode, flags, std::move(descriptor));
	return mDescriptorSets.back().descriptor.get();
}

VulkanImage *VkHardwareTexture::GetImage(FTexture *tex, int translation, int flags)
{
	if (!mImage)
	{
		CreateImage(tex, translation, flags);
	}
	return mImage.get();
}

VulkanImageView *VkHardwareTexture::GetImageView(FTexture *tex, int translation, int flags)
{
	if (!mImageView)
	{
		CreateImage(tex, translation, flags);
	}
	return mImageView.get();
}

VulkanImageView *VkHardwareTexture::GetDepthStencilView(FTexture *tex)
{
	if (!mDepthStencilView)
	{
		auto fb = GetVulkanFrameBuffer();

		VkFormat format = fb->GetBuffers()->SceneDepthStencilFormat;
		int w = tex->GetWidth();
		int h = tex->GetHeight();

		ImageBuilder builder;
		builder.setSize(w, h);
		builder.setSamples(VK_SAMPLE_COUNT_1_BIT);
		builder.setFormat(format);
		builder.setUsage(VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
		mDepthStencil = builder.create(fb->device);
		mDepthStencil->SetDebugName("VkHardwareTexture.DepthStencil");

		ImageViewBuilder viewbuilder;
		viewbuilder.setImage(mDepthStencil.get(), format, VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);
		mDepthStencilView = viewbuilder.create(fb->device);
		mDepthStencilView->SetDebugName("VkHardwareTexture.DepthStencilView");

		PipelineBarrier barrier;
		barrier.addImage(mDepthStencil.get(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 0, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);
		barrier.execute(fb->GetTransferCommands(), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);
	}
	return mDepthStencilView.get();
}

void VkHardwareTexture::CreateImage(FTexture *tex, int translation, int flags)
{
	if (!tex->isHardwareCanvas())
	{
		if (translation <= 0)
		{
			translation = -translation;
		}
		else
		{
			auto remap = TranslationToTable(translation);
			translation = remap == nullptr ? 0 : remap->GetUniqueIndex();
		}

		FTextureBuffer texbuffer = tex->CreateTexBuffer(translation, flags | CTF_ProcessData);
		CreateTexture(texbuffer.mWidth, texbuffer.mHeight, 4, VK_FORMAT_B8G8R8A8_UNORM, texbuffer.mBuffer);
	}
	else
	{
		auto fb = GetVulkanFrameBuffer();

		VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
		int w = tex->GetWidth();
		int h = tex->GetHeight();

		ImageBuilder imgbuilder;
		imgbuilder.setFormat(format);
		imgbuilder.setSize(w, h);
		imgbuilder.setUsage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
		mImage = imgbuilder.create(fb->device);
		mImage->SetDebugName("VkHardwareTexture.mImage");

		ImageViewBuilder viewbuilder;
		viewbuilder.setImage(mImage.get(), format);
		mImageView = viewbuilder.create(fb->device);
		mImageView->SetDebugName("VkHardwareTexture.mImageView");

		auto cmdbuffer = fb->GetTransferCommands();

		PipelineBarrier imageTransition;
		imageTransition.addImage(mImage.get(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, VK_ACCESS_SHADER_READ_BIT);
		imageTransition.execute(cmdbuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
	}
}

void VkHardwareTexture::CreateTexture(int w, int h, int pixelsize, VkFormat format, const void *pixels)
{
	auto fb = GetVulkanFrameBuffer();

	int totalSize = w * h * pixelsize;

	BufferBuilder bufbuilder;
	bufbuilder.setSize(totalSize);
	bufbuilder.setUsage(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
	std::unique_ptr<VulkanBuffer> stagingBuffer = bufbuilder.create(fb->device);
	stagingBuffer->SetDebugName("VkHardwareTexture.mStagingBuffer");

	uint8_t *data = (uint8_t*)stagingBuffer->Map(0, totalSize);
	memcpy(data, pixels, totalSize);
	stagingBuffer->Unmap();

	ImageBuilder imgbuilder;
	imgbuilder.setFormat(format);
	imgbuilder.setSize(w, h, GetMipLevels(w, h));
	imgbuilder.setUsage(VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
	mImage = imgbuilder.create(fb->device);
	mImage->SetDebugName("VkHardwareTexture.mImage");

	ImageViewBuilder viewbuilder;
	viewbuilder.setImage(mImage.get(), format);
	mImageView = viewbuilder.create(fb->device);
	mImageView->SetDebugName("VkHardwareTexture.mImageView");

	auto cmdbuffer = fb->GetTransferCommands();

	PipelineBarrier imageTransition0;
	imageTransition0.addImage(mImage.get(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, VK_ACCESS_TRANSFER_WRITE_BIT);
	imageTransition0.execute(cmdbuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

	VkBufferImageCopy region = {};
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.layerCount = 1;
	region.imageExtent.depth = 1;
	region.imageExtent.width = w;
	region.imageExtent.height = h;
	cmdbuffer->copyBufferToImage(stagingBuffer->buffer, mImage->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

	fb->FrameDeleteList.Buffers.push_back(std::move(stagingBuffer));

	GenerateMipmaps(mImage.get(), cmdbuffer);
}

void VkHardwareTexture::GenerateMipmaps(VulkanImage *image, VulkanCommandBuffer *cmdbuffer)
{
	int mipWidth = image->width;
	int mipHeight = image->height;
	int i;
	for (i = 1; mipWidth > 1 || mipHeight > 1; i++)
	{
		PipelineBarrier barrier0;
		barrier0.addImage(image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_ASPECT_COLOR_BIT, i - 1);
		barrier0.addImage(image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_ASPECT_COLOR_BIT, i);
		barrier0.execute(cmdbuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

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
		cmdbuffer->blitImage(image->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

		PipelineBarrier barrier1;
		barrier1.addImage(image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_ASPECT_COLOR_BIT, i - 1);
		barrier1.execute(cmdbuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

		mipWidth = nextWidth;
		mipHeight = nextHeight;
	}

	PipelineBarrier barrier2;
	barrier2.addImage(image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_ASPECT_COLOR_BIT, i - 1);
	barrier2.execute(cmdbuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
}

int VkHardwareTexture::GetMipLevels(int w, int h)
{
	int levels = 1;
	while (w > 1 || h > 1)
	{
		w = std::max(w >> 1, 1);
		h = std::max(h >> 1, 1);
		levels++;
	}
	return levels;
}

void VkHardwareTexture::AllocateBuffer(int w, int h, int texelsize)
{
	if (mImage && (mImage->width != w || mImage->height != h || mTexelsize != texelsize))
	{
		Reset();
	}

	if (!mImage)
	{
		auto fb = GetVulkanFrameBuffer();

		VkFormat format = texelsize == 4 ? VK_FORMAT_B8G8R8A8_UNORM : VK_FORMAT_R8_UNORM;

		ImageBuilder imgbuilder;
		VkDeviceSize allocatedBytes = 0;
		imgbuilder.setFormat(format);
		imgbuilder.setSize(w, h);
		imgbuilder.setLinearTiling();
		imgbuilder.setUsage(VK_IMAGE_USAGE_SAMPLED_BIT, VMA_MEMORY_USAGE_UNKNOWN, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT);
		imgbuilder.setMemoryType(
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		mImage = imgbuilder.create(fb->device, &allocatedBytes);
		mImage->SetDebugName("VkHardwareTexture.mImage");
		mImageLayout = VK_IMAGE_LAYOUT_GENERAL;
		mTexelsize = texelsize;

		ImageViewBuilder viewbuilder;
		viewbuilder.setImage(mImage.get(), format);
		mImageView = viewbuilder.create(fb->device);
		mImageView->SetDebugName("VkHardwareTexture.mImageView");

		auto cmdbuffer = fb->GetTransferCommands();

		PipelineBarrier imageTransition;
		imageTransition.addImage(mImage.get(), VK_IMAGE_LAYOUT_UNDEFINED, mImageLayout, 0, VK_ACCESS_SHADER_READ_BIT);
		imageTransition.execute(cmdbuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

		bufferpitch = int(allocatedBytes / h / texelsize);
	}
}

uint8_t *VkHardwareTexture::MapBuffer()
{
	return (uint8_t*)mImage->Map(0, mImage->width * mImage->height * mTexelsize);
}

unsigned int VkHardwareTexture::CreateTexture(unsigned char * buffer, int w, int h, int texunit, bool mipmap, int translation, const char *name)
{
	mImage->Unmap();
	return 0;
}

void VkHardwareTexture::CreateWipeTexture(int w, int h, const char *name)
{
	auto fb = GetVulkanFrameBuffer();

	VkFormat format = VK_FORMAT_B8G8R8A8_UNORM;

	ImageBuilder imgbuilder;
	imgbuilder.setFormat(format);
	imgbuilder.setSize(w, h);
	imgbuilder.setUsage(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY);
	mImage = imgbuilder.create(fb->device);
	mImage->SetDebugName(name);
	mImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	mTexelsize = 4;

	ImageViewBuilder viewbuilder;
	viewbuilder.setImage(mImage.get(), format);
	mImageView = viewbuilder.create(fb->device);
	mImageView->SetDebugName(name);

	fb->GetPostprocess()->BlitCurrentToImage(mImage.get(), &mImageLayout);
}
