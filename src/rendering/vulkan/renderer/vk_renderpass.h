
#pragma once

#include "vulkan/system/vk_objects.h"
#include "r_data/renderstyle.h"
#include <map>

class VKDataBuffer;

class VkRenderPassKey
{
public:
	FRenderStyle RenderStyle;
	int SpecialEffect;
	int EffectState;
	bool AlphaTest;

	bool operator<(const VkRenderPassKey &other) const
	{
		uint64_t a = RenderStyle.AsDWORD | (static_cast<uint64_t>(SpecialEffect) << 32) | (static_cast<uint64_t>(EffectState) << 40) | (static_cast<uint64_t>(AlphaTest) << 48);
		uint64_t b = other.RenderStyle.AsDWORD | (static_cast<uint64_t>(other.SpecialEffect) << 32) | (static_cast<uint64_t>(other.EffectState) << 40) | (static_cast<uint64_t>(other.AlphaTest) << 48);
		return a < b;
	}
};

class VkRenderPassSetup
{
public:
	VkRenderPassSetup(const VkRenderPassKey &key);

	std::unique_ptr<VulkanRenderPass> RenderPass;
	std::unique_ptr<VulkanPipeline> Pipeline;
	std::unique_ptr<VulkanFramebuffer> Framebuffer;

private:
	void CreatePipeline(const VkRenderPassKey &key);
	void CreateRenderPass();
	void CreateFramebuffer();
};

class VkRenderPassManager
{
public:
	VkRenderPassManager();

	void BeginFrame();
	VkRenderPassSetup *GetRenderPass(const VkRenderPassKey &key);

	std::unique_ptr<VulkanDescriptorSetLayout> DynamicSetLayout;
	std::unique_ptr<VulkanDescriptorSetLayout> TextureSetLayout;
	std::unique_ptr<VulkanPipelineLayout> PipelineLayout;
	std::unique_ptr<VulkanDescriptorPool> DescriptorPool;
	std::map<VkRenderPassKey, std::unique_ptr<VkRenderPassSetup>> RenderPassSetup;

	std::unique_ptr<VulkanImage> SceneColor;
	std::unique_ptr<VulkanImage> SceneDepthStencil;
	std::unique_ptr<VulkanImageView> SceneColorView;
	std::unique_ptr<VulkanImageView> SceneDepthStencilView;
	std::unique_ptr<VulkanImageView> SceneDepthView;

	std::unique_ptr<VulkanDescriptorSet> DynamicSet;

private:
	void CreateDynamicSetLayout();
	void CreateTextureSetLayout();
	void CreatePipelineLayout();
	void CreateDescriptorPool();
	void CreateDynamicSet();
};
