
#pragma once

#include "zvulkan/vulkanobjects.h"
#include "hwrenderer/postprocessing/hw_postprocess.h"
#include <string.h>
#include <map>

class VulkanRenderDevice;
class VkPPShader;
enum class WhichDepthStencil;

class VkPPRenderPassKey
{
public:
	VkPPShader* Shader;
	int Uniforms;
	int InputTextures;
	PPBlendMode BlendMode;
	VkFormat OutputFormat;
	int SwapChain;
	int ShadowMapBuffers;
	WhichDepthStencil StencilTest;
	VkSampleCountFlagBits Samples;

	bool operator<(const VkPPRenderPassKey& other) const { return memcmp(this, &other, sizeof(VkPPRenderPassKey)) < 0; }
	bool operator==(const VkPPRenderPassKey& other) const { return memcmp(this, &other, sizeof(VkPPRenderPassKey)) == 0; }
	bool operator!=(const VkPPRenderPassKey& other) const { return memcmp(this, &other, sizeof(VkPPRenderPassKey)) != 0; }
};

class VkPPRenderPassSetup
{
public:
	VkPPRenderPassSetup(VulkanRenderDevice* fb, const VkPPRenderPassKey& key);

	std::unique_ptr<VulkanDescriptorSetLayout> DescriptorLayout;
	std::unique_ptr<VulkanPipelineLayout> PipelineLayout;
	std::unique_ptr<VulkanRenderPass> RenderPass;
	std::unique_ptr<VulkanPipeline> Pipeline;

private:
	void CreateDescriptorLayout(const VkPPRenderPassKey& key);
	void CreatePipelineLayout(const VkPPRenderPassKey& key);
	void CreatePipeline(const VkPPRenderPassKey& key);
	void CreateRenderPass(const VkPPRenderPassKey& key);

	VulkanRenderDevice* fb = nullptr;
};
