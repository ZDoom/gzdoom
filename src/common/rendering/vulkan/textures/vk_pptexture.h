
#pragma once

#include "hwrenderer/postprocessing/hw_postprocess.h"
#include "vulkan/system/vk_objects.h"
#include "vulkan/textures/vk_imagetransition.h"

class VkPPTexture : public PPTextureBackend
{
public:
	VkPPTexture(PPTexture *texture);
	~VkPPTexture();

	VkTextureImage TexImage;
	std::unique_ptr<VulkanBuffer> Staging;
	VkFormat Format;
};
