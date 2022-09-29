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
#include "vulkan/renderer/vk_postprocess.h"
#include "vulkan/textures/vk_texture.h"
#include "vulkan/shaders/vk_shader.h"
#include "vulkan/system/vk_builders.h"
#include "vulkan/system/vk_framebuffer.h"
#include "vulkan/system/vk_commandbuffer.h"
#include "vulkan/system/vk_swapchain.h"
#include "hw_cvars.h"

VkRenderBuffers::VkRenderBuffers(VulkanFrameBuffer* fb) : fb(fb)
{
}

VkRenderBuffers::~VkRenderBuffers()
{
}

VkSampleCountFlagBits VkRenderBuffers::GetBestSampleCount()
{
	const auto &limits = fb->device->PhysicalDevice.Properties.limits;
	VkSampleCountFlags deviceSampleCounts = limits.sampledImageColorSampleCounts & limits.sampledImageDepthSampleCounts & limits.sampledImageStencilSampleCounts;

	int requestedSamples = clamp((int)gl_multisample, 0, 64);

	int samples = 1;
	VkSampleCountFlags bit = VK_SAMPLE_COUNT_1_BIT;
	VkSampleCountFlags best = bit;
	while (samples <= requestedSamples)
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
		fb->GetCommands()->WaitForCommands(false);
		fb->GetRenderPassManager()->RenderBuffersReset();
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

void VkRenderBuffers::CreatePipelineDepthStencil(int width, int height)
{
	ImageBuilder builder;
	builder.Size(width, height);
	builder.Format(PipelineDepthStencilFormat);
	builder.Usage(VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
	if (!builder.IsFormatSupported(fb->device))
	{
		PipelineDepthStencilFormat = VK_FORMAT_D32_SFLOAT_S8_UINT;
		builder.Format(PipelineDepthStencilFormat);
		if (!builder.IsFormatSupported(fb->device))
		{
			I_FatalError("This device does not support any of the required depth stencil image formats.");
		}
	}
	builder.DebugName("VkRenderBuffers.PipelineDepthStencil");

	PipelineDepthStencil.Image = builder.Create(fb->device);
	PipelineDepthStencil.AspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;

	PipelineDepthStencil.View = ImageViewBuilder()
		.Image(PipelineDepthStencil.Image.get(), PipelineDepthStencilFormat, VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)
		.DebugName("VkRenderBuffers.PipelineDepthStencilView")
		.Create(fb->device);

	PipelineDepthStencil.DepthOnlyView = ImageViewBuilder()
		.Image(PipelineDepthStencil.Image.get(), PipelineDepthStencilFormat, VK_IMAGE_ASPECT_DEPTH_BIT)
		.DebugName("VkRenderBuffers.PipelineDepthView")
		.Create(fb->device);
}

void VkRenderBuffers::CreatePipeline(int width, int height)
{
	for (int i = 0; i < NumPipelineImages; i++)
	{
		PipelineImage[i].Reset(fb);
	}
	PipelineDepthStencil.Reset(fb);

	CreatePipelineDepthStencil(width, height);

	VkImageTransition barrier;
	barrier.AddImage(&PipelineDepthStencil, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, true);
	for (int i = 0; i < NumPipelineImages; i++)
	{
		PipelineImage[i].Image = ImageBuilder()
			.Size(width, height)
			.Format(VK_FORMAT_R16G16B16A16_SFLOAT)
			.Usage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT)
			.DebugName("VkRenderBuffers.PipelineImage")
			.Create(fb->device);

		PipelineImage[i].View = ImageViewBuilder()
			.Image(PipelineImage[i].Image.get(), VK_FORMAT_R16G16B16A16_SFLOAT)
			.DebugName("VkRenderBuffers.PipelineView")
			.Create(fb->device);

		barrier.AddImage(&PipelineImage[i], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, true);
	}
	barrier.Execute(fb->GetCommands()->GetDrawCommands());
}

void VkRenderBuffers::CreateScene(int width, int height, VkSampleCountFlagBits samples)
{
	SceneColor.Reset(fb);
	SceneDepthStencil.Reset(fb);
	SceneNormal.Reset(fb);
	SceneFog.Reset(fb);

	CreateSceneColor(width, height, samples);
	CreateSceneDepthStencil(width, height, samples);
	CreateSceneNormal(width, height, samples);
	CreateSceneFog(width, height, samples);

	VkImageTransition()
		.AddImage(&SceneColor, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, true)
		.AddImage(&SceneDepthStencil, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, true)
		.AddImage(&SceneNormal, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, true)
		.AddImage(&SceneFog, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, true)
		.Execute(fb->GetCommands()->GetDrawCommands());
}

void VkRenderBuffers::CreateSceneColor(int width, int height, VkSampleCountFlagBits samples)
{
	SceneColor.Image = ImageBuilder()
		.Size(width, height)
		.Samples(samples)
		.Format(VK_FORMAT_R16G16B16A16_SFLOAT)
		.Usage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
		.DebugName("VkRenderBuffers.SceneColor")
		.Create(fb->device);

	SceneColor.View = ImageViewBuilder()
		.Image(SceneColor.Image.get(), VK_FORMAT_R16G16B16A16_SFLOAT)
		.DebugName("VkRenderBuffers.SceneColorView")
		.Create(fb->device);
}

void VkRenderBuffers::CreateSceneDepthStencil(int width, int height, VkSampleCountFlagBits samples)
{
	ImageBuilder builder;
	builder.Size(width, height);
	builder.Samples(samples);
	builder.Format(SceneDepthStencilFormat);
	builder.Usage(VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
	if (!builder.IsFormatSupported(fb->device))
	{
		SceneDepthStencilFormat = VK_FORMAT_D32_SFLOAT_S8_UINT;
		builder.Format(SceneDepthStencilFormat);
		if (!builder.IsFormatSupported(fb->device))
		{
			I_FatalError("This device does not support any of the required depth stencil image formats.");
		}
	}
	builder.DebugName("VkRenderBuffers.SceneDepthStencil");

	SceneDepthStencil.Image = builder.Create(fb->device);
	SceneDepthStencil.AspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;

	SceneDepthStencil.View = ImageViewBuilder()
		.Image(SceneDepthStencil.Image.get(), SceneDepthStencilFormat, VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)
		.DebugName("VkRenderBuffers.SceneDepthStencilView")
		.Create(fb->device);

	SceneDepthStencil.DepthOnlyView = ImageViewBuilder()
		.Image(SceneDepthStencil.Image.get(), SceneDepthStencilFormat, VK_IMAGE_ASPECT_DEPTH_BIT)
		.DebugName("VkRenderBuffers.SceneDepthView")
		.Create(fb->device);
}

void VkRenderBuffers::CreateSceneFog(int width, int height, VkSampleCountFlagBits samples)
{
	SceneFog.Image = ImageBuilder()
		.Size(width, height)
		.Samples(samples)
		.Format(VK_FORMAT_R8G8B8A8_UNORM)
		.Usage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
		.DebugName("VkRenderBuffers.SceneFog")
		.Create(fb->device);

	SceneFog.View = ImageViewBuilder()
		.Image(SceneFog.Image.get(), VK_FORMAT_R8G8B8A8_UNORM)
		.DebugName("VkRenderBuffers.SceneFogView")
		.Create(fb->device);
}

void VkRenderBuffers::CreateSceneNormal(int width, int height, VkSampleCountFlagBits samples)
{
	ImageBuilder builder;
	builder.Size(width, height);
	builder.Samples(samples);
	builder.Format(SceneNormalFormat);
	builder.Usage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
	if (!builder.IsFormatSupported(fb->device, VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT))
	{
		SceneNormalFormat = VK_FORMAT_R8G8B8A8_UNORM;
		builder.Format(SceneNormalFormat);
	}
	builder.DebugName("VkRenderBuffers.SceneNormal");

	SceneNormal.Image = builder.Create(fb->device);

	SceneNormal.View = ImageViewBuilder()
		.Image(SceneNormal.Image.get(), SceneNormalFormat)
		.DebugName("VkRenderBuffers.SceneNormalView")
		.Create(fb->device);
}

VulkanFramebuffer* VkRenderBuffers::GetOutput(VkPPRenderPassSetup* passSetup, const PPOutput& output, WhichDepthStencil stencilTest, int& framebufferWidth, int& framebufferHeight)
{
	VkTextureImage* tex = fb->GetTextureManager()->GetTexture(output.Type, output.Texture);

	VkImageView view;
	std::unique_ptr<VulkanFramebuffer>* framebufferptr = nullptr;
	int w, h;
	if (tex)
	{
		VkImageTransition imageTransition;
		imageTransition.AddImage(tex, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, output.Type == PPTextureType::NextPipelineTexture);
		if (stencilTest == WhichDepthStencil::Scene)
			imageTransition.AddImage(&fb->GetBuffers()->SceneDepthStencil, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, false);

		if (stencilTest == WhichDepthStencil::Pipeline)
			imageTransition.AddImage(&fb->GetBuffers()->PipelineDepthStencil, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, false);

		imageTransition.Execute(fb->GetCommands()->GetDrawCommands());

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

	auto& framebuffer = *framebufferptr;
	if (!framebuffer)
	{
		FramebufferBuilder builder;
		builder.RenderPass(passSetup->RenderPass.get());
		builder.Size(w, h);
		builder.AddAttachment(view);
		if (stencilTest == WhichDepthStencil::Scene)
			builder.AddAttachment(fb->GetBuffers()->SceneDepthStencil.View.get());
		if (stencilTest == WhichDepthStencil::Pipeline)
			builder.AddAttachment(fb->GetBuffers()->PipelineDepthStencil.View.get());
		builder.DebugName("PPOutputFB");
		framebuffer = builder.Create(fb->device);
	}

	framebufferWidth = w;
	framebufferHeight = h;
	return framebuffer.get();
}
