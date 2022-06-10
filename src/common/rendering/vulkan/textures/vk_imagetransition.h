
#pragma once

#include "vulkan/system/vk_objects.h"
#include "vulkan/system/vk_builders.h"
#include "vulkan/system/vk_framebuffer.h"
#include "vulkan/system/vk_commandbuffer.h"
#include "vulkan/renderer/vk_renderpass.h"

class VkTextureImage
{
public:
	void Reset(VulkanFrameBuffer* fb)
	{
		AspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		Layout = VK_IMAGE_LAYOUT_UNDEFINED;
		auto& deletelist = fb->GetCommands()->FrameDeleteList;
		deletelist.Framebuffers.push_back(std::move(PPFramebuffer));
		for (auto &it : RSFramebuffers)
			deletelist.Framebuffers.push_back(std::move(it.second));
		RSFramebuffers.clear();
		deletelist.ImageViews.push_back(std::move(DepthOnlyView));
		deletelist.ImageViews.push_back(std::move(View));
		deletelist.Images.push_back(std::move(Image));
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
	void addImage(VkTextureImage *image, VkImageLayout targetLayout, bool undefinedSrcLayout, int baseMipLevel = 0, int levelCount = 1);
	void execute(VulkanCommandBuffer *cmdbuffer);

private:
	PipelineBarrier barrier;
	VkPipelineStageFlags srcStageMask = 0;
	VkPipelineStageFlags dstStageMask = 0;
	bool needbarrier = false;
};
