
#pragma once

#include "hwrenderer/postprocessing/hw_postprocess.h"
#include <zvulkan/vulkanobjects.h>
#include "vulkan/textures/vk_imagetransition.h"
#include <list>

class VulkanRenderDevice;

class VkPPTexture : public PPTextureBackend
{
public:
	VkPPTexture(VulkanRenderDevice* fb, PPTexture *texture);
	~VkPPTexture();

	void Reset();

	VulkanRenderDevice* fb = nullptr;
	std::list<VkPPTexture*>::iterator it;

	VkTextureImage TexImage;
	std::unique_ptr<VulkanBuffer> Staging;
	VkFormat Format;
};
