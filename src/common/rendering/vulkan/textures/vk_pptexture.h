
#pragma once

#include "hwrenderer/postprocessing/hw_postprocess.h"
#include "vulkan/system/vk_objects.h"
#include "vulkan/textures/vk_imagetransition.h"

class VulkanFrameBuffer;

class VkPPTexture : public PPTextureBackend
{
public:
	VkPPTexture(VulkanFrameBuffer* fb, PPTexture *texture);
	~VkPPTexture();

	VulkanFrameBuffer* fb = nullptr;

	VkTextureImage TexImage;
	std::unique_ptr<VulkanBuffer> Staging;
	VkFormat Format;
};
