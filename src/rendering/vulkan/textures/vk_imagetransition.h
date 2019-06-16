
#pragma once

#include "vulkan/system/vk_objects.h"
#include "vulkan/system/vk_builders.h"
#include "vulkan/renderer/vk_renderpass.h"

class VkTextureImage
{
public:
	void reset()
	{
		AspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		Layout = VK_IMAGE_LAYOUT_UNDEFINED;
		PPFramebuffer.reset();
		RSFramebuffers.clear();
		DepthOnlyView.reset();
		View.reset();
		Image.reset();
	}

	void GenerateMipmaps(VulkanCommandBuffer *cmdbuffer);

	std::unique_ptr<VulkanImage> Image;
	std::unique_ptr<VulkanImageView> View;
	std::unique_ptr<VulkanImageView> DepthOnlyView;
	VkImageLayout Layout = VK_IMAGE_LAYOUT_UNDEFINED;
	VkImageAspectFlags AspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	std::unique_ptr<VulkanFramebuffer> PPFramebuffer;
	std::map<VkRenderPassKey, std::unique_ptr<VulkanFramebuffer>> RSFramebuffers;
};

class VkImageTransition
{
public:
	void addImage(VkTextureImage *image, VkImageLayout targetLayout, bool undefinedSrcLayout);
	void execute(VulkanCommandBuffer *cmdbuffer);

private:
	PipelineBarrier barrier;
	VkPipelineStageFlags srcStageMask = 0;
	VkPipelineStageFlags dstStageMask = 0;
	bool needbarrier = false;
};
