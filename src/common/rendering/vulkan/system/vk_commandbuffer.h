#pragma once

#include "vk_device.h"
#include "vk_objects.h"

class VulkanFrameBuffer;

class VkCommandBufferManager
{
public:
	VkCommandBufferManager(VulkanFrameBuffer* fb);
	~VkCommandBufferManager();

	void BeginFrame();

	VulkanCommandBuffer* GetTransferCommands();
	VulkanCommandBuffer* GetDrawCommands();

	void FlushCommands(bool finish, bool lastsubmit = false, bool uploadOnly = false);

	void WaitForCommands(bool finish) { WaitForCommands(finish, false); }
	void WaitForCommands(bool finish, bool uploadOnly);

	void PushGroup(const FString& name);
	void PopGroup();
	void UpdateGpuStats();

	class DeleteList
	{
	public:
		std::vector<std::unique_ptr<VulkanImage>> Images;
		std::vector<std::unique_ptr<VulkanImageView>> ImageViews;
		std::vector<std::unique_ptr<VulkanFramebuffer>> Framebuffers;
		std::vector<std::unique_ptr<VulkanBuffer>> Buffers;
		std::vector<std::unique_ptr<VulkanAccelerationStructure>> AccelStructs;
		std::vector<std::unique_ptr<VulkanDescriptorSet>> Descriptors;
		std::vector<std::unique_ptr<VulkanDescriptorPool>> DescriptorPools;
		std::vector<std::unique_ptr<VulkanCommandBuffer>> CommandBuffers;
	} FrameDeleteList;

	struct
	{
		std::vector<std::unique_ptr<VulkanBuffer>> Buffers;
		size_t TotalSize = 0;
	} FrameTextureUpload;

	void DeleteFrameObjects(bool uploadOnly = false);

	std::unique_ptr<VulkanSwapChain> swapChain;
	uint32_t presentImageIndex = 0xffffffff;

private:
	void FlushCommands(VulkanCommandBuffer** commands, size_t count, bool finish, bool lastsubmit);

	VulkanFrameBuffer* fb = nullptr;

	std::unique_ptr<VulkanCommandPool> mCommandPool;

	std::unique_ptr<VulkanCommandBuffer> mTransferCommands;
	std::unique_ptr<VulkanCommandBuffer> mDrawCommands;

	enum { maxConcurrentSubmitCount = 8 };
	std::unique_ptr<VulkanSemaphore> mSubmitSemaphore[maxConcurrentSubmitCount];
	std::unique_ptr<VulkanFence> mSubmitFence[maxConcurrentSubmitCount];
	VkFence mSubmitWaitFences[maxConcurrentSubmitCount];
	int mNextSubmit = 0;

	std::unique_ptr<VulkanSemaphore> mSwapChainImageAvailableSemaphore;
	std::unique_ptr<VulkanSemaphore> mRenderFinishedSemaphore;

	struct TimestampQuery
	{
		FString name;
		uint32_t startIndex;
		uint32_t endIndex;
	};

	enum { MaxTimestampQueries = 100 };
	std::unique_ptr<VulkanQueryPool> mTimestampQueryPool;
	int mNextTimestampQuery = 0;
	std::vector<size_t> mGroupStack;
	std::vector<TimestampQuery> timeElapsedQueries;
};
