
#include "vk_postprocess.h"
#include "vk_renderbuffers.h"
#include "vulkan/shaders/vk_shader.h"
#include "vulkan/system/vk_builders.h"
#include "vulkan/system/vk_framebuffer.h"
#include "vulkan/system/vk_buffers.h"
#include "hwrenderer/utility/hw_cvars.h"
#include "hwrenderer/postprocessing/hw_presentshader.h"
#include "hwrenderer/postprocessing/hw_postprocess.h"
#include "hwrenderer/postprocessing/hw_postprocess_cvars.h"
#include "hwrenderer/utility/hw_vrmodes.h"
#include "hwrenderer/data/flatvertices.h"
#include "r_videoscale.h"
#include "w_wad.h"

VkPostprocess::VkPostprocess()
{
}

VkPostprocess::~VkPostprocess()
{
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
	//BindCurrentFB();
	afterBloomDrawEndScene2D();
	RenderEffect("TonemapScene");
	RenderEffect("ColormapScene");
	RenderEffect("LensDistortScene");
	RenderEffect("ApplyFXAA");
	//mCustomPostProcessShaders->Run("scene");
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

void VkPostprocess::RenderBuffersReset()
{
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
				prolog = UniformBlockDecl::Create("Uniforms", desc.Uniforms, uniformbindingpoint);
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
	if (lump == -1) I_FatalError("Unable to load '%s'", lumpName);
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
}

void VkPostprocess::RenderScreenQuad()
{
	//auto buffer = static_cast<VKVertexBuffer *>(screen->mVertexData->GetBufferObjects().first);
	//buffer->Bind(nullptr);
	//glDrawArrays(GL_TRIANGLE_STRIP, FFlatVertexBuffer::PRESENT_INDEX, 4);
}

void VkPostprocess::NextEye(int eyeCount)
{
}
