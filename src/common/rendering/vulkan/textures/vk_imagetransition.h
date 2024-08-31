
#pragma once

#include "zvulkan/vulkanobjects.h"
#include "zvulkan/vulkanbuilders.h"
#include "vulkan/system/vk_renderdevice.h"
#include "vulkan/system/vk_commandbuffer.h"
#include "vulkan/renderer/vk_renderpass.h"

class VkTextureImage
{
public:
	void Reset(VulkanRenderDevice* fb)
	{
		AspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		Layout = VK_IMAGE_LAYOUT_UNDEFINED;
		auto deletelist = fb->GetCommands()->DrawDeleteList.get();
		deletelist->Add(std::move(PPFramebuffer));
		for (auto &it : RSFramebuffers)
			deletelist->Add(std::move(it.second));
		RSFramebuffers.clear();
		deletelist->Add(std::move(DepthOnlyView));
		deletelist->Add(std::move(View));
		deletelist->Add(std::move(Image));
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
	VkImageTransition& AddImage(VkTextureImage *image, VkImageLayout targetLayout, bool undefinedSrcLayout, int baseMipLevel = 0, int levelCount = 1);
	void Execute(VulkanCommandBuffer *cmdbuffer);

private:
	PipelineBarrier barrier;
	VkPipelineStageFlags srcStageMask = 0;
	VkPipelineStageFlags dstStageMask = 0;
	bool needbarrier = false;
};
