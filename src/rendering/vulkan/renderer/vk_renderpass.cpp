
#include "vk_renderpass.h"
#include "vk_renderbuffers.h"
#include "vk_renderstate.h"
#include "vulkan/shaders/vk_shader.h"
#include "vulkan/system/vk_builders.h"
#include "vulkan/system/vk_framebuffer.h"
#include "vulkan/system/vk_buffers.h"
#include "hwrenderer/data/flatvertices.h"
#include "hwrenderer/scene/hw_viewpointuniforms.h"
#include "rendering/2d/v_2ddrawer.h"

VkRenderPassManager::VkRenderPassManager()
{
}

void VkRenderPassManager::Init()
{
	CreateDynamicSetLayout();
	CreateTextureSetLayout();
	CreatePipelineLayout();
	CreateDescriptorPool();
	CreateDynamicSet();
}

void VkRenderPassManager::RenderBuffersReset()
{
	RenderPassSetup.clear();
}

void VkRenderPassManager::SetRenderTarget(VulkanImageView *view, int width, int height)
{
	GetVulkanFrameBuffer()->GetRenderState()->EndRenderPass();
	mRenderTargetView = view;
	mRenderTargetWidth = width;
	mRenderTargetHeight = height;
}

void VkRenderPassManager::BeginRenderPass(const VkRenderPassKey &key, VulkanCommandBuffer *cmdbuffer)
{
	auto fb = GetVulkanFrameBuffer();
	auto buffers = fb->GetBuffers();

	VkRenderPassSetup *passSetup = GetRenderPass(key);

	auto &framebuffer = passSetup->Framebuffer[mRenderTargetView->view];
	if (!framebuffer)
	{
		auto buffers = fb->GetBuffers();
		FramebufferBuilder builder;
		builder.setRenderPass(passSetup->RenderPass.get());
		builder.setSize(mRenderTargetWidth, mRenderTargetHeight);
		builder.addAttachment(mRenderTargetView->view);
		if (key.DepthTest || key.DepthWrite || key.StencilTest)
			builder.addAttachment(buffers->SceneDepthStencilView.get());
		framebuffer = builder.create(GetVulkanFrameBuffer()->device);
	}

	RenderPassBegin beginInfo;
	beginInfo.setRenderPass(passSetup->RenderPass.get());
	beginInfo.setRenderArea(0, 0, buffers->GetWidth(), buffers->GetHeight());
	beginInfo.setFramebuffer(framebuffer.get());
	cmdbuffer->beginRenderPass(beginInfo);
	cmdbuffer->bindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, passSetup->Pipeline.get());
}

VkRenderPassSetup *VkRenderPassManager::GetRenderPass(const VkRenderPassKey &key)
{
	auto &item = RenderPassSetup[key];
	if (!item)
		item.reset(new VkRenderPassSetup(key));
	return item.get();
}

int VkRenderPassManager::GetVertexFormat(int numBindingPoints, int numAttributes, size_t stride, const FVertexBufferAttribute *attrs)
{
	for (size_t i = 0; i < VertexFormats.size(); i++)
	{
		const auto &f = VertexFormats[i];
		if (f.Attrs.size() == numAttributes && f.NumBindingPoints == numBindingPoints && f.Stride == stride)
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
	fmt.UseVertexData = false;
	for (int j = 0; j < numAttributes; j++)
	{
		if (attrs[j].location == VATTR_COLOR)
			fmt.UseVertexData = true;
		fmt.Attrs.push_back(attrs[j]);
	}
	VertexFormats.push_back(fmt);
	return (int)VertexFormats.size() - 1;
}

void VkRenderPassManager::CreateDynamicSetLayout()
{
	DescriptorSetLayoutBuilder builder;
	builder.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
	builder.addBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
	builder.addBinding(2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
	builder.addBinding(3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
	DynamicSetLayout = builder.create(GetVulkanFrameBuffer()->device);
}

void VkRenderPassManager::CreateTextureSetLayout()
{
	DescriptorSetLayoutBuilder builder;
	for (int i = 0; i < 6; i++)
	{
		builder.addBinding(i, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
	}
	builder.addBinding(16, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
	TextureSetLayout = builder.create(GetVulkanFrameBuffer()->device);
}

void VkRenderPassManager::CreatePipelineLayout()
{
	PipelineLayoutBuilder builder;
	builder.addSetLayout(DynamicSetLayout.get());
	builder.addSetLayout(TextureSetLayout.get());
	builder.addPushConstantRange(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants));
	PipelineLayout = builder.create(GetVulkanFrameBuffer()->device);
}

void VkRenderPassManager::CreateDescriptorPool()
{
	DescriptorPoolBuilder builder;
	builder.addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 3);
	builder.addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1);
	builder.addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 5000 * 6);
	builder.setMaxSets(5000);
	DescriptorPool = builder.create(GetVulkanFrameBuffer()->device);
}

void VkRenderPassManager::CreateDynamicSet()
{
	DynamicSet = DescriptorPool->allocate(DynamicSetLayout.get());

	auto fb = GetVulkanFrameBuffer();
	WriteDescriptors update;
	update.addBuffer(DynamicSet.get(), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, fb->ViewpointUBO->mBuffer.get(), 0, sizeof(HWViewpointUniforms));
	update.addBuffer(DynamicSet.get(), 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, fb->LightBufferSSO->mBuffer.get(), 0, fb->GetLightBufferBlockSize());
	update.addBuffer(DynamicSet.get(), 2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, fb->MatricesUBO->mBuffer.get(), 0, sizeof(MatricesUBO));
	update.addBuffer(DynamicSet.get(), 3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, fb->StreamUBO->mBuffer.get(), 0, sizeof(StreamUBO));
	update.updateSets(fb->device);
}

/////////////////////////////////////////////////////////////////////////////

VkRenderPassSetup::VkRenderPassSetup(const VkRenderPassKey &key)
{
	CreateRenderPass(key);
	CreatePipeline(key);
}

void VkRenderPassSetup::CreateRenderPass(const VkRenderPassKey &key)
{
	RenderPassBuilder builder;
	builder.addRgba16fAttachment(false, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	if (key.DepthTest || key.DepthWrite || key.StencilTest)
		builder.addDepthStencilAttachment(false, GetVulkanFrameBuffer()->GetBuffers()->SceneDepthStencilFormat, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
	builder.addSubpass();
	builder.addSubpassColorAttachmentRef(0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	if (key.DepthTest || key.DepthWrite || key.StencilTest)
	{
		builder.addSubpassDepthStencilAttachmentRef(1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
		builder.addExternalSubpassDependency(
			VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT);
	}
	else
	{
		builder.addExternalSubpassDependency(
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_ACCESS_COLOR_ATTACHMENT_READ_BIT);
	}
	RenderPass = builder.create(GetVulkanFrameBuffer()->device);
}

void VkRenderPassSetup::CreatePipeline(const VkRenderPassKey &key)
{
	auto fb = GetVulkanFrameBuffer();
	GraphicsPipelineBuilder builder;

	VkShaderProgram *program;
	if (key.SpecialEffect != EFF_NONE)
	{
		program = fb->GetShaderManager()->GetEffect(key.SpecialEffect);
	}
	else
	{
		program = fb->GetShaderManager()->Get(key.EffectState, key.AlphaTest);
	}
	builder.addVertexShader(program->vert.get());
	builder.addFragmentShader(program->frag.get());

	const VkVertexFormat &vfmt = fb->GetRenderPassManager()->VertexFormats[key.VertexFormat];

	for (int i = 0; i < vfmt.NumBindingPoints; i++)
		builder.addVertexBufferBinding(i, vfmt.Stride);

	const static VkFormat vkfmts[] = {
		VK_FORMAT_R32G32B32A32_SFLOAT,
		VK_FORMAT_R32G32B32_SFLOAT,
		VK_FORMAT_R32G32_SFLOAT,
		VK_FORMAT_R32_SFLOAT,
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_FORMAT_A2R10G10B10_SNORM_PACK32
	};

	bool inputLocations[6] = { false, false, false, false, false, false };

	for (size_t i = 0; i < vfmt.Attrs.size(); i++)
	{
		const auto &attr = vfmt.Attrs[i];
		builder.addVertexAttribute(attr.location, attr.binding, vkfmts[attr.format], attr.offset);
		inputLocations[attr.location] = true;
	}

	// To do: does vulkan absolutely needs a binding for each location or not? What happens if it isn't specified? Better be safe than sorry..
	for (int i = 0; i < 6; i++)
	{
		if (!inputLocations[i])
			builder.addVertexAttribute(i, 0, VK_FORMAT_R32G32B32_SFLOAT, 0);
	}

	builder.addDynamicState(VK_DYNAMIC_STATE_VIEWPORT);
	builder.addDynamicState(VK_DYNAMIC_STATE_SCISSOR);
	// builder.addDynamicState(VK_DYNAMIC_STATE_LINE_WIDTH);
	builder.addDynamicState(VK_DYNAMIC_STATE_DEPTH_BIAS);
	// builder.addDynamicState(VK_DYNAMIC_STATE_BLEND_CONSTANTS);
	// builder.addDynamicState(VK_DYNAMIC_STATE_DEPTH_BOUNDS);
	// builder.addDynamicState(VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK);
	// builder.addDynamicState(VK_DYNAMIC_STATE_STENCIL_WRITE_MASK);
	builder.addDynamicState(VK_DYNAMIC_STATE_STENCIL_REFERENCE);

	builder.setViewport(0.0f, 0.0f, (float)SCREENWIDTH, (float)SCREENHEIGHT);
	builder.setScissor(0, 0, SCREENWIDTH, SCREENHEIGHT);

	static const VkPrimitiveTopology vktopology[] = {
		VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
		VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN,
		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP
	};

	static const VkStencilOp op2vk[] = { VK_STENCIL_OP_KEEP, VK_STENCIL_OP_INCREMENT_AND_CLAMP, VK_STENCIL_OP_DECREMENT_AND_CLAMP };
	static const VkCompareOp depthfunc2vk[] = { VK_COMPARE_OP_LESS, VK_COMPARE_OP_LESS_OR_EQUAL, VK_COMPARE_OP_ALWAYS };

	builder.setTopology(vktopology[key.DrawType]);
	builder.setDepthStencilEnable(key.DepthTest, key.DepthWrite, key.StencilTest);
	builder.setDepthFunc(depthfunc2vk[key.DepthFunc]);
	builder.setDepthClampEnable(key.DepthClamp);
	builder.setDepthBias(key.DepthBias, 0.0f, 0.0f, 0.0f);
	builder.setCull(key.CullMode == Cull_None ? VK_CULL_MODE_NONE : VK_CULL_MODE_FRONT_AND_BACK, key.CullMode == Cull_CW ? VK_FRONT_FACE_CLOCKWISE : VK_FRONT_FACE_COUNTER_CLOCKWISE);
	builder.setColorWriteMask((VkColorComponentFlags)key.ColorMask);
	builder.setStencil(VK_STENCIL_OP_KEEP, op2vk[key.StencilPassOp], VK_STENCIL_OP_KEEP, VK_COMPARE_OP_EQUAL, 0xffffffff, 0xffffffff, 0);
	builder.setBlendMode(key.RenderStyle);

	builder.setLayout(fb->GetRenderPassManager()->PipelineLayout.get());
	builder.setRenderPass(RenderPass.get());
	Pipeline = builder.create(fb->device);
}
