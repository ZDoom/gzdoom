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

#include "vk_renderpass.h"
#include "vk_renderstate.h"
#include "vk_descriptorset.h"
#include "vk_raytrace.h"
#include "vulkan/textures/vk_renderbuffers.h"
#include "vulkan/textures/vk_samplers.h"
#include "vulkan/shaders/vk_shader.h"
#include "vulkan/shaders/vk_ppshader.h"
#include "vulkan/system/vk_builders.h"
#include "vulkan/system/vk_framebuffer.h"
#include "vulkan/system/vk_hwbuffer.h"
#include "flatvertices.h"
#include "hw_viewpointuniforms.h"
#include "v_2ddrawer.h"

VkRenderPassManager::VkRenderPassManager(VulkanFrameBuffer* fb) : fb(fb)
{
}

VkRenderPassManager::~VkRenderPassManager()
{
}

void VkRenderPassManager::RenderBuffersReset()
{
	RenderPassSetup.clear();
	PPRenderPassSetup.clear();
}

VkRenderPassSetup *VkRenderPassManager::GetRenderPass(const VkRenderPassKey &key)
{
	auto &item = RenderPassSetup[key];
	if (!item)
		item.reset(new VkRenderPassSetup(fb, key));
	return item.get();
}

int VkRenderPassManager::GetVertexFormat(int numBindingPoints, int numAttributes, size_t stride, const FVertexBufferAttribute *attrs)
{
	for (size_t i = 0; i < VertexFormats.size(); i++)
	{
		const auto &f = VertexFormats[i];
		if (f.Attrs.size() == (size_t)numAttributes && f.NumBindingPoints == numBindingPoints && f.Stride == stride)
		{
			bool matches = true;
			for (int j = 0; j < numAttributes; j++)
			{
				if (memcmp(&f.Attrs[j], &attrs[j], sizeof(FVertexBufferAttribute)) != 0)
				{
					matches = false;
					break;
				}
			}

			if (matches)
				return (int)i;
		}
	}

	VkVertexFormat fmt;
	fmt.NumBindingPoints = numBindingPoints;
	fmt.Stride = stride;
	fmt.UseVertexData = 0;
	for (int j = 0; j < numAttributes; j++)
	{
		if (attrs[j].location == VATTR_COLOR)
			fmt.UseVertexData |= 1;
		else if (attrs[j].location == VATTR_NORMAL)
			fmt.UseVertexData |= 2;
		fmt.Attrs.push_back(attrs[j]);
	}
	VertexFormats.push_back(fmt);
	return (int)VertexFormats.size() - 1;
}

VkVertexFormat *VkRenderPassManager::GetVertexFormat(int index)
{
	return &VertexFormats[index];
}

VulkanPipelineLayout* VkRenderPassManager::GetPipelineLayout(int numLayers)
{
	if (PipelineLayouts.size() <= (size_t)numLayers)
		PipelineLayouts.resize(numLayers + 1);

	auto &layout = PipelineLayouts[numLayers];
	if (layout)
		return layout.get();

	auto descriptors = fb->GetDescriptorSetManager();

	PipelineLayoutBuilder builder;
	builder.AddSetLayout(descriptors->GetFixedSetLayout());
	builder.AddSetLayout(descriptors->GetHWBufferSetLayout());
	if (numLayers != 0)
		builder.AddSetLayout(descriptors->GetTextureSetLayout(numLayers));
	builder.AddPushConstantRange(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants));
	builder.DebugName("VkRenderPassManager.PipelineLayout");
	layout = builder.Create(fb->device);
	return layout.get();
}

VkPPRenderPassSetup* VkRenderPassManager::GetPPRenderPass(const VkPPRenderPassKey& key)
{
	auto& passSetup = PPRenderPassSetup[key];
	if (!passSetup)
		passSetup.reset(new VkPPRenderPassSetup(fb, key));
	return passSetup.get();
}

/////////////////////////////////////////////////////////////////////////////

VkRenderPassSetup::VkRenderPassSetup(VulkanFrameBuffer* fb, const VkRenderPassKey &key) : PassKey(key), fb(fb)
{
}

std::unique_ptr<VulkanRenderPass> VkRenderPassSetup::CreateRenderPass(int clearTargets)
{
	auto buffers = fb->GetBuffers();

	VkFormat drawBufferFormats[] = { VK_FORMAT_R16G16B16A16_SFLOAT, VK_FORMAT_R8G8B8A8_UNORM, buffers->SceneNormalFormat };

	RenderPassBuilder builder;

	builder.AddAttachment(
		PassKey.DrawBufferFormat, (VkSampleCountFlagBits)PassKey.Samples,
		(clearTargets & CT_Color) ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

	for (int i = 1; i < PassKey.DrawBuffers; i++)
	{
		builder.AddAttachment(
			drawBufferFormats[i], (VkSampleCountFlagBits)PassKey.Samples,
			(clearTargets & CT_Color) ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	}
	if (PassKey.DepthStencil)
	{
		builder.AddDepthStencilAttachment(
			buffers->SceneDepthStencilFormat, (VkSampleCountFlagBits)PassKey.Samples,
			(clearTargets & CT_Depth) ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE,
			(clearTargets & CT_Stencil) ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
	}
	builder.AddSubpass();
	for (int i = 0; i < PassKey.DrawBuffers; i++)
		builder.AddSubpassColorAttachmentRef(i, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	if (PassKey.DepthStencil)
	{
		builder.AddSubpassDepthStencilAttachmentRef(PassKey.DrawBuffers, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
		builder.AddExternalSubpassDependency(
			VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT);
	}
	else
	{
		builder.AddExternalSubpassDependency(
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_ACCESS_COLOR_ATTACHMENT_READ_BIT);
	}
	builder.DebugName("VkRenderPassSetup.RenderPass");
	return builder.Create(fb->device);
}

VulkanRenderPass *VkRenderPassSetup::GetRenderPass(int clearTargets)
{
	if (!RenderPasses[clearTargets])
		RenderPasses[clearTargets] = CreateRenderPass(clearTargets);
	return RenderPasses[clearTargets].get();
}

VulkanPipeline *VkRenderPassSetup::GetPipeline(const VkPipelineKey &key)
{
	auto &item = Pipelines[key];
	if (!item)
		item = CreatePipeline(key);
	return item.get();
}

std::unique_ptr<VulkanPipeline> VkRenderPassSetup::CreatePipeline(const VkPipelineKey &key)
{
	GraphicsPipelineBuilder builder;

	VkShaderProgram *program;
	if (key.SpecialEffect != EFF_NONE)
	{
		program = fb->GetShaderManager()->GetEffect(key.SpecialEffect, PassKey.DrawBuffers > 1 ? GBUFFER_PASS : NORMAL_PASS);
	}
	else
	{
		program = fb->GetShaderManager()->Get(key.EffectState, key.AlphaTest, PassKey.DrawBuffers > 1 ? GBUFFER_PASS : NORMAL_PASS);
	}
	builder.AddVertexShader(program->vert.get());
	builder.AddFragmentShader(program->frag.get());

	const VkVertexFormat &vfmt = *fb->GetRenderPassManager()->GetVertexFormat(key.VertexFormat);

	for (int i = 0; i < vfmt.NumBindingPoints; i++)
		builder.AddVertexBufferBinding(i, vfmt.Stride);

	const static VkFormat vkfmts[] = {
		VK_FORMAT_R32G32B32A32_SFLOAT,
		VK_FORMAT_R32G32B32_SFLOAT,
		VK_FORMAT_R32G32_SFLOAT,
		VK_FORMAT_R32_SFLOAT,
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_FORMAT_A2B10G10R10_SNORM_PACK32,
		VK_FORMAT_R8G8B8A8_UINT
	};

	bool inputLocations[VATTR_MAX] = {};

	for (size_t i = 0; i < vfmt.Attrs.size(); i++)
	{
		const auto &attr = vfmt.Attrs[i];
		builder.AddVertexAttribute(attr.location, attr.binding, vkfmts[attr.format], attr.offset);
		inputLocations[attr.location] = true;
	}

	// Vulkan requires an attribute binding for each location specified in the shader
	for (int i = 0; i < VATTR_MAX; i++)
	{
		if (!inputLocations[i])
			builder.AddVertexAttribute(i, 0, i != 8 ? VK_FORMAT_R32G32B32_SFLOAT : VK_FORMAT_R8G8B8A8_UINT, 0);
	}

	builder.AddDynamicState(VK_DYNAMIC_STATE_VIEWPORT);
	builder.AddDynamicState(VK_DYNAMIC_STATE_SCISSOR);
	builder.AddDynamicState(VK_DYNAMIC_STATE_DEPTH_BIAS);
	builder.AddDynamicState(VK_DYNAMIC_STATE_STENCIL_REFERENCE);

	// Note: the actual values are ignored since we use dynamic viewport+scissor states
	builder.Viewport(0.0f, 0.0f, 320.0f, 200.0f);
	builder.Scissor(0, 0, 320, 200);

	static const VkPrimitiveTopology vktopology[] = {
		VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
		VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN,
		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP
	};

	static const VkStencilOp op2vk[] = { VK_STENCIL_OP_KEEP, VK_STENCIL_OP_INCREMENT_AND_CLAMP, VK_STENCIL_OP_DECREMENT_AND_CLAMP };
	static const VkCompareOp depthfunc2vk[] = { VK_COMPARE_OP_LESS, VK_COMPARE_OP_LESS_OR_EQUAL, VK_COMPARE_OP_ALWAYS };

	builder.Topology(vktopology[key.DrawType]);
	builder.DepthStencilEnable(key.DepthTest, key.DepthWrite, key.StencilTest);
	builder.DepthFunc(depthfunc2vk[key.DepthFunc]);
	if (fb->device->UsedDeviceFeatures.depthClamp)
		builder.DepthClampEnable(key.DepthClamp);
	builder.DepthBias(key.DepthBias, 0.0f, 0.0f, 0.0f);

	// Note: CCW and CW is intentionally swapped here because the vulkan and opengl coordinate systems differ.
	// main.vp addresses this by patching up gl_Position.z, which has the side effect of flipping the sign of the front face calculations.
	builder.Cull(key.CullMode == Cull_None ? VK_CULL_MODE_NONE : VK_CULL_MODE_BACK_BIT, key.CullMode == Cull_CW ? VK_FRONT_FACE_COUNTER_CLOCKWISE : VK_FRONT_FACE_CLOCKWISE);

	builder.ColorWriteMask((VkColorComponentFlags)key.ColorMask);
	builder.Stencil(VK_STENCIL_OP_KEEP, op2vk[key.StencilPassOp], VK_STENCIL_OP_KEEP, VK_COMPARE_OP_EQUAL, 0xffffffff, 0xffffffff, 0);
	builder.BlendMode(key.RenderStyle);
	builder.SubpassColorAttachmentCount(PassKey.DrawBuffers);
	builder.RasterizationSamples((VkSampleCountFlagBits)PassKey.Samples);

	builder.Layout(fb->GetRenderPassManager()->GetPipelineLayout(key.NumTextureLayers));
	builder.RenderPass(GetRenderPass(0));
	builder.DebugName("VkRenderPassSetup.Pipeline");

	return builder.Create(fb->device);
}

/////////////////////////////////////////////////////////////////////////////

VkPPRenderPassSetup::VkPPRenderPassSetup(VulkanFrameBuffer* fb, const VkPPRenderPassKey& key) : fb(fb)
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
		builder.AddBinding(LIGHTNODES_BINDINGPOINT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
		builder.AddBinding(LIGHTLINES_BINDINGPOINT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
		builder.AddBinding(LIGHTLIST_BINDINGPOINT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
	}
	builder.DebugName("VkPPRenderPassSetup.DescriptorLayout");
	DescriptorLayout = builder.Create(fb->device);
}

void VkPPRenderPassSetup::CreatePipelineLayout(const VkPPRenderPassKey& key)
{
	PipelineLayoutBuilder builder;
	builder.AddSetLayout(DescriptorLayout.get());
	if (key.Uniforms > 0)
		builder.AddPushConstantRange(VK_SHADER_STAGE_FRAGMENT_BIT, 0, key.Uniforms);
	builder.DebugName("VkPPRenderPassSetup.PipelineLayout");
	PipelineLayout = builder.Create(fb->device);
}

void VkPPRenderPassSetup::CreatePipeline(const VkPPRenderPassKey& key)
{
	GraphicsPipelineBuilder builder;
	builder.AddVertexShader(key.Shader->VertexShader.get());
	builder.AddFragmentShader(key.Shader->FragmentShader.get());

	builder.AddVertexBufferBinding(0, sizeof(FFlatVertex));
	builder.AddVertexAttribute(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(FFlatVertex, x));
	builder.AddVertexAttribute(1, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(FFlatVertex, u));
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
	builder.BlendMode(key.BlendMode);
	builder.RasterizationSamples(key.Samples);
	builder.Layout(PipelineLayout.get());
	builder.RenderPass(RenderPass.get());
	builder.DebugName("VkPPRenderPassSetup.Pipeline");
	Pipeline = builder.Create(fb->device);
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
	RenderPass = builder.Create(fb->device);
}
