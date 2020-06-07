#ifndef __VK_SAMPLERS_H
#define __VK_SAMPLERS_H

#include "vulkan/system/vk_objects.h"

class VulkanDevice;

class VkSamplerManager
{
	VulkanDevice *vDevice;
	std::unique_ptr<VulkanSampler> mSamplers[NUMSAMPLERS];

	//void UnbindAll();

	void Create();
	void Destroy();

public:

	VkSamplerManager(VulkanDevice *dev);
	~VkSamplerManager();

	//uint8_t Bind(int texunit, int num, int lastval);
	void SetTextureFilterMode();

	VulkanSampler *Get(int no) const { return mSamplers[no].get(); }

};


#endif

