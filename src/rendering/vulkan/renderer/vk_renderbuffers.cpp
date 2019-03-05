
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

void VkRenderBuffers::BeginFrame(int width, int height, int sceneWidth, int sceneHeight)
{
	int samples = clamp((int)gl_multisample, 0, mMaxSamples);

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

	mWidth = width;
	mHeight = height;
	mSamples = samples;
	mSceneWidth = sceneWidth;
	mSceneHeight = sceneHeight;
}

void VkRenderBuffers::CreatePipeline(int width, int height)
{
}

void VkRenderBuffers::CreateScene(int width, int height, int samples)
{
	auto fb = GetVulkanFrameBuffer();

	SceneColorView.reset();
	SceneDepthStencilView.reset();
	SceneDepthView.reset();
	SceneColor.reset();
	SceneDepthStencil.reset();

	ImageBuilder builder;
	builder.setSize(SCREENWIDTH, SCREENHEIGHT);
	builder.setFormat(VK_FORMAT_R16G16B16A16_SFLOAT);
	builder.setUsage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
	SceneColor = builder.create(fb->device);

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

	ImageViewBuilder viewbuilder;
	viewbuilder.setImage(SceneColor.get(), VK_FORMAT_R16G16B16A16_SFLOAT);
	SceneColorView = viewbuilder.create(fb->device);

	viewbuilder.setImage(SceneDepthStencil.get(), SceneDepthStencilFormat, VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);
	SceneDepthStencilView = viewbuilder.create(fb->device);

	viewbuilder.setImage(SceneDepthStencil.get(), SceneDepthStencilFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
	SceneDepthView = viewbuilder.create(fb->device);

	PipelineBarrier barrier;
	barrier.addImage(SceneColor.get(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
	barrier.addImage(SceneDepthStencil.get(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 0, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);
	barrier.execute(fb->GetDrawCommands(), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);
}
