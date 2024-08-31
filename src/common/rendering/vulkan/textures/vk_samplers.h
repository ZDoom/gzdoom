
#pragma once

#include "zvulkan/vulkanobjects.h"
#include <array>

class VulkanRenderDevice;
enum class PPFilterMode;
enum class PPWrapMode;

class VkSamplerManager
{
public:
	VkSamplerManager(VulkanRenderDevice* fb);
	~VkSamplerManager();

	void ResetHWSamplers();

	VulkanSampler *Get(int no) const { return mSamplers[no].get(); }
	VulkanSampler* Get(PPFilterMode filter, PPWrapMode wrap);

	std::unique_ptr<VulkanSampler> ShadowmapSampler;
	std::unique_ptr<VulkanSampler> LightmapSampler;

private:
	void CreateHWSamplers();
	void DeleteHWSamplers();
	void CreateShadowmapSampler();
	void CreateLightmapSampler();

	VulkanRenderDevice* fb = nullptr;
	std::array<std::unique_ptr<VulkanSampler>, NUMSAMPLERS> mSamplers;
	std::array<std::unique_ptr<VulkanSampler>, 4> mPPSamplers;
};
