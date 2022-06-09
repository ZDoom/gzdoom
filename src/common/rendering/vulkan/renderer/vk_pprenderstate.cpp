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

#include "vk_pprenderstate.h"
#include "vk_postprocess.h"
#include "vulkan/system/vk_framebuffer.h"
#include "vulkan/system/vk_commandbuffer.h"
#include "vulkan/system/vk_swapchain.h"
#include "vulkan/shaders/vk_ppshader.h"
#include "vulkan/textures/vk_pptexture.h"
#include "vulkan/textures/vk_renderbuffers.h"
#include "vulkan/textures/vk_samplers.h"
#include "vulkan/renderer/vk_renderstate.h"
#include "flatvertices.h"

VkPPRenderState::VkPPRenderState(VulkanFrameBuffer* fb) : fb(fb)
{
}

void VkPPRenderState::PushGroup(const FString &name)
{
	fb->GetCommands()->PushGroup(name);
}

void VkPPRenderState::PopGroup()
{
	fb->GetCommands()->PopGroup();
}

void VkPPRenderState::Draw()
{
	auto pp = fb->GetPostprocess();

	fb->GetRenderState()->EndRenderPass();

	VkPPRenderPassKey key;
	key.BlendMode = BlendMode;
	key.InputTextures = Textures.Size();
	key.Uniforms = Uniforms.Data.Size();
	key.Shader = GetVkShader(Shader);
	key.SwapChain = (Output.Type == PPTextureType::SwapChain);
	key.ShadowMapBuffers = ShadowMapBuffers;
	if (Output.Type == PPTextureType::PPTexture)
		key.OutputFormat = GetVkTexture(Output.Texture)->Format;
	else if (Output.Type == PPTextureType::SwapChain)
		key.OutputFormat = fb->GetCommands()->swapChain->swapChainFormat.format;
	else if (Output.Type == PPTextureType::ShadowMap)
		key.OutputFormat = VK_FORMAT_R32_SFLOAT;
	else
		key.OutputFormat = VK_FORMAT_R16G16B16A16_SFLOAT;

	if (Output.Type == PPTextureType::SceneColor)
	{
		key.StencilTest = 1;
		key.Samples = fb->GetBuffers()->GetSceneSamples();
	}
	else
	{
		key.StencilTest = 0;
		key.Samples = VK_SAMPLE_COUNT_1_BIT;
	}

	auto passSetup = fb->GetRenderPassManager()->GetPPRenderPass(key);

	int framebufferWidth = 0, framebufferHeight = 0;
	VulkanDescriptorSet *input = GetInput(passSetup, Textures, ShadowMapBuffers);
	VulkanFramebuffer *output = GetOutput(passSetup, Output, key.StencilTest, framebufferWidth, framebufferHeight);

	RenderScreenQuad(passSetup, input, output, framebufferWidth, framebufferHeight, Viewport.left, Viewport.top, Viewport.width, Viewport.height, Uniforms.Data.Data(), Uniforms.Data.Size(), key.StencilTest);

	// Advance to next PP texture if our output was sent there
	if (Output.Type == PPTextureType::NextPipelineTexture)
	{
		pp->mCurrentPipelineImage = (pp->mCurrentPipelineImage + 1) % VkRenderBuffers::NumPipelineImages;
	}
}

void VkPPRenderState::RenderScreenQuad(VkPPRenderPassSetup *passSetup, VulkanDescriptorSet *descriptorSet, VulkanFramebuffer *framebuffer, int framebufferWidth, int framebufferHeight, int x, int y, int width, int height, const void *pushConstants, uint32_t pushConstantsSize, bool stencilTest)
{
	auto cmdbuffer = fb->GetCommands()->GetDrawCommands();

	VkViewport viewport = { };
	viewport.x = (float)x;
	viewport.y = (float)y;
	viewport.width = (float)width;
	viewport.height = (float)height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor = { };
	scissor.offset.x = 0;
	scissor.offset.y = 0;
	scissor.extent.width = framebufferWidth;
	scissor.extent.height = framebufferHeight;

	RenderPassBegin beginInfo;
	beginInfo.setRenderPass(passSetup->RenderPass.get());
	beginInfo.setRenderArea(0, 0, framebufferWidth, framebufferHeight);
	beginInfo.setFramebuffer(framebuffer);
	beginInfo.addClearColor(0.0f, 0.0f, 0.0f, 1.0f);

	VkBuffer vertexBuffers[] = { static_cast<VKVertexBuffer*>(screen->mVertexData->GetBufferObjects().first)->mBuffer->buffer };
	VkDeviceSize offsets[] = { 0 };

	cmdbuffer->beginRenderPass(beginInfo);
	cmdbuffer->bindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, passSetup->Pipeline.get());
	cmdbuffer->bindDescriptorSet(VK_PIPELINE_BIND_POINT_GRAPHICS, passSetup->PipelineLayout.get(), 0, descriptorSet);
	cmdbuffer->bindVertexBuffers(0, 1, vertexBuffers, offsets);
	cmdbuffer->setViewport(0, 1, &viewport);
	cmdbuffer->setScissor(0, 1, &scissor);
	if (stencilTest)
		cmdbuffer->setStencilReference(VK_STENCIL_FRONT_AND_BACK, screen->stencilValue);
	if (pushConstantsSize > 0)
		cmdbuffer->pushConstants(passSetup->PipelineLayout.get(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, pushConstantsSize, pushConstants);
	cmdbuffer->draw(3, 1, FFlatVertexBuffer::PRESENT_INDEX, 0);
	cmdbuffer->endRenderPass();
}

VulkanDescriptorSet *VkPPRenderState::GetInput(VkPPRenderPassSetup *passSetup, const TArray<PPTextureInput> &textures, bool bindShadowMapBuffers)
{
	auto descriptors = fb->GetPostprocess()->AllocateDescriptorSet(passSetup->DescriptorLayout.get());
	descriptors->SetDebugName("VkPostprocess.descriptors");

	WriteDescriptors write;
	VkImageTransition imageTransition;

	for (unsigned int index = 0; index < textures.Size(); index++)
	{
		const PPTextureInput &input = textures[index];
		VulkanSampler *sampler = fb->GetSamplerManager()->Get(input.Filter, input.Wrap);
		VkTextureImage *tex = GetTexture(input.Type, input.Texture);

		write.addCombinedImageSampler(descriptors.get(), index, tex->DepthOnlyView ? tex->DepthOnlyView.get() : tex->View.get(), sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		imageTransition.addImage(tex, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, false);
	}

	if (bindShadowMapBuffers)
	{
		write.addBuffer(descriptors.get(), LIGHTNODES_BINDINGPOINT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, fb->LightNodes->mBuffer.get());
		write.addBuffer(descriptors.get(), LIGHTLINES_BINDINGPOINT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, fb->LightLines->mBuffer.get());
		write.addBuffer(descriptors.get(), LIGHTLIST_BINDINGPOINT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, fb->LightList->mBuffer.get());
	}

	write.updateSets(fb->device);
	imageTransition.execute(fb->GetCommands()->GetDrawCommands());

	VulkanDescriptorSet *set = descriptors.get();
	fb->GetCommands()->FrameDeleteList.Descriptors.push_back(std::move(descriptors));
	return set;
}

VulkanFramebuffer *VkPPRenderState::GetOutput(VkPPRenderPassSetup *passSetup, const PPOutput &output, bool stencilTest, int &framebufferWidth, int &framebufferHeight)
{
	VkTextureImage *tex = GetTexture(output.Type, output.Texture);

	VkImageView view;
	std::unique_ptr<VulkanFramebuffer> *framebufferptr = nullptr;
	int w, h;
	if (tex)
	{
		VkImageTransition imageTransition;
		imageTransition.addImage(tex, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, output.Type == PPTextureType::NextPipelineTexture);
		if (stencilTest)
			imageTransition.addImage(&fb->GetBuffers()->SceneDepthStencil, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, false);
		imageTransition.execute(fb->GetCommands()->GetDrawCommands());

		view = tex->View->view;
		w = tex->Image->width;
		h = tex->Image->height;
		framebufferptr = &tex->PPFramebuffer;
	}
	else
	{
		view = fb->GetCommands()->swapChain->swapChainImageViews[fb->GetCommands()->presentImageIndex];
		framebufferptr = &fb->GetCommands()->swapChain->framebuffers[fb->GetCommands()->presentImageIndex];
		w = fb->GetCommands()->swapChain->actualExtent.width;
		h = fb->GetCommands()->swapChain->actualExtent.height;
	}

	auto &framebuffer = *framebufferptr;
	if (!framebuffer)
	{
		FramebufferBuilder builder;
		builder.setRenderPass(passSetup->RenderPass.get());
		builder.setSize(w, h);
		builder.addAttachment(view);
		if (stencilTest)
			builder.addAttachment(fb->GetBuffers()->SceneDepthStencil.View.get());
		framebuffer = builder.create(fb->device);
	}

	framebufferWidth = w;
	framebufferHeight = h;
	return framebuffer.get();
}

VkTextureImage *VkPPRenderState::GetTexture(const PPTextureType &type, PPTexture *pptexture)
{
	if (type == PPTextureType::CurrentPipelineTexture || type == PPTextureType::NextPipelineTexture)
	{
		int idx = fb->GetPostprocess()->mCurrentPipelineImage;
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

VkPPShader *VkPPRenderState::GetVkShader(PPShader *shader)
{
	if (!shader->Backend)
		shader->Backend = std::make_unique<VkPPShader>(fb, shader);
	return static_cast<VkPPShader*>(shader->Backend.get());
}

VkPPTexture *VkPPRenderState::GetVkTexture(PPTexture *texture)
{
	if (!texture->Backend)
		texture->Backend = std::make_unique<VkPPTexture>(fb, texture);
	return static_cast<VkPPTexture*>(texture->Backend.get());
}
