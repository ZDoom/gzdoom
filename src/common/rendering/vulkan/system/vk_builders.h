#pragma once

#include "vk_objects.h"
#include "zstring.h"
#include <cassert>

class ImageBuilder
{
public:
	ImageBuilder();

	void setSize(int width, int height, int miplevels = 1, int arrayLayers = 1);
	void setSamples(VkSampleCountFlagBits samples);
	void setFormat(VkFormat format);
	void setUsage(VkImageUsageFlags imageUsage, VmaMemoryUsage memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY, VmaAllocationCreateFlags allocFlags = 0);
	void setMemoryType(VkMemoryPropertyFlags requiredFlags, VkMemoryPropertyFlags preferredFlags, uint32_t memoryTypeBits = 0);
	void setLinearTiling();

	bool isFormatSupported(VulkanDevice *device, VkFormatFeatureFlags bufferFeatures = 0);

	std::unique_ptr<VulkanImage> create(VulkanDevice *device, VkDeviceSize* allocatedBytes = nullptr);
	std::unique_ptr<VulkanImage> tryCreate(VulkanDevice *device);

private:
	VkImageCreateInfo imageInfo = {};
	VmaAllocationCreateInfo allocInfo = {};
};

class ImageViewBuilder
{
public:
	ImageViewBuilder();

	void setType(VkImageViewType type);
	void setImage(VulkanImage *image, VkFormat format, VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT);

	std::unique_ptr<VulkanImageView> create(VulkanDevice *device);

private:
	VkImageViewCreateInfo viewInfo = {};
};

class SamplerBuilder
{
public:
	SamplerBuilder();

	void setAddressMode(VkSamplerAddressMode addressMode);
	void setAddressMode(VkSamplerAddressMode u, VkSamplerAddressMode v, VkSamplerAddressMode w);
	void setMinFilter(VkFilter minFilter);
	void setMagFilter(VkFilter magFilter);
	void setMipmapMode(VkSamplerMipmapMode mode);
	void setAnisotropy(float maxAnisotropy);
	void setMaxLod(float value);

	std::unique_ptr<VulkanSampler> create(VulkanDevice *device);

private:
	VkSamplerCreateInfo samplerInfo = {};
};

class BufferBuilder
{
public:
	BufferBuilder();

	void setSize(size_t size);
	void setUsage(VkBufferUsageFlags bufferUsage, VmaMemoryUsage memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY, VmaAllocationCreateFlags allocFlags = 0);
	void setMemoryType(VkMemoryPropertyFlags requiredFlags, VkMemoryPropertyFlags preferredFlags, uint32_t memoryTypeBits = 0);

	std::unique_ptr<VulkanBuffer> create(VulkanDevice *device);

private:
	VkBufferCreateInfo bufferInfo = {};
	VmaAllocationCreateInfo allocInfo = {};
};

class ShaderBuilder
{
public:
	ShaderBuilder();

	void setVertexShader(const FString &code);
	void setFragmentShader(const FString &code);

	std::unique_ptr<VulkanShader> create(const char *shadername, VulkanDevice *device);

private:
	FString code;
	int stage;
};

class ComputePipelineBuilder
{
public:
	ComputePipelineBuilder();

	void setLayout(VulkanPipelineLayout *layout);
	void setComputeShader(VulkanShader *shader);

	std::unique_ptr<VulkanPipeline> create(VulkanDevice *device);

private:
	VkComputePipelineCreateInfo pipelineInfo = {};
	VkPipelineShaderStageCreateInfo stageInfo = {};
};

class DescriptorSetLayoutBuilder
{
public:
	DescriptorSetLayoutBuilder();

	void addBinding(int binding, VkDescriptorType type, int arrayCount, VkShaderStageFlags stageFlags);

	std::unique_ptr<VulkanDescriptorSetLayout> create(VulkanDevice *device);

private:
	VkDescriptorSetLayoutCreateInfo layoutInfo = {};
	TArray<VkDescriptorSetLayoutBinding> bindings;
};

class DescriptorPoolBuilder
{
public:
	DescriptorPoolBuilder();

	void setMaxSets(int value);
	void addPoolSize(VkDescriptorType type, int count);

	std::unique_ptr<VulkanDescriptorPool> create(VulkanDevice *device);

private:
	std::vector<VkDescriptorPoolSize> poolSizes;
	VkDescriptorPoolCreateInfo poolInfo = {};
};

class QueryPoolBuilder
{
public:
	QueryPoolBuilder();

	void setQueryType(VkQueryType type, int count, VkQueryPipelineStatisticFlags pipelineStatistics = 0);

	std::unique_ptr<VulkanQueryPool> create(VulkanDevice *device);

private:
	VkQueryPoolCreateInfo poolInfo = {};
};

class FramebufferBuilder
{
public:
	FramebufferBuilder();

	void setRenderPass(VulkanRenderPass *renderPass);
	void addAttachment(VulkanImageView *view);
	void addAttachment(VkImageView view);
	void setSize(int width, int height, int layers = 1);

	std::unique_ptr<VulkanFramebuffer> create(VulkanDevice *device);

private:
	VkFramebufferCreateInfo framebufferInfo = {};
	std::vector<VkImageView> attachments;
};

union FRenderStyle;

class GraphicsPipelineBuilder
{
public:
	GraphicsPipelineBuilder();

	void setSubpass(int subpass);
	void setLayout(VulkanPipelineLayout *layout);
	void setRenderPass(VulkanRenderPass *renderPass);
	void setTopology(VkPrimitiveTopology topology);
	void setViewport(float x, float y, float width, float height, float minDepth = 0.0f, float maxDepth = 1.0f);
	void setScissor(int x, int y, int width, int height);
	void setRasterizationSamples(VkSampleCountFlagBits samples);

	void setCull(VkCullModeFlags cullMode, VkFrontFace frontFace);
	void setDepthStencilEnable(bool test, bool write, bool stencil);
	void setDepthFunc(VkCompareOp func);
	void setDepthClampEnable(bool value);
	void setDepthBias(bool enable, float biasConstantFactor, float biasClamp, float biasSlopeFactor);
	void setColorWriteMask(VkColorComponentFlags mask);
	void setStencil(VkStencilOp failOp, VkStencilOp passOp, VkStencilOp depthFailOp, VkCompareOp compareOp, uint32_t compareMask, uint32_t writeMask, uint32_t reference);

	void setAdditiveBlendMode();
	void setAlphaBlendMode();
	void setBlendMode(const FRenderStyle &style);
	void setBlendMode(VkBlendOp op, VkBlendFactor src, VkBlendFactor dst);
	void setSubpassColorAttachmentCount(int count);

	void addVertexShader(VulkanShader *shader);
	void addFragmentShader(VulkanShader *shader);

	void addVertexBufferBinding(int index, size_t stride);
	void addVertexAttribute(int location, int binding, VkFormat format, size_t offset);

	void addDynamicState(VkDynamicState state);

	std::unique_ptr<VulkanPipeline> create(VulkanDevice *device);

private:
	VkGraphicsPipelineCreateInfo pipelineInfo = { };
	VkPipelineVertexInputStateCreateInfo vertexInputInfo = { };
	VkPipelineInputAssemblyStateCreateInfo inputAssembly = { };
	VkViewport viewport = { };
	VkRect2D scissor = { };
	VkPipelineViewportStateCreateInfo viewportState = { };
	VkPipelineRasterizationStateCreateInfo rasterizer = { };
	VkPipelineMultisampleStateCreateInfo multisampling = { };
	VkPipelineColorBlendAttachmentState colorBlendAttachment = { };
	VkPipelineColorBlendStateCreateInfo colorBlending = { };
	VkPipelineDepthStencilStateCreateInfo depthStencil = { };
	VkPipelineDynamicStateCreateInfo dynamicState = {};

	std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
	std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachments;
	std::vector<VkVertexInputBindingDescription> vertexInputBindings;
	std::vector<VkVertexInputAttributeDescription> vertexInputAttributes;
	std::vector<VkDynamicState> dynamicStates;
};

class PipelineLayoutBuilder
{
public:
	PipelineLayoutBuilder();

	void addSetLayout(VulkanDescriptorSetLayout *setLayout);
	void addPushConstantRange(VkShaderStageFlags stageFlags, size_t offset, size_t size);

	std::unique_ptr<VulkanPipelineLayout> create(VulkanDevice *device);

private:
	VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
	std::vector<VkDescriptorSetLayout> setLayouts;
	std::vector<VkPushConstantRange> pushConstantRanges;
};

class RenderPassBuilder
{
public:
	RenderPassBuilder();

	void addAttachment(VkFormat format, VkSampleCountFlagBits samples, VkAttachmentLoadOp load, VkAttachmentStoreOp store, VkImageLayout initialLayout, VkImageLayout finalLayout);
	void addDepthStencilAttachment(VkFormat format, VkSampleCountFlagBits samples, VkAttachmentLoadOp load, VkAttachmentStoreOp store, VkAttachmentLoadOp stencilLoad, VkAttachmentStoreOp stencilStore, VkImageLayout initialLayout, VkImageLayout finalLayout);

	void addExternalSubpassDependency(VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask);

	void addSubpass();
	void addSubpassColorAttachmentRef(uint32_t index, VkImageLayout layout);
	void addSubpassDepthStencilAttachmentRef(uint32_t index, VkImageLayout layout);

	std::unique_ptr<VulkanRenderPass> create(VulkanDevice *device);

private:
	VkRenderPassCreateInfo renderPassInfo = { };

	std::vector<VkAttachmentDescription> attachments;
	std::vector<VkSubpassDependency> dependencies;
	std::vector<VkSubpassDescription> subpasses;

	struct SubpassData
	{
		std::vector<VkAttachmentReference> colorRefs;
		VkAttachmentReference depthRef = { };
	};

	std::vector<std::unique_ptr<SubpassData>> subpassData;
};

class PipelineBarrier
{
public:
	void addMemory(VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask);
	void addBuffer(VulkanBuffer *buffer, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask);
	void addBuffer(VulkanBuffer *buffer, VkDeviceSize offset, VkDeviceSize size, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask);
	void addImage(VulkanImage *image, VkImageLayout oldLayout, VkImageLayout newLayout, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask, VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, int baseMipLevel = 0, int levelCount = 1);
	void addImage(VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask, VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, int baseMipLevel = 0, int levelCount = 1);
	void addQueueTransfer(int srcFamily, int dstFamily, VulkanBuffer *buffer, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask);
	void addQueueTransfer(int srcFamily, int dstFamily, VulkanImage *image, VkImageLayout layout, VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, int baseMipLevel = 0, int levelCount = 1);

	void execute(VulkanCommandBuffer *commandBuffer, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkDependencyFlags dependencyFlags = 0);

private:
	std::vector<VkMemoryBarrier> memoryBarriers;
	std::vector<VkBufferMemoryBarrier> bufferMemoryBarriers;
	std::vector<VkImageMemoryBarrier> imageMemoryBarriers;
};

class QueueSubmit
{
public:
	QueueSubmit();

	void addCommandBuffer(VulkanCommandBuffer *buffer);
	void addWait(VkPipelineStageFlags waitStageMask, VulkanSemaphore *semaphore);
	void addSignal(VulkanSemaphore *semaphore);
	void execute(VulkanDevice *device, VkQueue queue, VulkanFence *fence = nullptr);

private:
	VkSubmitInfo submitInfo = {};
	std::vector<VkSemaphore> waitSemaphores;
	std::vector<VkPipelineStageFlags> waitStages;
	std::vector<VkSemaphore> signalSemaphores;
	std::vector<VkCommandBuffer> commandBuffers;
};

class WriteDescriptors
{
public:
	void addBuffer(VulkanDescriptorSet *descriptorSet, int binding, VkDescriptorType type, VulkanBuffer *buffer);
	void addBuffer(VulkanDescriptorSet *descriptorSet, int binding, VkDescriptorType type, VulkanBuffer *buffer, size_t offset, size_t range);
	void addStorageImage(VulkanDescriptorSet *descriptorSet, int binding, VulkanImageView *view, VkImageLayout imageLayout);
	void addCombinedImageSampler(VulkanDescriptorSet *descriptorSet, int binding, VulkanImageView *view, VulkanSampler *sampler, VkImageLayout imageLayout);
	void addAccelerationStructure(VulkanDescriptorSet* descriptorSet, int binding, VulkanAccelerationStructure* accelStruct);

	void updateSets(VulkanDevice *device);

private:
	struct WriteExtra
	{
		VkDescriptorImageInfo imageInfo;
		VkDescriptorBufferInfo bufferInfo;
		VkBufferView bufferView;
		VkWriteDescriptorSetAccelerationStructureKHR accelStruct;
	};

	std::vector<VkWriteDescriptorSet> writes;
	std::vector<std::unique_ptr<WriteExtra>> writeExtras;
};
