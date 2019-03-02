
#include "vk_renderpass.h"
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

void VkRenderPassManager::BeginFrame()
{
	if (!SceneColor || SceneColor->width != SCREENWIDTH || SceneColor->height != SCREENHEIGHT)
	{
		auto fb = GetVulkanFrameBuffer();

		RenderPassSetup.clear();
		SceneColorView.reset();
		SceneDepthStencilView.reset();
		SceneDepthView.reset();
		SceneColor.reset();
		SceneDepthStencil.reset();

		ImageBuilder builder;
		builder.setSize(SCREENWIDTH, SCREENHEIGHT);
		builder.setFormat(VK_FORMAT_R16G16B16A16_SFLOAT);
		builder.setUsage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
		SceneColor = builder.create(fb->device);

		builder.setFormat(VK_FORMAT_D24_UNORM_S8_UINT);
		builder.setUsage(VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
		SceneDepthStencil = builder.create(fb->device);

		ImageViewBuilder viewbuilder;
		viewbuilder.setImage(SceneColor.get(), VK_FORMAT_R16G16B16A16_SFLOAT);
		SceneColorView = viewbuilder.create(fb->device);

		viewbuilder.setImage(SceneDepthStencil.get(), VK_FORMAT_D24_UNORM_S8_UINT, VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);
		SceneDepthStencilView = viewbuilder.create(fb->device);

		viewbuilder.setImage(SceneDepthStencil.get(), VK_FORMAT_D24_UNORM_S8_UINT, VK_IMAGE_ASPECT_DEPTH_BIT);
		SceneDepthView = viewbuilder.create(fb->device);

		PipelineBarrier barrier;
		barrier.addImage(SceneColor.get(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
		barrier.addImage(SceneDepthStencil.get(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 0, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);
		barrier.execute(fb->GetDrawCommands(), 0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);
	}
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
	builder.addBinding(4, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
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
	builder.addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 4);
	builder.addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1);
	builder.addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 512 * 6);
	builder.setMaxSets(512);
	DescriptorPool = builder.create(GetVulkanFrameBuffer()->device);
}

void VkRenderPassManager::CreateDynamicSet()
{
	DynamicSet = DescriptorPool->allocate(DynamicSetLayout.get());

	auto fb = GetVulkanFrameBuffer();
	WriteDescriptors update;
	update.addBuffer(DynamicSet.get(), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, fb->ViewpointUBO->mBuffer.get(), 0, sizeof(HWViewpointUniforms));
	update.addBuffer(DynamicSet.get(), 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, fb->LightBufferSSO->mBuffer.get(), 0, 4096);
	update.addBuffer(DynamicSet.get(), 2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, fb->MatricesUBO->mBuffer.get(), 0, sizeof(MatricesUBO));
	update.addBuffer(DynamicSet.get(), 3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, fb->ColorsUBO->mBuffer.get(), 0, sizeof(ColorsUBO));
	update.addBuffer(DynamicSet.get(), 4, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, fb->GlowingWallsUBO->mBuffer.get(), 0, sizeof(GlowingWallsUBO));
	update.updateSets(fb->device);
}

/////////////////////////////////////////////////////////////////////////////

VkRenderPassSetup::VkRenderPassSetup(const VkRenderPassKey &key)
{
	CreateRenderPass(key);
	CreatePipeline(key);
	CreateFramebuffer(key);
}

void VkRenderPassSetup::CreateRenderPass(const VkRenderPassKey &key)
{
	RenderPassBuilder builder;
	builder.addRgba16fAttachment(false, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	if (key.DepthTest || key.DepthWrite)
		builder.addDepthStencilAttachment(false, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
	builder.addSubpass();
	builder.addSubpassColorAttachmentRef(0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	if (key.DepthTest || key.DepthWrite)
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
	// builder.addDynamicState(VK_DYNAMIC_STATE_DEPTH_BIAS);
	// builder.addDynamicState(VK_DYNAMIC_STATE_BLEND_CONSTANTS);
	// builder.addDynamicState(VK_DYNAMIC_STATE_DEPTH_BOUNDS);
	// builder.addDynamicState(VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK);
	// builder.addDynamicState(VK_DYNAMIC_STATE_STENCIL_WRITE_MASK);
	// builder.addDynamicState(VK_DYNAMIC_STATE_STENCIL_REFERENCE);

	builder.setViewport(0.0f, 0.0f, (float)SCREENWIDTH, (float)SCREENHEIGHT);
	builder.setScissor(0, 0, SCREENWIDTH, SCREENHEIGHT);

	static const VkPrimitiveTopology vktopology[] = {
		VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
		VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN,
		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP
	};

	builder.setTopology(vktopology[key.DrawType]);

	static const int blendstyles[] = {
		VK_BLEND_FACTOR_ZERO,
		VK_BLEND_FACTOR_ONE,
		VK_BLEND_FACTOR_SRC_ALPHA,
		VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
		VK_BLEND_FACTOR_SRC_COLOR,
		VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR,
		VK_BLEND_FACTOR_DST_COLOR,
		VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR,
	};

	static const int renderops[] = {
		0, VK_BLEND_OP_ADD, VK_BLEND_OP_SUBTRACT, VK_BLEND_OP_REVERSE_SUBTRACT, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
	};

	int srcblend = blendstyles[key.RenderStyle.SrcAlpha%STYLEALPHA_MAX];
	int dstblend = blendstyles[key.RenderStyle.DestAlpha%STYLEALPHA_MAX];
	int blendequation = renderops[key.RenderStyle.BlendOp & 15];

	if (blendequation == -1)	// This was a fuzz style.
	{
		srcblend = VK_BLEND_FACTOR_DST_COLOR;
		dstblend = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		blendequation = VK_BLEND_OP_ADD;
	}

	builder.setDepthEnable(key.DepthTest, key.DepthWrite);

	builder.setBlendMode((VkBlendOp)blendequation, (VkBlendFactor)srcblend, (VkBlendFactor)dstblend);

	builder.setLayout(fb->GetRenderPassManager()->PipelineLayout.get());
	builder.setRenderPass(RenderPass.get());
	Pipeline = builder.create(fb->device);
}

void VkRenderPassSetup::CreateFramebuffer(const VkRenderPassKey &key)
{
	auto fb = GetVulkanFrameBuffer();
	FramebufferBuilder builder;
	builder.setRenderPass(RenderPass.get());
	builder.setSize(SCREENWIDTH, SCREENHEIGHT);
	builder.addAttachment(fb->GetRenderPassManager()->SceneColorView.get());
	if (key.DepthTest || key.DepthWrite)
		builder.addAttachment(fb->GetRenderPassManager()->SceneDepthStencilView.get());
	Framebuffer = builder.create(GetVulkanFrameBuffer()->device);
}
