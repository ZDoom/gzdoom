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

VkTextureManager::VkTextureManager(VulkanRenderDevice* fb) : fb(fb)
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
	NullTexture = ImageBuilder()
		.Format(VK_FORMAT_R8G8B8A8_UNORM)
		.Size(1, 1)
		.Usage(VK_IMAGE_USAGE_SAMPLED_BIT)
		.DebugName("VkDescriptorSetManager.NullTexture")
		.Create(fb->device.get());

	NullTextureView = ImageViewBuilder()
		.Image(NullTexture.get(), VK_FORMAT_R8G8B8A8_UNORM)
		.DebugName("VkDescriptorSetManager.NullTextureView")
		.Create(fb->device.get());

	PipelineBarrier()
		.AddImage(NullTexture.get(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_ASPECT_COLOR_BIT)
		.Execute(fb->GetCommands()->GetTransferCommands(), VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
}

void VkTextureManager::CreateShadowmap()
{
	Shadowmap.Image = ImageBuilder()
		.Size(gl_shadowmap_quality, 1024)
		.Format(VK_FORMAT_R32_SFLOAT)
		.Usage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
		.DebugName("VkRenderBuffers.Shadowmap")
		.Create(fb->device.get());

	Shadowmap.View = ImageViewBuilder()
		.Image(Shadowmap.Image.get(), VK_FORMAT_R32_SFLOAT)
		.DebugName("VkRenderBuffers.ShadowmapView")
		.Create(fb->device.get());

	VkImageTransition()
		.AddImage(&Shadowmap, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, true)
		.Execute(fb->GetCommands()->GetDrawCommands());
}

void VkTextureManager::CreateLightmap()
{
	TArray<uint16_t> data;
	data.Push(0);
	data.Push(0);
	data.Push(0);
	data.Push(0x3c00); // half-float 1.0
	SetLightmap(1, 1, data);
}

void VkTextureManager::SetLightmap(int LMTextureSize, int LMTextureCount, const TArray<uint16_t>& LMTextureData)
{
	int w = LMTextureSize;
	int h = LMTextureSize;
	int count = LMTextureCount;
	int pixelsize = 8;

	Lightmap.Reset(fb);

	Lightmap.Image = ImageBuilder()
		.Size(w, h, 1, count)
		.Format(VK_FORMAT_R16G16B16A16_SFLOAT)
		.Usage(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT)
		.DebugName("VkRenderBuffers.Lightmap")
		.Create(fb->device.get());

	Lightmap.View = ImageViewBuilder()
		.Type(VK_IMAGE_VIEW_TYPE_2D_ARRAY)
		.Image(Lightmap.Image.get(), VK_FORMAT_R16G16B16A16_SFLOAT)
		.DebugName("VkRenderBuffers.LightmapView")
		.Create(fb->device.get());

	auto cmdbuffer = fb->GetCommands()->GetTransferCommands();

	int totalSize = w * h * count * pixelsize;

	auto stagingBuffer = BufferBuilder()
		.Size(totalSize)
		.Usage(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY)
		.DebugName("VkHardwareTexture.mStagingBuffer")
		.Create(fb->device.get());

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

	VkImageTransition()
		.AddImage(&Lightmap, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, true, 0, count)
		.Execute(cmdbuffer);

	VkBufferImageCopy region = {};
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.layerCount = count;
	region.imageExtent.depth = 1;
	region.imageExtent.width = w;
	region.imageExtent.height = h;
	cmdbuffer->copyBufferToImage(stagingBuffer->buffer, Lightmap.Image->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

	VkImageTransition()
		.AddImage(&Lightmap, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, false, 0, count)
		.Execute(cmdbuffer);

	fb->GetCommands()->TransferDeleteList->Add(std::move(stagingBuffer));
}
