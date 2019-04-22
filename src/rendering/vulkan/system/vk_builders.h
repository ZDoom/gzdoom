#pragma once

#include "vk_objects.h"
#include "zstring.h"
#include <cassert>

template<typename T, int maxsize>
class FixedSizeVector
{
public:
	const T *data() const { return mItems; }
	T *data() { return mItems; }

	size_t size() const { return mCount; }

	const T &back() const { return mItems[mCount - 1]; }
	T &back() { return mItems[mCount - 1]; }

	void push_back(T && value)
	{
		assert(mCount != maxsize && "FixedSizeVector is too small");
		mItems[mCount++] = std::move(value);
	}

	void push_back(const T &value)
	{
		assert(mCount != maxsize && "FixedSizeVector is too small");
		mItems[mCount++] = value;
	}

private:
	T mItems[maxsize];
	size_t mCount = 0;
};

class ImageBuilder
{
public:
	ImageBuilder();

	void setSize(int width, int height, int miplevels = 1);
	void setSamples(VkSampleCountFlagBits samples);
	void setFormat(VkFormat format);
	void setUsage(VkImageUsageFlags imageUsage, VmaMemoryUsage memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY, VmaAllocationCreateFlags allocFlags = 0);
	void setLinearTiling();

	bool isFormatSupported(VulkanDevice *device);

	std::unique_ptr<VulkanImage> create(VulkanDevice *device);

private:
	VkImageCreateInfo imageInfo = {};
	VmaAllocationCreateInfo allocInfo = {};
};

class ImageViewBuilder
{
public:
	ImageViewBuilder();

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
	FixedSizeVector<VkDescriptorSetLayoutBinding, 8> bindings;
};

class DescriptorPoolBuilder
{
public:
	DescriptorPoolBuilder();

	void setMaxSets(int value);
	void addPoolSize(VkDescriptorType type, int count);

	std::unique_ptr<VulkanDescriptorPool> create(VulkanDevice *device);

private:
	FixedSizeVector<VkDescriptorPoolSize, 8> poolSizes;
	VkDescriptorPoolCreateInfo poolInfo = {};
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
	FixedSizeVector<VkImageView, 8> attachments;
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

	FixedSizeVector<VkAttachmentDescription, 8> attachments;
	FixedSizeVector<VkSubpassDependency, 8> dependencies;
	FixedSizeVector<VkSubpassDescription, 8> subpasses;

	struct SubpassData
	{
		FixedSizeVector<VkAttachmentReference, 8> colorRefs;
		VkAttachmentReference depthRef = { };
	};

	FixedSizeVector<std::unique_ptr<SubpassData>, 8> subpassData;
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
	FixedSizeVector<VkMemoryBarrier, 8> memoryBarriers;
	FixedSizeVector<VkBufferMemoryBarrier, 8> bufferMemoryBarriers;
	FixedSizeVector<VkImageMemoryBarrier, 8> imageMemoryBarriers;
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
	FixedSizeVector<VkSemaphore, 8> waitSemaphores;
	FixedSizeVector<VkPipelineStageFlags, 8> waitStages;
	FixedSizeVector<VkSemaphore, 8> signalSemaphores;
	FixedSizeVector<VkCommandBuffer, 8> commandBuffers;
};

class WriteDescriptors
{
public:
	void addBuffer(VulkanDescriptorSet *descriptorSet, int binding, VkDescriptorType type, VulkanBuffer *buffer);
	void addBuffer(VulkanDescriptorSet *descriptorSet, int binding, VkDescriptorType type, VulkanBuffer *buffer, size_t offset, size_t range);
	void addStorageImage(VulkanDescriptorSet *descriptorSet, int binding, VulkanImageView *view, VkImageLayout imageLayout);
	void addCombinedImageSampler(VulkanDescriptorSet *descriptorSet, int binding, VulkanImageView *view, VulkanSampler *sampler, VkImageLayout imageLayout);

	void updateSets(VulkanDevice *device);

private:
	struct WriteExtra
	{
		VkDescriptorImageInfo imageInfo;
		VkDescriptorBufferInfo bufferInfo;
		VkBufferView bufferView;
	};

	std::vector<VkWriteDescriptorSet> writes;
	std::vector<std::unique_ptr<WriteExtra>> writeExtras;
};

/////////////////////////////////////////////////////////////////////////////

inline ImageBuilder::ImageBuilder()
{
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.depth = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; // Note: must either be VK_IMAGE_LAYOUT_UNDEFINED or VK_IMAGE_LAYOUT_PREINITIALIZED
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.flags = 0;
}

inline void ImageBuilder::setSize(int width, int height, int mipLevels)
{
	imageInfo.extent.width = width;
	imageInfo.extent.height = height;
	imageInfo.mipLevels = mipLevels;
}

inline void ImageBuilder::setSamples(VkSampleCountFlagBits samples)
{
	imageInfo.samples = samples;
}

inline void ImageBuilder::setFormat(VkFormat format)
{
	imageInfo.format = format;
}

inline void ImageBuilder::setLinearTiling()
{
	imageInfo.tiling = VK_IMAGE_TILING_LINEAR;
}

inline void ImageBuilder::setUsage(VkImageUsageFlags usage, VmaMemoryUsage memoryUsage, VmaAllocationCreateFlags allocFlags)
{
	imageInfo.usage = usage;
	allocInfo.usage = memoryUsage;
	allocInfo.flags = allocFlags;
}

inline bool ImageBuilder::isFormatSupported(VulkanDevice *device)
{
	VkImageFormatProperties properties = { };
	VkResult result = vkGetPhysicalDeviceImageFormatProperties(device->PhysicalDevice.Device, imageInfo.format, imageInfo.imageType, imageInfo.tiling, imageInfo.usage, imageInfo.flags, &properties);
	if (result != VK_SUCCESS) return false;
	if (imageInfo.extent.width > properties.maxExtent.width) return false;
	if (imageInfo.extent.height > properties.maxExtent.height) return false;
	if (imageInfo.extent.depth > properties.maxExtent.depth) return false;
	if (imageInfo.mipLevels > properties.maxMipLevels) return false;
	if (imageInfo.arrayLayers > properties.maxArrayLayers) return false;
	if ((imageInfo.samples & properties.sampleCounts) != imageInfo.samples) return false;
	return true;
}

inline std::unique_ptr<VulkanImage> ImageBuilder::create(VulkanDevice *device)
{
	VkImage image;
	VmaAllocation allocation;

	VkResult result = vmaCreateImage(device->allocator, &imageInfo, &allocInfo, &image, &allocation, nullptr);
	if (result != VK_SUCCESS)
		I_FatalError("Could not create vulkan image");

	return std::make_unique<VulkanImage>(device, image, allocation, imageInfo.extent.width, imageInfo.extent.height, imageInfo.mipLevels);
}

/////////////////////////////////////////////////////////////////////////////

inline ImageViewBuilder::ImageViewBuilder()
{
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;
	viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
}

inline void ImageViewBuilder::setImage(VulkanImage *image, VkFormat format, VkImageAspectFlags aspectMask)
{
	viewInfo.image = image->image;
	viewInfo.format = format;
	viewInfo.subresourceRange.levelCount = image->mipLevels;
	viewInfo.subresourceRange.aspectMask = aspectMask;
}

inline std::unique_ptr<VulkanImageView> ImageViewBuilder::create(VulkanDevice *device)
{
	VkImageView view;
	VkResult result = vkCreateImageView(device->device, &viewInfo, nullptr, &view);
	if (result != VK_SUCCESS)
		I_FatalError("Could not create texture image view");

	return std::make_unique<VulkanImageView>(device, view);
}

/////////////////////////////////////////////////////////////////////////////

inline SamplerBuilder::SamplerBuilder()
{
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.anisotropyEnable = VK_FALSE;
	samplerInfo.maxAnisotropy = 1.0f;
	samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.mipLodBias = 0.0f;
	samplerInfo.minLod = 0.0f;
	samplerInfo.maxLod = 100.0f;
}

inline void SamplerBuilder::setAddressMode(VkSamplerAddressMode addressMode)
{
	samplerInfo.addressModeU = addressMode;
	samplerInfo.addressModeV = addressMode;
	samplerInfo.addressModeW = addressMode;
}

inline void SamplerBuilder::setAddressMode(VkSamplerAddressMode u, VkSamplerAddressMode v, VkSamplerAddressMode w)
{
	samplerInfo.addressModeU = u;
	samplerInfo.addressModeV = v;
	samplerInfo.addressModeW = w;
}

inline void SamplerBuilder::setMinFilter(VkFilter minFilter)
{
	samplerInfo.minFilter = minFilter;
}

inline void SamplerBuilder::setMagFilter(VkFilter magFilter)
{
	samplerInfo.magFilter = magFilter;
}

inline void SamplerBuilder::setMipmapMode(VkSamplerMipmapMode mode)
{
	samplerInfo.mipmapMode = mode;
}

inline void SamplerBuilder::setAnisotropy(float maxAnisotropy)
{
	samplerInfo.anisotropyEnable = VK_TRUE;
	samplerInfo.maxAnisotropy = maxAnisotropy;
}

inline void SamplerBuilder::setMaxLod(float value)
{
	samplerInfo.maxLod = value;
}

inline std::unique_ptr<VulkanSampler> SamplerBuilder::create(VulkanDevice *device)
{
	VkSampler sampler;
	VkResult result = vkCreateSampler(device->device, &samplerInfo, nullptr, &sampler);
	if (result != VK_SUCCESS)
		I_FatalError("Could not create texture sampler");
	return std::make_unique<VulkanSampler>(device, sampler);
}

/////////////////////////////////////////////////////////////////////////////

inline BufferBuilder::BufferBuilder()
{
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
}

inline void BufferBuilder::setSize(size_t size)
{
	bufferInfo.size = size;
}

inline void BufferBuilder::setUsage(VkBufferUsageFlags bufferUsage, VmaMemoryUsage memoryUsage, VmaAllocationCreateFlags allocFlags)
{
	bufferInfo.usage = bufferUsage;
	allocInfo.usage = memoryUsage;
	allocInfo.flags = allocFlags;
}

inline std::unique_ptr<VulkanBuffer> BufferBuilder::create(VulkanDevice *device)
{
	VkBuffer buffer;
	VmaAllocation allocation;

	VkResult result = vmaCreateBuffer(device->allocator, &bufferInfo, &allocInfo, &buffer, &allocation, nullptr);
	if (result != VK_SUCCESS)
		I_FatalError("could not allocate memory for vulkan buffer");

	return std::make_unique<VulkanBuffer>(device, buffer, allocation, bufferInfo.size);
}

/////////////////////////////////////////////////////////////////////////////

inline ComputePipelineBuilder::ComputePipelineBuilder()
{
	pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
}

inline void ComputePipelineBuilder::setLayout(VulkanPipelineLayout *layout)
{
	pipelineInfo.layout = layout->layout;
}

inline void ComputePipelineBuilder::setComputeShader(VulkanShader *shader)
{
	stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stageInfo.module = shader->module;
	stageInfo.pName = "main";

	pipelineInfo.stage = stageInfo;
}

inline std::unique_ptr<VulkanPipeline> ComputePipelineBuilder::create(VulkanDevice *device)
{
	VkPipeline pipeline;
	vkCreateComputePipelines(device->device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline);
	return std::make_unique<VulkanPipeline>(device, pipeline);
}

/////////////////////////////////////////////////////////////////////////////

inline DescriptorSetLayoutBuilder::DescriptorSetLayoutBuilder()
{
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
}

inline void DescriptorSetLayoutBuilder::addBinding(int index, VkDescriptorType type, int arrayCount, VkShaderStageFlags stageFlags)
{
	VkDescriptorSetLayoutBinding binding = { };
	binding.binding = index;
	binding.descriptorType = type;
	binding.descriptorCount = arrayCount;
	binding.stageFlags = stageFlags;
	binding.pImmutableSamplers = nullptr;
	bindings.push_back(binding);

	layoutInfo.bindingCount = (uint32_t)bindings.size();
	layoutInfo.pBindings = bindings.data();
}

inline std::unique_ptr<VulkanDescriptorSetLayout> DescriptorSetLayoutBuilder::create(VulkanDevice *device)
{
	VkDescriptorSetLayout layout;
	VkResult result = vkCreateDescriptorSetLayout(device->device, &layoutInfo, nullptr, &layout);
	if (result != VK_SUCCESS)
		I_FatalError("Could not create descriptor set layout");
	return std::make_unique<VulkanDescriptorSetLayout>(device, layout);
}

/////////////////////////////////////////////////////////////////////////////

inline DescriptorPoolBuilder::DescriptorPoolBuilder()
{
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.maxSets = 1;
	poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
}

inline void DescriptorPoolBuilder::setMaxSets(int value)
{
	poolInfo.maxSets = value;
}

inline void DescriptorPoolBuilder::addPoolSize(VkDescriptorType type, int count)
{
	VkDescriptorPoolSize size;
	size.type = type;
	size.descriptorCount = count;
	poolSizes.push_back(size);

	poolInfo.poolSizeCount = (uint32_t)poolSizes.size();
	poolInfo.pPoolSizes = poolSizes.data();
}

inline std::unique_ptr<VulkanDescriptorPool> DescriptorPoolBuilder::create(VulkanDevice *device)
{
	VkDescriptorPool descriptorPool;
	VkResult result = vkCreateDescriptorPool(device->device, &poolInfo, nullptr, &descriptorPool);
	if (result != VK_SUCCESS)
		I_FatalError("Could not create descriptor pool");
	return std::make_unique<VulkanDescriptorPool>(device, descriptorPool);
}

/////////////////////////////////////////////////////////////////////////////

inline FramebufferBuilder::FramebufferBuilder()
{
	framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
}

inline void FramebufferBuilder::setRenderPass(VulkanRenderPass *renderPass)
{
	framebufferInfo.renderPass = renderPass->renderPass;
}

inline void FramebufferBuilder::addAttachment(VulkanImageView *view)
{
	attachments.push_back(view->view);

	framebufferInfo.attachmentCount = (uint32_t)attachments.size();
	framebufferInfo.pAttachments = attachments.data();
}

inline void FramebufferBuilder::addAttachment(VkImageView view)
{
	attachments.push_back(view);

	framebufferInfo.attachmentCount = (uint32_t)attachments.size();
	framebufferInfo.pAttachments = attachments.data();
}

inline void FramebufferBuilder::setSize(int width, int height, int layers)
{
	framebufferInfo.width = width;
	framebufferInfo.height = height;
	framebufferInfo.layers = 1;
}

inline std::unique_ptr<VulkanFramebuffer> FramebufferBuilder::create(VulkanDevice *device)
{
	VkFramebuffer framebuffer = 0;
	VkResult result = vkCreateFramebuffer(device->device, &framebufferInfo, nullptr, &framebuffer);
	if (result != VK_SUCCESS)
		I_FatalError("Failed to create framebuffer");
	return std::make_unique<VulkanFramebuffer>(device, framebuffer);
}

/////////////////////////////////////////////////////////////////////////////

inline GraphicsPipelineBuilder::GraphicsPipelineBuilder()
{
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pDepthStencilState = nullptr;
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDynamicState = &dynamicState;
	pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
	pipelineInfo.basePipelineIndex = -1;

	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 0;
	vertexInputInfo.pVertexBindingDescriptions = nullptr;
	vertexInputInfo.vertexAttributeDescriptionCount = 0;
	vertexInputInfo.pVertexAttributeDescriptions = nullptr;

	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;

	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
	depthStencil.depthBoundsTestEnable = VK_FALSE;
	depthStencil.minDepthBounds = 0.0f;
	depthStencil.maxDepthBounds = 1.0f;
	depthStencil.stencilTestEnable = VK_FALSE;
	depthStencil.front = {};
	depthStencil.back = {};

	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = VK_CULL_MODE_NONE;
	rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterizer.depthBiasEnable = VK_FALSE;
	rasterizer.depthBiasConstantFactor = 0.0f;
	rasterizer.depthBiasClamp = 0.0f;
	rasterizer.depthBiasSlopeFactor = 0.0f;

	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampling.minSampleShading = 1.0f;
	multisampling.pSampleMask = nullptr;
	multisampling.alphaToCoverageEnable = VK_FALSE;
	multisampling.alphaToOneEnable = VK_FALSE;

	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE;
	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = VK_LOGIC_OP_COPY;
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;
	colorBlending.blendConstants[0] = 0.0f;
	colorBlending.blendConstants[1] = 0.0f;
	colorBlending.blendConstants[2] = 0.0f;
	colorBlending.blendConstants[3] = 0.0f;

	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
}

inline void GraphicsPipelineBuilder::setRasterizationSamples(VkSampleCountFlagBits samples)
{
	multisampling.rasterizationSamples = samples;
}

inline void GraphicsPipelineBuilder::setSubpass(int subpass)
{
	pipelineInfo.subpass = subpass;
}

inline void GraphicsPipelineBuilder::setLayout(VulkanPipelineLayout *layout)
{
	pipelineInfo.layout = layout->layout;
}

inline void GraphicsPipelineBuilder::setRenderPass(VulkanRenderPass *renderPass)
{
	pipelineInfo.renderPass = renderPass->renderPass;
}

inline void GraphicsPipelineBuilder::setTopology(VkPrimitiveTopology topology)
{
	inputAssembly.topology = topology;
}

inline void GraphicsPipelineBuilder::setViewport(float x, float y, float width, float height, float minDepth, float maxDepth)
{
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = width;
	viewport.height = height;
	viewport.minDepth = minDepth;
	viewport.maxDepth = maxDepth;

	viewportState.viewportCount = 1;
	viewportState.pViewports = &viewport;
}

inline void GraphicsPipelineBuilder::setScissor(int x, int y, int width, int height)
{
	scissor.offset.x = x;
	scissor.offset.y = y;
	scissor.extent.width = width;
	scissor.extent.height = height;

	viewportState.scissorCount = 1;
	viewportState.pScissors = &scissor;
}

inline void GraphicsPipelineBuilder::setCull(VkCullModeFlags cullMode, VkFrontFace frontFace)
{
	rasterizer.cullMode = cullMode;
	rasterizer.frontFace = frontFace;
}

inline void GraphicsPipelineBuilder::setDepthStencilEnable(bool test, bool write, bool stencil)
{
	depthStencil.depthTestEnable = test ? VK_TRUE : VK_FALSE;
	depthStencil.depthWriteEnable = write ? VK_TRUE : VK_FALSE;
	depthStencil.stencilTestEnable = stencil ? VK_TRUE : VK_FALSE;

	pipelineInfo.pDepthStencilState = (test || write || stencil) ? &depthStencil : nullptr;
}

inline void GraphicsPipelineBuilder::setStencil(VkStencilOp failOp, VkStencilOp passOp, VkStencilOp depthFailOp, VkCompareOp compareOp, uint32_t compareMask, uint32_t writeMask, uint32_t reference)
{
	depthStencil.front.failOp = failOp;
	depthStencil.front.passOp = passOp;
	depthStencil.front.depthFailOp = depthFailOp;
	depthStencil.front.compareOp = compareOp;
	depthStencil.front.compareMask = compareMask;
	depthStencil.front.writeMask = writeMask;
	depthStencil.front.reference = reference;

	depthStencil.back.failOp = failOp;
	depthStencil.back.passOp = passOp;
	depthStencil.back.depthFailOp = depthFailOp;
	depthStencil.back.compareOp = compareOp;
	depthStencil.back.compareMask = compareMask;
	depthStencil.back.writeMask = writeMask;
	depthStencil.back.reference = reference;
}

inline void GraphicsPipelineBuilder::setDepthFunc(VkCompareOp func)
{
	depthStencil.depthCompareOp = func;
}

inline void GraphicsPipelineBuilder::setDepthClampEnable(bool value)
{
	rasterizer.depthClampEnable = value ? VK_TRUE : VK_FALSE;
}

inline void GraphicsPipelineBuilder::setDepthBias(bool enable, float biasConstantFactor, float biasClamp, float biasSlopeFactor)
{
	rasterizer.depthBiasEnable = enable ? VK_TRUE : VK_FALSE;
	rasterizer.depthBiasConstantFactor = biasConstantFactor;
	rasterizer.depthBiasClamp = biasClamp;
	rasterizer.depthBiasSlopeFactor = biasSlopeFactor;
}

inline void GraphicsPipelineBuilder::setColorWriteMask(VkColorComponentFlags mask)
{
	colorBlendAttachment.colorWriteMask = mask;
}

inline void GraphicsPipelineBuilder::setAdditiveBlendMode()
{
	colorBlendAttachment.blendEnable = VK_TRUE;
	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
}

inline void GraphicsPipelineBuilder::setAlphaBlendMode()
{
	colorBlendAttachment.blendEnable = VK_TRUE;
	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
}

inline void GraphicsPipelineBuilder::setBlendMode(VkBlendOp op, VkBlendFactor src, VkBlendFactor dst)
{
	colorBlendAttachment.blendEnable = VK_TRUE;
	colorBlendAttachment.srcColorBlendFactor = src;
	colorBlendAttachment.dstColorBlendFactor = dst;
	colorBlendAttachment.colorBlendOp = op;
	colorBlendAttachment.srcAlphaBlendFactor = src;
	colorBlendAttachment.dstAlphaBlendFactor = dst;
	colorBlendAttachment.alphaBlendOp = op;
}

inline void GraphicsPipelineBuilder::setSubpassColorAttachmentCount(int count)
{
	colorBlendAttachments.resize(count, colorBlendAttachment);
	colorBlending.pAttachments = colorBlendAttachments.data();
	colorBlending.attachmentCount = (uint32_t)colorBlendAttachments.size();
}

inline void GraphicsPipelineBuilder::addVertexShader(VulkanShader *shader)
{
	VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};
	vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertShaderStageInfo.module = shader->module;
	vertShaderStageInfo.pName = "main";
	shaderStages.push_back(vertShaderStageInfo);

	pipelineInfo.stageCount = (uint32_t)shaderStages.size();
	pipelineInfo.pStages = shaderStages.data();
}

inline void GraphicsPipelineBuilder::addFragmentShader(VulkanShader *shader)
{
	VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
	fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragShaderStageInfo.module = shader->module;
	fragShaderStageInfo.pName = "main";
	shaderStages.push_back(fragShaderStageInfo);

	pipelineInfo.stageCount = (uint32_t)shaderStages.size();
	pipelineInfo.pStages = shaderStages.data();
}

inline void GraphicsPipelineBuilder::addVertexBufferBinding(int index, size_t stride)
{
	VkVertexInputBindingDescription desc = {};
	desc.binding = index;
	desc.stride = (uint32_t)stride;
	desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	vertexInputBindings.push_back(desc);

	vertexInputInfo.vertexBindingDescriptionCount = (uint32_t)vertexInputBindings.size();
	vertexInputInfo.pVertexBindingDescriptions = vertexInputBindings.data();
}

inline void GraphicsPipelineBuilder::addVertexAttribute(int location, int binding, VkFormat format, size_t offset)
{
	VkVertexInputAttributeDescription desc = { };
	desc.location = location;
	desc.binding = binding;
	desc.format = format;
	desc.offset = (uint32_t)offset;
	vertexInputAttributes.push_back(desc);

	vertexInputInfo.vertexAttributeDescriptionCount = (uint32_t)vertexInputAttributes.size();
	vertexInputInfo.pVertexAttributeDescriptions = vertexInputAttributes.data();
}

inline void GraphicsPipelineBuilder::addDynamicState(VkDynamicState state)
{
	dynamicStates.push_back(state);
	dynamicState.dynamicStateCount = (uint32_t)dynamicStates.size();
	dynamicState.pDynamicStates = dynamicStates.data();
}

inline std::unique_ptr<VulkanPipeline> GraphicsPipelineBuilder::create(VulkanDevice *device)
{
	VkPipeline pipeline = 0;
	VkResult result = vkCreateGraphicsPipelines(device->device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline);
	if (result != VK_SUCCESS)
		I_FatalError("Could not create graphics pipeline");
	return std::make_unique<VulkanPipeline>(device, pipeline);
}

/////////////////////////////////////////////////////////////////////////////

inline PipelineLayoutBuilder::PipelineLayoutBuilder()
{
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
}

inline void PipelineLayoutBuilder::addSetLayout(VulkanDescriptorSetLayout *setLayout)
{
	setLayouts.push_back(setLayout->layout);
	pipelineLayoutInfo.setLayoutCount = (uint32_t)setLayouts.size();
	pipelineLayoutInfo.pSetLayouts = setLayouts.data();
}

inline void PipelineLayoutBuilder::addPushConstantRange(VkShaderStageFlags stageFlags, size_t offset, size_t size)
{
	VkPushConstantRange range = { };
	range.stageFlags = stageFlags;
	range.offset = (uint32_t)offset;
	range.size = (uint32_t)size;
	pushConstantRanges.push_back(range);
	pipelineLayoutInfo.pushConstantRangeCount = (uint32_t)pushConstantRanges.size();
	pipelineLayoutInfo.pPushConstantRanges = pushConstantRanges.data();
}

inline std::unique_ptr<VulkanPipelineLayout> PipelineLayoutBuilder::create(VulkanDevice *device)
{
	VkPipelineLayout pipelineLayout;
	VkResult result = vkCreatePipelineLayout(device->device, &pipelineLayoutInfo, nullptr, &pipelineLayout);
	if (result != VK_SUCCESS)
		I_FatalError("Could not create pipeline layout");
	return std::make_unique<VulkanPipelineLayout>(device, pipelineLayout);
}

/////////////////////////////////////////////////////////////////////////////

inline RenderPassBuilder::RenderPassBuilder()
{
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
}

inline void RenderPassBuilder::addAttachment(VkFormat format, VkSampleCountFlagBits samples, VkAttachmentLoadOp load, VkAttachmentStoreOp store, VkImageLayout initialLayout, VkImageLayout finalLayout)
{
	VkAttachmentDescription attachment = {};
	attachment.format = format;
	attachment.samples = samples;
	attachment.loadOp = load;
	attachment.storeOp = store;
	attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachment.initialLayout = initialLayout;
	attachment.finalLayout = finalLayout;
	attachments.push_back(attachment);
	renderPassInfo.pAttachments = attachments.data();
	renderPassInfo.attachmentCount = (uint32_t)attachments.size();
}

inline void RenderPassBuilder::addDepthStencilAttachment(VkFormat format, VkSampleCountFlagBits samples, VkAttachmentLoadOp load, VkAttachmentStoreOp store, VkAttachmentLoadOp stencilLoad, VkAttachmentStoreOp stencilStore, VkImageLayout initialLayout, VkImageLayout finalLayout)
{
	VkAttachmentDescription attachment = {};
	attachment.format = format;
	attachment.samples = samples;
	attachment.loadOp = load;
	attachment.storeOp = store;
	attachment.stencilLoadOp = stencilLoad;
	attachment.stencilStoreOp = stencilStore;
	attachment.initialLayout = initialLayout;
	attachment.finalLayout = finalLayout;
	attachments.push_back(attachment);
	renderPassInfo.pAttachments = attachments.data();
	renderPassInfo.attachmentCount = (uint32_t)attachments.size();
}

inline void RenderPassBuilder::addExternalSubpassDependency(VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask)
{
	VkSubpassDependency dependency = {};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = srcStageMask;
	dependency.srcAccessMask = srcAccessMask;
	dependency.dstStageMask = dstStageMask;
	dependency.dstAccessMask = dstAccessMask;

	dependencies.push_back(dependency);
	renderPassInfo.pDependencies = dependencies.data();
	renderPassInfo.dependencyCount = (uint32_t)dependencies.size();
}

inline void RenderPassBuilder::addSubpass()
{
	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

	subpasses.push_back(subpass);
	renderPassInfo.pSubpasses = subpasses.data();
	renderPassInfo.subpassCount = (uint32_t)subpasses.size();

	subpassData.push_back(std::make_unique<SubpassData>());
}

inline void RenderPassBuilder::addSubpassColorAttachmentRef(uint32_t index, VkImageLayout layout)
{
	VkAttachmentReference colorAttachmentRef = {};
	colorAttachmentRef.attachment = index;
	colorAttachmentRef.layout = layout;

	subpassData.back()->colorRefs.push_back(colorAttachmentRef);
	subpasses.back().pColorAttachments = subpassData.back()->colorRefs.data();
	subpasses.back().colorAttachmentCount = (uint32_t)subpassData.back()->colorRefs.size();
}

inline void RenderPassBuilder::addSubpassDepthStencilAttachmentRef(uint32_t index, VkImageLayout layout)
{
	VkAttachmentReference &depthAttachmentRef = subpassData.back()->depthRef;
	depthAttachmentRef.attachment = index;
	depthAttachmentRef.layout = layout;

	VkSubpassDescription &subpass = subpasses.back();
	subpass.pDepthStencilAttachment = &depthAttachmentRef;
}

inline std::unique_ptr<VulkanRenderPass> RenderPassBuilder::create(VulkanDevice *device)
{
	VkRenderPass renderPass = 0;
	VkResult result = vkCreateRenderPass(device->device, &renderPassInfo, nullptr, &renderPass);
	if (result != VK_SUCCESS)
		I_FatalError("Could not create render pass");
	return std::make_unique<VulkanRenderPass>(device, renderPass);
}

/////////////////////////////////////////////////////////////////////////////

inline void PipelineBarrier::addMemory(VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask)
{
	VkMemoryBarrier barrier = { };
	barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
	barrier.srcAccessMask = srcAccessMask;
	barrier.dstAccessMask = dstAccessMask;
	memoryBarriers.push_back(barrier);
}

inline void PipelineBarrier::addBuffer(VulkanBuffer *buffer, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask)
{
	addBuffer(buffer, 0, buffer->size, srcAccessMask, dstAccessMask);
}

inline void PipelineBarrier::addBuffer(VulkanBuffer *buffer, VkDeviceSize offset, VkDeviceSize size, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask)
{
	VkBufferMemoryBarrier barrier = { };
	barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	barrier.srcAccessMask = srcAccessMask;
	barrier.dstAccessMask = dstAccessMask;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.buffer = buffer->buffer;
	barrier.offset = offset;
	barrier.size = size;
	bufferMemoryBarriers.push_back(barrier);
}

inline void PipelineBarrier::addImage(VulkanImage *image, VkImageLayout oldLayout, VkImageLayout newLayout, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask, VkImageAspectFlags aspectMask, int baseMipLevel, int levelCount)
{
	addImage(image->image, oldLayout, newLayout, srcAccessMask, dstAccessMask, aspectMask, baseMipLevel, levelCount);
}

inline void PipelineBarrier::addImage(VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask, VkImageAspectFlags aspectMask, int baseMipLevel, int levelCount)
{
	VkImageMemoryBarrier barrier = { };
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.srcAccessMask = srcAccessMask;
	barrier.dstAccessMask = dstAccessMask;
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange.aspectMask = aspectMask;
	barrier.subresourceRange.baseMipLevel = baseMipLevel;
	barrier.subresourceRange.levelCount = levelCount;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;
	imageMemoryBarriers.push_back(barrier);
}

inline void PipelineBarrier::addQueueTransfer(int srcFamily, int dstFamily, VulkanBuffer *buffer, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask)
{
	VkBufferMemoryBarrier barrier = { };
	barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	barrier.srcAccessMask = srcAccessMask;
	barrier.dstAccessMask = dstAccessMask;
	barrier.srcQueueFamilyIndex = srcFamily;
	barrier.dstQueueFamilyIndex = dstFamily;
	barrier.buffer = buffer->buffer;
	barrier.offset = 0;
	barrier.size = buffer->size;
	bufferMemoryBarriers.push_back(barrier);
}

inline void PipelineBarrier::addQueueTransfer(int srcFamily, int dstFamily, VulkanImage *image, VkImageLayout layout, VkImageAspectFlags aspectMask, int baseMipLevel, int levelCount)
{
	VkImageMemoryBarrier barrier = { };
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = layout;
	barrier.newLayout = layout;
	barrier.srcQueueFamilyIndex = srcFamily;
	barrier.dstQueueFamilyIndex = dstFamily;
	barrier.image = image->image;
	barrier.subresourceRange.aspectMask = aspectMask;
	barrier.subresourceRange.baseMipLevel = baseMipLevel;
	barrier.subresourceRange.levelCount = levelCount;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;
	imageMemoryBarriers.push_back(barrier);
}

inline void PipelineBarrier::execute(VulkanCommandBuffer *commandBuffer, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkDependencyFlags dependencyFlags)
{
	commandBuffer->pipelineBarrier(
		srcStageMask, dstStageMask, dependencyFlags,
		(uint32_t)memoryBarriers.size(), memoryBarriers.data(),
		(uint32_t)bufferMemoryBarriers.size(), bufferMemoryBarriers.data(),
		(uint32_t)imageMemoryBarriers.size(), imageMemoryBarriers.data());
}

/////////////////////////////////////////////////////////////////////////////

inline QueueSubmit::QueueSubmit()
{
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
}

inline void QueueSubmit::addCommandBuffer(VulkanCommandBuffer *buffer)
{
	commandBuffers.push_back(buffer->buffer);
	submitInfo.pCommandBuffers = commandBuffers.data();
	submitInfo.commandBufferCount = (uint32_t)commandBuffers.size();
}

inline void QueueSubmit::addWait(VkPipelineStageFlags waitStageMask, VulkanSemaphore *semaphore)
{
	waitStages.push_back(waitStageMask);
	waitSemaphores.push_back(semaphore->semaphore);

	submitInfo.pWaitDstStageMask = waitStages.data();
	submitInfo.pWaitSemaphores = waitSemaphores.data();
	submitInfo.waitSemaphoreCount = (uint32_t)waitSemaphores.size();
}

inline void QueueSubmit::addSignal(VulkanSemaphore *semaphore)
{
	signalSemaphores.push_back(semaphore->semaphore);
	submitInfo.pSignalSemaphores = signalSemaphores.data();
	submitInfo.signalSemaphoreCount = (uint32_t)signalSemaphores.size();
}

inline void QueueSubmit::execute(VulkanDevice *device, VkQueue queue, VulkanFence *fence)
{
	VkResult result = vkQueueSubmit(device->graphicsQueue, 1, &submitInfo, fence ? fence->fence : VK_NULL_HANDLE);
	if (result < VK_SUCCESS)
		I_FatalError("Failed to submit command buffer");
}

/////////////////////////////////////////////////////////////////////////////

inline void WriteDescriptors::addBuffer(VulkanDescriptorSet *descriptorSet, int binding, VkDescriptorType type, VulkanBuffer *buffer)
{
	addBuffer(descriptorSet, binding, type, buffer, 0, buffer->size);
}

inline void WriteDescriptors::addBuffer(VulkanDescriptorSet *descriptorSet, int binding, VkDescriptorType type, VulkanBuffer *buffer, size_t offset, size_t range)
{
	VkDescriptorBufferInfo bufferInfo = {};
	bufferInfo.buffer = buffer->buffer;
	bufferInfo.offset = offset;
	bufferInfo.range = range;

	auto extra = std::make_unique<WriteExtra>();
	extra->bufferInfo = bufferInfo;

	VkWriteDescriptorSet descriptorWrite = {};
	descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrite.dstSet = descriptorSet->set;
	descriptorWrite.dstBinding = binding;
	descriptorWrite.dstArrayElement = 0;
	descriptorWrite.descriptorType = type;
	descriptorWrite.descriptorCount = 1;
	descriptorWrite.pBufferInfo = &extra->bufferInfo;
	writes.push_back(descriptorWrite);
	writeExtras.push_back(std::move(extra));
}

inline void WriteDescriptors::addStorageImage(VulkanDescriptorSet *descriptorSet, int binding, VulkanImageView *view, VkImageLayout imageLayout)
{
	VkDescriptorImageInfo imageInfo = {};
	imageInfo.imageView = view->view;
	imageInfo.imageLayout = imageLayout;

	auto extra = std::make_unique<WriteExtra>();
	extra->imageInfo = imageInfo;

	VkWriteDescriptorSet descriptorWrite = {};
	descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrite.dstSet = descriptorSet->set;
	descriptorWrite.dstBinding = binding;
	descriptorWrite.dstArrayElement = 0;
	descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	descriptorWrite.descriptorCount = 1;
	descriptorWrite.pImageInfo = &extra->imageInfo;
	writes.push_back(descriptorWrite);
	writeExtras.push_back(std::move(extra));
}

inline void WriteDescriptors::addCombinedImageSampler(VulkanDescriptorSet *descriptorSet, int binding, VulkanImageView *view, VulkanSampler *sampler, VkImageLayout imageLayout)
{
	VkDescriptorImageInfo imageInfo = {};
	imageInfo.imageView = view->view;
	imageInfo.sampler = sampler->sampler;
	imageInfo.imageLayout = imageLayout;

	auto extra = std::make_unique<WriteExtra>();
	extra->imageInfo = imageInfo;

	VkWriteDescriptorSet descriptorWrite = {};
	descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrite.dstSet = descriptorSet->set;
	descriptorWrite.dstBinding = binding;
	descriptorWrite.dstArrayElement = 0;
	descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descriptorWrite.descriptorCount = 1;
	descriptorWrite.pImageInfo = &extra->imageInfo;
	writes.push_back(descriptorWrite);
	writeExtras.push_back(std::move(extra));
}

inline void WriteDescriptors::updateSets(VulkanDevice *device)
{
	vkUpdateDescriptorSets(device->device, (uint32_t)writes.size(), writes.data(), 0, nullptr);
}
