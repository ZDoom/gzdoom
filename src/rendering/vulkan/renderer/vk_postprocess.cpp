
#include "vk_postprocess.h"
#include "vk_renderbuffers.h"
#include "vulkan/shaders/vk_shader.h"
#include "vulkan/system/vk_builders.h"
#include "vulkan/system/vk_framebuffer.h"
#include "vulkan/system/vk_buffers.h"
#include "vulkan/system/vk_swapchain.h"
#include "vulkan/renderer/vk_renderstate.h"
#include "hwrenderer/utility/hw_cvars.h"
#include "hwrenderer/postprocessing/hw_presentshader.h"
#include "hwrenderer/postprocessing/hw_postprocess.h"
#include "hwrenderer/postprocessing/hw_postprocess_cvars.h"
#include "hwrenderer/utility/hw_vrmodes.h"
#include "hwrenderer/data/flatvertices.h"
#include "r_videoscale.h"
#include "w_wad.h"

EXTERN_CVAR(Int, gl_dither_bpc)

VkPostprocess::VkPostprocess()
{
}

VkPostprocess::~VkPostprocess()
{
}

void VkPostprocess::SetActiveRenderTarget()
{
	auto fb = GetVulkanFrameBuffer();
	auto buffers = fb->GetBuffers();
	fb->GetRenderPassManager()->SetRenderTarget(buffers->PipelineView[mCurrentPipelineImage].get(), buffers->GetWidth(), buffers->GetHeight());
}

void VkPostprocess::PostProcessScene(int fixedcm, const std::function<void()> &afterBloomDrawEndScene2D)
{
	auto fb = GetVulkanFrameBuffer();

	hw_postprocess.fixedcm = fixedcm;
	hw_postprocess.SceneWidth = fb->GetBuffers()->GetSceneWidth();
	hw_postprocess.SceneHeight = fb->GetBuffers()->GetSceneHeight();

	hw_postprocess.DeclareShaders();
	hw_postprocess.UpdateTextures();
	hw_postprocess.UpdateSteps();

	CompileEffectShaders();
	UpdateEffectTextures();

	RenderEffect("UpdateCameraExposure");
	//mCustomPostProcessShaders->Run("beforebloom");
	RenderEffect("BloomScene");
	SetActiveRenderTarget();
	afterBloomDrawEndScene2D();
	RenderEffect("TonemapScene");
	RenderEffect("ColormapScene");
	RenderEffect("LensDistortScene");
	RenderEffect("ApplyFXAA");
	//mCustomPostProcessShaders->Run("scene");
}

void VkPostprocess::BlitSceneToTexture()
{
	auto fb = GetVulkanFrameBuffer();

	fb->GetRenderState()->EndRenderPass();

	auto buffers = fb->GetBuffers();
	auto cmdbuffer = fb->GetDrawCommands();

	auto sceneColor = buffers->SceneColor.get();

	mCurrentPipelineImage = 0;

	PipelineBarrier barrier0;
	barrier0.addImage(sceneColor, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
	barrier0.addImage(buffers->PipelineImage[mCurrentPipelineImage].get(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
	barrier0.execute(cmdbuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

	if (buffers->GetSceneSamples() != VK_SAMPLE_COUNT_1_BIT)
	{
		VkImageResolve resolve = {};
		resolve.srcOffset = { 0, 0, 0 };
		resolve.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		resolve.srcSubresource.mipLevel = 0;
		resolve.srcSubresource.baseArrayLayer = 0;
		resolve.srcSubresource.layerCount = 1;
		resolve.dstOffset = { 0, 0, 0 };
		resolve.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		resolve.dstSubresource.mipLevel = 0;
		resolve.dstSubresource.baseArrayLayer = 0;
		resolve.dstSubresource.layerCount = 1;
		resolve.extent = { (uint32_t)sceneColor->width, (uint32_t)sceneColor->height, 1 };
		cmdbuffer->resolveImage(
			sceneColor->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			buffers->PipelineImage[mCurrentPipelineImage]->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1, &resolve);
	}
	else
	{
		VkImageBlit blit = {};
		blit.srcOffsets[0] = { 0, 0, 0 };
		blit.srcOffsets[1] = { sceneColor->width, sceneColor->height, 1 };
		blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.srcSubresource.mipLevel = 0;
		blit.srcSubresource.baseArrayLayer = 0;
		blit.srcSubresource.layerCount = 1;
		blit.dstOffsets[0] = { 0, 0, 0 };
		blit.dstOffsets[1] = { sceneColor->width, sceneColor->height, 1 };
		blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.dstSubresource.mipLevel = 0;
		blit.dstSubresource.baseArrayLayer = 0;
		blit.dstSubresource.layerCount = 1;
		cmdbuffer->blitImage(
			sceneColor->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			buffers->PipelineImage[mCurrentPipelineImage]->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1, &blit, VK_FILTER_NEAREST);
	}

	// Note: this destroys the sceneColor contents
	PipelineBarrier barrier1;
	barrier1.addImage(sceneColor, VK_IMAGE_LAYOUT_UNDEFINED/*VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL*/, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ACCESS_TRANSFER_READ_BIT, 0);
	barrier1.execute(cmdbuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

	buffers->SceneColorLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	buffers->PipelineLayout[mCurrentPipelineImage] = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
}

void VkPostprocess::DrawPresentTexture(const IntRect &box, bool applyGamma, bool clearBorders)
{
	auto fb = GetVulkanFrameBuffer();

	hw_postprocess.DeclareShaders();
	hw_postprocess.UpdateTextures();
	hw_postprocess.UpdateSteps();
	CompileEffectShaders();
	UpdateEffectTextures();

	PresentUniforms uniforms;
	if (!applyGamma /*|| framebuffer->IsHWGammaActive()*/)
	{
		uniforms.InvGamma = 1.0f;
		uniforms.Contrast = 1.0f;
		uniforms.Brightness = 0.0f;
		uniforms.Saturation = 1.0f;
	}
	else
	{
		uniforms.InvGamma = 1.0f / clamp<float>(Gamma, 0.1f, 4.f);
		uniforms.Contrast = clamp<float>(vid_contrast, 0.1f, 3.f);
		uniforms.Brightness = clamp<float>(vid_brightness, -0.8f, 0.8f);
		uniforms.Saturation = clamp<float>(vid_saturation, -15.0f, 15.f);
		uniforms.GrayFormula = static_cast<int>(gl_satformula);
	}
	uniforms.ColorScale = (gl_dither_bpc == -1) ? 255.0f : (float)((1 << gl_dither_bpc) - 1);
	uniforms.Scale = { screen->mScreenViewport.width / (float)fb->GetBuffers()->GetWidth(), screen->mScreenViewport.height / (float)fb->GetBuffers()->GetHeight() };

	PPStep step;
	step.ShaderName = "Present";
	step.Uniforms.Set(uniforms);
	step.Viewport = box;
	step.SetInputCurrent(0, ViewportLinearScale() ? PPFilterMode::Linear : PPFilterMode::Nearest);
	step.SetInputTexture(1, "PresentDither", PPFilterMode::Nearest, PPWrapMode::Repeat);
	step.SetOutputSwapChain();
	step.SetNoBlend();
	//if (clearBorders) step.SetClearBorders();

	TArray<PPStep> steps;
	steps.Push(step);
	hw_postprocess.Effects["Present"] = steps;

	RenderEffect("Present");
}

void VkPostprocess::AmbientOccludeScene(float m5)
{
	auto fb = GetVulkanFrameBuffer();

	hw_postprocess.SceneWidth = fb->GetBuffers()->GetSceneWidth();
	hw_postprocess.SceneHeight = fb->GetBuffers()->GetSceneHeight();
	hw_postprocess.m5 = m5;

	hw_postprocess.DeclareShaders();
	hw_postprocess.UpdateTextures();
	hw_postprocess.UpdateSteps();

	CompileEffectShaders();
	UpdateEffectTextures();

	RenderEffect("AmbientOccludeScene");
}

void VkPostprocess::BlurScene(float gameinfobluramount)
{
	auto fb = GetVulkanFrameBuffer();
	hw_postprocess.SceneWidth = fb->GetBuffers()->GetSceneWidth();
	hw_postprocess.SceneHeight = fb->GetBuffers()->GetSceneHeight();
	hw_postprocess.gameinfobluramount = gameinfobluramount;

	hw_postprocess.DeclareShaders();
	hw_postprocess.UpdateTextures();
	hw_postprocess.UpdateSteps();

	CompileEffectShaders();
	UpdateEffectTextures();

	auto vrmode = VRMode::GetVRMode(true);
	int eyeCount = vrmode->mEyeCount;
	for (int i = 0; i < eyeCount; ++i)
	{
		RenderEffect("BlurScene");
		if (eyeCount - i > 1) NextEye(eyeCount);
	}
}

void VkPostprocess::ClearTonemapPalette()
{
	hw_postprocess.Textures.Remove("Tonemap.Palette");
}

void VkPostprocess::BeginFrame()
{
	mFrameDescriptorSets.clear();

	if (!mDescriptorPool)
	{
		DescriptorPoolBuilder builder;
		builder.addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 100);
		builder.setMaxSets(100);
		mDescriptorPool = builder.create(GetVulkanFrameBuffer()->device);
	}
}

void VkPostprocess::RenderBuffersReset()
{
	mRenderPassSetup.clear();
}

void VkPostprocess::UpdateEffectTextures()
{
	auto fb = GetVulkanFrameBuffer();

	TMap<FString, PPTextureDesc>::Iterator it(hw_postprocess.Textures);
	TMap<FString, PPTextureDesc>::Pair *pair;
	while (it.NextPair(pair))
	{
		const auto &desc = pair->Value;
		auto &vktex = mTextures[pair->Key];

		if (vktex && (vktex->Image->width != desc.Width || vktex->Image->height != desc.Height))
			vktex.reset();

		if (!vktex)
		{
			vktex.reset(new VkPPTexture());

			VkFormat format;
			int pixelsize;
			switch (pair->Value.Format)
			{
			default:
			case PixelFormat::Rgba8: format = VK_FORMAT_R8G8B8A8_UNORM; pixelsize = 4; break;
			case PixelFormat::Rgba16f: format = VK_FORMAT_R16G16B16A16_SFLOAT; pixelsize = 8; break;
			case PixelFormat::R32f: format = VK_FORMAT_R32_SFLOAT; pixelsize = 4; break;
			case PixelFormat::Rg16f: format = VK_FORMAT_R16G16_SFLOAT; pixelsize = 4; break;
			case PixelFormat::Rgba16_snorm: format = VK_FORMAT_R16G16B16A16_SNORM; pixelsize = 8; break;
			}

			ImageBuilder imgbuilder;
			imgbuilder.setFormat(format);
			imgbuilder.setSize(desc.Width, desc.Height);
			if (desc.Data)
				imgbuilder.setUsage(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
			else
				imgbuilder.setUsage(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
			if (!imgbuilder.isFormatSupported(fb->device))
				I_FatalError("Vulkan device does not support the image format required by %s\n", pair->Key.GetChars());
			vktex->Image = imgbuilder.create(fb->device);
			vktex->Format = format;

			ImageViewBuilder viewbuilder;
			viewbuilder.setImage(vktex->Image.get(), format);
			vktex->View = viewbuilder.create(fb->device);

			if (desc.Data)
			{
				size_t totalsize = desc.Width * desc.Height * pixelsize;
				BufferBuilder stagingbuilder;
				stagingbuilder.setSize(totalsize);
				stagingbuilder.setUsage(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
				vktex->Staging = stagingbuilder.create(fb->device);

				PipelineBarrier barrier0;
				barrier0.addImage(vktex->Image.get(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, VK_ACCESS_TRANSFER_WRITE_BIT);
				barrier0.execute(fb->GetUploadCommands(), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

				void *data = vktex->Staging->Map(0, totalsize);
				memcpy(data, desc.Data.get(), totalsize);
				vktex->Staging->Unmap();

				VkBufferImageCopy region = {};
				region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				region.imageSubresource.layerCount = 1;
				region.imageExtent.depth = 1;
				region.imageExtent.width = desc.Width;
				region.imageExtent.height = desc.Height;
				fb->GetUploadCommands()->copyBufferToImage(vktex->Staging->buffer, vktex->Image->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

				PipelineBarrier barrier1;
				barrier1.addImage(vktex->Image.get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
				barrier1.execute(fb->GetUploadCommands(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
				vktex->Layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			}
			else
			{
				PipelineBarrier barrier;
				barrier.addImage(vktex->Image.get(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT);
				barrier.execute(fb->GetUploadCommands(), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
				vktex->Layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			}
		}
	}
}

void VkPostprocess::CompileEffectShaders()
{
	auto fb = GetVulkanFrameBuffer();

	TMap<FString, PPShader>::Iterator it(hw_postprocess.Shaders);
	TMap<FString, PPShader>::Pair *pair;
	while (it.NextPair(pair))
	{
		const auto &desc = pair->Value;
		auto &vkshader = mShaders[pair->Key];
		if (!vkshader)
		{
			vkshader.reset(new VkPPShader());

			FString prolog;
			if (!desc.Uniforms.empty())
				prolog = UniformBlockDecl::Create("Uniforms", desc.Uniforms, -1);
			prolog += desc.Defines;

			ShaderBuilder vertbuilder;
			vertbuilder.setVertexShader(LoadShaderCode(desc.VertexShader, "", desc.Version));
			vkshader->VertexShader = vertbuilder.create(fb->device);

			ShaderBuilder fragbuilder;
			fragbuilder.setFragmentShader(LoadShaderCode(desc.FragmentShader, prolog, desc.Version));
			vkshader->FragmentShader = fragbuilder.create(fb->device);
		}
	}
}

FString VkPostprocess::LoadShaderCode(const FString &lumpName, const FString &defines, int version)
{
	int lump = Wads.CheckNumForFullName(lumpName, 0);
	if (lump == -1) I_FatalError("Unable to load '%s'", lumpName.GetChars());
	FString code = Wads.ReadLump(lump).GetString().GetChars();

	FString patchedCode;
	patchedCode.AppendFormat("#version %d\n", 450);
	patchedCode << defines;
	patchedCode << "#line 1\n";
	patchedCode << code;
	return patchedCode;
}

void VkPostprocess::RenderEffect(const FString &name)
{
	GetVulkanFrameBuffer()->GetRenderState()->EndRenderPass();

	if (hw_postprocess.Effects[name].Size() == 0)
		return;

	for (const PPStep &step : hw_postprocess.Effects[name])
	{
		VkPPRenderPassKey key;
		key.BlendMode = step.BlendMode;
		key.InputTextures = step.Textures.Size();
		key.Uniforms = step.Uniforms.Data.Size();
		key.Shader = mShaders[step.ShaderName].get();
		if (step.Output.Type == PPTextureType::PPTexture)
			key.OutputFormat = mTextures[step.Output.Texture]->Format;
		else
			key.OutputFormat = VK_FORMAT_R16G16B16A16_SFLOAT;

		auto &passSetup = mRenderPassSetup[key];
		if (!passSetup)
			passSetup.reset(new VkPPRenderPassSetup(key));

		VulkanDescriptorSet *input = GetInput(passSetup.get(), step.Textures);
		VulkanFramebuffer *output = GetOutput(passSetup.get(), step.Output);

		RenderScreenQuad(passSetup.get(), input, output, step.Viewport.left, step.Viewport.top, step.Viewport.width, step.Viewport.height, step.Uniforms.Data.Data(), step.Uniforms.Data.Size());

		// Advance to next PP texture if our output was sent there
		if (step.Output.Type == PPTextureType::NextPipelineTexture)
		{
			mCurrentPipelineImage = (mCurrentPipelineImage + 1) % VkRenderBuffers::NumPipelineImages;
		}
	}
}

void VkPostprocess::RenderScreenQuad(VkPPRenderPassSetup *passSetup, VulkanDescriptorSet *descriptorSet, VulkanFramebuffer *framebuffer, int x, int y, int width, int height, const void *pushConstants, uint32_t pushConstantsSize)
{
	auto fb = GetVulkanFrameBuffer();
	auto cmdbuffer = fb->GetDrawCommands();

	RenderPassBegin beginInfo;
	beginInfo.setRenderPass(passSetup->RenderPass.get());
	beginInfo.setRenderArea(x, y, width, height);
	beginInfo.setFramebuffer(framebuffer);

	VkViewport viewport = { };
	viewport.x = x;
	viewport.y = y;
	viewport.width = width;
	viewport.height = height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkBuffer vertexBuffers[] = { static_cast<VKVertexBuffer*>(screen->mVertexData->GetBufferObjects().first)->mBuffer->buffer };
	VkDeviceSize offsets[] = { 0 };

	cmdbuffer->beginRenderPass(beginInfo);
	cmdbuffer->bindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, passSetup->Pipeline.get());
	cmdbuffer->bindDescriptorSet(VK_PIPELINE_BIND_POINT_GRAPHICS, passSetup->PipelineLayout.get(), 0, descriptorSet);
	cmdbuffer->bindVertexBuffers(0, 1, vertexBuffers, offsets);
	cmdbuffer->setViewport(0, 1, &viewport);
	if (pushConstantsSize > 0)
		cmdbuffer->pushConstants(passSetup->PipelineLayout.get(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, pushConstantsSize, pushConstants);
	cmdbuffer->draw(4, 1, FFlatVertexBuffer::PRESENT_INDEX, 0);
	cmdbuffer->endRenderPass();
}

VulkanDescriptorSet *VkPostprocess::GetInput(VkPPRenderPassSetup *passSetup, const TArray<PPTextureInput> &textures)
{
	auto fb = GetVulkanFrameBuffer();
	auto descriptors = mDescriptorPool->allocate(passSetup->DescriptorLayout.get());

	WriteDescriptors write;
	PipelineBarrier barrier;
	bool needbarrier = false;

	VkPipelineStageFlags srcPipelineBits = 0;

	for (unsigned int index = 0; index < textures.Size(); index++)
	{
		const PPTextureInput &input = textures[index];
		VulkanSampler *sampler = GetSampler(input.Filter, input.Wrap);
		TextureImage tex = GetTexture(input.Type, input.Texture);

		write.addCombinedImageSampler(descriptors.get(), index, tex.view, sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		if (*tex.layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
		{
			srcPipelineBits |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			barrier.addImage(tex.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
			*tex.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			needbarrier = true;
		}
		else if (*tex.layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
		{
			srcPipelineBits |= VK_PIPELINE_STAGE_TRANSFER_BIT;
			barrier.addImage(tex.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
			*tex.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		}
	}

	write.updateSets(fb->device);
	if (needbarrier)
		barrier.execute(fb->GetDrawCommands(), srcPipelineBits, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

	mFrameDescriptorSets.push_back(std::move(descriptors));
	return mFrameDescriptorSets.back().get();
}

VulkanFramebuffer *VkPostprocess::GetOutput(VkPPRenderPassSetup *passSetup, const PPOutput &output)
{
	auto fb = GetVulkanFrameBuffer();

	TextureImage tex = GetTexture(output.Type, output.Texture);

	if (tex.layout && *tex.layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
	{
		PipelineBarrier barrier;
		barrier.addImage(tex.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
		barrier.execute(fb->GetDrawCommands(), VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
		*tex.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	}
	else if (tex.layout && *tex.layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
	{
		PipelineBarrier barrier;
		barrier.addImage(tex.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
		barrier.execute(fb->GetDrawCommands(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
		*tex.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	}

	VkImageView view;
	int w, h;
	if (tex.view)
	{
		view = tex.view->view;
		w = tex.image->width;
		h = tex.image->height;
	}
	else
	{
		view = fb->device->swapChain->swapChainImageViews[fb->device->presentImageIndex];
		w = fb->device->swapChain->actualExtent.width;
		h = fb->device->swapChain->actualExtent.height;
	}

	auto &framebuffer = passSetup->Framebuffers[view];
	if (!framebuffer)
	{
		FramebufferBuilder builder;
		builder.setRenderPass(passSetup->RenderPass.get());
		builder.setSize(w, h);
		builder.addAttachment(view);
		framebuffer = builder.create(GetVulkanFrameBuffer()->device);
	}

	return framebuffer.get();
}

VkPostprocess::TextureImage VkPostprocess::GetTexture(const PPTextureType &type, const PPTextureName &name)
{
	auto fb = GetVulkanFrameBuffer();
	TextureImage tex = {};

	if (type == PPTextureType::CurrentPipelineTexture || type == PPTextureType::NextPipelineTexture)
	{
		int idx = mCurrentPipelineImage;
		if (type == PPTextureType::NextPipelineTexture)
			idx = (idx + 1) % VkRenderBuffers::NumPipelineImages;

		tex.image = fb->GetBuffers()->PipelineImage[idx].get();
		tex.view = fb->GetBuffers()->PipelineView[idx].get();
		tex.layout = &fb->GetBuffers()->PipelineLayout[idx];
	}
	else if (type == PPTextureType::PPTexture)
	{
		tex.image = mTextures[name]->Image.get();
		tex.view = mTextures[name]->View.get();
		tex.layout = &mTextures[name]->Layout;
	}
	else if (type == PPTextureType::SceneColor)
	{
		tex.image = fb->GetBuffers()->SceneColor.get();
		tex.view = fb->GetBuffers()->SceneColorView.get();
		tex.layout = &fb->GetBuffers()->SceneColorLayout;
	}
	else if (type == PPTextureType::SceneNormal)
	{
		tex.image = fb->GetBuffers()->SceneNormal.get();
		tex.view = fb->GetBuffers()->SceneNormalView.get();
		tex.layout = &fb->GetBuffers()->SceneNormalLayout;
	}
	else if (type == PPTextureType::SceneFog)
	{
		tex.image = fb->GetBuffers()->SceneFog.get();
		tex.view = fb->GetBuffers()->SceneFogView.get();
		tex.layout = &fb->GetBuffers()->SceneFogLayout;
	}
	else if (type == PPTextureType::SceneDepth)
	{
		tex.image = fb->GetBuffers()->SceneDepthStencil.get();
		tex.view = fb->GetBuffers()->SceneDepthView.get();
		tex.layout = &fb->GetBuffers()->SceneDepthStencilLayout;
	}
	else if (type == PPTextureType::SwapChain)
	{
		tex.image = nullptr;
		tex.view = nullptr;
		tex.layout = nullptr;
	}
	else
	{
		I_FatalError("VkPostprocess::GetTexture not implemented yet for this texture type");
	}

	return tex;
}

VulkanSampler *VkPostprocess::GetSampler(PPFilterMode filter, PPWrapMode wrap)
{
	int index = (((int)filter) << 2) | (int)wrap;
	auto &sampler = mSamplers[index];
	if (sampler)
		return sampler.get();

	SamplerBuilder builder;
	builder.setMipmapMode(VK_SAMPLER_MIPMAP_MODE_NEAREST);
	builder.setMinFilter(filter == PPFilterMode::Nearest ? VK_FILTER_NEAREST : VK_FILTER_LINEAR);
	builder.setMagFilter(filter == PPFilterMode::Nearest ? VK_FILTER_NEAREST : VK_FILTER_LINEAR);
	builder.setAddressMode(wrap == PPWrapMode::Clamp ? VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE : VK_SAMPLER_ADDRESS_MODE_REPEAT);
	sampler = builder.create(GetVulkanFrameBuffer()->device);
	return sampler.get();
}

void VkPostprocess::NextEye(int eyeCount)
{
}

/////////////////////////////////////////////////////////////////////////////

VkPPRenderPassSetup::VkPPRenderPassSetup(const VkPPRenderPassKey &key)
{
	CreateDescriptorLayout(key);
	CreatePipelineLayout(key);
	CreateRenderPass(key);
	CreatePipeline(key);
}

void VkPPRenderPassSetup::CreateDescriptorLayout(const VkPPRenderPassKey &key)
{
	DescriptorSetLayoutBuilder builder;
	for (int i = 0; i < key.InputTextures; i++)
		builder.addBinding(i, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
	DescriptorLayout = builder.create(GetVulkanFrameBuffer()->device);
}

void VkPPRenderPassSetup::CreatePipelineLayout(const VkPPRenderPassKey &key)
{
	PipelineLayoutBuilder builder;
	builder.addSetLayout(DescriptorLayout.get());
	if (key.Uniforms > 0)
		builder.addPushConstantRange(VK_SHADER_STAGE_FRAGMENT_BIT, 0, key.Uniforms);
	PipelineLayout = builder.create(GetVulkanFrameBuffer()->device);
}

void VkPPRenderPassSetup::CreatePipeline(const VkPPRenderPassKey &key)
{
	GraphicsPipelineBuilder builder;
	builder.addVertexShader(key.Shader->VertexShader.get());
	builder.addFragmentShader(key.Shader->FragmentShader.get());

	builder.addVertexBufferBinding(0, sizeof(FFlatVertex));
	builder.addVertexAttribute(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(FFlatVertex, x));
	builder.addVertexAttribute(1, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(FFlatVertex, u));
	builder.addDynamicState(VK_DYNAMIC_STATE_VIEWPORT);
	builder.setViewport(0.0f, 0.0f, (float)SCREENWIDTH, (float)SCREENHEIGHT);
	builder.setTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP);
	builder.setBlendMode(key.BlendMode);
	builder.setLayout(PipelineLayout.get());
	builder.setRenderPass(RenderPass.get());
	Pipeline = builder.create(GetVulkanFrameBuffer()->device);
}

void VkPPRenderPassSetup::CreateRenderPass(const VkPPRenderPassKey &key)
{
	RenderPassBuilder builder;
	builder.addColorAttachment(false, key.OutputFormat, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	builder.addSubpass();
	builder.addSubpassColorAttachmentRef(0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	builder.addExternalSubpassDependency(
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		VK_ACCESS_COLOR_ATTACHMENT_READ_BIT);
	RenderPass = builder.create(GetVulkanFrameBuffer()->device);
}
