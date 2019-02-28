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
#include "vk_hwtexture.h"

VkHardwareTexture::VkHardwareTexture()
{
}

VkHardwareTexture::~VkHardwareTexture()
{
}

void VkHardwareTexture::Reset()
{
	mDescriptorSet.reset();
	mImage.reset();
	mImageView.reset();
	mStagingBuffer.reset();
}

VulkanDescriptorSet *VkHardwareTexture::GetDescriptorSet(const FMaterialState &state)
{
	if (!mImage)
	{
		FTexture *tex = state.mMaterial->tex;

		if (!tex->isHardwareCanvas())
		{
			int clampmode = state.mClampMode;
			//if (tex->UseType == ETextureType::SWCanvas) clampmode = CLAMP_NOFILTER;
			//if (tex->isHardwareCanvas()) clampmode = CLAMP_CAMTEX;
			//else if ((tex->isWarped() || tex->shaderindex >= FIRST_USER_SHADER) && clampmode <= CLAMP_XY) clampmode = CLAMP_NONE;

			int translation = state.mTranslation;
			if (translation <= 0)
			{
				translation = -translation;
			}
			else
			{
				auto remap = TranslationToTable(translation);
				translation = remap == nullptr ? 0 : remap->GetUniqueIndex();
			}

			bool needmipmap = (clampmode <= CLAMP_XY);

			// Textures that are already scaled in the texture lump will not get replaced by hires textures.
			int flags = state.mMaterial->isExpanded() ? CTF_Expand : (gl_texture_usehires && !tex->isScaled() && clampmode <= CLAMP_XY) ? CTF_CheckHires : 0;

			FTextureBuffer texbuffer = tex->CreateTexBuffer(translation, flags | CTF_ProcessData);
			CreateTexture(texbuffer.mWidth, texbuffer.mHeight, 4, VK_FORMAT_B8G8R8A8_UNORM, texbuffer.mBuffer);
		}
		else
		{
			static const uint32_t testpixels[4 * 4] =
			{
				0xff0000ff, 0xff0000ff, 0xffff00ff, 0xffff00ff,
				0xff0000ff, 0xff0000ff, 0xffff00ff, 0xffff00ff,
				0xff00ff00, 0xff00ff00, 0x0000ffff, 0xff00ffff,
				0xff00ff00, 0xff00ff00, 0x0000ffff, 0xff00ffff,
			};
			CreateTexture(4, 4, 4, VK_FORMAT_R8G8B8A8_UNORM, testpixels);
		}
	}

	if (!mDescriptorSet)
	{
		auto fb = GetVulkanFrameBuffer();
		mDescriptorSet = fb->GetRenderPassManager()->DescriptorPool->allocate(fb->GetRenderPassManager()->TextureSetLayout.get());

		WriteDescriptors update;
		update.addCombinedImageSampler(mDescriptorSet.get(), 0, mImageView.get(), fb->GetSamplerManager()->Get(0), mImageLayout);
		update.updateSets(fb->device);
	}

	return mDescriptorSet.get();
}

void VkHardwareTexture::CreateTexture(int w, int h, int pixelsize, VkFormat format, const void *pixels)
{
	auto fb = GetVulkanFrameBuffer();

	int totalSize = w * h * pixelsize;

	BufferBuilder bufbuilder;
	bufbuilder.setSize(totalSize);
	bufbuilder.setUsage(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
	mStagingBuffer = bufbuilder.create(fb->device);

	uint8_t *data = (uint8_t*)mStagingBuffer->Map(0, totalSize);
	memcpy(data, pixels, totalSize);
	mStagingBuffer->Unmap();

	ImageBuilder imgbuilder;
	imgbuilder.setFormat(format);
	imgbuilder.setSize(w, h, GetMipLevels(w, h));
	imgbuilder.setUsage(VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
	mImage = imgbuilder.create(fb->device);

	ImageViewBuilder viewbuilder;
	viewbuilder.setImage(mImage.get(), format);
	mImageView = viewbuilder.create(fb->device);

	auto cmdbuffer = fb->GetUploadCommands();

	PipelineBarrier imageTransition0;
	imageTransition0.addImage(mImage.get(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, VK_ACCESS_TRANSFER_WRITE_BIT);
	imageTransition0.execute(cmdbuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

	VkBufferImageCopy region = {};
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.layerCount = 1;
	region.imageExtent.depth = 1;
	region.imageExtent.width = w;
	region.imageExtent.height = h;
	cmdbuffer->copyBufferToImage(mStagingBuffer->buffer, mImage->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

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
	barrier2.addImage(image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_ASPECT_COLOR_BIT, i - 1);
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
	if (!mImage)
	{
		auto fb = GetVulkanFrameBuffer();

		VkFormat format = texelsize == 4 ? VK_FORMAT_B8G8R8A8_UNORM : VK_FORMAT_R8_UNORM;

		ImageBuilder imgbuilder;
		imgbuilder.setFormat(format);
		imgbuilder.setSize(w, h);
		imgbuilder.setUsage(VK_IMAGE_USAGE_SAMPLED_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT);
		imgbuilder.setLinearTiling();
		mImage = imgbuilder.create(fb->device);
		mImageLayout = VK_IMAGE_LAYOUT_GENERAL;
		mTexelsize = texelsize;

		ImageViewBuilder viewbuilder;
		viewbuilder.setImage(mImage.get(), format);
		mImageView = viewbuilder.create(fb->device);

		auto cmdbuffer = fb->GetUploadCommands();

		PipelineBarrier imageTransition;
		imageTransition.addImage(mImage.get(), VK_IMAGE_LAYOUT_UNDEFINED, mImageLayout, 0, VK_ACCESS_SHADER_READ_BIT);
		imageTransition.execute(cmdbuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
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

#if 0

//===========================================================================
// 
//	Creates the low level texture object
//
//===========================================================================

VkResult VkHardwareTexture::CreateTexture(unsigned char * buffer, int w, int h, bool mipmap, int translation)
{
	int rh,rw;
	bool deletebuffer=false;
	auto tTex = GetTexID(translation);

	// We cannot determine if the old texture is still needed, so if something is trying to recreate a still existing texture this must fail.
	// Normally it should never occur that a texture needs to be recreated in the middle of a frame.
	if (tTex->vkTexture != nullptr) return VK_ERROR_INITIALIZATION_FAILED;

	tTex->vkTexture = new VkTexture(vDevice);;

	rw = vDevice->GetTexDimension(w);
	rh = vDevice->GetTexDimension(h);
	if (rw < w || rh < h)
	{
		// The texture is larger than what the hardware can handle so scale it down.
		unsigned char * scaledbuffer=(unsigned char *)calloc(4,rw * (rh+1));
		if (scaledbuffer)
		{
			ResizeTexture(w, h, rw, rh, buffer, scaledbuffer);
			deletebuffer=true;
			buffer=scaledbuffer;
		}
	}

	auto res = tTex->vkTexture->Create(buffer, w, h, mipmap, vkTextureBytes);
	if (res != VK_SUCCESS)
	{
		delete tTex->vkTexture;
		tTex->vkTexture = nullptr;
	}
	return res;
}

//===========================================================================
// 
//	Creates a texture
//
//===========================================================================

VkHardwareTexture::VkHardwareTexture(VulkanDevice *dev, bool nocompression)
{
	forcenocompression = nocompression;

	vkDefTex.vkTexture = nullptr;
	vkDefTex.translation = 0;
	vkDefTex.mipmapped = false;
	vDevice = dev;
}


//===========================================================================
// 
//	Deletes a texture id and unbinds it from the texture units
//
//===========================================================================

void VkHardwareTexture::TranslatedTexture::Delete()
{
	if (vkTexture != nullptr) 
	{
		delete vkTexture;
		vkTexture = nullptr;
		mipmapped = false;
	}
}

//===========================================================================
// 
//	Frees all associated resources
//
//===========================================================================

void VkHardwareTexture::Clean(bool all)
{
	int cm_arraysize = CM_FIRSTSPECIALCOLORMAP + SpecialColormaps.Size();

	if (all)
	{
		vkDefTex.Delete();
	}
	for(unsigned int i=0;i<vkTex_Translated.Size();i++)
	{
		vkTex_Translated[i].Delete();
	}
	vkTex_Translated.Clear();
}

//===========================================================================
// 
// Deletes all allocated resources and considers translations
// This will only be called for sprites
//
//===========================================================================

void VkHardwareTexture::CleanUnused(SpriteHits &usedtranslations)
{
	if (usedtranslations.CheckKey(0) == nullptr)
	{
		vkDefTex.Delete();
	}
	for (int i = vkTex_Translated.Size()-1; i>= 0; i--)
	{
		if (usedtranslations.CheckKey(vkTex_Translated[i].translation) == nullptr)
		{
			vkTex_Translated[i].Delete();
			vkTex_Translated.Delete(i);
		}
	}
}

//===========================================================================
// 
//	Destroys the texture
//
//===========================================================================
VkHardwareTexture::~VkHardwareTexture() 
{ 
	Clean(true); 
}


//===========================================================================
// 
//	Gets a texture ID address and validates all required data
//
//===========================================================================

VkHardwareTexture::TranslatedTexture *VkHardwareTexture::GetTexID(int translation)
{
	if (translation == 0)
	{
		return &vkDefTex;
	}

	// normally there aren't more than very few different 
	// translations here so this isn't performance critical.
	// Maps only start to pay off for larger amounts of elements.
	for (auto &tex : vkTex_Translated)
	{
		if (tex.translation == translation)
		{
			return &tex;
		}
	}

	int add = vkTex_Translated.Reserve(1);
	vkTex_Translated[add].translation = translation;
	vkTex_Translated[add].vkTexture = nullptr;
	vkTex_Translated[add].mipmapped = false;
	return &vkTex_Translated[add];
}

//===========================================================================
// 
//
//
//===========================================================================

VkTexture *VkHardwareTexture::GetVkTexture(FTexture *tex, int translation, bool needmipmap, int flags)
{
	int usebright = false;

	if (translation <= 0)
	{
		translation = -translation;
	}
	else
	{
		auto remap = TranslationToTable(translation);
		translation = remap == nullptr ? 0 : remap->GetUniqueIndex();
	}

	TranslatedTexture *pTex = GetTexID(translation);

	if (pTex->vkTexture == nullptr)
	{
		int w, h;
		auto buffer = tex->CreateTexBuffer(translation, w, h, flags | CTF_ProcessData);
		auto res = CreateTexture(buffer, w, h, needmipmap, translation);
		delete[] buffer;
	}
	return pTex->vkTexture;
}

#endif
