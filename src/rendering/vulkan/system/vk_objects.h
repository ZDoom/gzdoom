#pragma once

#include "vk_device.h"
#include "doomerrors.h"

class VulkanCommandPool;
class VulkanDescriptorPool;

class VulkanSemaphore
{
public:
	VulkanSemaphore(VulkanDevice *device);
	~VulkanSemaphore();

	void SetDebugName(const char *name) { device->SetDebugObjectName(name, (uint64_t)semaphore, VK_OBJECT_TYPE_SEMAPHORE); }

	VulkanDevice *device = nullptr;
	VkSemaphore semaphore = VK_NULL_HANDLE;

private:
	VulkanSemaphore(const VulkanSemaphore &) = delete;
	VulkanSemaphore &operator=(const VulkanSemaphore &) = delete;
};

class VulkanFence
{
public:
	VulkanFence(VulkanDevice *device);
	~VulkanFence();

	void SetDebugName(const char *name) { device->SetDebugObjectName(name, (uint64_t)fence, VK_OBJECT_TYPE_FENCE); }

	VulkanDevice *device = nullptr;
	VkFence fence = VK_NULL_HANDLE;

private:
	VulkanFence(const VulkanFence &) = delete;
	VulkanFence &operator=(const VulkanFence &) = delete;
};

class VulkanBuffer
{
public:
	VulkanBuffer(VulkanDevice *device, VkBuffer buffer, VmaAllocation allocation, size_t size);
	~VulkanBuffer();

	void SetDebugName(const char *name) { device->SetDebugObjectName(name, (uint64_t)buffer, VK_OBJECT_TYPE_BUFFER); }

	VulkanDevice *device = nullptr;

	VkBuffer buffer;
	VmaAllocation allocation;
	size_t size = 0;

	void *Map(size_t offset, size_t size);
	void Unmap();

private:
	VulkanBuffer(const VulkanBuffer &) = delete;
	VulkanBuffer &operator=(const VulkanBuffer &) = delete;
};

class VulkanFramebuffer
{
public:
	VulkanFramebuffer(VulkanDevice *device, VkFramebuffer framebuffer);
	~VulkanFramebuffer();

	void SetDebugName(const char *name) { device->SetDebugObjectName(name, (uint64_t)framebuffer, VK_OBJECT_TYPE_FRAMEBUFFER); }

	VulkanDevice *device;
	VkFramebuffer framebuffer;

private:
	VulkanFramebuffer(const VulkanFramebuffer &) = delete;
	VulkanFramebuffer &operator=(const VulkanFramebuffer &) = delete;
};

class VulkanImage
{
public:
	VulkanImage(VulkanDevice *device, VkImage image, VmaAllocation allocation, int width, int height, int mipLevels);
	~VulkanImage();

	void SetDebugName(const char *name) { device->SetDebugObjectName(name, (uint64_t)image, VK_OBJECT_TYPE_IMAGE); }

	VkImage image = VK_NULL_HANDLE;
	int width = 0;
	int height = 0;
	int mipLevels = 1;

	void *Map(size_t offset, size_t size);
	void Unmap();

private:
	VulkanDevice *device = nullptr;
	VmaAllocation allocation;

	VulkanImage(const VulkanImage &) = delete;
	VulkanImage &operator=(const VulkanImage &) = delete;
};

class VulkanImageView
{
public:
	VulkanImageView(VulkanDevice *device, VkImageView view);
	~VulkanImageView();

	void SetDebugName(const char *name) { device->SetDebugObjectName(name, (uint64_t)view, VK_OBJECT_TYPE_IMAGE_VIEW); }

	VkImageView view = VK_NULL_HANDLE;

private:
	VulkanImageView(const VulkanImageView &) = delete;
	VulkanImageView &operator=(const VulkanImageView &) = delete;

	VulkanDevice *device = nullptr;
};

class VulkanSampler
{
public:
	VulkanSampler(VulkanDevice *device, VkSampler sampler);
	~VulkanSampler();

	void SetDebugName(const char *name) { device->SetDebugObjectName(name, (uint64_t)sampler, VK_OBJECT_TYPE_SAMPLER); }

	VkSampler sampler = VK_NULL_HANDLE;

private:
	VulkanSampler(const VulkanSampler &) = delete;
	VulkanSampler &operator=(const VulkanSampler &) = delete;

	VulkanDevice *device = nullptr;
};

class VulkanShader
{
public:
	VulkanShader(VulkanDevice *device, VkShaderModule module);
	~VulkanShader();

	void SetDebugName(const char *name) { device->SetDebugObjectName(name, (uint64_t)module, VK_OBJECT_TYPE_SHADER_MODULE); }

	VkShaderModule module = VK_NULL_HANDLE;

private:
	VulkanDevice *device = nullptr;

	VulkanShader(const VulkanShader &) = delete;
	VulkanShader &operator=(const VulkanShader &) = delete;
};

class VulkanDescriptorSetLayout
{
public:
	VulkanDescriptorSetLayout(VulkanDevice *device, VkDescriptorSetLayout layout);
	~VulkanDescriptorSetLayout();

	void SetDebugName(const char *name) { device->SetDebugObjectName(name, (uint64_t)layout, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT); }

	VulkanDevice *device;
	VkDescriptorSetLayout layout;

private:
	VulkanDescriptorSetLayout(const VulkanDescriptorSetLayout &) = delete;
	VulkanDescriptorSetLayout &operator=(const VulkanDescriptorSetLayout &) = delete;
};

class VulkanDescriptorSet
{
public:
	VulkanDescriptorSet(VulkanDevice *device, VulkanDescriptorPool *pool, VkDescriptorSet set);
	~VulkanDescriptorSet();

	void SetDebugName(const char *name) { device->SetDebugObjectName(name, (uint64_t)set, VK_OBJECT_TYPE_DESCRIPTOR_SET); }

	VulkanDevice *device;
	VulkanDescriptorPool *pool;
	VkDescriptorSet set;

private:
	VulkanDescriptorSet(const VulkanDescriptorSet &) = delete;
	VulkanDescriptorSet &operator=(const VulkanDescriptorSet &) = delete;
};

class VulkanDescriptorPool
{
public:
	VulkanDescriptorPool(VulkanDevice *device, VkDescriptorPool pool);
	~VulkanDescriptorPool();

	void SetDebugName(const char *name) { device->SetDebugObjectName(name, (uint64_t)pool, VK_OBJECT_TYPE_DESCRIPTOR_POOL); }

	std::unique_ptr<VulkanDescriptorSet> allocate(VulkanDescriptorSetLayout *layout);

	VulkanDevice *device;
	VkDescriptorPool pool;

private:
	VulkanDescriptorPool(const VulkanDescriptorPool &) = delete;
	VulkanDescriptorPool &operator=(const VulkanDescriptorPool &) = delete;
};

class VulkanQueryPool
{
public:
	VulkanQueryPool(VulkanDevice *device, VkQueryPool pool);
	~VulkanQueryPool();

	void SetDebugName(const char *name) { device->SetDebugObjectName(name, (uint64_t)pool, VK_OBJECT_TYPE_QUERY_POOL); }

	bool getResults(uint32_t firstQuery, uint32_t queryCount, size_t dataSize, void *data, VkDeviceSize stride, VkQueryResultFlags flags);

	VulkanDevice *device = nullptr;
	VkQueryPool pool = VK_NULL_HANDLE;

private:
	VulkanQueryPool(const VulkanQueryPool &) = delete;
	VulkanQueryPool &operator=(const VulkanQueryPool &) = delete;
};

class VulkanPipeline
{
public:
	VulkanPipeline(VulkanDevice *device, VkPipeline pipeline);
	~VulkanPipeline();

	void SetDebugName(const char *name) { device->SetDebugObjectName(name, (uint64_t)pipeline, VK_OBJECT_TYPE_PIPELINE); }

	VulkanDevice *device;
	VkPipeline pipeline;

private:
	VulkanPipeline(const VulkanPipeline &) = delete;
	VulkanPipeline &operator=(const VulkanPipeline &) = delete;
};

class VulkanPipelineLayout
{
public:
	VulkanPipelineLayout(VulkanDevice *device, VkPipelineLayout layout);
	~VulkanPipelineLayout();

	void SetDebugName(const char *name) { device->SetDebugObjectName(name, (uint64_t)layout, VK_OBJECT_TYPE_PIPELINE_LAYOUT); }

	VulkanDevice *device;
	VkPipelineLayout layout;

private:
	VulkanPipelineLayout(const VulkanPipelineLayout &) = delete;
	VulkanPipelineLayout &operator=(const VulkanPipelineLayout &) = delete;
};

class VulkanRenderPass
{
public:
	VulkanRenderPass(VulkanDevice *device, VkRenderPass renderPass);
	~VulkanRenderPass();

	void SetDebugName(const char *name) { device->SetDebugObjectName(name, (uint64_t)renderPass, VK_OBJECT_TYPE_RENDER_PASS); }

	VulkanDevice *device;
	VkRenderPass renderPass;

private:
	VulkanRenderPass(const VulkanRenderPass &) = delete;
	VulkanRenderPass &operator=(const VulkanRenderPass &) = delete;
};

class RenderPassBegin
{
public:
	RenderPassBegin();

	void setRenderPass(VulkanRenderPass *renderpass);
	void setRenderArea(int x, int y, int width, int height);
	void setFramebuffer(VulkanFramebuffer *framebuffer);
	void addClearColor(float r, float g, float b, float a);
	void addClearDepth(float value);
	void addClearStencil(int value);
	void addClearDepthStencil(float depthValue, int stencilValue);

	VkRenderPassBeginInfo renderPassInfo = {};

private:
	std::vector<VkClearValue> clearValues;
};

class VulkanCommandBuffer
{
public:
	VulkanCommandBuffer(VulkanCommandPool *pool);
	~VulkanCommandBuffer();

	void SetDebugName(const char *name);

	void begin();
	void end();

	void bindPipeline(VkPipelineBindPoint pipelineBindPoint, VulkanPipeline *pipeline);
	void bindPipeline(VkPipelineBindPoint pipelineBindPoint, VkPipeline pipeline);
	void setViewport(uint32_t firstViewport, uint32_t viewportCount, const VkViewport* pViewports);
	void setScissor(uint32_t firstScissor, uint32_t scissorCount, const VkRect2D* pScissors);
	void setLineWidth(float lineWidth);
	void setDepthBias(float depthBiasConstantFactor, float depthBiasClamp, float depthBiasSlopeFactor);
	void setBlendConstants(const float blendConstants[4]);
	void setDepthBounds(float minDepthBounds, float maxDepthBounds);
	void setStencilCompareMask(VkStencilFaceFlags faceMask, uint32_t compareMask);
	void setStencilWriteMask(VkStencilFaceFlags faceMask, uint32_t writeMask);
	void setStencilReference(VkStencilFaceFlags faceMask, uint32_t reference);
	void bindDescriptorSet(VkPipelineBindPoint pipelineBindPoint, VulkanPipelineLayout *layout, uint32_t setIndex, VulkanDescriptorSet *descriptorSet, uint32_t dynamicOffsetCount = 0, const uint32_t* pDynamicOffsets = nullptr);
	void bindDescriptorSets(VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout, uint32_t firstSet, uint32_t descriptorSetCount, const VkDescriptorSet* pDescriptorSets, uint32_t dynamicOffsetCount, const uint32_t* pDynamicOffsets);
	void bindIndexBuffer(VkBuffer buffer, VkDeviceSize offset, VkIndexType indexType);
	void bindVertexBuffers(uint32_t firstBinding, uint32_t bindingCount, const VkBuffer* pBuffers, const VkDeviceSize* pOffsets);
	void draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance);
	void drawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance);
	void drawIndirect(VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride);
	void drawIndexedIndirect(VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride);
	void dispatch(uint32_t x, uint32_t y, uint32_t z);
	void dispatchIndirect(VkBuffer buffer, VkDeviceSize offset);
	void copyBuffer(VulkanBuffer *srcBuffer, VulkanBuffer *dstBuffer, VkDeviceSize srcOffset = 0, VkDeviceSize dstOffset = 0, VkDeviceSize size = VK_WHOLE_SIZE);
	void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, uint32_t regionCount, const VkBufferCopy* pRegions);
	void copyImage(VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkImageCopy* pRegions);
	void blitImage(VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkImageBlit* pRegions, VkFilter filter);
	void copyBufferToImage(VkBuffer srcBuffer, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkBufferImageCopy* pRegions);
	void copyImageToBuffer(VkImage srcImage, VkImageLayout srcImageLayout, VkBuffer dstBuffer, uint32_t regionCount, const VkBufferImageCopy* pRegions);
	void updateBuffer(VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize dataSize, const void* pData);
	void fillBuffer(VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize size, uint32_t data);
	void clearColorImage(VkImage image, VkImageLayout imageLayout, const VkClearColorValue* pColor, uint32_t rangeCount, const VkImageSubresourceRange* pRanges);
	void clearDepthStencilImage(VkImage image, VkImageLayout imageLayout, const VkClearDepthStencilValue* pDepthStencil, uint32_t rangeCount, const VkImageSubresourceRange* pRanges);
	void clearAttachments(uint32_t attachmentCount, const VkClearAttachment* pAttachments, uint32_t rectCount, const VkClearRect* pRects);
	void resolveImage(VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkImageResolve* pRegions);
	void setEvent(VkEvent event, VkPipelineStageFlags stageMask);
	void resetEvent(VkEvent event, VkPipelineStageFlags stageMask);
	void waitEvents(uint32_t eventCount, const VkEvent* pEvents, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, uint32_t memoryBarrierCount, const VkMemoryBarrier* pMemoryBarriers, uint32_t bufferMemoryBarrierCount, const VkBufferMemoryBarrier* pBufferMemoryBarriers, uint32_t imageMemoryBarrierCount, const VkImageMemoryBarrier* pImageMemoryBarriers);
	void pipelineBarrier(VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkDependencyFlags dependencyFlags, uint32_t memoryBarrierCount, const VkMemoryBarrier* pMemoryBarriers, uint32_t bufferMemoryBarrierCount, const VkBufferMemoryBarrier* pBufferMemoryBarriers, uint32_t imageMemoryBarrierCount, const VkImageMemoryBarrier* pImageMemoryBarriers);
	void beginQuery(VulkanQueryPool *queryPool, uint32_t query, VkQueryControlFlags flags);
	void beginQuery(VkQueryPool queryPool, uint32_t query, VkQueryControlFlags flags);
	void endQuery(VulkanQueryPool *queryPool, uint32_t query);
	void endQuery(VkQueryPool queryPool, uint32_t query);
	void resetQueryPool(VulkanQueryPool *queryPool, uint32_t firstQuery, uint32_t queryCount);
	void resetQueryPool(VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount);
	void writeTimestamp(VkPipelineStageFlagBits pipelineStage, VulkanQueryPool *queryPool, uint32_t query);
	void writeTimestamp(VkPipelineStageFlagBits pipelineStage, VkQueryPool queryPool, uint32_t query);
	void copyQueryPoolResults(VulkanQueryPool *queryPool, uint32_t firstQuery, uint32_t queryCount, VulkanBuffer *dstBuffer, VkDeviceSize dstOffset, VkDeviceSize stride, VkQueryResultFlags flags);
	void copyQueryPoolResults(VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount, VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize stride, VkQueryResultFlags flags);
	void pushConstants(VulkanPipelineLayout *layout, VkShaderStageFlags stageFlags, uint32_t offset, uint32_t size, const void* pValues);
	void pushConstants(VkPipelineLayout layout, VkShaderStageFlags stageFlags, uint32_t offset, uint32_t size, const void* pValues);
	void beginRenderPass(const RenderPassBegin &renderPassBegin, VkSubpassContents contents = VK_SUBPASS_CONTENTS_INLINE);
	void beginRenderPass(const VkRenderPassBeginInfo* pRenderPassBegin, VkSubpassContents contents);
	void nextSubpass(VkSubpassContents contents);
	void endRenderPass();
	void executeCommands(uint32_t commandBufferCount, const VkCommandBuffer* pCommandBuffers);

	void debugFullPipelineBarrier();

	VkCommandBuffer buffer = nullptr;

private:
	VulkanCommandPool *pool = nullptr;

	VulkanCommandBuffer(const VulkanCommandBuffer &) = delete;
	VulkanCommandBuffer &operator=(const VulkanCommandBuffer &) = delete;
};

class VulkanCommandPool
{
public:
	VulkanCommandPool(VulkanDevice *device, int queueFamilyIndex);
	~VulkanCommandPool();

	void SetDebugName(const char *name) { device->SetDebugObjectName(name, (uint64_t)pool, VK_OBJECT_TYPE_COMMAND_POOL); }

	std::unique_ptr<VulkanCommandBuffer> createBuffer();

	VkCommandPool pool = VK_NULL_HANDLE;

private:
	VulkanDevice *device = nullptr;

	VulkanCommandPool(const VulkanCommandPool &) = delete;
	VulkanCommandPool &operator=(const VulkanCommandPool &) = delete;

	friend class VulkanCommandBuffer;
};

/////////////////////////////////////////////////////////////////////////////

inline VulkanSemaphore::VulkanSemaphore(VulkanDevice *device) : device(device)
{
	VkSemaphoreCreateInfo semaphoreInfo = {};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	VkResult result = vkCreateSemaphore(device->device, &semaphoreInfo, nullptr, &semaphore);
	if (result != VK_SUCCESS)
		I_Error("Failed to create semaphore!");
}

inline VulkanSemaphore::~VulkanSemaphore()
{
	vkDestroySemaphore(device->device, semaphore, nullptr);
}

/////////////////////////////////////////////////////////////////////////////

inline VulkanFence::VulkanFence(VulkanDevice *device) : device(device)
{
	VkFenceCreateInfo fenceInfo = {};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	VkResult result = vkCreateFence(device->device, &fenceInfo, nullptr, &fence);
	if (result != VK_SUCCESS)
		I_Error("Failed to create fence!");
}

inline VulkanFence::~VulkanFence()
{
	vkDestroyFence(device->device, fence, nullptr);
}

/////////////////////////////////////////////////////////////////////////////

inline VulkanBuffer::VulkanBuffer(VulkanDevice *device, VkBuffer buffer, VmaAllocation allocation, size_t size) : device(device), buffer(buffer), allocation(allocation), size(size)
{
}

inline VulkanBuffer::~VulkanBuffer()
{
	vmaDestroyBuffer(device->allocator, buffer, allocation);
}

inline void *VulkanBuffer::Map(size_t offset, size_t size)
{
	void *data;
	VkResult result = vmaMapMemory(device->allocator, allocation, &data);
	return (result == VK_SUCCESS) ? ((uint8_t*)data) + offset : nullptr;
}

inline void VulkanBuffer::Unmap()
{
	vmaUnmapMemory(device->allocator, allocation);
}

/////////////////////////////////////////////////////////////////////////////

inline VulkanCommandPool::VulkanCommandPool(VulkanDevice *device, int queueFamilyIndex) : device(device)
{
	VkCommandPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.queueFamilyIndex = queueFamilyIndex;
	poolInfo.flags = 0;

	VkResult result = vkCreateCommandPool(device->device, &poolInfo, nullptr, &pool);
	if (result != VK_SUCCESS)
		I_Error("Could not create command pool");
}

inline VulkanCommandPool::~VulkanCommandPool()
{
	vkDestroyCommandPool(device->device, pool, nullptr);
}

inline std::unique_ptr<VulkanCommandBuffer> VulkanCommandPool::createBuffer()
{
	return std::make_unique<VulkanCommandBuffer>(this);
}

/////////////////////////////////////////////////////////////////////////////

inline RenderPassBegin::RenderPassBegin()
{
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
}

inline void RenderPassBegin::setRenderPass(VulkanRenderPass *renderPass)
{
	renderPassInfo.renderPass = renderPass->renderPass;
}

inline void RenderPassBegin::setRenderArea(int x, int y, int width, int height)
{
	renderPassInfo.renderArea.offset.x = x;
	renderPassInfo.renderArea.offset.y = y;
	renderPassInfo.renderArea.extent.width = width;
	renderPassInfo.renderArea.extent.height = height;
}

inline void RenderPassBegin::setFramebuffer(VulkanFramebuffer *framebuffer)
{
	renderPassInfo.framebuffer = framebuffer->framebuffer;
}

inline void RenderPassBegin::addClearColor(float r, float g, float b, float a)
{
	VkClearValue clearValue = { };
	clearValue.color = { {r, g, b, a} };
	clearValues.push_back(clearValue);

	renderPassInfo.clearValueCount = (uint32_t)clearValues.size();
	renderPassInfo.pClearValues = clearValues.data();
}

inline void RenderPassBegin::addClearDepth(float value)
{
	VkClearValue clearValue = { };
	clearValue.depthStencil.depth = value;
	clearValues.push_back(clearValue);

	renderPassInfo.clearValueCount = (uint32_t)clearValues.size();
	renderPassInfo.pClearValues = clearValues.data();
}

inline void RenderPassBegin::addClearStencil(int value)
{
	VkClearValue clearValue = { };
	clearValue.depthStencil.stencil = value;
	clearValues.push_back(clearValue);

	renderPassInfo.clearValueCount = (uint32_t)clearValues.size();
	renderPassInfo.pClearValues = clearValues.data();
}

inline void RenderPassBegin::addClearDepthStencil(float depthValue, int stencilValue)
{
	VkClearValue clearValue = { };
	clearValue.depthStencil.depth = depthValue;
	clearValue.depthStencil.stencil = stencilValue;
	clearValues.push_back(clearValue);

	renderPassInfo.clearValueCount = (uint32_t)clearValues.size();
	renderPassInfo.pClearValues = clearValues.data();
}

/////////////////////////////////////////////////////////////////////////////

inline VulkanCommandBuffer::VulkanCommandBuffer(VulkanCommandPool *pool) : pool(pool)
{
	VkCommandBufferAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool = pool->pool;
	allocInfo.commandBufferCount = 1;

	VkResult result = vkAllocateCommandBuffers(pool->device->device, &allocInfo, &buffer);
	if (result != VK_SUCCESS)
		I_Error("Could not create command buffer");
}

inline VulkanCommandBuffer::~VulkanCommandBuffer()
{
	vkFreeCommandBuffers(pool->device->device, pool->pool, 1, &buffer);
}

inline void VulkanCommandBuffer::begin()
{
	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
	beginInfo.pInheritanceInfo = nullptr;

	VkResult result = vkBeginCommandBuffer(buffer, &beginInfo);
	if (result != VK_SUCCESS)
		I_Error("Failed to begin recording command buffer!");
}

inline void VulkanCommandBuffer::end()
{
	VkResult result = vkEndCommandBuffer(buffer);
	if (result != VK_SUCCESS)
		I_Error("Failed to record command buffer!");
}

inline void VulkanCommandBuffer::debugFullPipelineBarrier()
{
	VkMemoryBarrier barrier = { };
	barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
	barrier.srcAccessMask =
		VK_ACCESS_INDIRECT_COMMAND_READ_BIT |
		VK_ACCESS_INDEX_READ_BIT |
		VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT |
		VK_ACCESS_UNIFORM_READ_BIT |
		VK_ACCESS_INPUT_ATTACHMENT_READ_BIT |
		VK_ACCESS_SHADER_READ_BIT |
		VK_ACCESS_SHADER_WRITE_BIT |
		VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
		VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
		VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
		VK_ACCESS_TRANSFER_READ_BIT |
		VK_ACCESS_TRANSFER_WRITE_BIT |
		VK_ACCESS_HOST_READ_BIT |
		VK_ACCESS_HOST_WRITE_BIT;
	barrier.dstAccessMask =
		VK_ACCESS_INDIRECT_COMMAND_READ_BIT |
		VK_ACCESS_INDEX_READ_BIT |
		VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT |
		VK_ACCESS_UNIFORM_READ_BIT |
		VK_ACCESS_INPUT_ATTACHMENT_READ_BIT |
		VK_ACCESS_SHADER_READ_BIT |
		VK_ACCESS_SHADER_WRITE_BIT |
		VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
		VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
		VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
		VK_ACCESS_TRANSFER_READ_BIT |
		VK_ACCESS_TRANSFER_WRITE_BIT |
		VK_ACCESS_HOST_READ_BIT |
		VK_ACCESS_HOST_WRITE_BIT;

	vkCmdPipelineBarrier(buffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 1, &barrier, 0, nullptr, 0, nullptr);
}

inline void VulkanCommandBuffer::bindPipeline(VkPipelineBindPoint pipelineBindPoint, VulkanPipeline *pipeline)
{
	bindPipeline(pipelineBindPoint, pipeline->pipeline);
}

inline void VulkanCommandBuffer::bindPipeline(VkPipelineBindPoint pipelineBindPoint, VkPipeline pipeline)
{
	vkCmdBindPipeline(buffer, pipelineBindPoint, pipeline);
}

inline void VulkanCommandBuffer::setViewport(uint32_t firstViewport, uint32_t viewportCount, const VkViewport* pViewports)
{
	vkCmdSetViewport(buffer, firstViewport, viewportCount, pViewports);
}

inline void VulkanCommandBuffer::setScissor(uint32_t firstScissor, uint32_t scissorCount, const VkRect2D* pScissors)
{
	vkCmdSetScissor(buffer, firstScissor, scissorCount, pScissors);
}

inline void VulkanCommandBuffer::setLineWidth(float lineWidth)
{
	vkCmdSetLineWidth(buffer, lineWidth);
}

inline void VulkanCommandBuffer::setDepthBias(float depthBiasConstantFactor, float depthBiasClamp, float depthBiasSlopeFactor)
{
	vkCmdSetDepthBias(buffer, depthBiasConstantFactor, depthBiasClamp, depthBiasSlopeFactor);
}

inline void VulkanCommandBuffer::setBlendConstants(const float blendConstants[4])
{
	vkCmdSetBlendConstants(buffer, blendConstants);
}

inline void VulkanCommandBuffer::setDepthBounds(float minDepthBounds, float maxDepthBounds)
{
	vkCmdSetDepthBounds(buffer, minDepthBounds, maxDepthBounds);
}

inline void VulkanCommandBuffer::setStencilCompareMask(VkStencilFaceFlags faceMask, uint32_t compareMask)
{
	vkCmdSetStencilCompareMask(buffer, faceMask, compareMask);
}

inline void VulkanCommandBuffer::setStencilWriteMask(VkStencilFaceFlags faceMask, uint32_t writeMask)
{
	vkCmdSetStencilWriteMask(buffer, faceMask, writeMask);
}

inline void VulkanCommandBuffer::setStencilReference(VkStencilFaceFlags faceMask, uint32_t reference)
{
	vkCmdSetStencilReference(buffer, faceMask, reference);
}

inline void VulkanCommandBuffer::bindDescriptorSet(VkPipelineBindPoint pipelineBindPoint, VulkanPipelineLayout *layout, uint32_t setIndex, VulkanDescriptorSet *descriptorSet, uint32_t dynamicOffsetCount, const uint32_t* pDynamicOffsets)
{
	bindDescriptorSets(pipelineBindPoint, layout->layout, setIndex, 1, &descriptorSet->set, dynamicOffsetCount, pDynamicOffsets);
}

inline void VulkanCommandBuffer::bindDescriptorSets(VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout, uint32_t firstSet, uint32_t descriptorSetCount, const VkDescriptorSet* pDescriptorSets, uint32_t dynamicOffsetCount, const uint32_t* pDynamicOffsets)
{
	vkCmdBindDescriptorSets(buffer, pipelineBindPoint, layout, firstSet, descriptorSetCount, pDescriptorSets, dynamicOffsetCount, pDynamicOffsets);
}

inline void VulkanCommandBuffer::bindIndexBuffer(VkBuffer buffer, VkDeviceSize offset, VkIndexType indexType)
{
	vkCmdBindIndexBuffer(this->buffer, buffer, offset, indexType);
}

inline void VulkanCommandBuffer::bindVertexBuffers(uint32_t firstBinding, uint32_t bindingCount, const VkBuffer* pBuffers, const VkDeviceSize* pOffsets)
{
	vkCmdBindVertexBuffers(buffer, firstBinding, bindingCount, pBuffers, pOffsets);
}

inline void VulkanCommandBuffer::draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
{
	vkCmdDraw(buffer, vertexCount, instanceCount, firstVertex, firstInstance);
}

inline void VulkanCommandBuffer::drawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance)
{
	vkCmdDrawIndexed(buffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

inline void VulkanCommandBuffer::drawIndirect(VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride)
{
	vkCmdDrawIndirect(this->buffer, buffer, offset, drawCount, stride);
}

inline void VulkanCommandBuffer::drawIndexedIndirect(VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride)
{
	vkCmdDrawIndexedIndirect(this->buffer, buffer, offset, drawCount, stride);
}

inline void VulkanCommandBuffer::dispatch(uint32_t x, uint32_t y, uint32_t z)
{
	vkCmdDispatch(buffer, x, y, z);
}

inline void VulkanCommandBuffer::dispatchIndirect(VkBuffer buffer, VkDeviceSize offset)
{
	vkCmdDispatchIndirect(this->buffer, buffer, offset);
}

inline void VulkanCommandBuffer::copyBuffer(VulkanBuffer *srcBuffer, VulkanBuffer *dstBuffer, VkDeviceSize srcOffset, VkDeviceSize dstOffset, VkDeviceSize size)
{
	VkBufferCopy region = { };
	region.srcOffset = srcOffset;
	region.dstOffset = dstOffset;
	region.size = (size == VK_WHOLE_SIZE) ? dstBuffer->size : size;
	copyBuffer(srcBuffer->buffer, dstBuffer->buffer, 1, &region);
}

inline void VulkanCommandBuffer::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, uint32_t regionCount, const VkBufferCopy* pRegions)
{
	vkCmdCopyBuffer(buffer, srcBuffer, dstBuffer, regionCount, pRegions);
}

inline void VulkanCommandBuffer::copyImage(VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkImageCopy* pRegions)
{
	vkCmdCopyImage(buffer, srcImage, srcImageLayout, dstImage, dstImageLayout, regionCount, pRegions);
}

inline void VulkanCommandBuffer::blitImage(VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkImageBlit* pRegions, VkFilter filter)
{
	vkCmdBlitImage(buffer, srcImage, srcImageLayout, dstImage, dstImageLayout, regionCount, pRegions, filter);
}

inline void VulkanCommandBuffer::copyBufferToImage(VkBuffer srcBuffer, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkBufferImageCopy* pRegions)
{
	vkCmdCopyBufferToImage(buffer, srcBuffer, dstImage, dstImageLayout, regionCount, pRegions);
}

inline void VulkanCommandBuffer::copyImageToBuffer(VkImage srcImage, VkImageLayout srcImageLayout, VkBuffer dstBuffer, uint32_t regionCount, const VkBufferImageCopy* pRegions)
{
	vkCmdCopyImageToBuffer(buffer, srcImage, srcImageLayout, dstBuffer, regionCount, pRegions);
}

inline void VulkanCommandBuffer::updateBuffer(VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize dataSize, const void* pData)
{
	vkCmdUpdateBuffer(buffer, dstBuffer, dstOffset, dataSize, pData);
}

inline void VulkanCommandBuffer::fillBuffer(VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize size, uint32_t data)
{
	vkCmdFillBuffer(buffer, dstBuffer, dstOffset, size, data);
}

inline void VulkanCommandBuffer::clearColorImage(VkImage image, VkImageLayout imageLayout, const VkClearColorValue* pColor, uint32_t rangeCount, const VkImageSubresourceRange* pRanges)
{
	vkCmdClearColorImage(buffer, image, imageLayout, pColor, rangeCount, pRanges);
}

inline void VulkanCommandBuffer::clearDepthStencilImage(VkImage image, VkImageLayout imageLayout, const VkClearDepthStencilValue* pDepthStencil, uint32_t rangeCount, const VkImageSubresourceRange* pRanges)
{
	vkCmdClearDepthStencilImage(buffer, image, imageLayout, pDepthStencil, rangeCount, pRanges);
}

inline void VulkanCommandBuffer::clearAttachments(uint32_t attachmentCount, const VkClearAttachment* pAttachments, uint32_t rectCount, const VkClearRect* pRects)
{
	vkCmdClearAttachments(buffer, attachmentCount, pAttachments, rectCount, pRects);
}

inline void VulkanCommandBuffer::resolveImage(VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkImageResolve* pRegions)
{
	vkCmdResolveImage(buffer, srcImage, srcImageLayout, dstImage, dstImageLayout, regionCount, pRegions);
}

inline void VulkanCommandBuffer::setEvent(VkEvent event, VkPipelineStageFlags stageMask)
{
	vkCmdSetEvent(buffer, event, stageMask);
}

inline void VulkanCommandBuffer::resetEvent(VkEvent event, VkPipelineStageFlags stageMask)
{
	vkCmdResetEvent(buffer, event, stageMask);
}

inline void VulkanCommandBuffer::waitEvents(uint32_t eventCount, const VkEvent* pEvents, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, uint32_t memoryBarrierCount, const VkMemoryBarrier* pMemoryBarriers, uint32_t bufferMemoryBarrierCount, const VkBufferMemoryBarrier* pBufferMemoryBarriers, uint32_t imageMemoryBarrierCount, const VkImageMemoryBarrier* pImageMemoryBarriers)
{
	vkCmdWaitEvents(buffer, eventCount, pEvents, srcStageMask, dstStageMask, memoryBarrierCount, pMemoryBarriers, bufferMemoryBarrierCount, pBufferMemoryBarriers, imageMemoryBarrierCount, pImageMemoryBarriers);
}

inline void VulkanCommandBuffer::pipelineBarrier(VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkDependencyFlags dependencyFlags, uint32_t memoryBarrierCount, const VkMemoryBarrier* pMemoryBarriers, uint32_t bufferMemoryBarrierCount, const VkBufferMemoryBarrier* pBufferMemoryBarriers, uint32_t imageMemoryBarrierCount, const VkImageMemoryBarrier* pImageMemoryBarriers)
{
	vkCmdPipelineBarrier(buffer, srcStageMask, dstStageMask, dependencyFlags, memoryBarrierCount, pMemoryBarriers, bufferMemoryBarrierCount, pBufferMemoryBarriers, imageMemoryBarrierCount, pImageMemoryBarriers);
}

inline void VulkanCommandBuffer::beginQuery(VulkanQueryPool *queryPool, uint32_t query, VkQueryControlFlags flags)
{
	beginQuery(queryPool->pool, query, flags);
}

inline void VulkanCommandBuffer::beginQuery(VkQueryPool queryPool, uint32_t query, VkQueryControlFlags flags)
{
	vkCmdBeginQuery(buffer, queryPool, query, flags);
}

inline void VulkanCommandBuffer::endQuery(VulkanQueryPool *queryPool, uint32_t query)
{
	endQuery(queryPool->pool, query);
}

inline void VulkanCommandBuffer::endQuery(VkQueryPool queryPool, uint32_t query)
{
	vkCmdEndQuery(buffer, queryPool, query);
}

inline void VulkanCommandBuffer::resetQueryPool(VulkanQueryPool *queryPool, uint32_t firstQuery, uint32_t queryCount)
{
	resetQueryPool(queryPool->pool, firstQuery, queryCount);
}

inline void VulkanCommandBuffer::resetQueryPool(VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount)
{
	vkCmdResetQueryPool(buffer, queryPool, firstQuery, queryCount);
}

inline void VulkanCommandBuffer::writeTimestamp(VkPipelineStageFlagBits pipelineStage, VulkanQueryPool *queryPool, uint32_t query)
{
	writeTimestamp(pipelineStage, queryPool->pool, query);
}

inline void VulkanCommandBuffer::writeTimestamp(VkPipelineStageFlagBits pipelineStage, VkQueryPool queryPool, uint32_t query)
{
	vkCmdWriteTimestamp(buffer, pipelineStage, queryPool, query);
}

inline void VulkanCommandBuffer::copyQueryPoolResults(VulkanQueryPool *queryPool, uint32_t firstQuery, uint32_t queryCount, VulkanBuffer *dstBuffer, VkDeviceSize dstOffset, VkDeviceSize stride, VkQueryResultFlags flags)
{
	copyQueryPoolResults(queryPool->pool, firstQuery, queryCount, dstBuffer->buffer, dstOffset, stride, flags);
}

inline void VulkanCommandBuffer::copyQueryPoolResults(VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount, VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize stride, VkQueryResultFlags flags)
{
	vkCmdCopyQueryPoolResults(buffer, queryPool, firstQuery, queryCount, dstBuffer, dstOffset, stride, flags);
}

inline void VulkanCommandBuffer::pushConstants(VulkanPipelineLayout *layout, VkShaderStageFlags stageFlags, uint32_t offset, uint32_t size, const void* pValues)
{
	pushConstants(layout->layout, stageFlags, offset, size, pValues);
}

inline void VulkanCommandBuffer::pushConstants(VkPipelineLayout layout, VkShaderStageFlags stageFlags, uint32_t offset, uint32_t size, const void* pValues)
{
	vkCmdPushConstants(buffer, layout, stageFlags, offset, size, pValues);
}

inline void VulkanCommandBuffer::beginRenderPass(const RenderPassBegin &renderPassBegin, VkSubpassContents contents)
{
	beginRenderPass(&renderPassBegin.renderPassInfo, contents);
}

inline void VulkanCommandBuffer::beginRenderPass(const VkRenderPassBeginInfo* pRenderPassBegin, VkSubpassContents contents)
{
	vkCmdBeginRenderPass(buffer, pRenderPassBegin, contents);
}

inline void VulkanCommandBuffer::nextSubpass(VkSubpassContents contents)
{
	vkCmdNextSubpass(buffer, contents);
}

inline void VulkanCommandBuffer::endRenderPass()
{
	vkCmdEndRenderPass(buffer);
}

inline void VulkanCommandBuffer::executeCommands(uint32_t commandBufferCount, const VkCommandBuffer* pCommandBuffers)
{
	vkCmdExecuteCommands(buffer, commandBufferCount, pCommandBuffers);
}

inline void VulkanCommandBuffer::SetDebugName(const char *name)
{
	pool->device->SetDebugObjectName(name, (uint64_t)buffer, VK_OBJECT_TYPE_COMMAND_BUFFER);
}

/////////////////////////////////////////////////////////////////////////////

inline VulkanShader::VulkanShader(VulkanDevice *device, VkShaderModule module) : module(module), device(device)
{
}

inline VulkanShader::~VulkanShader()
{
	vkDestroyShaderModule(device->device, module, nullptr);
}

/////////////////////////////////////////////////////////////////////////////

inline VulkanDescriptorSetLayout::VulkanDescriptorSetLayout(VulkanDevice *device, VkDescriptorSetLayout layout) : device(device), layout(layout)
{
}

inline VulkanDescriptorSetLayout::~VulkanDescriptorSetLayout()
{
	vkDestroyDescriptorSetLayout(device->device, layout, nullptr);
}

/////////////////////////////////////////////////////////////////////////////

inline VulkanDescriptorSet::VulkanDescriptorSet(VulkanDevice *device, VulkanDescriptorPool *pool, VkDescriptorSet set) : device(device), pool(pool), set(set)
{
}

inline VulkanDescriptorSet::~VulkanDescriptorSet()
{
	vkFreeDescriptorSets(device->device, pool->pool, 1, &set);
}

/////////////////////////////////////////////////////////////////////////////

inline VulkanDescriptorPool::VulkanDescriptorPool(VulkanDevice *device, VkDescriptorPool pool) : device(device), pool(pool)
{
}

inline VulkanDescriptorPool::~VulkanDescriptorPool()
{
	vkDestroyDescriptorPool(device->device, pool, nullptr);
}

inline std::unique_ptr<VulkanDescriptorSet> VulkanDescriptorPool::allocate(VulkanDescriptorSetLayout *layout)
{
	VkDescriptorSetAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = pool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &layout->layout;

	VkDescriptorSet descriptorSet;
	VkResult result = vkAllocateDescriptorSets(device->device, &allocInfo, &descriptorSet);
	if (result != VK_SUCCESS)
		I_Error("Could not allocate descriptor sets");

	return std::make_unique<VulkanDescriptorSet>(device, this, descriptorSet);
}

/////////////////////////////////////////////////////////////////////////////

inline VulkanQueryPool::VulkanQueryPool(VulkanDevice *device, VkQueryPool pool) : device(device), pool(pool)
{
}

inline VulkanQueryPool::~VulkanQueryPool()
{
	vkDestroyQueryPool(device->device, pool, nullptr);
}

inline bool VulkanQueryPool::getResults(uint32_t firstQuery, uint32_t queryCount, size_t dataSize, void *data, VkDeviceSize stride, VkQueryResultFlags flags)
{
	VkResult result = vkGetQueryPoolResults(device->device, pool, firstQuery, queryCount, dataSize, data, stride, flags);
	if (result != VK_SUCCESS && result != VK_NOT_READY)
		I_Error("vkGetQueryPoolResults failed");
	return result == VK_SUCCESS;
}

/////////////////////////////////////////////////////////////////////////////

inline VulkanFramebuffer::VulkanFramebuffer(VulkanDevice *device, VkFramebuffer framebuffer) : device(device), framebuffer(framebuffer)
{
}

inline VulkanFramebuffer::~VulkanFramebuffer()
{
	vkDestroyFramebuffer(device->device, framebuffer, nullptr);
}

/////////////////////////////////////////////////////////////////////////////

inline VulkanImage::VulkanImage(VulkanDevice *device, VkImage image, VmaAllocation allocation, int width, int height, int mipLevels) : image(image), width(width), height(height), mipLevels(mipLevels), device(device), allocation(allocation)
{
}

inline VulkanImage::~VulkanImage()
{
	vmaDestroyImage(device->allocator, image, allocation);
}

inline void *VulkanImage::Map(size_t offset, size_t size)
{
	void *data;
	VkResult result = vmaMapMemory(device->allocator, allocation, &data);
	return (result == VK_SUCCESS) ? ((uint8_t*)data) + offset : nullptr;
}

inline void VulkanImage::Unmap()
{
	vmaUnmapMemory(device->allocator, allocation);
}

/////////////////////////////////////////////////////////////////////////////

inline VulkanImageView::VulkanImageView(VulkanDevice *device, VkImageView view) : view(view), device(device)
{
}

inline VulkanImageView::~VulkanImageView()
{
	vkDestroyImageView(device->device, view, nullptr);
}

/////////////////////////////////////////////////////////////////////////////

inline VulkanSampler::VulkanSampler(VulkanDevice *device, VkSampler sampler) : sampler(sampler), device(device)
{
}

inline VulkanSampler::~VulkanSampler()
{
	vkDestroySampler(device->device, sampler, nullptr);
}

/////////////////////////////////////////////////////////////////////////////

inline VulkanPipeline::VulkanPipeline(VulkanDevice *device, VkPipeline pipeline) : device(device), pipeline(pipeline)
{
}

inline VulkanPipeline::~VulkanPipeline()
{
	vkDestroyPipeline(device->device, pipeline, nullptr);
}

/////////////////////////////////////////////////////////////////////////////

inline VulkanPipelineLayout::VulkanPipelineLayout(VulkanDevice *device, VkPipelineLayout layout) : device(device), layout(layout)
{
}

inline VulkanPipelineLayout::~VulkanPipelineLayout()
{
	vkDestroyPipelineLayout(device->device, layout, nullptr);
}

/////////////////////////////////////////////////////////////////////////////

inline VulkanRenderPass::VulkanRenderPass(VulkanDevice *device, VkRenderPass renderPass) : device(device), renderPass(renderPass)
{
}

inline VulkanRenderPass::~VulkanRenderPass()
{
	vkDestroyRenderPass(device->device, renderPass, nullptr);
}
