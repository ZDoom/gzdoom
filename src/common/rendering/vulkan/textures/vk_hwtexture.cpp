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

#include "templates.h"
#include "c_cvars.h"
#include "hw_material.h"
#include "hw_cvars.h"
#include "hw_renderstate.h"
#include "vulkan/system/vk_objects.h"
#include "vulkan/system/vk_builders.h"
#include "vulkan/system/vk_framebuffer.h"
#include "vulkan/textures/vk_samplers.h"
#include "vulkan/renderer/vk_renderpass.h"
#include "vulkan/renderer/vk_postprocess.h"
#include "vulkan/renderer/vk_renderbuffers.h"
#include "vulkan/shaders/vk_shader.h"
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
		if (mappedSWFB)
		{
			mImage.Image->Unmap();
			mappedSWFB = nullptr;
		}

		auto &deleteList = fb->FrameDeleteList;
		if (mImage.Image) deleteList.Images.push_back(std::move(mImage.Image));
		if (mImage.View) deleteList.ImageViews.push_back(std::move(mImage.View));
		for (auto &it : mImage.RSFramebuffers) deleteList.Framebuffers.push_back(std::move(it.second));
		if (mDepthStencil.Image) deleteList.Images.push_back(std::move(mDepthStencil.Image));
		if (mDepthStencil.View) deleteList.ImageViews.push_back(std::move(mDepthStencil.View));
		for (auto &it : mDepthStencil.RSFramebuffers) deleteList.Framebuffers.push_back(std::move(it.second));
		mImage.reset();
		mDepthStencil.reset();
	}
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

		ImageBuilder imgbuilder;
		VkDeviceSize allocatedBytes = 0;
		imgbuilder.setFormat(format);
		imgbuilder.setSize(w, h);
		imgbuilder.setLinearTiling();
		imgbuilder.setUsage(VK_IMAGE_USAGE_SAMPLED_BIT, VMA_MEMORY_USAGE_UNKNOWN, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT);
		imgbuilder.setMemoryType(
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		mImage.Image = imgbuilder.create(fb->device, &allocatedBytes);
		mImage.Image->SetDebugName("VkHardwareTexture.mImage");
		mTexelsize = texelsize;

		ImageViewBuilder viewbuilder;
		viewbuilder.setImage(mImage.Image.get(), format);
		mImage.View = viewbuilder.create(fb->device);
		mImage.View->SetDebugName("VkHardwareTexture.mImageView");

		auto cmdbuffer = fb->GetTransferCommands();

		VkImageTransition imageTransition;
		imageTransition.addImage(&mImage, VK_IMAGE_LAYOUT_GENERAL, true);
		imageTransition.execute(cmdbuffer);

		bufferpitch = int(allocatedBytes / h / texelsize);
	}
}

uint8_t *VkHardwareTexture::MapBuffer()
{
	if (!mappedSWFB)
		mappedSWFB = (uint8_t*)mImage.Image->Map(0, mImage.Image->width * mImage.Image->height * mTexelsize);
	return mappedSWFB;
}

unsigned int VkHardwareTexture::CreateTexture(unsigned char * buffer, int w, int h, int texunit, bool mipmap, const char *name)
{
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


VkMaterial* VkMaterial::First = nullptr;

VkMaterial::VkMaterial(FGameTexture* tex, int scaleflags) : FMaterial(tex, scaleflags)
{
	Next = First;
	First = this;
	if (Next) Next->Prev = this;
}

VkMaterial::~VkMaterial()
{
	if (Next) Next->Prev = Prev;
	if (Prev) Prev->Next = Next;
	else First = Next;

	DeleteDescriptors();
}

void VkMaterial::DeleteDescriptors()
{
	if (auto fb = GetVulkanFrameBuffer())
	{
		auto& deleteList = fb->FrameDeleteList;

		for (auto& it : mDescriptorSets)
		{
			deleteList.Descriptors.push_back(std::move(it.descriptor));
		}
	}

	mDescriptorSets.clear();
}

void VkMaterial::ResetAllDescriptors()
{
	for (VkMaterial* cur = First; cur; cur = cur->Next)
		cur->DeleteDescriptors();

	auto fb = GetVulkanFrameBuffer();
	if (fb)
		fb->GetRenderPassManager()->TextureSetPoolReset();
}

VulkanDescriptorSet* VkMaterial::GetDescriptorSet(const FMaterialState& state)
{
	auto base = Source();
	int clampmode = state.mClampMode;
	int translation = state.mTranslation;

	auto remap = translation <= 0 ? nullptr : GPalette.TranslationToTable(translation);
	if (remap) 
		translation = remap->Index;

	clampmode = base->GetClampMode(clampmode);

	for (auto& set : mDescriptorSets)
	{
		if (set.descriptor && set.clampmode == clampmode && set.flags == translation) return set.descriptor.get();
	}

	int numLayers = NumLayers();

	auto fb = GetVulkanFrameBuffer();
	auto descriptor = fb->GetRenderPassManager()->AllocateTextureDescriptorSet(std::max(numLayers, SHADER_MIN_REQUIRED_TEXTURE_LAYERS));

	descriptor->SetDebugName("VkHardwareTexture.mDescriptorSets");

	VulkanSampler* sampler = fb->GetSamplerManager()->Get(clampmode);

	WriteDescriptors update;
	MaterialLayerInfo *layer;
	auto systex = static_cast<VkHardwareTexture*>(GetLayer(0, state.mTranslation, &layer));
	update.addCombinedImageSampler(descriptor.get(), 0, systex->GetImage(layer->layerTexture, state.mTranslation, layer->scaleFlags)->View.get(), sampler, systex->mImage.Layout);
	for (int i = 1; i < numLayers; i++)
	{
		auto systex = static_cast<VkHardwareTexture*>(GetLayer(i, 0, &layer));
		update.addCombinedImageSampler(descriptor.get(), i, systex->GetImage(layer->layerTexture, 0, layer->scaleFlags)->View.get(), sampler, systex->mImage.Layout);
	}

	auto dummyImage = fb->GetRenderPassManager()->GetNullTextureView();
	for (int i = numLayers; i < SHADER_MIN_REQUIRED_TEXTURE_LAYERS; i++)
	{
		update.addCombinedImageSampler(descriptor.get(), i, dummyImage, sampler, systex->mImage.Layout);
	}

	update.updateSets(fb->device);
	mDescriptorSets.emplace_back(clampmode, translation, std::move(descriptor));
	return mDescriptorSets.back().descriptor.get();
}

