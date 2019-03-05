
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
