
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
class PipelineBarrier;

class VkPostprocess
{
public:
	VkPostprocess();
	~VkPostprocess();

	void SetActiveRenderTarget();
	void PostProcessScene(int fixedcm, float flash, const std::function<void()> &afterBloomDrawEndScene2D);

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

	std::unique_ptr<VulkanDescriptorPool> mDescriptorPool;
	int mCurrentPipelineImage = 0;

	friend class VkPPRenderState;
};
