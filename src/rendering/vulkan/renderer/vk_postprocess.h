
#pragma once

#include <functional>
#include <map>
#include <array>

#include "hwrenderer/postprocessing/hw_postprocess.h"
#include "vulkan/system/vk_objects.h"
#include "vulkan/system/vk_builders.h"

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

	bool operator<(const VkPPRenderPassKey &other) const { return memcmp(this, &other, sizeof(VkPPRenderPassKey)) < 0; }
	bool operator==(const VkPPRenderPassKey &other) const { return memcmp(this, &other, sizeof(VkPPRenderPassKey)) == 0; }
	bool operator!=(const VkPPRenderPassKey &other) const { return memcmp(this, &other, sizeof(VkPPRenderPassKey)) != 0; }
};

class VkPPImageTransition
{
public:
	void addImage(VulkanImage *image, VkImageLayout *layout, VkImageLayout targetLayout, bool undefinedSrcLayout);
	void execute(VulkanCommandBuffer *cmdbuffer);

private:
	PipelineBarrier barrier;
	VkPipelineStageFlags srcStageMask = 0;
	VkPipelineStageFlags dstStageMask = 0;
	bool needbarrier = false;
};

class VkPostprocess
{
public:
	VkPostprocess();
	~VkPostprocess();

	void BeginFrame();
	void RenderBuffersReset();

	void SetActiveRenderTarget();
	void PostProcessScene(int fixedcm, const std::function<void()> &afterBloomDrawEndScene2D);

	void AmbientOccludeScene(float m5);
	void BlurScene(float gameinfobluramount);
	void ClearTonemapPalette();

	void UpdateShadowMap();

	void BlitSceneToTexture();
	void BlitCurrentToImage(VulkanImage *image, VkImageLayout *layout, VkImageLayout finallayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	void DrawPresentTexture(const IntRect &box, bool applyGamma, bool clearBorders);

private:
	void UpdateEffectTextures();
	void CompileEffectShaders();
	FString LoadShaderCode(const FString &lumpname, const FString &defines, int version);
	void RenderEffect(const FString &name);
	void NextEye(int eyeCount);
	void RenderScreenQuad(VkPPRenderPassSetup *passSetup, VulkanDescriptorSet *descriptorSet, VulkanFramebuffer *framebuffer, int framebufferWidth, int framebufferHeight, int x, int y, int width, int height, const void *pushConstants, uint32_t pushConstantsSize);

	VulkanDescriptorSet *GetInput(VkPPRenderPassSetup *passSetup, const TArray<PPTextureInput> &textures, bool bindShadowMapBuffers);
	VulkanFramebuffer *GetOutput(VkPPRenderPassSetup *passSetup, const PPOutput &output, int &framebufferWidth, int &framebufferHeight);
	VulkanSampler *GetSampler(PPFilterMode filter, PPWrapMode wrap);

	struct TextureImage
	{
		VulkanImage *image;
		VulkanImageView *view;
		VkImageLayout *layout;
		const char *debugname;
	};
	TextureImage GetTexture(const PPTextureType &type, const PPTextureName &name);

	std::map<PPTextureName, std::unique_ptr<VkPPTexture>> mTextures;
	std::map<PPShaderName, std::unique_ptr<VkPPShader>> mShaders;
	std::array<std::unique_ptr<VulkanSampler>, 16> mSamplers;
	std::map<VkPPRenderPassKey, std::unique_ptr<VkPPRenderPassSetup>> mRenderPassSetup;
	std::unique_ptr<VulkanDescriptorPool> mDescriptorPool;
	std::vector<std::unique_ptr<VulkanDescriptorSet>> mFrameDescriptorSets;
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
	std::map<VkImageView, std::unique_ptr<VulkanFramebuffer>> Framebuffers;

private:
	void CreateDescriptorLayout(const VkPPRenderPassKey &key);
	void CreatePipelineLayout(const VkPPRenderPassKey &key);
	void CreatePipeline(const VkPPRenderPassKey &key);
	void CreateRenderPass(const VkPPRenderPassKey &key);
};
