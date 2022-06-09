
#pragma once

#include "vulkan/system/vk_objects.h"
#include <array>

class VulkanFrameBuffer;
enum class PPFilterMode;
enum class PPWrapMode;

class VkSamplerManager
{
public:
	VkSamplerManager(VulkanFrameBuffer* fb);
	~VkSamplerManager();

	void FilterModeChanged();

	VulkanSampler *Get(int no) const { return mSamplers[no].get(); }
	VulkanSampler* Get(PPFilterMode filter, PPWrapMode wrap);

private:
	void CreateHWSamplers();
	void DeleteHWSamplers();

	VulkanFrameBuffer* fb = nullptr;
	std::array<std::unique_ptr<VulkanSampler>, NUMSAMPLERS> mSamplers;
	std::array<std::unique_ptr<VulkanSampler>, 4> mPPSamplers;
};
