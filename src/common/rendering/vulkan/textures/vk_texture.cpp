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

VkTextureManager::VkTextureManager(VulkanFrameBuffer* fb) : fb(fb)
{
}

VkTextureManager::~VkTextureManager()
{
	while (!Textures.empty())
		RemoveTexture(Textures.back());
	while (!PPTextures.empty())
		RemovePPTexture(PPTextures.back());
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
		return &fb->GetBuffers()->Shadowmap;
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
