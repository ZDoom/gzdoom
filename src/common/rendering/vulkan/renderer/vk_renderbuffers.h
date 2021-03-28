
#pragma once

#include "vulkan/system/vk_objects.h"
#include "vulkan/textures/vk_imagetransition.h"

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

	VkTextureImage SceneColor;
	VkTextureImage SceneDepthStencil;
	VkTextureImage SceneNormal;
	VkTextureImage SceneFog;

	VkFormat SceneDepthStencilFormat = VK_FORMAT_D24_UNORM_S8_UINT;
	VkFormat SceneNormalFormat = VK_FORMAT_A2R10G10B10_UNORM_PACK32;

	static const int NumPipelineImages = 2;
	VkTextureImage PipelineImage[NumPipelineImages];

	VkTextureImage Shadowmap;
	std::unique_ptr<VulkanSampler> ShadowmapSampler;

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
