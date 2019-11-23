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

		if (mappedSWFB)
		{
			if (mTransferBuffer)
				mTransferBuffer->Unmap();
			else
				mImage.Image->Unmap();
			mappedSWFB = nullptr;
		}

		auto &deleteList = fb->FrameDeleteList;
		if (mImage.Image) deleteList.Images.push_back(std::move(mImage.Image));
		if (mImage.View) deleteList.ImageViews.push_back(std::move(mImage.View));
		if (mTransferImage) deleteList.Images.push_back(std::move(mTransferImage));
		if (mTransferBuffer) deleteList.Buffers.push_back(std::move(mTransferBuffer));
		for (auto &it : mImage.RSFramebuffers) deleteList.Framebuffers.push_back(std::move(it.second));
		if (mDepthStencil.Image) deleteList.Images.push_back(std::move(mDepthStencil.Image));
		if (mDepthStencil.View) deleteList.ImageViews.push_back(std::move(mDepthStencil.View));
		for (auto &it : mDepthStencil.RSFramebuffers) deleteList.Framebuffers.push_back(std::move(it.second));
		mImage.reset();
		mDepthStencil.reset();
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
	GetImage(mat->tex, translation, flags);
	for (int i = 1; i < numLayers; i++)
	{
		FTexture *layer;
		auto systex = static_cast<VkHardwareTexture*>(mat->GetLayer(i, 0, &layer));
		systex->GetImage(layer, 0, mat->isExpanded() ? CTF_Expand : 0);
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

	WriteDescriptors update;
	update.addCombinedImageSampler(descriptor.get(), 0, GetImage(tex, translation, flags)->View.get(), sampler, mImage.Layout);
	for (int i = 1; i < numLayers; i++)
	{
		FTexture *layer;
		auto systex = static_cast<VkHardwareTexture*>(mat->GetLayer(i, 0, &layer));
		update.addCombinedImageSampler(descriptor.get(), i, systex->GetImage(layer, 0, mat->isExpanded() ? CTF_Expand : 0)->View.get(), sampler, systex->mImage.Layout);
	}
	update.updateSets(fb->device);
	mDescriptorSets.emplace_back(clampmode, flags, std::move(descriptor));
	return mDescriptorSets.back().descriptor.get();
}

VkTextureImage *VkHardwareTexture::GetImage(FTexture *tex, int translation, int flags)
{
	if (!mImage.Image)
	{
		CreateImage(tex, translation, flags);
	}
	return &mImage;
}

VkTextureImage *VkHardwareTexture::GetDepthStencil(FTexture *tex)
{
	if (!mDepthStencil.View)
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
		mDepthStencil.Image = builder.create(fb->device);
		mDepthStencil.Image->SetDebugName("VkHardwareTexture.DepthStencil");
		mDepthStencil.AspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;

		ImageViewBuilder viewbuilder;
		viewbuilder.setImage(mDepthStencil.Image.get(), format, VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);
		mDepthStencil.View = viewbuilder.create(fb->device);
		mDepthStencil.View->SetDebugName("VkHardwareTexture.DepthStencilView");

		VkImageTransition barrier;
		barrier.addImage(&mDepthStencil, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, true);
		barrier.execute(fb->GetTransferCommands());
	}
	return &mDepthStencil;
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
		mImage.Image = imgbuilder.create(fb->device);
		mImage.Image->SetDebugName("VkHardwareTexture.mImage");

		ImageViewBuilder viewbuilder;
		viewbuilder.setImage(mImage.Image.get(), format);
		mImage.View = viewbuilder.create(fb->device);
		mImage.View->SetDebugName("VkHardwareTexture.mImageView");

		auto cmdbuffer = fb->GetTransferCommands();

		VkImageTransition imageTransition;
		imageTransition.addImage(&mImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, true);
		imageTransition.execute(cmdbuffer);
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
	mImage.Image = imgbuilder.create(fb->device);
	mImage.Image->SetDebugName("VkHardwareTexture.mImage");

	ImageViewBuilder viewbuilder;
	viewbuilder.setImage(mImage.Image.get(), format);
	mImage.View = viewbuilder.create(fb->device);
	mImage.View->SetDebugName("VkHardwareTexture.mImageView");

	auto cmdbuffer = fb->GetTransferCommands();

	VkImageTransition imageTransition;
	imageTransition.addImage(&mImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, true);
	imageTransition.execute(cmdbuffer);

	VkBufferImageCopy region = {};
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.layerCount = 1;
	region.imageExtent.depth = 1;
	region.imageExtent.width = w;
	region.imageExtent.height = h;
	cmdbuffer->copyBufferToImage(stagingBuffer->buffer, mImage.Image->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

	fb->FrameDeleteList.Buffers.push_back(std::move(stagingBuffer));

	mImage.GenerateMipmaps(cmdbuffer);
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
	if (mImage.Image && (mImage.Image->width != w || mImage.Image->height != h || mTexelsize != texelsize))
	{
		Reset();
	}

	if (!mImage.Image)
	{
		auto fb = GetVulkanFrameBuffer();

		VkFormat format = texelsize == 4 ? VK_FORMAT_B8G8R8A8_UNORM : VK_FORMAT_R8_UNORM;

		if (fb->device->copyQueueTransferFamily != -1)
		{
			// Use DMA transfer to get the image to the GPU

			BufferBuilder bufbuilder;
			bufbuilder.setUsage(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT);
			bufbuilder.setSize(w * h * texelsize);
			mTransferBuffer = bufbuilder.create(fb->device);
			mTransferBuffer->SetDebugName("VkHardwareTexture.mTransferBuffer");

			ImageBuilder imgbuilder0;
			imgbuilder0.setFormat(format);
			imgbuilder0.setSize(w, h);
			imgbuilder0.setUsage(VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
			mTransferImage = imgbuilder0.create(fb->device);
			mTransferImage->SetDebugName("VkHardwareTexture.mTransferImage");

			ImageBuilder imgbuilder1;
			imgbuilder1.setFormat(format);
			imgbuilder1.setSize(w, h);
			imgbuilder1.setUsage(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
			mImage.Image = imgbuilder1.create(fb->device);
			mImage.Image->SetDebugName("VkHardwareTexture.mImage");

			bufferpitch = w;
		}
		else
		{
			// Memory map the image directly for GPUs where we have no transfer queue (i.e. Intel embedded GPUs)

			ImageBuilder imgbuilder;
			imgbuilder.setFormat(format);
			imgbuilder.setSize(w, h);
			imgbuilder.setLinearTiling();
			imgbuilder.setUsage(VK_IMAGE_USAGE_SAMPLED_BIT, VMA_MEMORY_USAGE_UNKNOWN, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT);
			imgbuilder.setMemoryType(
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

			VkDeviceSize allocatedBytes = 0;
			mImage.Image = imgbuilder.create(fb->device, &allocatedBytes);
			mImage.Image->SetDebugName("VkHardwareTexture.mImage");

			bufferpitch = int(allocatedBytes / h / texelsize);
		}

		mTexelsize = texelsize;

		ImageViewBuilder viewbuilder;
		viewbuilder.setImage(mImage.Image.get(), format);
		mImage.View = viewbuilder.create(fb->device);
		mImage.View->SetDebugName("VkHardwareTexture.mImageView");

		auto cmdbuffer = fb->GetTransferCommands();

		VkImageTransition imageTransition;
		imageTransition.addImage(&mImage, VK_IMAGE_LAYOUT_GENERAL, true);
		imageTransition.execute(cmdbuffer);
	}
}

uint8_t *VkHardwareTexture::MapBuffer()
{
	if (!mappedSWFB)
	{
		if (mTransferBuffer)
			mappedSWFB = (uint8_t*)mTransferBuffer->Map(0, mImage.Image->width * mImage.Image->height * mTexelsize);
		else
			mappedSWFB = (uint8_t*)mImage.Image->Map(0, mImage.Image->width * mImage.Image->height * mTexelsize);
	}
	return mappedSWFB;
}

unsigned int VkHardwareTexture::CreateTexture(unsigned char * buffer, int w, int h, int texunit, bool mipmap, int translation, const char *name)
{
	if (mTransferBuffer)
	{
		auto fb = GetVulkanFrameBuffer();
		auto copyqueue = fb->GetCopyQueueCommands();

		// Acquire image, transfer buffer via copy queue (PCIe DMA), release image

		PipelineBarrier barrier0;
		barrier0.addQueueTransfer(fb->device->graphicsFamily, fb->device->copyQueueTransferFamily, mTransferImage.get(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		barrier0.execute(copyqueue, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

		VkBufferImageCopy region = {};
		region.imageExtent.width = mTransferImage->width;
		region.imageExtent.height = mTransferImage->height;
		region.imageExtent.depth = 1;
		region.imageSubresource.mipLevel = 0;
		region.imageSubresource.layerCount = 1;
		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		copyqueue->copyBufferToImage(mTransferBuffer->buffer, mTransferImage->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

		PipelineBarrier barrier1;
		barrier1.addQueueTransfer(fb->device->copyQueueTransferFamily, fb->device->graphicsFamily, mTransferImage.get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
		barrier1.execute(copyqueue, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

		// Acquire image on graphics queue, make a copy of it (on the GPU), then release the image again back to the copy queue

		auto gfxqueue = fb->GetTransferCommands();

		PipelineBarrier barrier2;
		barrier2.addImage(mImage.Image.get(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
		barrier2.addQueueTransfer(fb->device->copyQueueTransferFamily, fb->device->graphicsFamily, mTransferImage.get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
		barrier2.execute(gfxqueue, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

		VkImageCopy imgregion = {};
		imgregion.extent = region.imageExtent;
		imgregion.srcSubresource = region.imageSubresource;
		imgregion.dstSubresource = region.imageSubresource;
		gfxqueue->copyImage(mTransferImage->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, mImage.Image->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imgregion);

		PipelineBarrier barrier3;
		barrier3.addImage(mImage.Image.get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, mImage.Layout, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
		barrier3.addQueueTransfer(fb->device->graphicsFamily, fb->device->copyQueueTransferFamily, mTransferImage.get(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		barrier3.execute(gfxqueue, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
	}

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
	mImage.Image = imgbuilder.create(fb->device);
	mImage.Image->SetDebugName(name);
	mTexelsize = 4;

	ImageViewBuilder viewbuilder;
	viewbuilder.setImage(mImage.Image.get(), format);
	mImage.View = viewbuilder.create(fb->device);
	mImage.View->SetDebugName(name);

	if (fb->GetBuffers()->GetWidth() > 0 && fb->GetBuffers()->GetHeight() > 0)
	{
		fb->GetPostprocess()->BlitCurrentToImage(&mImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	}
	else
	{
		// hwrenderer asked image data from a frame buffer that was never written into. Let's give it that..
		// (ideally the hwrenderer wouldn't do this, but the calling code is too complex for me to fix)

		VkImageTransition transition0;
		transition0.addImage(&mImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, true);
		transition0.execute(fb->GetTransferCommands());

		VkImageSubresourceRange range = {};
		range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		range.layerCount = 1;
		range.levelCount = 1;

		VkClearColorValue value = {};
		value.float32[0] = 0.0f;
		value.float32[1] = 0.0f;
		value.float32[2] = 0.0f;
		value.float32[3] = 1.0f;
		fb->GetTransferCommands()->clearColorImage(mImage.Image->image, mImage.Layout, &value, 1, &range);

		VkImageTransition transition1;
		transition1.addImage(&mImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, false);
		transition1.execute(fb->GetTransferCommands());
	}
}
