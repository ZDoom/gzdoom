#ifndef __VK_SAMPLERS_H
#define __VK_SAMPLERS_H

//#include "vk_hwtexture.h"
class VulkanDevice;

class VkSamplerManager
{
	VulkanDevice *vDevice;
	VkSampler mSamplers[7];

	//void UnbindAll();

	void Create();
	void Destroy();

public:

	VkSamplerManager(VulkanDevice *dev);
	~VkSamplerManager();

	//uint8_t Bind(int texunit, int num, int lastval);
	void SetTextureFilterMode();

	VkSampler Get(int no) const { return mSamplers[no]; }

};


#endif

