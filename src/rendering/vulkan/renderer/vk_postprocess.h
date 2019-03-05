
#pragma once

#include <functional>
#include <map>

#include "hwrenderer/postprocessing/hw_postprocess.h"
#include "vulkan/system/vk_objects.h"

class FString;

class VkPPShader;
class VkPPTexture;

class VkPostprocess
{
public:
	VkPostprocess();
	~VkPostprocess();

	void RenderBuffersReset();

	void PostProcessScene(int fixedcm, const std::function<void()> &afterBloomDrawEndScene2D);

	void AmbientOccludeScene(float m5);
	void BlurScene(float gameinfobluramount);
	void ClearTonemapPalette();

private:
	void UpdateEffectTextures();
	void CompileEffectShaders();
	FString LoadShaderCode(const FString &lumpname, const FString &defines, int version);
	void RenderEffect(const FString &name);
	void NextEye(int eyeCount);
	void RenderScreenQuad();

	std::map<PPTextureName, std::unique_ptr<VkPPTexture>> mTextures;
	std::map<PPShaderName, std::unique_ptr<VkPPShader>> mShaders;

	const static int uniformbindingpoint = 7;
};

class VkPPShader
{
public:
	std::unique_ptr<VulkanShader> VertexShader;
	std::unique_ptr<VulkanShader> FragmentShader;
};

class VkPPTexture
{
public:
	std::unique_ptr<VulkanImage> Image;
	std::unique_ptr<VulkanImageView> View;
};
