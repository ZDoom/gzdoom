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

#include "vk_renderbuffers.h"
#include "vk_renderpass.h"
#include "vk_postprocess.h"
#include "vulkan/shaders/vk_shader.h"
#include "vulkan/system/vk_builders.h"
#include "vulkan/system/vk_framebuffer.h"
#include "hw_cvars.h"


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
	CreateLightmapSampler();

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
	}

	VkImageTransition barrier;
	for (int i = 0; i < NumPipelineImages; i++)
	{
		ImageBuilder builder;
		builder.setSize(width, height);
		builder.setFormat(VK_FORMAT_R16G16B16A16_SFLOAT);
		builder.setUsage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
		PipelineImage[i].Image = builder.create(fb->device);
		PipelineImage[i].Image->SetDebugName("VkRenderBuffers.PipelineImage");

		ImageViewBuilder viewbuilder;
		viewbuilder.setImage(PipelineImage[i].Image.get(), VK_FORMAT_R16G16B16A16_SFLOAT);
		PipelineImage[i].View = viewbuilder.create(fb->device);
		PipelineImage[i].View->SetDebugName("VkRenderBuffers.PipelineView");

		barrier.addImage(&PipelineImage[i], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, true);
	}
	barrier.execute(fb->GetDrawCommands());
}

void VkRenderBuffers::CreateScene(int width, int height, VkSampleCountFlagBits samples)
{
	auto fb = GetVulkanFrameBuffer();

	SceneColor.reset();
	SceneDepthStencil.reset();
	SceneNormal.reset();
	SceneFog.reset();

	CreateSceneColor(width, height, samples);
	CreateSceneDepthStencil(width, height, samples);
	CreateSceneNormal(width, height, samples);
	CreateSceneFog(width, height, samples);

	VkImageTransition barrier;
	barrier.addImage(&SceneColor, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, true);
	barrier.addImage(&SceneDepthStencil, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, true);
	barrier.addImage(&SceneNormal, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, true);
	barrier.addImage(&SceneFog, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, true);
	barrier.execute(fb->GetDrawCommands());
}

void VkRenderBuffers::CreateSceneColor(int width, int height, VkSampleCountFlagBits samples)
{
	auto fb = GetVulkanFrameBuffer();

	ImageBuilder builder;
	builder.setSize(width, height);
	builder.setSamples(samples);
	builder.setFormat(VK_FORMAT_R16G16B16A16_SFLOAT);
	builder.setUsage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
	SceneColor.Image = builder.create(fb->device);
	SceneColor.Image->SetDebugName("VkRenderBuffers.SceneColor");

	ImageViewBuilder viewbuilder;
	viewbuilder.setImage(SceneColor.Image.get(), VK_FORMAT_R16G16B16A16_SFLOAT);
	SceneColor.View = viewbuilder.create(fb->device);
	SceneColor.View->SetDebugName("VkRenderBuffers.SceneColorView");
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
	SceneDepthStencil.Image = builder.create(fb->device);
	SceneDepthStencil.Image->SetDebugName("VkRenderBuffers.SceneDepthStencil");
	SceneDepthStencil.AspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;

	ImageViewBuilder viewbuilder;
	viewbuilder.setImage(SceneDepthStencil.Image.get(), SceneDepthStencilFormat, VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);
	SceneDepthStencil.View = viewbuilder.create(fb->device);
	SceneDepthStencil.View->SetDebugName("VkRenderBuffers.SceneDepthStencilView");

	viewbuilder.setImage(SceneDepthStencil.Image.get(), SceneDepthStencilFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
	SceneDepthStencil.DepthOnlyView = viewbuilder.create(fb->device);
	SceneDepthStencil.DepthOnlyView->SetDebugName("VkRenderBuffers.SceneDepthView");
}

void VkRenderBuffers::CreateSceneFog(int width, int height, VkSampleCountFlagBits samples)
{
	auto fb = GetVulkanFrameBuffer();

	ImageBuilder builder;
	builder.setSize(width, height);
	builder.setSamples(samples);
	builder.setFormat(VK_FORMAT_R8G8B8A8_UNORM);
	builder.setUsage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
	SceneFog.Image = builder.create(fb->device);
	SceneFog.Image->SetDebugName("VkRenderBuffers.SceneFog");

	ImageViewBuilder viewbuilder;
	viewbuilder.setImage(SceneFog.Image.get(), VK_FORMAT_R8G8B8A8_UNORM);
	SceneFog.View = viewbuilder.create(fb->device);
	SceneFog.View->SetDebugName("VkRenderBuffers.SceneFogView");
}

void VkRenderBuffers::CreateSceneNormal(int width, int height, VkSampleCountFlagBits samples)
{
	auto fb = GetVulkanFrameBuffer();

	ImageBuilder builder;
	builder.setSize(width, height);
	builder.setSamples(samples);
	builder.setFormat(SceneNormalFormat);
	builder.setUsage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
	if (!builder.isFormatSupported(fb->device, VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT))
	{
		SceneNormalFormat = VK_FORMAT_R8G8B8A8_UNORM;
		builder.setFormat(SceneNormalFormat);
	}
	SceneNormal.Image = builder.create(fb->device);
	SceneNormal.Image->SetDebugName("VkRenderBuffers.SceneNormal");

	ImageViewBuilder viewbuilder;
	viewbuilder.setImage(SceneNormal.Image.get(), SceneNormalFormat);
	SceneNormal.View = viewbuilder.create(fb->device);
	SceneNormal.View->SetDebugName("VkRenderBuffers.SceneNormalView");
}

void VkRenderBuffers::CreateShadowmap()
{
	if (Shadowmap.Image && Shadowmap.Image->width == gl_shadowmap_quality)
		return;

	Shadowmap.reset();

	auto fb = GetVulkanFrameBuffer();

	ImageBuilder builder;
	builder.setSize(gl_shadowmap_quality, 1024);
	builder.setFormat(VK_FORMAT_R32_SFLOAT);
	builder.setUsage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
	Shadowmap.Image = builder.create(fb->device);
	Shadowmap.Image->SetDebugName("VkRenderBuffers.Shadowmap");

	ImageViewBuilder viewbuilder;
	viewbuilder.setImage(Shadowmap.Image.get(), VK_FORMAT_R32_SFLOAT);
	Shadowmap.View = viewbuilder.create(fb->device);
	Shadowmap.View->SetDebugName("VkRenderBuffers.ShadowmapView");

	VkImageTransition barrier;
	barrier.addImage(&Shadowmap, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, true);
	barrier.execute(fb->GetDrawCommands());

	if (!ShadowmapSampler)
	{
		SamplerBuilder samplerBuilder;
		samplerBuilder.setMipmapMode(VK_SAMPLER_MIPMAP_MODE_NEAREST);
		samplerBuilder.setMinFilter(VK_FILTER_NEAREST);
		samplerBuilder.setMagFilter(VK_FILTER_NEAREST);
		samplerBuilder.setAddressMode(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
		ShadowmapSampler = samplerBuilder.create(fb->device);
		ShadowmapSampler->SetDebugName("VkRenderBuffers.ShadowmapSampler");
	}
}

void VkRenderBuffers::CreateLightmapSampler()
{
	if (!Lightmap.Image)
	{
		auto fb = GetVulkanFrameBuffer();

		ImageBuilder builder;
		builder.setSize(1, 1);
		builder.setFormat(VK_FORMAT_R16G16B16A16_SFLOAT);
		builder.setUsage(VK_IMAGE_USAGE_SAMPLED_BIT);
		Lightmap.Image = builder.create(fb->device);
		Lightmap.Image->SetDebugName("VkRenderBuffers.Lightmap");

		ImageViewBuilder viewbuilder;
		viewbuilder.setType(VK_IMAGE_VIEW_TYPE_2D_ARRAY);
		viewbuilder.setImage(Lightmap.Image.get(), VK_FORMAT_R16G16B16A16_SFLOAT);
		Lightmap.View = viewbuilder.create(fb->device);
		Lightmap.View->SetDebugName("VkRenderBuffers.LightmapView");

		VkImageTransition barrier;
		barrier.addImage(&Lightmap, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, true);
		barrier.execute(fb->GetDrawCommands());
	}

	if (!LightmapSampler)
	{
		auto fb = GetVulkanFrameBuffer();

		SamplerBuilder builder;
		builder.setMipmapMode(VK_SAMPLER_MIPMAP_MODE_LINEAR);
		builder.setMinFilter(VK_FILTER_LINEAR);
		builder.setMagFilter(VK_FILTER_LINEAR);
		builder.setAddressMode(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
		LightmapSampler = builder.create(fb->device);
		LightmapSampler->SetDebugName("VkRenderBuffers.LightmapSampler");
	}
}
