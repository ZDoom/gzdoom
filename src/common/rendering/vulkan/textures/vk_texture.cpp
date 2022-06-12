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

#include "vk_texture.h"
#include "vk_hwtexture.h"
#include "vk_pptexture.h"
#include "vk_renderbuffers.h"
#include "vulkan/renderer/vk_postprocess.h"
#include "hw_cvars.h"

VkTextureManager::VkTextureManager(VulkanFrameBuffer* fb) : fb(fb)
{
	CreateNullTexture();
	CreateShadowmap();
	CreateLightmap();
}

VkTextureManager::~VkTextureManager()
{
	while (!Textures.empty())
		RemoveTexture(Textures.back());
	while (!PPTextures.empty())
		RemovePPTexture(PPTextures.back());
}

void VkTextureManager::Deinit()
{
	while (!Textures.empty())
		RemoveTexture(Textures.back());
	while (!PPTextures.empty())
		RemovePPTexture(PPTextures.back());
}

void VkTextureManager::BeginFrame()
{
	if (!Shadowmap.Image || Shadowmap.Image->width != gl_shadowmap_quality)
	{
		Shadowmap.Reset(fb);
		CreateShadowmap();
	}
}

void VkTextureManager::AddTexture(VkHardwareTexture* texture)
{
	texture->it = Textures.insert(Textures.end(), texture);
}

void VkTextureManager::RemoveTexture(VkHardwareTexture* texture)
{
	texture->Reset();
	texture->fb = nullptr;
	Textures.erase(texture->it);
}

void VkTextureManager::AddPPTexture(VkPPTexture* texture)
{
	texture->it = PPTextures.insert(PPTextures.end(), texture);
}

void VkTextureManager::RemovePPTexture(VkPPTexture* texture)
{
	texture->Reset();
	texture->fb = nullptr;
	PPTextures.erase(texture->it);
}

VkTextureImage* VkTextureManager::GetTexture(const PPTextureType& type, PPTexture* pptexture)
{
	if (type == PPTextureType::CurrentPipelineTexture || type == PPTextureType::NextPipelineTexture)
	{
		int idx = fb->GetPostprocess()->GetCurrentPipelineImage();
		if (type == PPTextureType::NextPipelineTexture)
			idx = (idx + 1) % VkRenderBuffers::NumPipelineImages;

		return &fb->GetBuffers()->PipelineImage[idx];
	}
	else if (type == PPTextureType::PPTexture)
	{
		auto vktex = GetVkTexture(pptexture);
		return &vktex->TexImage;
	}
	else if (type == PPTextureType::SceneColor)
	{
		return &fb->GetBuffers()->SceneColor;
	}
	else if (type == PPTextureType::SceneNormal)
	{
		return &fb->GetBuffers()->SceneNormal;
	}
	else if (type == PPTextureType::SceneFog)
	{
		return &fb->GetBuffers()->SceneFog;
	}
	else if (type == PPTextureType::SceneDepth)
	{
		return &fb->GetBuffers()->SceneDepthStencil;
	}
	else if (type == PPTextureType::ShadowMap)
	{
		return &Shadowmap;
	}
	else if (type == PPTextureType::SwapChain)
	{
		return nullptr;
	}
	else
	{
		I_FatalError("VkPPRenderState::GetTexture not implemented yet for this texture type");
		return nullptr;
	}
}

VkFormat VkTextureManager::GetTextureFormat(PPTexture* texture)
{
	return GetVkTexture(texture)->Format;
}

VkPPTexture* VkTextureManager::GetVkTexture(PPTexture* texture)
{
	if (!texture->Backend)
		texture->Backend = std::make_unique<VkPPTexture>(fb, texture);
	return static_cast<VkPPTexture*>(texture->Backend.get());
}

void VkTextureManager::CreateNullTexture()
{
	ImageBuilder imgbuilder;
	imgbuilder.setFormat(VK_FORMAT_R8G8B8A8_UNORM);
	imgbuilder.setSize(1, 1);
	imgbuilder.setUsage(VK_IMAGE_USAGE_SAMPLED_BIT);
	NullTexture = imgbuilder.create(fb->device);
	NullTexture->SetDebugName("VkDescriptorSetManager.NullTexture");

	ImageViewBuilder viewbuilder;
	viewbuilder.setImage(NullTexture.get(), VK_FORMAT_R8G8B8A8_UNORM);
	NullTextureView = viewbuilder.create(fb->device);
	NullTextureView->SetDebugName("VkDescriptorSetManager.NullTextureView");

	PipelineBarrier barrier;
	barrier.addImage(NullTexture.get(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
	barrier.execute(fb->GetCommands()->GetTransferCommands(), VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
}

void VkTextureManager::CreateShadowmap()
{
	ImageBuilder builder;
	builder.setSize(gl_shadowmap_quality, 1024);
	builder.setFormat(VK_FORMAT_R32_SFLOAT);
	builder.setUsage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
	Shadowmap.Image = builder.create(fb->device);
	Shadowmap.Image->SetDebugName("VkRenderBuffers.Shadowmap");

	ImageViewBuilder viewbuilder;
	viewbuilder.setImage(Shadowmap.Image.get(), VK_FORMAT_R32_SFLOAT);
	Shadowmap.View = viewbuilder.create(fb->device);
	Shadowmap.View->SetDebugName("VkRenderBuffers.ShadowmapView");

	VkImageTransition barrier;
	barrier.addImage(&Shadowmap, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, true);
	barrier.execute(fb->GetCommands()->GetDrawCommands());
}

void VkTextureManager::CreateLightmap()
{
	ImageBuilder builder;
	builder.setSize(1, 1);
	builder.setFormat(VK_FORMAT_R16G16B16A16_SFLOAT);
	builder.setUsage(VK_IMAGE_USAGE_SAMPLED_BIT);
	Lightmap.Image = builder.create(fb->device);
	Lightmap.Image->SetDebugName("VkRenderBuffers.Lightmap");

	ImageViewBuilder viewbuilder;
	viewbuilder.setType(VK_IMAGE_VIEW_TYPE_2D_ARRAY);
	viewbuilder.setImage(Lightmap.Image.get(), VK_FORMAT_R16G16B16A16_SFLOAT);
	Lightmap.View = viewbuilder.create(fb->device);
	Lightmap.View->SetDebugName("VkRenderBuffers.LightmapView");

	VkImageTransition barrier;
	barrier.addImage(&Lightmap, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, true);
	barrier.execute(fb->GetCommands()->GetDrawCommands());
}

void VkTextureManager::SetLightmap(int LMTextureSize, int LMTextureCount, const TArray<uint16_t>& LMTextureData)
{
	int w = LMTextureSize;
	int h = LMTextureSize;
	int count = LMTextureCount;
	int pixelsize = 8;

	Lightmap.Reset(fb);

	ImageBuilder builder;
	builder.setSize(w, h, 1, count);
	builder.setFormat(VK_FORMAT_R16G16B16A16_SFLOAT);
	builder.setUsage(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
	Lightmap.Image = builder.create(fb->device);
	Lightmap.Image->SetDebugName("VkRenderBuffers.Lightmap");

	ImageViewBuilder viewbuilder;
	viewbuilder.setType(VK_IMAGE_VIEW_TYPE_2D_ARRAY);
	viewbuilder.setImage(Lightmap.Image.get(), VK_FORMAT_R16G16B16A16_SFLOAT);
	Lightmap.View = viewbuilder.create(fb->device);
	Lightmap.View->SetDebugName("VkRenderBuffers.LightmapView");

	auto cmdbuffer = fb->GetCommands()->GetTransferCommands();

	int totalSize = w * h * count * pixelsize;

	BufferBuilder bufbuilder;
	bufbuilder.setSize(totalSize);
	bufbuilder.setUsage(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
	std::unique_ptr<VulkanBuffer> stagingBuffer = bufbuilder.create(fb->device);
	stagingBuffer->SetDebugName("VkHardwareTexture.mStagingBuffer");

	uint16_t one = 0x3c00; // half-float 1.0
	const uint16_t* src = LMTextureData.Data();
	uint16_t* data = (uint16_t*)stagingBuffer->Map(0, totalSize);
	for (int i = w * h * count; i > 0; i--)
	{
		*(data++) = *(src++);
		*(data++) = *(src++);
		*(data++) = *(src++);
		*(data++) = one;
	}
	stagingBuffer->Unmap();

	VkImageTransition imageTransition;
	imageTransition.addImage(&Lightmap, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, true, 0, count);
	imageTransition.execute(cmdbuffer);

	VkBufferImageCopy region = {};
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.layerCount = count;
	region.imageExtent.depth = 1;
	region.imageExtent.width = w;
	region.imageExtent.height = h;
	cmdbuffer->copyBufferToImage(stagingBuffer->buffer, Lightmap.Image->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

	VkImageTransition barrier;
	barrier.addImage(&Lightmap, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, false, 0, count);
	barrier.execute(cmdbuffer);

	fb->GetCommands()->TransferDeleteList->Add(std::move(stagingBuffer));
}
