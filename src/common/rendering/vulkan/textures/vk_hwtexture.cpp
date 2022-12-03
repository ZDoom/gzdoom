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


#include "c_cvars.h"
#include "hw_material.h"
#include "hw_cvars.h"
#include "hw_renderstate.h"
#include <zvulkan/vulkanobjects.h>
#include <zvulkan/vulkanbuilders.h>
#include "vulkan/system/vk_renderdevice.h"
#include "vulkan/system/vk_commandbuffer.h"
#include "vulkan/textures/vk_samplers.h"
#include "vulkan/textures/vk_renderbuffers.h"
#include "vulkan/textures/vk_texture.h"
#include "vulkan/renderer/vk_descriptorset.h"
#include "vulkan/renderer/vk_postprocess.h"
#include "vulkan/shaders/vk_shader.h"
#include "vk_hwtexture.h"

VkHardwareTexture::VkHardwareTexture(VulkanRenderDevice* fb, int numchannels) : fb(fb)
{
	mTexelsize = numchannels;
	fb->GetTextureManager()->AddTexture(this);
}

VkHardwareTexture::~VkHardwareTexture()
{
	if (fb)
		fb->GetTextureManager()->RemoveTexture(this);
}

void VkHardwareTexture::Reset()
{
	if (fb)
	{
		if (mappedSWFB)
		{
			mImage.Image->Unmap();
			mappedSWFB = nullptr;
		}

		mImage.Reset(fb);
		mDepthStencil.Reset(fb);
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
		VkFormat format = fb->GetBuffers()->SceneDepthStencilFormat;
		int w = tex->GetWidth();
		int h = tex->GetHeight();

		mDepthStencil.Image = ImageBuilder()
			.Size(w, h)
			.Samples(VK_SAMPLE_COUNT_1_BIT)
			.Format(format)
			.Usage(VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
			.DebugName("VkHardwareTexture.DepthStencil")
			.Create(fb->device.get());

		mDepthStencil.AspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;

		mDepthStencil.View = ImageViewBuilder()
			.Image(mDepthStencil.Image.get(), format, VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)
			.DebugName("VkHardwareTexture.DepthStencilView")
			.Create(fb->device.get());

		VkImageTransition()
			.AddImage(&mDepthStencil, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, true)
			.Execute(fb->GetCommands()->GetTransferCommands());
	}
	return &mDepthStencil;
}

void VkHardwareTexture::CreateImage(FTexture *tex, int translation, int flags)
{
	if (!tex->isHardwareCanvas())
	{
		FTextureBuffer texbuffer = tex->CreateTexBuffer(translation, flags | CTF_ProcessData);
		bool indexed = flags & CTF_Indexed;
		CreateTexture(texbuffer.mWidth, texbuffer.mHeight,indexed? 1 : 4, indexed? VK_FORMAT_R8_UNORM : VK_FORMAT_B8G8R8A8_UNORM, texbuffer.mBuffer, !indexed);
	}
	else
	{
		VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
		int w = tex->GetWidth();
		int h = tex->GetHeight();

		mImage.Image = ImageBuilder()
			.Format(format)
			.Size(w, h)
			.Usage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
			.DebugName("VkHardwareTexture.mImage")
			.Create(fb->device.get());

		mImage.View = ImageViewBuilder()
			.Image(mImage.Image.get(), format)
			.DebugName("VkHardwareTexture.mImageView")
			.Create(fb->device.get());

		VkImageTransition()
			.AddImage(&mImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, true)
			.Execute(fb->GetCommands()->GetTransferCommands());
	}
}

void VkHardwareTexture::CreateTexture(int w, int h, int pixelsize, VkFormat format, const void *pixels, bool mipmap)
{
	if (w <= 0 || h <= 0)
		throw CVulkanError("Trying to create zero size texture");

	int totalSize = w * h * pixelsize;

	auto stagingBuffer = BufferBuilder()
		.Size(totalSize)
		.Usage(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY)
		.DebugName("VkHardwareTexture.mStagingBuffer")
		.Create(fb->device.get());

	uint8_t *data = (uint8_t*)stagingBuffer->Map(0, totalSize);
	memcpy(data, pixels, totalSize);
	stagingBuffer->Unmap();

	mImage.Image = ImageBuilder()
		.Format(format)
		.Size(w, h, !mipmap ? 1 : GetMipLevels(w, h))
		.Usage(VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
		.DebugName("VkHardwareTexture.mImage")
		.Create(fb->device.get());

	mImage.View = ImageViewBuilder()
		.Image(mImage.Image.get(), format)
		.DebugName("VkHardwareTexture.mImageView")
		.Create(fb->device.get());

	auto cmdbuffer = fb->GetCommands()->GetTransferCommands();

	VkImageTransition()
		.AddImage(&mImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, true)
		.Execute(cmdbuffer);

	VkBufferImageCopy region = {};
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.layerCount = 1;
	region.imageExtent.depth = 1;
	region.imageExtent.width = w;
	region.imageExtent.height = h;
	cmdbuffer->copyBufferToImage(stagingBuffer->buffer, mImage.Image->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

	if (mipmap) mImage.GenerateMipmaps(cmdbuffer);

	// If we queued more than 64 MB of data already: wait until the uploads finish before continuing
	fb->GetCommands()->TransferDeleteList->Add(std::move(stagingBuffer));
	if (fb->GetCommands()->TransferDeleteList->TotalSize > 64 * 1024 * 1024)
		fb->GetCommands()->WaitForCommands(false, true);
}

int VkHardwareTexture::GetMipLevels(int w, int h)
{
	int levels = 1;
	while (w > 1 || h > 1)
	{
		w = max(w >> 1, 1);
		h = max(h >> 1, 1);
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
		VkFormat format = texelsize == 4 ? VK_FORMAT_B8G8R8A8_UNORM : VK_FORMAT_R8_UNORM;

		VkDeviceSize allocatedBytes = 0;
		mImage.Image = ImageBuilder()
			.Format(format)
			.Size(w, h)
			.LinearTiling()
			.Usage(VK_IMAGE_USAGE_SAMPLED_BIT, VMA_MEMORY_USAGE_UNKNOWN, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT)
			.MemoryType(
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
			.DebugName("VkHardwareTexture.mImage")
			.Create(fb->device.get(), &allocatedBytes);

		mTexelsize = texelsize;

		mImage.View = ImageViewBuilder()
			.Image(mImage.Image.get(), format)
			.DebugName("VkHardwareTexture.mImageView")
			.Create(fb->device.get());

		VkImageTransition()
			.AddImage(&mImage, VK_IMAGE_LAYOUT_GENERAL, true)
			.Execute(fb->GetCommands()->GetTransferCommands());

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
	// CreateTexture is used by the software renderer to create a screen output but without any screen data.
	if (buffer)
		CreateTexture(w, h, mTexelsize, mTexelsize == 4 ? VK_FORMAT_B8G8R8A8_UNORM : VK_FORMAT_R8_UNORM, buffer, mipmap);
	return 0;
}

void VkHardwareTexture::CreateWipeTexture(int w, int h, const char *name)
{
	VkFormat format = VK_FORMAT_B8G8R8A8_UNORM;

	mImage.Image = ImageBuilder()
		.Format(format)
		.Size(w, h)
		.Usage(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY)
		.DebugName(name)
		.Create(fb->device.get());

	mTexelsize = 4;

	mImage.View = ImageViewBuilder()
		.Image(mImage.Image.get(), format)
		.DebugName(name)
		.Create(fb->device.get());

	if (fb->GetBuffers()->GetWidth() > 0 && fb->GetBuffers()->GetHeight() > 0)
	{
		fb->GetPostprocess()->BlitCurrentToImage(&mImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	}
	else
	{
		// hwrenderer asked image data from a frame buffer that was never written into. Let's give it that..
		// (ideally the hwrenderer wouldn't do this, but the calling code is too complex for me to fix)

		VkImageTransition()
			.AddImage(&mImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, true)
			.Execute(fb->GetCommands()->GetTransferCommands());

		VkImageSubresourceRange range = {};
		range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		range.layerCount = 1;
		range.levelCount = 1;

		VkClearColorValue value = {};
		value.float32[0] = 0.0f;
		value.float32[1] = 0.0f;
		value.float32[2] = 0.0f;
		value.float32[3] = 1.0f;
		fb->GetCommands()->GetTransferCommands()->clearColorImage(mImage.Image->image, mImage.Layout, &value, 1, &range);

		VkImageTransition()
			.AddImage(&mImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, false)
			.Execute(fb->GetCommands()->GetTransferCommands());
	}
}

/////////////////////////////////////////////////////////////////////////////

VkMaterial::VkMaterial(VulkanRenderDevice* fb, FGameTexture* tex, int scaleflags) : FMaterial(tex, scaleflags), fb(fb)
{
	fb->GetDescriptorSetManager()->AddMaterial(this);
}

VkMaterial::~VkMaterial()
{
	if (fb)
		fb->GetDescriptorSetManager()->RemoveMaterial(this);
}

void VkMaterial::DeleteDescriptors()
{
	if (fb)
	{
		auto deleteList = fb->GetCommands()->DrawDeleteList.get();
		for (auto& it : mDescriptorSets)
		{
			deleteList->Add(std::move(it.descriptor));
		}
		mDescriptorSets.clear();
	}
}

VulkanDescriptorSet* VkMaterial::GetDescriptorSet(const FMaterialState& state)
{
	auto base = Source();
	int clampmode = state.mClampMode;
	int translation = state.mTranslation;
	auto translationp = IsLuminosityTranslation(translation)? translation : intptr_t(GPalette.GetTranslation(GetTranslationType(translation), GetTranslationIndex(translation)));

	clampmode = base->GetClampMode(clampmode);

	for (auto& set : mDescriptorSets)
	{
		if (set.descriptor && set.clampmode == clampmode && set.remap == translationp) return set.descriptor.get();
	}

	int numLayers = NumLayers();

	auto descriptor = fb->GetDescriptorSetManager()->AllocateTextureDescriptorSet(max(numLayers, SHADER_MIN_REQUIRED_TEXTURE_LAYERS));

	descriptor->SetDebugName("VkHardwareTexture.mDescriptorSets");

	VulkanSampler* sampler = fb->GetSamplerManager()->Get(clampmode);

	WriteDescriptors update;
	MaterialLayerInfo *layer;
	auto systex = static_cast<VkHardwareTexture*>(GetLayer(0, state.mTranslation, &layer));
	auto systeximage = systex->GetImage(layer->layerTexture, state.mTranslation, layer->scaleFlags);
	update.AddCombinedImageSampler(descriptor.get(), 0, systeximage->View.get(), sampler, systeximage->Layout);

	if (!(layer->scaleFlags & CTF_Indexed))
	{
		for (int i = 1; i < numLayers; i++)
		{
			auto syslayer = static_cast<VkHardwareTexture*>(GetLayer(i, 0, &layer));
			auto syslayerimage = syslayer->GetImage(layer->layerTexture, 0, layer->scaleFlags);
			update.AddCombinedImageSampler(descriptor.get(), i, syslayerimage->View.get(), sampler, syslayerimage->Layout);
		}
	}
	else
	{
		for (int i = 1; i < 3; i++)
		{
			auto syslayer = static_cast<VkHardwareTexture*>(GetLayer(i, translation, &layer));
			auto syslayerimage = syslayer->GetImage(layer->layerTexture, 0, layer->scaleFlags);
			update.AddCombinedImageSampler(descriptor.get(), i, syslayerimage->View.get(), sampler, syslayerimage->Layout);
		}
		numLayers = 3;
	}

	auto dummyImage = fb->GetTextureManager()->GetNullTextureView();
	for (int i = numLayers; i < SHADER_MIN_REQUIRED_TEXTURE_LAYERS; i++)
	{
		update.AddCombinedImageSampler(descriptor.get(), i, dummyImage, sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	}

	update.Execute(fb->device.get());
	mDescriptorSets.emplace_back(clampmode, translationp, std::move(descriptor));
	return mDescriptorSets.back().descriptor.get();
}

