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

#include "vk_pprenderpass.h"
#include "vulkan/vk_renderstate.h"
#include "vulkan/vk_renderdevice.h"
#include "vulkan/shaders/vk_ppshader.h"
#include "vulkan/textures/vk_renderbuffers.h"
#include <zvulkan/vulkanbuilders.h>

VkPPRenderPassSetup::VkPPRenderPassSetup(VulkanRenderDevice* fb, const VkPPRenderPassKey& key) : fb(fb)
{
	CreateDescriptorLayout(key);
	CreatePipelineLayout(key);
	CreateRenderPass(key);
	CreatePipeline(key);
}

void VkPPRenderPassSetup::CreateDescriptorLayout(const VkPPRenderPassKey& key)
{
	DescriptorSetLayoutBuilder builder;
	for (int i = 0; i < key.InputTextures; i++)
		builder.AddBinding(i, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
	if (key.ShadowMapBuffers)
	{
		builder.AddBinding(4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
		builder.AddBinding(5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
		builder.AddBinding(6, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
	}
	builder.DebugName("VkPPRenderPassSetup.DescriptorLayout");
	DescriptorLayout = builder.Create(fb->GetDevice());
}

void VkPPRenderPassSetup::CreatePipelineLayout(const VkPPRenderPassKey& key)
{
	PipelineLayoutBuilder builder;
	builder.AddSetLayout(DescriptorLayout.get());
	if (key.Uniforms > 0)
		builder.AddPushConstantRange(VK_SHADER_STAGE_FRAGMENT_BIT, 0, key.Uniforms);
	builder.DebugName("VkPPRenderPassSetup.PipelineLayout");
	PipelineLayout = builder.Create(fb->GetDevice());
}

void VkPPRenderPassSetup::CreatePipeline(const VkPPRenderPassKey& key)
{
	GraphicsPipelineBuilder builder;
	builder.Cache(fb->GetRenderPassManager()->GetCache());
	builder.AddVertexShader(key.Shader->VertexShader.get());
	builder.AddFragmentShader(key.Shader->FragmentShader.get());

	builder.AddDynamicState(VK_DYNAMIC_STATE_VIEWPORT);
	builder.AddDynamicState(VK_DYNAMIC_STATE_SCISSOR);
	// Note: the actual values are ignored since we use dynamic viewport+scissor states
	builder.Viewport(0.0f, 0.0f, 320.0f, 200.0f);
	builder.Scissor(0, 0, 320, 200);
	if (key.StencilTest != WhichDepthStencil::None)
	{
		builder.AddDynamicState(VK_DYNAMIC_STATE_STENCIL_REFERENCE);
		builder.DepthStencilEnable(false, false, true);
		builder.Stencil(VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_EQUAL, 0xffffffff, 0xffffffff, 0);
	}
	builder.Topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP);
	BlendMode(builder, key.BlendMode);
	builder.RasterizationSamples(key.Samples);
	builder.Layout(PipelineLayout.get());
	builder.RenderPass(RenderPass.get());
	builder.DebugName("VkPPRenderPassSetup.Pipeline");
	Pipeline = builder.Create(fb->GetDevice());
}

void VkPPRenderPassSetup::CreateRenderPass(const VkPPRenderPassKey& key)
{
	RenderPassBuilder builder;
	if (key.SwapChain)
		builder.AddAttachment(key.OutputFormat, key.Samples, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
	else
		builder.AddAttachment(key.OutputFormat, key.Samples, VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	if (key.StencilTest == WhichDepthStencil::Scene)
	{
		builder.AddDepthStencilAttachment(
			fb->GetBuffers()->SceneDepthStencilFormat, key.Samples,
			VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE,
			VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
	}
	if (key.StencilTest == WhichDepthStencil::Pipeline)
	{
		builder.AddDepthStencilAttachment(
			fb->GetBuffers()->PipelineDepthStencilFormat, key.Samples,
			VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE,
			VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
	}

	builder.AddSubpass();
	builder.AddSubpassColorAttachmentRef(0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	if (key.StencilTest != WhichDepthStencil::None)
	{
		builder.AddSubpassDepthStencilAttachmentRef(1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
		builder.AddExternalSubpassDependency(
			VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_SHADER_READ_BIT);
	}
	else
	{
		builder.AddExternalSubpassDependency(
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_SHADER_READ_BIT);
	}

	builder.DebugName("VkPPRenderPassSetup.RenderPass");
	RenderPass = builder.Create(fb->GetDevice());
}
