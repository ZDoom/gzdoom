
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

	std::unique_ptr<VulkanImage> SceneColor;
	std::unique_ptr<VulkanImage> SceneDepthStencil;
	std::unique_ptr<VulkanImageView> SceneColorView;
	std::unique_ptr<VulkanImageView> SceneDepthStencilView;
	std::unique_ptr<VulkanImageView> SceneDepthView;
	VkFormat SceneDepthStencilFormat = VK_FORMAT_D24_UNORM_S8_UINT;

	static const int NumPipelineImages = 2;
	std::unique_ptr<VulkanImage> PipelineImage[NumPipelineImages];
	std::unique_ptr<VulkanImageView> PipelineView[NumPipelineImages];
	VkImageLayout PipelineLayout[NumPipelineImages];

private:
	void CreatePipeline(int width, int height);
	void CreateScene(int width, int height, int samples);

	int mWidth = 0;
	int mHeight = 0;
	int mSceneWidth = 0;
	int mSceneHeight = 0;
	int mSamples = 0;
	int mMaxSamples = 16;
};
