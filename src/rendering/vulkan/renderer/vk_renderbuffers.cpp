
#include "vk_renderbuffers.h"
#include "vk_renderpass.h"
#include "vk_postprocess.h"
#include "vulkan/shaders/vk_shader.h"
#include "vulkan/system/vk_builders.h"
#include "vulkan/system/vk_framebuffer.h"
#include "hwrenderer/utility/hw_cvars.h"

VkRenderBuffers::VkRenderBuffers()
{
}

VkRenderBuffers::~VkRenderBuffers()
{
}

VkSampleCountFlagBits VkRenderBuffers::GetBestSampleCount()
{
	auto fb = GetVulkanFrameBuffer();
	const auto &limits = fb->device->PhysicalDevice.Properties.limits;
	VkSampleCountFlags deviceSampleCounts = limits.sampledImageColorSampleCounts & limits.sampledImageDepthSampleCounts & limits.sampledImageStencilSampleCounts;

	int requestedSamples = clamp((int)gl_multisample, 0, 64);

	int samples = 1;
	VkSampleCountFlags bit = VK_SAMPLE_COUNT_1_BIT;
	VkSampleCountFlags best = bit;
	while (samples < requestedSamples)
	{
		if (deviceSampleCounts & bit)
		{
			best = bit;
		}
		samples <<= 1;
		bit <<= 1;
	}
	return (VkSampleCountFlagBits)best;
}

void VkRenderBuffers::BeginFrame(int width, int height, int sceneWidth, int sceneHeight)
{
	VkSampleCountFlagBits samples = GetBestSampleCount();

	if (width != mWidth || height != mHeight || mSamples != samples)
	{
		auto fb = GetVulkanFrameBuffer();
		fb->GetRenderPassManager()->RenderBuffersReset();
		fb->GetPostprocess()->RenderBuffersReset();
	}

	if (width != mWidth || height != mHeight)
		CreatePipeline(width, height);

	if (width != mWidth || height != mHeight || mSamples != samples)
		CreateScene(width, height, samples);

	CreateShadowmap();

	mWidth = width;
	mHeight = height;
	mSamples = samples;
	mSceneWidth = sceneWidth;
	mSceneHeight = sceneHeight;
}

void VkRenderBuffers::CreatePipeline(int width, int height)
{
	auto fb = GetVulkanFrameBuffer();

	for (int i = 0; i < NumPipelineImages; i++)
	{
		PipelineImage[i].reset();
		PipelineView[i].reset();
	}

	PipelineBarrier barrier;
	for (int i = 0; i < NumPipelineImages; i++)
	{
		ImageBuilder builder;
		builder.setSize(width, height);
		builder.setFormat(VK_FORMAT_R16G16B16A16_SFLOAT);
		builder.setUsage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
		PipelineImage[i] = builder.create(fb->device);
		PipelineImage[i]->SetDebugName("VkRenderBuffers.PipelineImage");

		ImageViewBuilder viewbuilder;
		viewbuilder.setImage(PipelineImage[i].get(), VK_FORMAT_R16G16B16A16_SFLOAT);
		PipelineView[i] = viewbuilder.create(fb->device);
		PipelineView[i]->SetDebugName("VkRenderBuffers.PipelineView");

		barrier.addImage(PipelineImage[i].get(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
		PipelineLayout[i] = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	}
	barrier.execute(fb->GetDrawCommands(), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);
}

void VkRenderBuffers::CreateScene(int width, int height, VkSampleCountFlagBits samples)
{
	auto fb = GetVulkanFrameBuffer();

	SceneColorView.reset();
	SceneDepthStencilView.reset();
	SceneDepthView.reset();
	SceneNormalView.reset();
	SceneFogView.reset();
	SceneColor.reset();
	SceneDepthStencil.reset();
	SceneNormal.reset();
	SceneFog.reset();

	CreateSceneColor(width, height, samples);
	CreateSceneDepthStencil(width, height, samples);
	CreateSceneNormal(width, height, samples);
	CreateSceneFog(width, height, samples);

	PipelineBarrier barrier;
	barrier.addImage(SceneColor.get(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
	barrier.addImage(SceneDepthStencil.get(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 0, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);
	barrier.addImage(SceneNormal.get(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
	barrier.addImage(SceneFog.get(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
	barrier.execute(fb->GetDrawCommands(), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);

	SceneColorLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
}

void VkRenderBuffers::CreateSceneColor(int width, int height, VkSampleCountFlagBits samples)
{
	auto fb = GetVulkanFrameBuffer();

	ImageBuilder builder;
	builder.setSize(width, height);
	builder.setSamples(samples);
	builder.setFormat(VK_FORMAT_R16G16B16A16_SFLOAT);
	builder.setUsage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
	SceneColor = builder.create(fb->device);
	SceneColor->SetDebugName("VkRenderBuffers.SceneColor");

	ImageViewBuilder viewbuilder;
	viewbuilder.setImage(SceneColor.get(), VK_FORMAT_R16G16B16A16_SFLOAT);
	SceneColorView = viewbuilder.create(fb->device);
	SceneColorView->SetDebugName("VkRenderBuffers.SceneColorView");
}

void VkRenderBuffers::CreateSceneDepthStencil(int width, int height, VkSampleCountFlagBits samples)
{
	auto fb = GetVulkanFrameBuffer();

	ImageBuilder builder;
	builder.setSize(width, height);
	builder.setSamples(samples);
	builder.setFormat(SceneDepthStencilFormat);
	builder.setUsage(VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
	if (!builder.isFormatSupported(fb->device))
	{
		SceneDepthStencilFormat = VK_FORMAT_D32_SFLOAT_S8_UINT;
		builder.setFormat(SceneDepthStencilFormat);
		if (!builder.isFormatSupported(fb->device))
		{
			I_FatalError("This device does not support any of the required depth stencil image formats.");
		}
	}
	SceneDepthStencil = builder.create(fb->device);
	SceneDepthStencil->SetDebugName("VkRenderBuffers.SceneDepthStencil");

	ImageViewBuilder viewbuilder;
	viewbuilder.setImage(SceneDepthStencil.get(), SceneDepthStencilFormat, VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);
	SceneDepthStencilView = viewbuilder.create(fb->device);
	SceneDepthStencilView->SetDebugName("VkRenderBuffers.SceneDepthStencilView");

	viewbuilder.setImage(SceneDepthStencil.get(), SceneDepthStencilFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
	SceneDepthView = viewbuilder.create(fb->device);
	SceneDepthView->SetDebugName("VkRenderBuffers.SceneDepthView");
}

void VkRenderBuffers::CreateSceneFog(int width, int height, VkSampleCountFlagBits samples)
{
	auto fb = GetVulkanFrameBuffer();

	ImageBuilder builder;
	builder.setSize(width, height);
	builder.setSamples(samples);
	builder.setFormat(VK_FORMAT_R8G8B8A8_UNORM);
	builder.setUsage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
	SceneFog = builder.create(fb->device);
	SceneFog->SetDebugName("VkRenderBuffers.SceneFog");

	ImageViewBuilder viewbuilder;
	viewbuilder.setImage(SceneFog.get(), VK_FORMAT_R8G8B8A8_UNORM);
	SceneFogView = viewbuilder.create(fb->device);
	SceneFogView->SetDebugName("VkRenderBuffers.SceneFogView");
}

void VkRenderBuffers::CreateSceneNormal(int width, int height, VkSampleCountFlagBits samples)
{
	auto fb = GetVulkanFrameBuffer();

	ImageBuilder builder;
	builder.setSize(width, height);
	builder.setSamples(samples);
	builder.setFormat(VK_FORMAT_A2R10G10B10_UNORM_PACK32);
	builder.setUsage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
	SceneNormal = builder.create(fb->device);
	SceneNormal->SetDebugName("VkRenderBuffers.SceneNormal");

	ImageViewBuilder viewbuilder;
	viewbuilder.setImage(SceneNormal.get(), VK_FORMAT_A2R10G10B10_UNORM_PACK32);
	SceneNormalView = viewbuilder.create(fb->device);
	SceneNormalView->SetDebugName("VkRenderBuffers.SceneNormalView");
}

void VkRenderBuffers::CreateShadowmap()
{
	if (Shadowmap && Shadowmap->width == gl_shadowmap_quality)
		return;

	Shadowmap.reset();
	ShadowmapView.reset();

	auto fb = GetVulkanFrameBuffer();

	ImageBuilder builder;
	builder.setSize(gl_shadowmap_quality, 1024);
	builder.setFormat(VK_FORMAT_R32_SFLOAT);
	builder.setUsage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
	Shadowmap = builder.create(fb->device);
	Shadowmap->SetDebugName("VkRenderBuffers.Shadowmap");

	ImageViewBuilder viewbuilder;
	viewbuilder.setImage(Shadowmap.get(), VK_FORMAT_R32_SFLOAT);
	ShadowmapView = viewbuilder.create(fb->device);
	ShadowmapView->SetDebugName("VkRenderBuffers.ShadowmapView");

	PipelineBarrier barrier;
	barrier.addImage(Shadowmap.get(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, VK_ACCESS_SHADER_READ_BIT);
	barrier.execute(fb->GetDrawCommands(), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

	if (!ShadowmapSampler)
	{
		SamplerBuilder builder;
		builder.setMipmapMode(VK_SAMPLER_MIPMAP_MODE_NEAREST);
		builder.setMinFilter(VK_FILTER_NEAREST);
		builder.setMagFilter(VK_FILTER_NEAREST);
		builder.setAddressMode(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
		ShadowmapSampler = builder.create(fb->device);
		ShadowmapSampler->SetDebugName("VkRenderBuffers.ShadowmapSampler");
	}
}
