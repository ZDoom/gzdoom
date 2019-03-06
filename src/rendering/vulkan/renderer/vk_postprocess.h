
#pragma once

#include <functional>
#include <map>
#include <array>

#include "hwrenderer/postprocessing/hw_postprocess.h"
#include "vulkan/system/vk_objects.h"

class FString;

class VkPPShader;
class VkPPTexture;
class VkPPRenderPassSetup;

class VkPPRenderPassKey
{
public:
	VkPPShader *Shader;
	int Uniforms;
	int InputTextures;
	PPBlendMode BlendMode;
	VkFormat OutputFormat;

	bool operator<(const VkPPRenderPassKey &other) const { return memcmp(this, &other, sizeof(VkPPRenderPassKey)) < 0; }
	bool operator==(const VkPPRenderPassKey &other) const { return memcmp(this, &other, sizeof(VkPPRenderPassKey)) == 0; }
	bool operator!=(const VkPPRenderPassKey &other) const { return memcmp(this, &other, sizeof(VkPPRenderPassKey)) != 0; }
};

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
	void NextTexture();
	void RenderScreenQuad(VkPPRenderPassSetup *passSetup, VulkanDescriptorSet *descriptorSet, VulkanFramebuffer *framebuffer, int x, int y, int width, int height, const void *pushConstants, uint32_t pushConstantsSize);

	VulkanDescriptorSet *GetInput(VkPPRenderPassSetup *passSetup, const TArray<PPTextureInput> &textures);
	VulkanFramebuffer *GetOutput(VkPPRenderPassSetup *passSetup, const PPOutput &output);
	VulkanSampler *GetSampler(PPFilterMode filter, PPWrapMode wrap);

	std::map<PPTextureName, std::unique_ptr<VkPPTexture>> mTextures;
	std::map<PPShaderName, std::unique_ptr<VkPPShader>> mShaders;
	std::array<std::unique_ptr<VulkanSampler>, 16> mSamplers;
	std::map<VkPPRenderPassKey, std::unique_ptr<VkPPRenderPassSetup>> mRenderPassSetup;
	int mCurrentPipelineImage = 0;
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
	std::unique_ptr<VulkanBuffer> Staging;
	VkImageLayout Layout = VK_IMAGE_LAYOUT_UNDEFINED;
	VkFormat Format;
};

class VkPPRenderPassSetup
{
public:
	VkPPRenderPassSetup(const VkPPRenderPassKey &key);

	std::unique_ptr<VulkanDescriptorSetLayout> DescriptorLayout;
	std::unique_ptr<VulkanPipelineLayout> PipelineLayout;
	std::unique_ptr<VulkanRenderPass> RenderPass;
	std::unique_ptr<VulkanPipeline> Pipeline;

	//std::unique_ptr<VulkanDescriptorPool> DescriptorPool;

	//std::unique_ptr<VulkanFramebuffer> Framebuffer;
	//std::unique_ptr<VulkanDescriptorSet> DescriptorSet;

private:
	void CreateDescriptorLayout(const VkPPRenderPassKey &key);
	void CreatePipelineLayout(const VkPPRenderPassKey &key);
	void CreatePipeline(const VkPPRenderPassKey &key);
	void CreateRenderPass(const VkPPRenderPassKey &key);
};
