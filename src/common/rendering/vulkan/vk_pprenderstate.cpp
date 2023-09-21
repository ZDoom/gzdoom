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
#include "vulkan/vk_renderdevice.h"
#include "vulkan/vk_renderstate.h"
#include "vulkan/commands/vk_commandbuffer.h"
#include "vulkan/buffers/vk_buffer.h"
#include "vulkan/shaders/vk_ppshader.h"
#include "vulkan/textures/vk_pptexture.h"
#include "vulkan/textures/vk_renderbuffers.h"
#include "vulkan/textures/vk_texture.h"
#include "vulkan/samplers/vk_samplers.h"
#include "vulkan/framebuffers/vk_framebuffer.h"
#include "vulkan/descriptorsets/vk_descriptorset.h"
#include "vulkan/pipelines/vk_pprenderpass.h"
#include <zvulkan/vulkanswapchain.h>
#include "flatvertices.h"

VkPPRenderState::VkPPRenderState(VulkanRenderDevice* fb) : fb(fb)
{
}

void VkPPRenderState::PushGroup(const FString &name)
{
	fb->GetCommands()->PushGroup(fb->GetCommands()->GetDrawCommands(), name);
}

void VkPPRenderState::PopGroup()
{
	fb->GetCommands()->PopGroup(fb->GetCommands()->GetDrawCommands());
}

void VkPPRenderState::Draw()
{
	fb->GetRenderState(0)->EndRenderPass();

	VkPPRenderPassKey key;
	key.BlendMode = BlendMode;
	key.InputTextures = Textures.Size();
	key.Uniforms = Uniforms.Data.Size();
	key.Shader = fb->GetShaderManager()->GetVkShader(Shader);
	key.SwapChain = (Output.Type == PPTextureType::SwapChain);
	key.ShadowMapBuffers = ShadowMapBuffers;
	if (Output.Type == PPTextureType::PPTexture)
		key.OutputFormat = fb->GetTextureManager()->GetTextureFormat(Output.Texture);
	else if (Output.Type == PPTextureType::SwapChain)
		key.OutputFormat = fb->GetFramebufferManager()->SwapChain->Format().format;
	else if (Output.Type == PPTextureType::ShadowMap)
		key.OutputFormat = VK_FORMAT_R32_SFLOAT;
	else
		key.OutputFormat = VK_FORMAT_R16G16B16A16_SFLOAT;

	if (Output.Type == PPTextureType::SceneColor)
	{
		key.StencilTest = WhichDepthStencil::Scene;
		key.Samples = fb->GetBuffers()->GetSceneSamples();
	}
	else
	{
		key.StencilTest = WhichDepthStencil::None;
		key.Samples = VK_SAMPLE_COUNT_1_BIT;
	}

	auto passSetup = fb->GetRenderPassManager()->GetPPRenderPass(key);

	int framebufferWidth = 0, framebufferHeight = 0;
	VulkanDescriptorSet *input = fb->GetDescriptorSetManager()->GetInput(passSetup, Textures, ShadowMapBuffers);
	VulkanFramebuffer *output = fb->GetBuffers()->GetOutput(passSetup, Output, key.StencilTest, framebufferWidth, framebufferHeight);

	RenderScreenQuad(passSetup, input, output, framebufferWidth, framebufferHeight, Viewport.left, Viewport.top, Viewport.width, Viewport.height, Uniforms.Data.Data(), Uniforms.Data.Size(), key.StencilTest == WhichDepthStencil::Scene);

	// Advance to next PP texture if our output was sent there
	if (Output.Type == PPTextureType::NextPipelineTexture)
	{
		auto pp = fb->GetPostprocess();
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

	RenderPassBegin()
		.RenderPass(passSetup->RenderPass.get())
		.RenderArea(0, 0, framebufferWidth, framebufferHeight)
		.Framebuffer(framebuffer)
		.AddClearColor(0.0f, 0.0f, 0.0f, 1.0f)
		.Execute(cmdbuffer);

	cmdbuffer->bindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, passSetup->Pipeline.get());
	cmdbuffer->bindDescriptorSet(VK_PIPELINE_BIND_POINT_GRAPHICS, passSetup->PipelineLayout.get(), 0, descriptorSet);
	cmdbuffer->setViewport(0, 1, &viewport);
	cmdbuffer->setScissor(0, 1, &scissor);
	if (stencilTest)
		cmdbuffer->setStencilReference(VK_STENCIL_FRONT_AND_BACK, screen->stencilValue);
	if (pushConstantsSize > 0)
		cmdbuffer->pushConstants(passSetup->PipelineLayout.get(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, pushConstantsSize, pushConstants);
	cmdbuffer->draw(3, 1, 0, 0);
	cmdbuffer->endRenderPass();
}
