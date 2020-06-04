
#pragma once

#include <functional>
#include <map>
#include <array>

#include "hwrenderer/postprocessing/hw_postprocess.h"
#include "vulkan/system/vk_objects.h"
#include "vulkan/system/vk_builders.h"
#include "vulkan/textures/vk_imagetransition.h"

class FString;

class VkPPShader;
class VkPPTexture;
class VkPPRenderPassSetup;
class PipelineBarrier;

class VkPPRenderPassKey
{
public:
	VkPPShader *Shader;
	int Uniforms;
	int InputTextures;
	PPBlendMode BlendMode;
	VkFormat OutputFormat;
	int SwapChain;
	int ShadowMapBuffers;
	int StencilTest;
	VkSampleCountFlagBits Samples;

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

	void SetActiveRenderTarget();
	void PostProcessScene(int fixedcm, const std::function<void()> &afterBloomDrawEndScene2D);

	void AmbientOccludeScene(float m5);
	void BlurScene(float gameinfobluramount);
	void ClearTonemapPalette();

	void UpdateShadowMap();

	void ImageTransitionScene(bool undefinedSrcLayout);

	void BlitSceneToPostprocess();
	void BlitCurrentToImage(VkTextureImage *image, VkImageLayout finallayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	void DrawPresentTexture(const IntRect &box, bool applyGamma, bool screenshot);

private:
	void NextEye(int eyeCount);

	std::unique_ptr<VulkanDescriptorSet> AllocateDescriptorSet(VulkanDescriptorSetLayout *layout);
	VulkanSampler *GetSampler(PPFilterMode filter, PPWrapMode wrap);

	std::array<std::unique_ptr<VulkanSampler>, 4> mSamplers;
	std::map<VkPPRenderPassKey, std::unique_ptr<VkPPRenderPassSetup>> mRenderPassSetup;
	std::unique_ptr<VulkanDescriptorPool> mDescriptorPool;
	int mCurrentPipelineImage = 0;

	friend class VkPPRenderState;
};

class VkPPShader : public PPShaderBackend
{
public:
	VkPPShader(PPShader *shader);

	std::unique_ptr<VulkanShader> VertexShader;
	std::unique_ptr<VulkanShader> FragmentShader;

private:
	FString LoadShaderCode(const FString &lumpname, const FString &defines, int version);
};

class VkPPTexture : public PPTextureBackend
{
public:
	VkPPTexture(PPTexture *texture);
	~VkPPTexture();

	VkTextureImage TexImage;
	std::unique_ptr<VulkanBuffer> Staging;
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

private:
	void CreateDescriptorLayout(const VkPPRenderPassKey &key);
	void CreatePipelineLayout(const VkPPRenderPassKey &key);
	void CreatePipeline(const VkPPRenderPassKey &key);
	void CreateRenderPass(const VkPPRenderPassKey &key);
};

class VkPPRenderState : public PPRenderState
{
public:
	void PushGroup(const FString &name) override;
	void PopGroup() override;

	void Draw() override;

private:
	void RenderScreenQuad(VkPPRenderPassSetup *passSetup, VulkanDescriptorSet *descriptorSet, VulkanFramebuffer *framebuffer, int framebufferWidth, int framebufferHeight, int x, int y, int width, int height, const void *pushConstants, uint32_t pushConstantsSize, bool stencilTest);

	VulkanDescriptorSet *GetInput(VkPPRenderPassSetup *passSetup, const TArray<PPTextureInput> &textures, bool bindShadowMapBuffers);
	VulkanFramebuffer *GetOutput(VkPPRenderPassSetup *passSetup, const PPOutput &output, bool stencilTest, int &framebufferWidth, int &framebufferHeight);

	VkPPShader *GetVkShader(PPShader *shader);
	VkPPTexture *GetVkTexture(PPTexture *texture);

	VkTextureImage *GetTexture(const PPTextureType &type, PPTexture *tex);
};
