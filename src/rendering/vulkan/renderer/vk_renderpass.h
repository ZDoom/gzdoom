
#pragma once

#include "vulkan/system/vk_objects.h"
#include "r_data/renderstyle.h"
#include <map>

class VKDataBuffer;

class VkRenderPassKey
{
public:
	FRenderStyle RenderStyle;

	bool operator<(const VkRenderPassKey &other) const
	{
		return RenderStyle.AsDWORD < other.RenderStyle.AsDWORD;
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
