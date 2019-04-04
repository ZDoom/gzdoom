
#include "vk_postprocess.h"
#include "vk_renderbuffers.h"
#include "vulkan/shaders/vk_shader.h"
#include "vulkan/system/vk_builders.h"
#include "vulkan/system/vk_framebuffer.h"
#include "vulkan/system/vk_buffers.h"
#include "vulkan/system/vk_swapchain.h"
#include "vulkan/renderer/vk_renderstate.h"
#include "hwrenderer/utility/hw_cvars.h"
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

	VkPPImageTransition imageTransition;
	imageTransition.addImage(buffers->PipelineImage[mCurrentPipelineImage].get(), &buffers->PipelineLayout[mCurrentPipelineImage], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, false);
	imageTransition.execute(fb->GetDrawCommands());

	fb->GetRenderState()->SetRenderTarget(buffers->PipelineView[mCurrentPipelineImage].get(), buffers->GetWidth(), buffers->GetHeight(), VK_SAMPLE_COUNT_1_BIT);
}

void VkPostprocess::PostProcessScene(int fixedcm, const std::function<void()> &afterBloomDrawEndScene2D)
{
	auto fb = GetVulkanFrameBuffer();
	int sceneWidth = fb->GetBuffers()->GetSceneWidth();
	int sceneHeight = fb->GetBuffers()->GetSceneHeight();

	VkPPRenderState renderstate;

	hw_postprocess.exposure.Render(&renderstate, sceneWidth, sceneHeight);
	//mCustomPostProcessShaders->Run("beforebloom");
	hw_postprocess.bloom.RenderBloom(&renderstate, sceneWidth, sceneHeight, fixedcm);

	SetActiveRenderTarget();
	afterBloomDrawEndScene2D();

	hw_postprocess.tonemap.Render(&renderstate);
	hw_postprocess.colormap.Render(&renderstate, fixedcm);
	hw_postprocess.lens.Render(&renderstate);
	hw_postprocess.fxaa.Render(&renderstate);
	//mCustomPostProcessShaders->Run("scene");
}

void VkPostprocess::BlitSceneToPostprocess()
{
	auto fb = GetVulkanFrameBuffer();

	fb->GetRenderState()->EndRenderPass();

	auto buffers = fb->GetBuffers();
	auto cmdbuffer = fb->GetDrawCommands();

	mCurrentPipelineImage = 0;

	VkPPImageTransition imageTransition0;
	imageTransition0.addImage(buffers->SceneColor.get(), &buffers->SceneColorLayout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, false);
	imageTransition0.addImage(buffers->PipelineImage[mCurrentPipelineImage].get(), &buffers->PipelineLayout[mCurrentPipelineImage], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, true);
	imageTransition0.execute(fb->GetDrawCommands());

	if (buffers->GetSceneSamples() != VK_SAMPLE_COUNT_1_BIT)
	{
		auto sceneColor = buffers->SceneColor.get();
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
		auto sceneColor = buffers->SceneColor.get();
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
}

void VkPostprocess::ImageTransitionScene(bool undefinedSrcLayout)
{
	auto fb = GetVulkanFrameBuffer();
	auto buffers = fb->GetBuffers();

	VkPPImageTransition imageTransition;
	imageTransition.addImage(buffers->SceneColor.get(), &buffers->SceneColorLayout, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, undefinedSrcLayout);
	imageTransition.addImage(buffers->SceneFog.get(), &buffers->SceneFogLayout, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, undefinedSrcLayout);
	imageTransition.addImage(buffers->SceneNormal.get(), &buffers->SceneNormalLayout, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, undefinedSrcLayout);
	imageTransition.addImage(buffers->SceneDepthStencil.get(), &buffers->SceneDepthStencilLayout, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, undefinedSrcLayout);
	imageTransition.execute(fb->GetDrawCommands());
}

void VkPostprocess::BlitCurrentToImage(VulkanImage *dstimage, VkImageLayout *dstlayout, VkImageLayout finallayout)
{
	auto fb = GetVulkanFrameBuffer();

	fb->GetRenderState()->EndRenderPass();

	auto srcimage = fb->GetBuffers()->PipelineImage[mCurrentPipelineImage].get();
	auto srclayout = &fb->GetBuffers()->PipelineLayout[mCurrentPipelineImage];
	auto cmdbuffer = fb->GetDrawCommands();

	*dstlayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; // needed by VkPPImageTransition.addImage. Actual layout is undefined.

	VkPPImageTransition imageTransition0;
	imageTransition0.addImage(srcimage, srclayout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, false);
	imageTransition0.addImage(dstimage, dstlayout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, true);
	imageTransition0.execute(cmdbuffer);

	VkImageBlit blit = {};
	blit.srcOffsets[0] = { 0, 0, 0 };
	blit.srcOffsets[1] = { srcimage->width, srcimage->height, 1 };
	blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	blit.srcSubresource.mipLevel = 0;
	blit.srcSubresource.baseArrayLayer = 0;
	blit.srcSubresource.layerCount = 1;
	blit.dstOffsets[0] = { 0, 0, 0 };
	blit.dstOffsets[1] = { dstimage->width, dstimage->height, 1 };
	blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	blit.dstSubresource.mipLevel = 0;
	blit.dstSubresource.baseArrayLayer = 0;
	blit.dstSubresource.layerCount = 1;
	cmdbuffer->blitImage(
		srcimage->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		dstimage->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1, &blit, VK_FILTER_NEAREST);

	VkPPImageTransition imageTransition1;
	imageTransition1.addImage(dstimage, dstlayout, finallayout, false);
	imageTransition1.execute(cmdbuffer);
}

void VkPostprocess::DrawPresentTexture(const IntRect &box, bool applyGamma, bool clearBorders)
{
	auto fb = GetVulkanFrameBuffer();

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
	uniforms.Scale = { screen->mScreenViewport.width / (float)fb->GetBuffers()->GetWidth(), -screen->mScreenViewport.height / (float)fb->GetBuffers()->GetHeight() };
	uniforms.Offset = { 0.0f, 1.0f };

	if (applyGamma && fb->swapChain->swapChainFormat.colorSpace == VK_COLOR_SPACE_HDR10_ST2084_EXT)
	{
		uniforms.InvGamma *= 2.2f;
	}

	VkPPRenderState renderstate;
	renderstate.Shader = &hw_postprocess.present.Present;
	renderstate.Uniforms.Set(uniforms);
	renderstate.Viewport = box;
	renderstate.SetInputCurrent(0, ViewportLinearScale() ? PPFilterMode::Linear : PPFilterMode::Nearest);
	renderstate.SetInputTexture(1, &hw_postprocess.present.Dither, PPFilterMode::Nearest, PPWrapMode::Repeat);
	renderstate.SetOutputSwapChain();
	renderstate.SetNoBlend();
	//if (clearBorders) renderstate.SetClearBorders();
	renderstate.Draw();
}

void VkPostprocess::AmbientOccludeScene(float m5)
{
	auto fb = GetVulkanFrameBuffer();
	int sceneWidth = fb->GetBuffers()->GetSceneWidth();
	int sceneHeight = fb->GetBuffers()->GetSceneHeight();

	VkPPRenderState renderstate;
	hw_postprocess.ssao.Render(&renderstate, m5, sceneWidth, sceneHeight);

	ImageTransitionScene(false);
}

void VkPostprocess::BlurScene(float gameinfobluramount)
{
	auto fb = GetVulkanFrameBuffer();
	int sceneWidth = fb->GetBuffers()->GetSceneWidth();
	int sceneHeight = fb->GetBuffers()->GetSceneHeight();

	VkPPRenderState renderstate;

	auto vrmode = VRMode::GetVRMode(true);
	int eyeCount = vrmode->mEyeCount;
	for (int i = 0; i < eyeCount; ++i)
	{
		hw_postprocess.bloom.RenderBlur(&renderstate, sceneWidth, sceneHeight, gameinfobluramount);
		if (eyeCount - i > 1) NextEye(eyeCount);
	}
}

void VkPostprocess::ClearTonemapPalette()
{
	hw_postprocess.tonemap.ClearTonemapPalette();
}

void VkPostprocess::UpdateShadowMap()
{
	if (screen->mShadowMap.PerformUpdate())
	{
		VkPPRenderState renderstate;
		hw_postprocess.shadowmap.Update(&renderstate);

		auto fb = GetVulkanFrameBuffer();
		auto buffers = fb->GetBuffers();

		VkPPImageTransition imageTransition;
		imageTransition.addImage(buffers->Shadowmap.get(), &buffers->ShadowmapLayout, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, false);
		imageTransition.execute(fb->GetDrawCommands());

		screen->mShadowMap.FinishUpdate();
	}
}

void VkPostprocess::BeginFrame()
{
	mFrameDescriptorSets.clear();

	if (!mDescriptorPool)
	{
		DescriptorPoolBuilder builder;
		builder.addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 200);
		builder.addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4);
		builder.setMaxSets(100);
		mDescriptorPool = builder.create(GetVulkanFrameBuffer()->device);
		mDescriptorPool->SetDebugName("VkPostprocess.mDescriptorPool");
	}
}

void VkPostprocess::RenderBuffersReset()
{
	mRenderPassSetup.clear();
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
	sampler->SetDebugName("VkPostprocess.mSamplers");
	return sampler.get();
}

void VkPostprocess::NextEye(int eyeCount)
{
}

/////////////////////////////////////////////////////////////////////////////

VkPPTexture::VkPPTexture(PPTexture *texture)
{
	auto fb = GetVulkanFrameBuffer();

	VkFormat format;
	int pixelsize;
	switch (texture->Format)
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
	imgbuilder.setSize(texture->Width, texture->Height);
	if (texture->Data)
		imgbuilder.setUsage(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
	else
		imgbuilder.setUsage(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
	if (!imgbuilder.isFormatSupported(fb->device))
		I_FatalError("Vulkan device does not support the image format required by a postprocess texture\n");
	Image = imgbuilder.create(fb->device);
	Image->SetDebugName("VkPPTexture");
	Format = format;

	ImageViewBuilder viewbuilder;
	viewbuilder.setImage(Image.get(), format);
	View = viewbuilder.create(fb->device);
	View->SetDebugName("VkPPTextureView");

	if (texture->Data)
	{
		size_t totalsize = texture->Width * texture->Height * pixelsize;
		BufferBuilder stagingbuilder;
		stagingbuilder.setSize(totalsize);
		stagingbuilder.setUsage(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
		Staging = stagingbuilder.create(fb->device);
		Staging->SetDebugName("VkPPTextureStaging");

		PipelineBarrier barrier0;
		barrier0.addImage(Image.get(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, VK_ACCESS_TRANSFER_WRITE_BIT);
		barrier0.execute(fb->GetTransferCommands(), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

		void *data = Staging->Map(0, totalsize);
		memcpy(data, texture->Data.get(), totalsize);
		Staging->Unmap();

		VkBufferImageCopy region = {};
		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.layerCount = 1;
		region.imageExtent.depth = 1;
		region.imageExtent.width = texture->Width;
		region.imageExtent.height = texture->Height;
		fb->GetTransferCommands()->copyBufferToImage(Staging->buffer, Image->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

		PipelineBarrier barrier1;
		barrier1.addImage(Image.get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
		barrier1.execute(fb->GetTransferCommands(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
		Layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	}
	else
	{
		PipelineBarrier barrier;
		barrier.addImage(Image.get(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT);
		barrier.execute(fb->GetTransferCommands(), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
		Layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	}
}

/////////////////////////////////////////////////////////////////////////////

VkPPShader::VkPPShader(PPShader *shader)
{
	auto fb = GetVulkanFrameBuffer();

	FString prolog;
	if (!shader->Uniforms.empty())
		prolog = UniformBlockDecl::Create("Uniforms", shader->Uniforms, -1);
	prolog += shader->Defines;

	ShaderBuilder vertbuilder;
	vertbuilder.setVertexShader(LoadShaderCode(shader->VertexShader, "", shader->Version));
	VertexShader = vertbuilder.create(fb->device);
	VertexShader->SetDebugName(shader->VertexShader.GetChars());

	ShaderBuilder fragbuilder;
	fragbuilder.setFragmentShader(LoadShaderCode(shader->FragmentShader, prolog, shader->Version));
	FragmentShader = fragbuilder.create(fb->device);
	FragmentShader->SetDebugName(shader->FragmentShader.GetChars());
}

FString VkPPShader::LoadShaderCode(const FString &lumpName, const FString &defines, int version)
{
	int lump = Wads.CheckNumForFullName(lumpName);
	if (lump == -1) I_FatalError("Unable to load '%s'", lumpName.GetChars());
	FString code = Wads.ReadLump(lump).GetString().GetChars();

	FString patchedCode;
	patchedCode.AppendFormat("#version %d\n", 450);
	patchedCode << defines;
	patchedCode << "#line 1\n";
	patchedCode << code;
	return patchedCode;
}

/////////////////////////////////////////////////////////////////////////////

void VkPPRenderState::Draw()
{
	auto fb = GetVulkanFrameBuffer();
	auto pp = fb->GetPostprocess();

	fb->GetRenderState()->EndRenderPass();

	VkPPRenderPassKey key;
	key.BlendMode = BlendMode;
	key.InputTextures = Textures.Size();
	key.Uniforms = Uniforms.Data.Size();
	key.Shader = GetVkShader(Shader);
	key.SwapChain = (Output.Type == PPTextureType::SwapChain);
	key.ShadowMapBuffers = ShadowMapBuffers;
	if (Output.Type == PPTextureType::PPTexture)
		key.OutputFormat = GetVkTexture(Output.Texture)->Format;
	else if (Output.Type == PPTextureType::SwapChain)
		key.OutputFormat = GetVulkanFrameBuffer()->swapChain->swapChainFormat.format;
	else if (Output.Type == PPTextureType::ShadowMap)
		key.OutputFormat = VK_FORMAT_R32_SFLOAT;
	else
		key.OutputFormat = VK_FORMAT_R16G16B16A16_SFLOAT;

	if (Output.Type == PPTextureType::SceneColor)
		key.Samples = fb->GetBuffers()->GetSceneSamples();
	else
		key.Samples = VK_SAMPLE_COUNT_1_BIT;

	auto &passSetup = pp->mRenderPassSetup[key];
	if (!passSetup)
		passSetup.reset(new VkPPRenderPassSetup(key));

	int framebufferWidth = 0, framebufferHeight = 0;
	VulkanDescriptorSet *input = GetInput(passSetup.get(), Textures, ShadowMapBuffers);
	VulkanFramebuffer *output = GetOutput(passSetup.get(), Output, framebufferWidth, framebufferHeight);

	RenderScreenQuad(passSetup.get(), input, output, framebufferWidth, framebufferHeight, Viewport.left, Viewport.top, Viewport.width, Viewport.height, Uniforms.Data.Data(), Uniforms.Data.Size());

	// Advance to next PP texture if our output was sent there
	if (Output.Type == PPTextureType::NextPipelineTexture)
	{
		pp->mCurrentPipelineImage = (pp->mCurrentPipelineImage + 1) % VkRenderBuffers::NumPipelineImages;
	}
}

void VkPPRenderState::RenderScreenQuad(VkPPRenderPassSetup *passSetup, VulkanDescriptorSet *descriptorSet, VulkanFramebuffer *framebuffer, int framebufferWidth, int framebufferHeight, int x, int y, int width, int height, const void *pushConstants, uint32_t pushConstantsSize)
{
	auto fb = GetVulkanFrameBuffer();
	auto cmdbuffer = fb->GetDrawCommands();

	VkViewport viewport = { };
	viewport.x = x;
	viewport.y = y;
	viewport.width = width;
	viewport.height = height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor = { };
	scissor.offset.x = 0;
	scissor.offset.y = 0;
	scissor.extent.width = framebufferWidth;
	scissor.extent.height = framebufferHeight;

	RenderPassBegin beginInfo;
	beginInfo.setRenderPass(passSetup->RenderPass.get());
	beginInfo.setRenderArea(0, 0, framebufferWidth, framebufferHeight);
	beginInfo.setFramebuffer(framebuffer);
	beginInfo.addClearColor(0.0f, 0.0f, 0.0f, 1.0f);

	VkBuffer vertexBuffers[] = { static_cast<VKVertexBuffer*>(screen->mVertexData->GetBufferObjects().first)->mBuffer->buffer };
	VkDeviceSize offsets[] = { 0 };

	cmdbuffer->beginRenderPass(beginInfo);
	cmdbuffer->bindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, passSetup->Pipeline.get());
	cmdbuffer->bindDescriptorSet(VK_PIPELINE_BIND_POINT_GRAPHICS, passSetup->PipelineLayout.get(), 0, descriptorSet);
	cmdbuffer->bindVertexBuffers(0, 1, vertexBuffers, offsets);
	cmdbuffer->setViewport(0, 1, &viewport);
	cmdbuffer->setScissor(0, 1, &scissor);
	if (pushConstantsSize > 0)
		cmdbuffer->pushConstants(passSetup->PipelineLayout.get(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, pushConstantsSize, pushConstants);
	cmdbuffer->draw(4, 1, FFlatVertexBuffer::PRESENT_INDEX, 0);
	cmdbuffer->endRenderPass();
}

VulkanDescriptorSet *VkPPRenderState::GetInput(VkPPRenderPassSetup *passSetup, const TArray<PPTextureInput> &textures, bool bindShadowMapBuffers)
{
	auto fb = GetVulkanFrameBuffer();
	auto pp = fb->GetPostprocess();
	auto descriptors = pp->mDescriptorPool->allocate(passSetup->DescriptorLayout.get());
	descriptors->SetDebugName("VkPostprocess.descriptors");

	WriteDescriptors write;
	VkPPImageTransition imageTransition;

	for (unsigned int index = 0; index < textures.Size(); index++)
	{
		const PPTextureInput &input = textures[index];
		VulkanSampler *sampler = pp->GetSampler(input.Filter, input.Wrap);
		TextureImage tex = GetTexture(input.Type, input.Texture);

		write.addCombinedImageSampler(descriptors.get(), index, tex.view, sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		imageTransition.addImage(tex.image, tex.layout, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, false);
	}

	if (bindShadowMapBuffers)
	{
		write.addBuffer(descriptors.get(), LIGHTNODES_BINDINGPOINT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, fb->LightNodes->mBuffer.get());
		write.addBuffer(descriptors.get(), LIGHTLINES_BINDINGPOINT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, fb->LightLines->mBuffer.get());
		write.addBuffer(descriptors.get(), LIGHTLIST_BINDINGPOINT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, fb->LightList->mBuffer.get());
	}

	write.updateSets(fb->device);
	imageTransition.execute(fb->GetDrawCommands());

	pp->mFrameDescriptorSets.push_back(std::move(descriptors));
	return pp->mFrameDescriptorSets.back().get();
}

VulkanFramebuffer *VkPPRenderState::GetOutput(VkPPRenderPassSetup *passSetup, const PPOutput &output, int &framebufferWidth, int &framebufferHeight)
{
	auto fb = GetVulkanFrameBuffer();

	TextureImage tex = GetTexture(output.Type, output.Texture);

	VkImageView view;
	int w, h;
	if (tex.view)
	{
		VkPPImageTransition imageTransition;
		imageTransition.addImage(tex.image, tex.layout, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, output.Type == PPTextureType::NextPipelineTexture);
		imageTransition.execute(fb->GetDrawCommands());

		view = tex.view->view;
		w = tex.image->width;
		h = tex.image->height;
	}
	else
	{
		view = fb->swapChain->swapChainImageViews[fb->presentImageIndex];
		w = fb->swapChain->actualExtent.width;
		h = fb->swapChain->actualExtent.height;
	}

	auto &framebuffer = passSetup->Framebuffers[view];
	if (!framebuffer)
	{
		FramebufferBuilder builder;
		builder.setRenderPass(passSetup->RenderPass.get());
		builder.setSize(w, h);
		builder.addAttachment(view);
		framebuffer = builder.create(GetVulkanFrameBuffer()->device);
		framebuffer->SetDebugName(tex.debugname);
	}

	framebufferWidth = w;
	framebufferHeight = h;
	return framebuffer.get();
}

VkPPRenderState::TextureImage VkPPRenderState::GetTexture(const PPTextureType &type, PPTexture *pptexture)
{
	auto fb = GetVulkanFrameBuffer();
	TextureImage tex = {};

	if (type == PPTextureType::CurrentPipelineTexture || type == PPTextureType::NextPipelineTexture)
	{
		int idx = fb->GetPostprocess()->mCurrentPipelineImage;
		if (type == PPTextureType::NextPipelineTexture)
			idx = (idx + 1) % VkRenderBuffers::NumPipelineImages;

		tex.image = fb->GetBuffers()->PipelineImage[idx].get();
		tex.view = fb->GetBuffers()->PipelineView[idx].get();
		tex.layout = &fb->GetBuffers()->PipelineLayout[idx];
		tex.debugname = "PipelineTexture";
	}
	else if (type == PPTextureType::PPTexture)
	{
		auto vktex = GetVkTexture(pptexture);
		tex.image = vktex->Image.get();
		tex.view = vktex->View.get();
		tex.layout = &vktex->Layout;
		tex.debugname = "PPTexture";
	}
	else if (type == PPTextureType::SceneColor)
	{
		tex.image = fb->GetBuffers()->SceneColor.get();
		tex.view = fb->GetBuffers()->SceneColorView.get();
		tex.layout = &fb->GetBuffers()->SceneColorLayout;
		tex.debugname = "SceneColor";
	}
	else if (type == PPTextureType::SceneNormal)
	{
		tex.image = fb->GetBuffers()->SceneNormal.get();
		tex.view = fb->GetBuffers()->SceneNormalView.get();
		tex.layout = &fb->GetBuffers()->SceneNormalLayout;
		tex.debugname = "SceneNormal";
	}
	else if (type == PPTextureType::SceneFog)
	{
		tex.image = fb->GetBuffers()->SceneFog.get();
		tex.view = fb->GetBuffers()->SceneFogView.get();
		tex.layout = &fb->GetBuffers()->SceneFogLayout;
		tex.debugname = "SceneFog";
	}
	else if (type == PPTextureType::SceneDepth)
	{
		tex.image = fb->GetBuffers()->SceneDepthStencil.get();
		tex.view = fb->GetBuffers()->SceneDepthView.get();
		tex.layout = &fb->GetBuffers()->SceneDepthStencilLayout;
		tex.debugname = "SceneDepth";
	}
	else if (type == PPTextureType::ShadowMap)
	{
		tex.image = fb->GetBuffers()->Shadowmap.get();
		tex.view = fb->GetBuffers()->ShadowmapView.get();
		tex.layout = &fb->GetBuffers()->ShadowmapLayout;
		tex.debugname = "Shadowmap";
	}
	else if (type == PPTextureType::SwapChain)
	{
		tex.image = nullptr;
		tex.view = nullptr;
		tex.layout = nullptr;
		tex.debugname = "SwapChain";
	}
	else
	{
		I_FatalError("VkPPRenderState::GetTexture not implemented yet for this texture type");
	}

	return tex;
}

VkPPShader *VkPPRenderState::GetVkShader(PPShader *shader)
{
	if (!shader->Backend)
		shader->Backend = std::make_unique<VkPPShader>(shader);
	return static_cast<VkPPShader*>(shader->Backend.get());
}

VkPPTexture *VkPPRenderState::GetVkTexture(PPTexture *texture)
{
	if (!texture->Backend)
		texture->Backend = std::make_unique<VkPPTexture>(texture);
	return static_cast<VkPPTexture*>(texture->Backend.get());
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
	if (key.ShadowMapBuffers)
	{
		builder.addBinding(LIGHTNODES_BINDINGPOINT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
		builder.addBinding(LIGHTLINES_BINDINGPOINT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
		builder.addBinding(LIGHTLIST_BINDINGPOINT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
	}
	DescriptorLayout = builder.create(GetVulkanFrameBuffer()->device);
	DescriptorLayout->SetDebugName("VkPPRenderPassSetup.DescriptorLayout");
}

void VkPPRenderPassSetup::CreatePipelineLayout(const VkPPRenderPassKey &key)
{
	PipelineLayoutBuilder builder;
	builder.addSetLayout(DescriptorLayout.get());
	if (key.Uniforms > 0)
		builder.addPushConstantRange(VK_SHADER_STAGE_FRAGMENT_BIT, 0, key.Uniforms);
	PipelineLayout = builder.create(GetVulkanFrameBuffer()->device);
	PipelineLayout->SetDebugName("VkPPRenderPassSetup.PipelineLayout");
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
	builder.addDynamicState(VK_DYNAMIC_STATE_SCISSOR);
	// Note: the actual values are ignored since we use dynamic viewport+scissor states
	builder.setViewport(0.0f, 0.0f, 320.0f, 200.0f);
	builder.setScissor(0.0f, 0.0f, 320.0f, 200.0f);
	builder.setTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP);
	builder.setBlendMode(key.BlendMode);
	builder.setRasterizationSamples(key.Samples);
	builder.setLayout(PipelineLayout.get());
	builder.setRenderPass(RenderPass.get());
	Pipeline = builder.create(GetVulkanFrameBuffer()->device);
	Pipeline->SetDebugName("VkPPRenderPassSetup.Pipeline");
}

void VkPPRenderPassSetup::CreateRenderPass(const VkPPRenderPassKey &key)
{
	RenderPassBuilder builder;
	if (key.SwapChain)
		builder.addAttachment(key.OutputFormat, key.Samples, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
	else
		builder.addAttachment(key.OutputFormat, key.Samples, VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	builder.addSubpass();
	builder.addSubpassColorAttachmentRef(0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	builder.addExternalSubpassDependency(
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		VK_ACCESS_COLOR_ATTACHMENT_READ_BIT);
	RenderPass = builder.create(GetVulkanFrameBuffer()->device);
	RenderPass->SetDebugName("VkPPRenderPassSetup.RenderPass");
}

/////////////////////////////////////////////////////////////////////////////

void VkPPImageTransition::addImage(VulkanImage *image, VkImageLayout *layout, VkImageLayout targetLayout, bool undefinedSrcLayout)
{
	if (*layout == targetLayout)
		return;

	VkAccessFlags srcAccess = 0;
	VkAccessFlags dstAccess = 0;
	VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

	switch (*layout)
	{
	case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
		srcAccess = VK_ACCESS_TRANSFER_READ_BIT;
		srcStageMask |= VK_PIPELINE_STAGE_TRANSFER_BIT;
		break;
	case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
		srcAccess = VK_ACCESS_SHADER_READ_BIT;
		srcStageMask |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		break;
	case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
		srcAccess = VK_ACCESS_TRANSFER_WRITE_BIT;
		srcStageMask |= VK_PIPELINE_STAGE_TRANSFER_BIT;
		break;
	case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
		srcAccess = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		srcStageMask |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		break;
	case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
		srcAccess = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		srcStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
		break;
	default:
		I_FatalError("Unimplemented src image layout transition\n");
	}

	switch (targetLayout)
	{
	case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
		dstAccess = VK_ACCESS_TRANSFER_READ_BIT;
		dstStageMask |= VK_PIPELINE_STAGE_TRANSFER_BIT;
		break;
	case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
		dstAccess = VK_ACCESS_SHADER_READ_BIT;
		dstStageMask |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		break;
	case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
		dstAccess = VK_ACCESS_TRANSFER_WRITE_BIT;
		dstStageMask |= VK_PIPELINE_STAGE_TRANSFER_BIT;
		break;
	case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
		dstAccess = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dstStageMask |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		break;
	case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
		srcAccess = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		srcStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
		break;
	default:
		I_FatalError("Unimplemented dst image layout transition\n");
	}

	barrier.addImage(image, undefinedSrcLayout ? VK_IMAGE_LAYOUT_UNDEFINED : *layout, targetLayout, srcAccess, dstAccess, aspectMask);
	needbarrier = true;
	*layout = targetLayout;
}

void VkPPImageTransition::execute(VulkanCommandBuffer *cmdbuffer)
{
	if (needbarrier)
		barrier.execute(cmdbuffer, srcStageMask, dstStageMask);
}
