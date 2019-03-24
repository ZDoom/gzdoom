
#pragma once

#include "vulkan/system/vk_objects.h"

class VkRenderBuffers
{
public:
	VkRenderBuffers();
	~VkRenderBuffers();

	void BeginFrame(int width, int height, int sceneWidth, int sceneHeight);

	int GetWidth() const { return mWidth; }
	int GetHeight() const { return mHeight; }
	int GetSceneWidth() const { return mSceneWidth; }
	int GetSceneHeight() const { return mSceneHeight; }
	VkSampleCountFlagBits GetSceneSamples() const { return mSamples; }

	std::unique_ptr<VulkanImage> SceneColor;
	std::unique_ptr<VulkanImage> SceneDepthStencil;
	std::unique_ptr<VulkanImage> SceneNormal;
	std::unique_ptr<VulkanImage> SceneFog;
	std::unique_ptr<VulkanImageView> SceneColorView;
	std::unique_ptr<VulkanImageView> SceneDepthStencilView;
	std::unique_ptr<VulkanImageView> SceneDepthView;
	std::unique_ptr<VulkanImageView> SceneNormalView;
	std::unique_ptr<VulkanImageView> SceneFogView;
	VkFormat SceneDepthStencilFormat = VK_FORMAT_D24_UNORM_S8_UINT;
	VkImageLayout SceneColorLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	VkImageLayout SceneDepthStencilLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	VkImageLayout SceneNormalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	VkImageLayout SceneFogLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	static const int NumPipelineImages = 2;
	std::unique_ptr<VulkanImage> PipelineImage[NumPipelineImages];
	std::unique_ptr<VulkanImageView> PipelineView[NumPipelineImages];
	VkImageLayout PipelineLayout[NumPipelineImages] = { VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

	std::unique_ptr<VulkanImage> Shadowmap;
	std::unique_ptr<VulkanImageView> ShadowmapView;
	std::unique_ptr<VulkanSampler> ShadowmapSampler;
	VkImageLayout ShadowmapLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

private:
	void CreatePipeline(int width, int height);
	void CreateScene(int width, int height, VkSampleCountFlagBits samples);
	void CreateSceneColor(int width, int height, VkSampleCountFlagBits samples);
	void CreateSceneDepthStencil(int width, int height, VkSampleCountFlagBits samples);
	void CreateSceneFog(int width, int height, VkSampleCountFlagBits samples);
	void CreateSceneNormal(int width, int height, VkSampleCountFlagBits samples);
	void CreateShadowmap();
	VkSampleCountFlagBits GetBestSampleCount();

	int mWidth = 0;
	int mHeight = 0;
	int mSceneWidth = 0;
	int mSceneHeight = 0;
	VkSampleCountFlagBits mSamples = VK_SAMPLE_COUNT_1_BIT;
};
