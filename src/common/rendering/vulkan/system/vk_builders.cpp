/*
**  Vulkan backend
**  Copyright (c) 2016-2020 Magnus Norddahl
**
**  This software is provided 'as-is', without any express or implied
**  warranty.  In no event will the authors be held liable for any damages
**  arising from the use of this software.
**
**  Permission is granted to anyone to use this software for any purpose,
**  including commercial applications, and to alter it and redistribute it
**  freely, subject to the following restrictions:
**
**  1. The origin of this software must not be misrepresented; you must not
**     claim that you wrote the original software. If you use this software
**     in a product, an acknowledgment in the product documentation would be
**     appreciated but is not required.
**  2. Altered source versions must be plainly marked as such, and must not be
**     misrepresented as being the original software.
**  3. This notice may not be removed or altered from any source distribution.
**
*/

#include "vk_builders.h"
#include "engineerrors.h"
#include "renderstyle.h"
#include <ShaderLang.h>
#include <GlslangToSpv.h>

static const TBuiltInResource DefaultTBuiltInResource = {
	/* .MaxLights = */ 32,
	/* .MaxClipPlanes = */ 6,
	/* .MaxTextureUnits = */ 32,
	/* .MaxTextureCoords = */ 32,
	/* .MaxVertexAttribs = */ 64,
	/* .MaxVertexUniformComponents = */ 4096,
	/* .MaxVaryingFloats = */ 64,
	/* .MaxVertexTextureImageUnits = */ 32,
	/* .MaxCombinedTextureImageUnits = */ 80,
	/* .MaxTextureImageUnits = */ 32,
	/* .MaxFragmentUniformComponents = */ 4096,
	/* .MaxDrawBuffers = */ 32,
	/* .MaxVertexUniformVectors = */ 128,
	/* .MaxVaryingVectors = */ 8,
	/* .MaxFragmentUniformVectors = */ 16,
	/* .MaxVertexOutputVectors = */ 16,
	/* .MaxFragmentInputVectors = */ 15,
	/* .MinProgramTexelOffset = */ -8,
	/* .MaxProgramTexelOffset = */ 7,
	/* .MaxClipDistances = */ 8,
	/* .MaxComputeWorkGroupCountX = */ 65535,
	/* .MaxComputeWorkGroupCountY = */ 65535,
	/* .MaxComputeWorkGroupCountZ = */ 65535,
	/* .MaxComputeWorkGroupSizeX = */ 1024,
	/* .MaxComputeWorkGroupSizeY = */ 1024,
	/* .MaxComputeWorkGroupSizeZ = */ 64,
	/* .MaxComputeUniformComponents = */ 1024,
	/* .MaxComputeTextureImageUnits = */ 16,
	/* .MaxComputeImageUniforms = */ 8,
	/* .MaxComputeAtomicCounters = */ 8,
	/* .MaxComputeAtomicCounterBuffers = */ 1,
	/* .MaxVaryingComponents = */ 60,
	/* .MaxVertexOutputComponents = */ 64,
	/* .MaxGeometryInputComponents = */ 64,
	/* .MaxGeometryOutputComponents = */ 128,
	/* .MaxFragmentInputComponents = */ 128,
	/* .MaxImageUnits = */ 8,
	/* .MaxCombinedImageUnitsAndFragmentOutputs = */ 8,
	/* .MaxCombinedShaderOutputResources = */ 8,
	/* .MaxImageSamples = */ 0,
	/* .MaxVertexImageUniforms = */ 0,
	/* .MaxTessControlImageUniforms = */ 0,
	/* .MaxTessEvaluationImageUniforms = */ 0,
	/* .MaxGeometryImageUniforms = */ 0,
	/* .MaxFragmentImageUniforms = */ 8,
	/* .MaxCombinedImageUniforms = */ 8,
	/* .MaxGeometryTextureImageUnits = */ 16,
	/* .MaxGeometryOutputVertices = */ 256,
	/* .MaxGeometryTotalOutputComponents = */ 1024,
	/* .MaxGeometryUniformComponents = */ 1024,
	/* .MaxGeometryVaryingComponents = */ 64,
	/* .MaxTessControlInputComponents = */ 128,
	/* .MaxTessControlOutputComponents = */ 128,
	/* .MaxTessControlTextureImageUnits = */ 16,
	/* .MaxTessControlUniformComponents = */ 1024,
	/* .MaxTessControlTotalOutputComponents = */ 4096,
	/* .MaxTessEvaluationInputComponents = */ 128,
	/* .MaxTessEvaluationOutputComponents = */ 128,
	/* .MaxTessEvaluationTextureImageUnits = */ 16,
	/* .MaxTessEvaluationUniformComponents = */ 1024,
	/* .MaxTessPatchComponents = */ 120,
	/* .MaxPatchVertices = */ 32,
	/* .MaxTessGenLevel = */ 64,
	/* .MaxViewports = */ 16,
	/* .MaxVertexAtomicCounters = */ 0,
	/* .MaxTessControlAtomicCounters = */ 0,
	/* .MaxTessEvaluationAtomicCounters = */ 0,
	/* .MaxGeometryAtomicCounters = */ 0,
	/* .MaxFragmentAtomicCounters = */ 8,
	/* .MaxCombinedAtomicCounters = */ 8,
	/* .MaxAtomicCounterBindings = */ 1,
	/* .MaxVertexAtomicCounterBuffers = */ 0,
	/* .MaxTessControlAtomicCounterBuffers = */ 0,
	/* .MaxTessEvaluationAtomicCounterBuffers = */ 0,
	/* .MaxGeometryAtomicCounterBuffers = */ 0,
	/* .MaxFragmentAtomicCounterBuffers = */ 1,
	/* .MaxCombinedAtomicCounterBuffers = */ 1,
	/* .MaxAtomicCounterBufferSize = */ 16384,
	/* .MaxTransformFeedbackBuffers = */ 4,
	/* .MaxTransformFeedbackInterleavedComponents = */ 64,
	/* .MaxCullDistances = */ 8,
	/* .MaxCombinedClipAndCullDistances = */ 8,
	/* .MaxSamples = */ 4,
	/* .maxMeshOutputVerticesNV = */ 256,
	/* .maxMeshOutputPrimitivesNV = */ 512,
	/* .maxMeshWorkGroupSizeX_NV = */ 32,
	/* .maxMeshWorkGroupSizeY_NV = */ 1,
	/* .maxMeshWorkGroupSizeZ_NV = */ 1,
	/* .maxTaskWorkGroupSizeX_NV = */ 32,
	/* .maxTaskWorkGroupSizeY_NV = */ 1,
	/* .maxTaskWorkGroupSizeZ_NV = */ 1,
	/* .maxMeshViewCountNV = */ 4,
	/* .maxDualSourceDrawBuffersEXT = */ 1,

	/* .limits = */ {
		/* .nonInductiveForLoops = */ 1,
		/* .whileLoops = */ 1,
		/* .doWhileLoops = */ 1,
		/* .generalUniformIndexing = */ 1,
		/* .generalAttributeMatrixVectorIndexing = */ 1,
		/* .generalVaryingIndexing = */ 1,
		/* .generalSamplerIndexing = */ 1,
		/* .generalVariableIndexing = */ 1,
		/* .generalConstantMatrixVectorIndexing = */ 1,
	}
};

ShaderBuilder::ShaderBuilder()
{
}

void ShaderBuilder::setVertexShader(const FString &c)
{
	code = c;
	stage = EShLanguage::EShLangVertex;
}

void ShaderBuilder::setFragmentShader(const FString &c)
{
	code = c;
	stage = EShLanguage::EShLangFragment;
}

std::unique_ptr<VulkanShader> ShaderBuilder::create(const char *shadername, VulkanDevice *device)
{
	EShLanguage stage = (EShLanguage)this->stage;
	const char *sources[] = { code.GetChars() };

	TBuiltInResource resources = DefaultTBuiltInResource;

	glslang::TShader shader(stage);
	shader.setStrings(sources, 1);
	shader.setEnvInput(glslang::EShSourceGlsl, stage, glslang::EShClientVulkan, 100);
    if (device->ApiVersion >= VK_API_VERSION_1_2)
    {
        shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_2);
        shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_4);
    }
    else
    {
        shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_0);
        shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_0);
    }
	bool compileSuccess = shader.parse(&resources, 110, false, EShMsgVulkanRules);
	if (!compileSuccess)
	{
		I_FatalError("Shader '%s' could not be compiled:\n%s\n", shadername, shader.getInfoLog());
	}

	glslang::TProgram program;
	program.addShader(&shader);
	bool linkSuccess = program.link(EShMsgDefault);
	if (!linkSuccess)
	{
		I_FatalError("Shader '%s' could not be linked:\n%s\n", shadername, program.getInfoLog());
	}

	glslang::TIntermediate *intermediate = program.getIntermediate(stage);
	if (!intermediate)
	{
		I_FatalError("Internal shader compiler error while processing '%s'\n", shadername);
	}

	glslang::SpvOptions spvOptions;
	spvOptions.generateDebugInfo = false;
	spvOptions.disableOptimizer = false;
	spvOptions.optimizeSize = true;

	std::vector<unsigned int> spirv;
	spv::SpvBuildLogger logger;
	glslang::GlslangToSpv(*intermediate, spirv, &logger, &spvOptions);

	VkShaderModuleCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = spirv.size() * sizeof(unsigned int);
	createInfo.pCode = spirv.data();

	VkShaderModule shaderModule;
	VkResult result = vkCreateShaderModule(device->device, &createInfo, nullptr, &shaderModule);
	if (result != VK_SUCCESS)
	{
		FString msg;
		msg.Format("Could not create vulkan shader module for '%s': %s", shadername, VkResultToString(result).GetChars());
		VulkanError(msg.GetChars());
	}

	return std::make_unique<VulkanShader>(device, shaderModule);
}

/////////////////////////////////////////////////////////////////////////////

void GraphicsPipelineBuilder::setBlendMode(const FRenderStyle &style)
{
	// Just in case Vulkan doesn't do this optimization itself
	if (style.BlendOp == STYLEOP_Add && style.SrcAlpha == STYLEALPHA_One && style.DestAlpha == STYLEALPHA_Zero && style.Flags == 0)
	{
		colorBlendAttachment.blendEnable = VK_FALSE;
		return;
	}

	static const int blendstyles[] = {
		VK_BLEND_FACTOR_ZERO,
		VK_BLEND_FACTOR_ONE,
		VK_BLEND_FACTOR_SRC_ALPHA,
		VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
		VK_BLEND_FACTOR_SRC_COLOR,
		VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR,
		VK_BLEND_FACTOR_DST_COLOR,
		VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR,
		VK_BLEND_FACTOR_DST_ALPHA,
		VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA,
	};

	static const int renderops[] = {
		0, VK_BLEND_OP_ADD, VK_BLEND_OP_SUBTRACT, VK_BLEND_OP_REVERSE_SUBTRACT, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
	};

	int srcblend = blendstyles[style.SrcAlpha%STYLEALPHA_MAX];
	int dstblend = blendstyles[style.DestAlpha%STYLEALPHA_MAX];
	int blendequation = renderops[style.BlendOp & 15];

	if (blendequation == -1)	// This was a fuzz style.
	{
		srcblend = VK_BLEND_FACTOR_DST_COLOR;
		dstblend = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		blendequation = VK_BLEND_OP_ADD;
	}

	setBlendMode((VkBlendOp)blendequation, (VkBlendFactor)srcblend, (VkBlendFactor)dstblend);
}

/////////////////////////////////////////////////////////////////////////////

ImageBuilder::ImageBuilder()
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

void ImageBuilder::setSize(int width, int height, int mipLevels, int arrayLayers)
{
	imageInfo.extent.width = width;
	imageInfo.extent.height = height;
	imageInfo.mipLevels = mipLevels;
	imageInfo.arrayLayers = arrayLayers;
}

void ImageBuilder::setSamples(VkSampleCountFlagBits samples)
{
	imageInfo.samples = samples;
}

void ImageBuilder::setFormat(VkFormat format)
{
	imageInfo.format = format;
}

void ImageBuilder::setLinearTiling()
{
	imageInfo.tiling = VK_IMAGE_TILING_LINEAR;
}

void ImageBuilder::setUsage(VkImageUsageFlags usage, VmaMemoryUsage memoryUsage, VmaAllocationCreateFlags allocFlags)
{
	imageInfo.usage = usage;
	allocInfo.usage = memoryUsage;
	allocInfo.flags = allocFlags;
}

void ImageBuilder::setMemoryType(VkMemoryPropertyFlags requiredFlags, VkMemoryPropertyFlags preferredFlags, uint32_t memoryTypeBits)
{
	allocInfo.requiredFlags = requiredFlags;
	allocInfo.preferredFlags = preferredFlags;
	allocInfo.memoryTypeBits = memoryTypeBits;
}

bool ImageBuilder::isFormatSupported(VulkanDevice* device, VkFormatFeatureFlags bufferFeatures)
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
	if (bufferFeatures != 0)
	{
		VkFormatProperties formatProperties = { };
		vkGetPhysicalDeviceFormatProperties(device->PhysicalDevice.Device, imageInfo.format, &formatProperties);
		if ((formatProperties.bufferFeatures & bufferFeatures) != bufferFeatures)
			return false;
	}
	return true;
}

std::unique_ptr<VulkanImage> ImageBuilder::create(VulkanDevice* device, VkDeviceSize* allocatedBytes)
{
	VkImage image;
	VmaAllocation allocation;

	VkResult result = vmaCreateImage(device->allocator, &imageInfo, &allocInfo, &image, &allocation, nullptr);
	CheckVulkanError(result, "Could not create vulkan image");

	if (allocatedBytes != nullptr)
	{
		VmaAllocationInfo allocatedInfo;
		vmaGetAllocationInfo(device->allocator, allocation, &allocatedInfo);

		*allocatedBytes = allocatedInfo.size;
	}

	return std::make_unique<VulkanImage>(device, image, allocation, imageInfo.extent.width, imageInfo.extent.height, imageInfo.mipLevels, imageInfo.arrayLayers);
}

std::unique_ptr<VulkanImage> ImageBuilder::tryCreate(VulkanDevice* device)
{
	VkImage image;
	VmaAllocation allocation;

	VkResult result = vmaCreateImage(device->allocator, &imageInfo, &allocInfo, &image, &allocation, nullptr);
	if (result != VK_SUCCESS)
		return nullptr;

	return std::make_unique<VulkanImage>(device, image, allocation, imageInfo.extent.width, imageInfo.extent.height, imageInfo.mipLevels, imageInfo.arrayLayers);
}

/////////////////////////////////////////////////////////////////////////////

ImageViewBuilder::ImageViewBuilder()
{
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;
	viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
}

void ImageViewBuilder::setType(VkImageViewType type)
{
	viewInfo.viewType = type;
}

void ImageViewBuilder::setImage(VulkanImage* image, VkFormat format, VkImageAspectFlags aspectMask)
{
	viewInfo.image = image->image;
	viewInfo.format = format;
	viewInfo.subresourceRange.levelCount = image->mipLevels;
	viewInfo.subresourceRange.aspectMask = aspectMask;
	viewInfo.subresourceRange.layerCount = image->layerCount;
}

std::unique_ptr<VulkanImageView> ImageViewBuilder::create(VulkanDevice* device)
{
	VkImageView view;
	VkResult result = vkCreateImageView(device->device, &viewInfo, nullptr, &view);
	CheckVulkanError(result, "Could not create texture image view");

	return std::make_unique<VulkanImageView>(device, view);
}

/////////////////////////////////////////////////////////////////////////////

SamplerBuilder::SamplerBuilder()
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

void SamplerBuilder::setAddressMode(VkSamplerAddressMode addressMode)
{
	samplerInfo.addressModeU = addressMode;
	samplerInfo.addressModeV = addressMode;
	samplerInfo.addressModeW = addressMode;
}

void SamplerBuilder::setAddressMode(VkSamplerAddressMode u, VkSamplerAddressMode v, VkSamplerAddressMode w)
{
	samplerInfo.addressModeU = u;
	samplerInfo.addressModeV = v;
	samplerInfo.addressModeW = w;
}

void SamplerBuilder::setMinFilter(VkFilter minFilter)
{
	samplerInfo.minFilter = minFilter;
}

void SamplerBuilder::setMagFilter(VkFilter magFilter)
{
	samplerInfo.magFilter = magFilter;
}

void SamplerBuilder::setMipmapMode(VkSamplerMipmapMode mode)
{
	samplerInfo.mipmapMode = mode;
}

void SamplerBuilder::setAnisotropy(float maxAnisotropy)
{
	samplerInfo.anisotropyEnable = VK_TRUE;
	samplerInfo.maxAnisotropy = maxAnisotropy;
}

void SamplerBuilder::setMaxLod(float value)
{
	samplerInfo.maxLod = value;
}

std::unique_ptr<VulkanSampler> SamplerBuilder::create(VulkanDevice* device)
{
	VkSampler sampler;
	VkResult result = vkCreateSampler(device->device, &samplerInfo, nullptr, &sampler);
	CheckVulkanError(result, "Could not create texture sampler");
	return std::make_unique<VulkanSampler>(device, sampler);
}

/////////////////////////////////////////////////////////////////////////////

BufferBuilder::BufferBuilder()
{
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
}

void BufferBuilder::setSize(size_t size)
{
	bufferInfo.size = max(size, (size_t)16);
}

void BufferBuilder::setUsage(VkBufferUsageFlags bufferUsage, VmaMemoryUsage memoryUsage, VmaAllocationCreateFlags allocFlags)
{
	bufferInfo.usage = bufferUsage;
	allocInfo.usage = memoryUsage;
	allocInfo.flags = allocFlags;
}

void BufferBuilder::setMemoryType(VkMemoryPropertyFlags requiredFlags, VkMemoryPropertyFlags preferredFlags, uint32_t memoryTypeBits)
{
	allocInfo.requiredFlags = requiredFlags;
	allocInfo.preferredFlags = preferredFlags;
	allocInfo.memoryTypeBits = memoryTypeBits;
}

std::unique_ptr<VulkanBuffer> BufferBuilder::create(VulkanDevice* device)
{
	VkBuffer buffer;
	VmaAllocation allocation;

	VkResult result = vmaCreateBuffer(device->allocator, &bufferInfo, &allocInfo, &buffer, &allocation, nullptr);
	CheckVulkanError(result, "Could not allocate memory for vulkan buffer");

	return std::make_unique<VulkanBuffer>(device, buffer, allocation, bufferInfo.size);
}

/////////////////////////////////////////////////////////////////////////////

ComputePipelineBuilder::ComputePipelineBuilder()
{
	pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
}

void ComputePipelineBuilder::setLayout(VulkanPipelineLayout* layout)
{
	pipelineInfo.layout = layout->layout;
}

void ComputePipelineBuilder::setComputeShader(VulkanShader* shader)
{
	stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stageInfo.module = shader->module;
	stageInfo.pName = "main";

	pipelineInfo.stage = stageInfo;
}

std::unique_ptr<VulkanPipeline> ComputePipelineBuilder::create(VulkanDevice* device)
{
	VkPipeline pipeline;
	vkCreateComputePipelines(device->device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline);
	return std::make_unique<VulkanPipeline>(device, pipeline);
}

/////////////////////////////////////////////////////////////////////////////

DescriptorSetLayoutBuilder::DescriptorSetLayoutBuilder()
{
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
}

void DescriptorSetLayoutBuilder::addBinding(int index, VkDescriptorType type, int arrayCount, VkShaderStageFlags stageFlags)
{
	VkDescriptorSetLayoutBinding binding = { };
	binding.binding = index;
	binding.descriptorType = type;
	binding.descriptorCount = arrayCount;
	binding.stageFlags = stageFlags;
	binding.pImmutableSamplers = nullptr;
	bindings.Push(binding);

	layoutInfo.bindingCount = (uint32_t)bindings.Size();
	layoutInfo.pBindings = &bindings[0];
}

std::unique_ptr<VulkanDescriptorSetLayout> DescriptorSetLayoutBuilder::create(VulkanDevice* device)
{
	VkDescriptorSetLayout layout;
	VkResult result = vkCreateDescriptorSetLayout(device->device, &layoutInfo, nullptr, &layout);
	CheckVulkanError(result, "Could not create descriptor set layout");
	return std::make_unique<VulkanDescriptorSetLayout>(device, layout);
}

/////////////////////////////////////////////////////////////////////////////

DescriptorPoolBuilder::DescriptorPoolBuilder()
{
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.maxSets = 1;
	poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
}

void DescriptorPoolBuilder::setMaxSets(int value)
{
	poolInfo.maxSets = value;
}

void DescriptorPoolBuilder::addPoolSize(VkDescriptorType type, int count)
{
	VkDescriptorPoolSize size;
	size.type = type;
	size.descriptorCount = count;
	poolSizes.push_back(size);

	poolInfo.poolSizeCount = (uint32_t)poolSizes.size();
	poolInfo.pPoolSizes = poolSizes.data();
}

std::unique_ptr<VulkanDescriptorPool> DescriptorPoolBuilder::create(VulkanDevice* device)
{
	VkDescriptorPool descriptorPool;
	VkResult result = vkCreateDescriptorPool(device->device, &poolInfo, nullptr, &descriptorPool);
	CheckVulkanError(result, "Could not create descriptor pool");
	return std::make_unique<VulkanDescriptorPool>(device, descriptorPool);
}

/////////////////////////////////////////////////////////////////////////////

QueryPoolBuilder::QueryPoolBuilder()
{
	poolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
}

void QueryPoolBuilder::setQueryType(VkQueryType type, int count, VkQueryPipelineStatisticFlags pipelineStatistics)
{
	poolInfo.queryType = type;
	poolInfo.queryCount = count;
	poolInfo.pipelineStatistics = pipelineStatistics;
}

std::unique_ptr<VulkanQueryPool> QueryPoolBuilder::create(VulkanDevice* device)
{
	VkQueryPool queryPool;
	VkResult result = vkCreateQueryPool(device->device, &poolInfo, nullptr, &queryPool);
	CheckVulkanError(result, "Could not create query pool");
	return std::make_unique<VulkanQueryPool>(device, queryPool);
}

/////////////////////////////////////////////////////////////////////////////

FramebufferBuilder::FramebufferBuilder()
{
	framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
}

void FramebufferBuilder::setRenderPass(VulkanRenderPass* renderPass)
{
	framebufferInfo.renderPass = renderPass->renderPass;
}

void FramebufferBuilder::addAttachment(VulkanImageView* view)
{
	attachments.push_back(view->view);

	framebufferInfo.attachmentCount = (uint32_t)attachments.size();
	framebufferInfo.pAttachments = attachments.data();
}

void FramebufferBuilder::addAttachment(VkImageView view)
{
	attachments.push_back(view);

	framebufferInfo.attachmentCount = (uint32_t)attachments.size();
	framebufferInfo.pAttachments = attachments.data();
}

void FramebufferBuilder::setSize(int width, int height, int layers)
{
	framebufferInfo.width = width;
	framebufferInfo.height = height;
	framebufferInfo.layers = 1;
}

std::unique_ptr<VulkanFramebuffer> FramebufferBuilder::create(VulkanDevice* device)
{
	VkFramebuffer framebuffer = 0;
	VkResult result = vkCreateFramebuffer(device->device, &framebufferInfo, nullptr, &framebuffer);
	CheckVulkanError(result, "Could not create framebuffer");
	return std::make_unique<VulkanFramebuffer>(device, framebuffer);
}

/////////////////////////////////////////////////////////////////////////////

GraphicsPipelineBuilder::GraphicsPipelineBuilder()
{
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pDepthStencilState = &depthStencil;
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

void GraphicsPipelineBuilder::setRasterizationSamples(VkSampleCountFlagBits samples)
{
	multisampling.rasterizationSamples = samples;
}

void GraphicsPipelineBuilder::setSubpass(int subpass)
{
	pipelineInfo.subpass = subpass;
}

void GraphicsPipelineBuilder::setLayout(VulkanPipelineLayout* layout)
{
	pipelineInfo.layout = layout->layout;
}

void GraphicsPipelineBuilder::setRenderPass(VulkanRenderPass* renderPass)
{
	pipelineInfo.renderPass = renderPass->renderPass;
}

void GraphicsPipelineBuilder::setTopology(VkPrimitiveTopology topology)
{
	inputAssembly.topology = topology;
}

void GraphicsPipelineBuilder::setViewport(float x, float y, float width, float height, float minDepth, float maxDepth)
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

void GraphicsPipelineBuilder::setScissor(int x, int y, int width, int height)
{
	scissor.offset.x = x;
	scissor.offset.y = y;
	scissor.extent.width = width;
	scissor.extent.height = height;

	viewportState.scissorCount = 1;
	viewportState.pScissors = &scissor;
}

void GraphicsPipelineBuilder::setCull(VkCullModeFlags cullMode, VkFrontFace frontFace)
{
	rasterizer.cullMode = cullMode;
	rasterizer.frontFace = frontFace;
}

void GraphicsPipelineBuilder::setDepthStencilEnable(bool test, bool write, bool stencil)
{
	depthStencil.depthTestEnable = test ? VK_TRUE : VK_FALSE;
	depthStencil.depthWriteEnable = write ? VK_TRUE : VK_FALSE;
	depthStencil.stencilTestEnable = stencil ? VK_TRUE : VK_FALSE;
}

void GraphicsPipelineBuilder::setStencil(VkStencilOp failOp, VkStencilOp passOp, VkStencilOp depthFailOp, VkCompareOp compareOp, uint32_t compareMask, uint32_t writeMask, uint32_t reference)
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

void GraphicsPipelineBuilder::setDepthFunc(VkCompareOp func)
{
	depthStencil.depthCompareOp = func;
}

void GraphicsPipelineBuilder::setDepthClampEnable(bool value)
{
	rasterizer.depthClampEnable = value ? VK_TRUE : VK_FALSE;
}

void GraphicsPipelineBuilder::setDepthBias(bool enable, float biasConstantFactor, float biasClamp, float biasSlopeFactor)
{
	rasterizer.depthBiasEnable = enable ? VK_TRUE : VK_FALSE;
	rasterizer.depthBiasConstantFactor = biasConstantFactor;
	rasterizer.depthBiasClamp = biasClamp;
	rasterizer.depthBiasSlopeFactor = biasSlopeFactor;
}

void GraphicsPipelineBuilder::setColorWriteMask(VkColorComponentFlags mask)
{
	colorBlendAttachment.colorWriteMask = mask;
}

void GraphicsPipelineBuilder::setAdditiveBlendMode()
{
	colorBlendAttachment.blendEnable = VK_TRUE;
	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
}

void GraphicsPipelineBuilder::setAlphaBlendMode()
{
	colorBlendAttachment.blendEnable = VK_TRUE;
	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
}

void GraphicsPipelineBuilder::setBlendMode(VkBlendOp op, VkBlendFactor src, VkBlendFactor dst)
{
	colorBlendAttachment.blendEnable = VK_TRUE;
	colorBlendAttachment.srcColorBlendFactor = src;
	colorBlendAttachment.dstColorBlendFactor = dst;
	colorBlendAttachment.colorBlendOp = op;
	colorBlendAttachment.srcAlphaBlendFactor = src;
	colorBlendAttachment.dstAlphaBlendFactor = dst;
	colorBlendAttachment.alphaBlendOp = op;
}

void GraphicsPipelineBuilder::setSubpassColorAttachmentCount(int count)
{
	colorBlendAttachments.resize(count, colorBlendAttachment);
	colorBlending.pAttachments = colorBlendAttachments.data();
	colorBlending.attachmentCount = (uint32_t)colorBlendAttachments.size();
}

void GraphicsPipelineBuilder::addVertexShader(VulkanShader* shader)
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

void GraphicsPipelineBuilder::addFragmentShader(VulkanShader* shader)
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

void GraphicsPipelineBuilder::addVertexBufferBinding(int index, size_t stride)
{
	VkVertexInputBindingDescription desc = {};
	desc.binding = index;
	desc.stride = (uint32_t)stride;
	desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	vertexInputBindings.push_back(desc);

	vertexInputInfo.vertexBindingDescriptionCount = (uint32_t)vertexInputBindings.size();
	vertexInputInfo.pVertexBindingDescriptions = vertexInputBindings.data();
}

void GraphicsPipelineBuilder::addVertexAttribute(int location, int binding, VkFormat format, size_t offset)
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

void GraphicsPipelineBuilder::addDynamicState(VkDynamicState state)
{
	dynamicStates.push_back(state);
	dynamicState.dynamicStateCount = (uint32_t)dynamicStates.size();
	dynamicState.pDynamicStates = dynamicStates.data();
}

std::unique_ptr<VulkanPipeline> GraphicsPipelineBuilder::create(VulkanDevice* device)
{
	VkPipeline pipeline = 0;
	VkResult result = vkCreateGraphicsPipelines(device->device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline);
	CheckVulkanError(result, "Could not create graphics pipeline");
	return std::make_unique<VulkanPipeline>(device, pipeline);
}

/////////////////////////////////////////////////////////////////////////////

PipelineLayoutBuilder::PipelineLayoutBuilder()
{
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
}

void PipelineLayoutBuilder::addSetLayout(VulkanDescriptorSetLayout* setLayout)
{
	setLayouts.push_back(setLayout->layout);
	pipelineLayoutInfo.setLayoutCount = (uint32_t)setLayouts.size();
	pipelineLayoutInfo.pSetLayouts = setLayouts.data();
}

void PipelineLayoutBuilder::addPushConstantRange(VkShaderStageFlags stageFlags, size_t offset, size_t size)
{
	VkPushConstantRange range = { };
	range.stageFlags = stageFlags;
	range.offset = (uint32_t)offset;
	range.size = (uint32_t)size;
	pushConstantRanges.push_back(range);
	pipelineLayoutInfo.pushConstantRangeCount = (uint32_t)pushConstantRanges.size();
	pipelineLayoutInfo.pPushConstantRanges = pushConstantRanges.data();
}

std::unique_ptr<VulkanPipelineLayout> PipelineLayoutBuilder::create(VulkanDevice* device)
{
	VkPipelineLayout pipelineLayout;
	VkResult result = vkCreatePipelineLayout(device->device, &pipelineLayoutInfo, nullptr, &pipelineLayout);
	CheckVulkanError(result, "Could not create pipeline layout");
	return std::make_unique<VulkanPipelineLayout>(device, pipelineLayout);
}

/////////////////////////////////////////////////////////////////////////////

RenderPassBuilder::RenderPassBuilder()
{
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
}

void RenderPassBuilder::addAttachment(VkFormat format, VkSampleCountFlagBits samples, VkAttachmentLoadOp load, VkAttachmentStoreOp store, VkImageLayout initialLayout, VkImageLayout finalLayout)
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

void RenderPassBuilder::addDepthStencilAttachment(VkFormat format, VkSampleCountFlagBits samples, VkAttachmentLoadOp load, VkAttachmentStoreOp store, VkAttachmentLoadOp stencilLoad, VkAttachmentStoreOp stencilStore, VkImageLayout initialLayout, VkImageLayout finalLayout)
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

void RenderPassBuilder::addExternalSubpassDependency(VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask)
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

void RenderPassBuilder::addSubpass()
{
	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

	subpasses.push_back(subpass);
	renderPassInfo.pSubpasses = subpasses.data();
	renderPassInfo.subpassCount = (uint32_t)subpasses.size();

	subpassData.push_back(std::make_unique<SubpassData>());
}

void RenderPassBuilder::addSubpassColorAttachmentRef(uint32_t index, VkImageLayout layout)
{
	VkAttachmentReference colorAttachmentRef = {};
	colorAttachmentRef.attachment = index;
	colorAttachmentRef.layout = layout;

	subpassData.back()->colorRefs.push_back(colorAttachmentRef);
	subpasses.back().pColorAttachments = subpassData.back()->colorRefs.data();
	subpasses.back().colorAttachmentCount = (uint32_t)subpassData.back()->colorRefs.size();
}

void RenderPassBuilder::addSubpassDepthStencilAttachmentRef(uint32_t index, VkImageLayout layout)
{
	VkAttachmentReference& depthAttachmentRef = subpassData.back()->depthRef;
	depthAttachmentRef.attachment = index;
	depthAttachmentRef.layout = layout;

	VkSubpassDescription& subpass = subpasses.back();
	subpass.pDepthStencilAttachment = &depthAttachmentRef;
}

std::unique_ptr<VulkanRenderPass> RenderPassBuilder::create(VulkanDevice* device)
{
	VkRenderPass renderPass = 0;
	VkResult result = vkCreateRenderPass(device->device, &renderPassInfo, nullptr, &renderPass);
	CheckVulkanError(result, "Could not create render pass");
	return std::make_unique<VulkanRenderPass>(device, renderPass);
}

/////////////////////////////////////////////////////////////////////////////

void PipelineBarrier::addMemory(VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask)
{
	VkMemoryBarrier barrier = { };
	barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
	barrier.srcAccessMask = srcAccessMask;
	barrier.dstAccessMask = dstAccessMask;
	memoryBarriers.push_back(barrier);
}

void PipelineBarrier::addBuffer(VulkanBuffer* buffer, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask)
{
	addBuffer(buffer, 0, buffer->size, srcAccessMask, dstAccessMask);
}

void PipelineBarrier::addBuffer(VulkanBuffer* buffer, VkDeviceSize offset, VkDeviceSize size, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask)
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

void PipelineBarrier::addImage(VulkanImage* image, VkImageLayout oldLayout, VkImageLayout newLayout, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask, VkImageAspectFlags aspectMask, int baseMipLevel, int levelCount)
{
	addImage(image->image, oldLayout, newLayout, srcAccessMask, dstAccessMask, aspectMask, baseMipLevel, levelCount);
}

void PipelineBarrier::addImage(VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask, VkImageAspectFlags aspectMask, int baseMipLevel, int levelCount)
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

void PipelineBarrier::addQueueTransfer(int srcFamily, int dstFamily, VulkanBuffer* buffer, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask)
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

void PipelineBarrier::addQueueTransfer(int srcFamily, int dstFamily, VulkanImage* image, VkImageLayout layout, VkImageAspectFlags aspectMask, int baseMipLevel, int levelCount)
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

void PipelineBarrier::execute(VulkanCommandBuffer* commandBuffer, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkDependencyFlags dependencyFlags)
{
	commandBuffer->pipelineBarrier(
		srcStageMask, dstStageMask, dependencyFlags,
		(uint32_t)memoryBarriers.size(), memoryBarriers.data(),
		(uint32_t)bufferMemoryBarriers.size(), bufferMemoryBarriers.data(),
		(uint32_t)imageMemoryBarriers.size(), imageMemoryBarriers.data());
}

/////////////////////////////////////////////////////////////////////////////

QueueSubmit::QueueSubmit()
{
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
}

void QueueSubmit::addCommandBuffer(VulkanCommandBuffer* buffer)
{
	commandBuffers.push_back(buffer->buffer);
	submitInfo.pCommandBuffers = commandBuffers.data();
	submitInfo.commandBufferCount = (uint32_t)commandBuffers.size();
}

void QueueSubmit::addWait(VkPipelineStageFlags waitStageMask, VulkanSemaphore* semaphore)
{
	waitStages.push_back(waitStageMask);
	waitSemaphores.push_back(semaphore->semaphore);

	submitInfo.pWaitDstStageMask = waitStages.data();
	submitInfo.pWaitSemaphores = waitSemaphores.data();
	submitInfo.waitSemaphoreCount = (uint32_t)waitSemaphores.size();
}

void QueueSubmit::addSignal(VulkanSemaphore* semaphore)
{
	signalSemaphores.push_back(semaphore->semaphore);
	submitInfo.pSignalSemaphores = signalSemaphores.data();
	submitInfo.signalSemaphoreCount = (uint32_t)signalSemaphores.size();
}

void QueueSubmit::execute(VulkanDevice* device, VkQueue queue, VulkanFence* fence)
{
	VkResult result = vkQueueSubmit(device->graphicsQueue, 1, &submitInfo, fence ? fence->fence : VK_NULL_HANDLE);
	CheckVulkanError(result, "Could not submit command buffer");
}

/////////////////////////////////////////////////////////////////////////////

void WriteDescriptors::addBuffer(VulkanDescriptorSet* descriptorSet, int binding, VkDescriptorType type, VulkanBuffer* buffer)
{
	addBuffer(descriptorSet, binding, type, buffer, 0, buffer->size);
}

void WriteDescriptors::addBuffer(VulkanDescriptorSet* descriptorSet, int binding, VkDescriptorType type, VulkanBuffer* buffer, size_t offset, size_t range)
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

void WriteDescriptors::addStorageImage(VulkanDescriptorSet* descriptorSet, int binding, VulkanImageView* view, VkImageLayout imageLayout)
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

void WriteDescriptors::addCombinedImageSampler(VulkanDescriptorSet* descriptorSet, int binding, VulkanImageView* view, VulkanSampler* sampler, VkImageLayout imageLayout)
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

void WriteDescriptors::addAccelerationStructure(VulkanDescriptorSet* descriptorSet, int binding, VulkanAccelerationStructure* accelStruct)
{
	auto extra = std::make_unique<WriteExtra>();
	extra->accelStruct = {};
	extra->accelStruct.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
	extra->accelStruct.accelerationStructureCount = 1;
	extra->accelStruct.pAccelerationStructures = &accelStruct->accelstruct;

	VkWriteDescriptorSet descriptorWrite = {};
	descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrite.dstSet = descriptorSet->set;
	descriptorWrite.dstBinding = binding;
	descriptorWrite.dstArrayElement = 0;
	descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
	descriptorWrite.descriptorCount = 1;
	descriptorWrite.pNext = &extra->accelStruct;
	writes.push_back(descriptorWrite);
	writeExtras.push_back(std::move(extra));
}

void WriteDescriptors::updateSets(VulkanDevice* device)
{
	vkUpdateDescriptorSets(device->device, (uint32_t)writes.size(), writes.data(), 0, nullptr);
}
