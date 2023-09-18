#include "vulkanbuilders.h"
#include "vulkansurface.h"
#include "vulkancompatibledevice.h"
#include "vulkanswapchain.h"
#include "glslang/glslang/Public/ShaderLang.h"
#include "glslang/spirv/GlslangToSpv.h"

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

void ShaderBuilder::Init()
{
	ShInitialize();
}

void ShaderBuilder::Deinit()
{
	ShFinalize();
}

ShaderBuilder::ShaderBuilder()
{
}

ShaderBuilder& ShaderBuilder::Type(ShaderType type)
{
	switch (type)
	{
	case ShaderType::Vertex: stage = EShLanguage::EShLangVertex; break;
	case ShaderType::TessControl: stage = EShLanguage::EShLangTessControl; break;
	case ShaderType::TessEvaluation: stage = EShLanguage::EShLangTessEvaluation; break;
	case ShaderType::Geometry: stage = EShLanguage::EShLangGeometry; break;
	case ShaderType::Fragment: stage = EShLanguage::EShLangFragment; break;
	case ShaderType::Compute: stage = EShLanguage::EShLangCompute; break;
	}
	return *this;
}

ShaderBuilder& ShaderBuilder::AddSource(const std::string& name, const std::string& code)
{
	sources.push_back({ name, code });
	return *this;
}

ShaderBuilder& ShaderBuilder::OnIncludeSystem(std::function<ShaderIncludeResult(std::string headerName, std::string includerName, size_t inclusionDepth)> onIncludeSystem)
{
	this->onIncludeSystem = std::move(onIncludeSystem);
	return *this;
}

ShaderBuilder& ShaderBuilder::OnIncludeLocal(std::function<ShaderIncludeResult(std::string headerName, std::string includerName, size_t inclusionDepth)> onIncludeLocal)
{
	this->onIncludeLocal = std::move(onIncludeLocal);
	return *this;
}

class ShaderBuilderIncluderImpl : public glslang::TShader::Includer
{
public:
	ShaderBuilderIncluderImpl(ShaderBuilder* shaderBuilder) : shaderBuilder(shaderBuilder)
	{
	}

	IncludeResult* includeSystem(const char* headerName, const char* includerName, size_t inclusionDepth) override
	{
		if (!shaderBuilder->onIncludeSystem)
		{
			return nullptr;
		}

		try
		{
			std::unique_ptr<ShaderIncludeResult> result;
			try
			{
				result = std::make_unique<ShaderIncludeResult>(shaderBuilder->onIncludeSystem(headerName, includerName, inclusionDepth));
			}
			catch (const std::exception& e)
			{
				result = std::make_unique<ShaderIncludeResult>(e.what());
			}

			if (!result || (result->name.empty() && result->text.empty()))
			{
				return nullptr;
			}

			IncludeResult* outer = new IncludeResult(result->name, result->text.data(), result->text.size(), result.get());
			result.release();
			return outer;
		}
		catch (...)
		{
			return nullptr;
		}
	}

	IncludeResult* includeLocal(const char* headerName, const char* includerName, size_t inclusionDepth) override
	{
		if (!shaderBuilder->onIncludeLocal)
		{
			return nullptr;
		}

		try
		{
			std::unique_ptr<ShaderIncludeResult> result;
			try
			{
				result = std::make_unique<ShaderIncludeResult>(shaderBuilder->onIncludeLocal(headerName, includerName, inclusionDepth));
			}
			catch (const std::exception& e)
			{
				result = std::make_unique<ShaderIncludeResult>(e.what());
			}

			if (!result || (result->name.empty() && result->text.empty()))
			{
				return nullptr;
			}

			IncludeResult* outer = new IncludeResult(result->name, result->text.data(), result->text.size(), result.get());
			result.release();
			return outer;
		}
		catch (...)
		{
			return nullptr;
		}
	}

	void releaseInclude(IncludeResult* result) override
	{
		if (result)
		{
			delete (ShaderIncludeResult*)result->userData;
			delete result;
		}
	}

private:
	ShaderBuilder* shaderBuilder = nullptr;
};

std::unique_ptr<VulkanShader> ShaderBuilder::Create(const char *shadername, VulkanDevice *device)
{
	EShLanguage stage = (EShLanguage)this->stage;

	std::vector<const char*> namesC, sourcesC;
	std::vector<int> lengthsC;
	for (const auto& s : sources)
	{
		namesC.push_back(s.first.c_str());
		sourcesC.push_back(s.second.c_str());
		lengthsC.push_back((int)s.second.size());
	}

	TBuiltInResource resources = DefaultTBuiltInResource;

	glslang::TShader shader(stage);
	shader.setStringsWithLengthsAndNames(sourcesC.data(), lengthsC.data(), namesC.data(), (int)sources.size());
	shader.setEnvInput(glslang::EShSourceGlsl, stage, glslang::EShClientVulkan, 100);
    if (device->Instance->ApiVersion >= VK_API_VERSION_1_2)
    {
        shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_2);
        shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_4);
    }
    else
    {
        shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_0);
        shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_0);
    }

	ShaderBuilderIncluderImpl includer(this);
	bool compileSuccess = shader.parse(&resources, 110, false, EShMsgVulkanRules, includer);
	if (!compileSuccess)
	{
		VulkanError((std::string("Shader compile failed: ") + shader.getInfoLog()).c_str());
	}

	glslang::TProgram program;
	program.addShader(&shader);
	bool linkSuccess = program.link(EShMsgDefault);
	if (!linkSuccess)
	{
		VulkanError((std::string("Shader link failed: ") + program.getInfoLog()).c_str());
	}

	glslang::TIntermediate *intermediate = program.getIntermediate(stage);
	if (!intermediate)
	{
		VulkanError("Internal shader compiler error");
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
		VulkanError("Could not create vulkan shader module");

	auto obj = std::make_unique<VulkanShader>(device, shaderModule);
	if (debugName)
		obj->SetDebugName(debugName);
	return obj;
}

/////////////////////////////////////////////////////////////////////////////

CommandPoolBuilder::CommandPoolBuilder()
{
}

CommandPoolBuilder& CommandPoolBuilder::QueueFamily(int index)
{
	queueFamilyIndex = index;
	return *this;
}

std::unique_ptr<VulkanCommandPool> CommandPoolBuilder::Create(VulkanDevice* device)
{
	auto obj = std::make_unique<VulkanCommandPool>(device, queueFamilyIndex != -1 ? queueFamilyIndex : device->GraphicsFamily);
	if (debugName)
		obj->SetDebugName(debugName);
	return obj;
}

/////////////////////////////////////////////////////////////////////////////

SemaphoreBuilder::SemaphoreBuilder()
{
}

std::unique_ptr<VulkanSemaphore> SemaphoreBuilder::Create(VulkanDevice* device)
{
	auto obj = std::make_unique<VulkanSemaphore>(device);
	if (debugName)
		obj->SetDebugName(debugName);
	return obj;
}

/////////////////////////////////////////////////////////////////////////////

FenceBuilder::FenceBuilder()
{
}

std::unique_ptr<VulkanFence> FenceBuilder::Create(VulkanDevice* device)
{
	auto obj = std::make_unique<VulkanFence>(device);
	if (debugName)
		obj->SetDebugName(debugName);
	return obj;
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

ImageBuilder& ImageBuilder::Size(int width, int height, int mipLevels, int arrayLayers)
{
	imageInfo.extent.width = width;
	imageInfo.extent.height = height;
	imageInfo.mipLevels = mipLevels;
	imageInfo.arrayLayers = arrayLayers;
	return *this;
}

ImageBuilder& ImageBuilder::Samples(VkSampleCountFlagBits samples)
{
	imageInfo.samples = samples;
	return *this;
}

ImageBuilder& ImageBuilder::Format(VkFormat format)
{
	imageInfo.format = format;
	return *this;
}

ImageBuilder& ImageBuilder::LinearTiling()
{
	imageInfo.tiling = VK_IMAGE_TILING_LINEAR;
	return *this;
}

ImageBuilder& ImageBuilder::Usage(VkImageUsageFlags usage, VmaMemoryUsage memoryUsage, VmaAllocationCreateFlags allocFlags)
{
	imageInfo.usage = usage;
	allocInfo.usage = memoryUsage;
	allocInfo.flags = allocFlags;
	return *this;
}

ImageBuilder& ImageBuilder::MemoryType(VkMemoryPropertyFlags requiredFlags, VkMemoryPropertyFlags preferredFlags, uint32_t memoryTypeBits)
{
	allocInfo.requiredFlags = requiredFlags;
	allocInfo.preferredFlags = preferredFlags;
	allocInfo.memoryTypeBits = memoryTypeBits;
	return *this;
}

bool ImageBuilder::IsFormatSupported(VulkanDevice* device, VkFormatFeatureFlags bufferFeatures)
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

std::unique_ptr<VulkanImage> ImageBuilder::Create(VulkanDevice* device, VkDeviceSize* allocatedBytes)
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

	auto obj = std::make_unique<VulkanImage>(device, image, allocation, imageInfo.extent.width, imageInfo.extent.height, imageInfo.mipLevels, imageInfo.arrayLayers);
	if (debugName)
		obj->SetDebugName(debugName);
	return obj;
}

std::unique_ptr<VulkanImage> ImageBuilder::TryCreate(VulkanDevice* device)
{
	VkImage image;
	VmaAllocation allocation;

	VkResult result = vmaCreateImage(device->allocator, &imageInfo, &allocInfo, &image, &allocation, nullptr);
	if (result != VK_SUCCESS)
		return nullptr;

	auto obj = std::make_unique<VulkanImage>(device, image, allocation, imageInfo.extent.width, imageInfo.extent.height, imageInfo.mipLevels, imageInfo.arrayLayers);
	if (debugName)
		obj->SetDebugName(debugName);
	return obj;
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

ImageViewBuilder& ImageViewBuilder::Type(VkImageViewType type)
{
	viewInfo.viewType = type;
	return *this;
}

ImageViewBuilder& ImageViewBuilder::Image(VulkanImage* image, VkFormat format, VkImageAspectFlags aspectMask)
{
	viewInfo.image = image->image;
	viewInfo.format = format;
	viewInfo.subresourceRange.levelCount = image->mipLevels;
	viewInfo.subresourceRange.aspectMask = aspectMask;
	viewInfo.subresourceRange.layerCount = image->layerCount;
	return *this;
}

std::unique_ptr<VulkanImageView> ImageViewBuilder::Create(VulkanDevice* device)
{
	VkImageView view;
	VkResult result = vkCreateImageView(device->device, &viewInfo, nullptr, &view);
	CheckVulkanError(result, "Could not create texture image view");

	auto obj = std::make_unique<VulkanImageView>(device, view);
	if (debugName)
		obj->SetDebugName(debugName);
	return obj;
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

SamplerBuilder& SamplerBuilder::AddressMode(VkSamplerAddressMode addressMode)
{
	samplerInfo.addressModeU = addressMode;
	samplerInfo.addressModeV = addressMode;
	samplerInfo.addressModeW = addressMode;
	return *this;
}

SamplerBuilder& SamplerBuilder::AddressMode(VkSamplerAddressMode u, VkSamplerAddressMode v, VkSamplerAddressMode w)
{
	samplerInfo.addressModeU = u;
	samplerInfo.addressModeV = v;
	samplerInfo.addressModeW = w;
	return *this;
}

SamplerBuilder& SamplerBuilder::MinFilter(VkFilter minFilter)
{
	samplerInfo.minFilter = minFilter;
	return *this;
}

SamplerBuilder& SamplerBuilder::MagFilter(VkFilter magFilter)
{
	samplerInfo.magFilter = magFilter;
	return *this;
}

SamplerBuilder& SamplerBuilder::MipmapMode(VkSamplerMipmapMode mode)
{
	samplerInfo.mipmapMode = mode;
	return *this;
}

SamplerBuilder& SamplerBuilder::Anisotropy(float maxAnisotropy)
{
	samplerInfo.anisotropyEnable = VK_TRUE;
	samplerInfo.maxAnisotropy = maxAnisotropy;
	return *this;
}

SamplerBuilder& SamplerBuilder::MipLodBias(float bias)
{
	samplerInfo.mipLodBias = bias;
	return *this;
}

SamplerBuilder& SamplerBuilder::MaxLod(float value)
{
	samplerInfo.maxLod = value;
	return *this;
}

std::unique_ptr<VulkanSampler> SamplerBuilder::Create(VulkanDevice* device)
{
	VkSampler sampler;
	VkResult result = vkCreateSampler(device->device, &samplerInfo, nullptr, &sampler);
	CheckVulkanError(result, "Could not create texture sampler");
	auto obj = std::make_unique<VulkanSampler>(device, sampler);
	if (debugName)
		obj->SetDebugName(debugName);
	return obj;
}

/////////////////////////////////////////////////////////////////////////////

BufferBuilder::BufferBuilder()
{
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
}

BufferBuilder& BufferBuilder::Size(size_t size)
{
	bufferInfo.size = std::max(size, (size_t)16);
	return *this;
}

BufferBuilder& BufferBuilder::Usage(VkBufferUsageFlags bufferUsage, VmaMemoryUsage memoryUsage, VmaAllocationCreateFlags allocFlags)
{
	bufferInfo.usage = bufferUsage;
	allocInfo.usage = memoryUsage;
	allocInfo.flags = allocFlags;
	return *this;
}

BufferBuilder& BufferBuilder::MemoryType(VkMemoryPropertyFlags requiredFlags, VkMemoryPropertyFlags preferredFlags, uint32_t memoryTypeBits)
{
	allocInfo.requiredFlags = requiredFlags;
	allocInfo.preferredFlags = preferredFlags;
	allocInfo.memoryTypeBits = memoryTypeBits;
	return *this;
}

BufferBuilder& BufferBuilder::MinAlignment(VkDeviceSize memoryAlignment)
{
	minAlignment = memoryAlignment;
	return *this;
}

std::unique_ptr<VulkanBuffer> BufferBuilder::Create(VulkanDevice* device)
{
	VkBuffer buffer;
	VmaAllocation allocation;

	if (minAlignment == 0)
	{
		VkResult result = vmaCreateBuffer(device->allocator, &bufferInfo, &allocInfo, &buffer, &allocation, nullptr);
		CheckVulkanError(result, "Could not allocate memory for vulkan buffer");
	}
	else
	{
		VkResult result = vmaCreateBufferWithAlignment(device->allocator, &bufferInfo, &allocInfo, minAlignment, &buffer, &allocation, nullptr);
		CheckVulkanError(result, "Could not allocate memory for vulkan buffer");
	}

	auto obj = std::make_unique<VulkanBuffer>(device, buffer, allocation, bufferInfo.size);
	if (debugName)
		obj->SetDebugName(debugName);
	return obj;
}

/////////////////////////////////////////////////////////////////////////////

AccelerationStructureBuilder::AccelerationStructureBuilder()
{
}

AccelerationStructureBuilder& AccelerationStructureBuilder::Type(VkAccelerationStructureTypeKHR type)
{
	createInfo.type = type;
	return *this;
}

AccelerationStructureBuilder& AccelerationStructureBuilder::Buffer(VulkanBuffer* buffer, VkDeviceSize size)
{
	createInfo.buffer = buffer->buffer;
	createInfo.offset = 0;
	createInfo.size = size;
	return *this;
}

AccelerationStructureBuilder& AccelerationStructureBuilder::Buffer(VulkanBuffer* buffer, VkDeviceSize offset, VkDeviceSize size)
{
	createInfo.buffer = buffer->buffer;
	createInfo.offset = offset;
	createInfo.size = size;
	return *this;
}

std::unique_ptr<VulkanAccelerationStructure> AccelerationStructureBuilder::Create(VulkanDevice* device)
{
	VkAccelerationStructureKHR hande = {};
	VkResult result = vkCreateAccelerationStructureKHR(device->device, &createInfo, nullptr, &hande);
	if (result != VK_SUCCESS)
		VulkanError("vkCreateAccelerationStructureKHR failed");
	auto obj = std::make_unique<VulkanAccelerationStructure>(device, hande);
	if (debugName)
		obj->SetDebugName(debugName);
	return obj;
}

/////////////////////////////////////////////////////////////////////////////

ComputePipelineBuilder::ComputePipelineBuilder()
{
	pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
}

ComputePipelineBuilder& ComputePipelineBuilder::Cache(VulkanPipelineCache* cache)
{
	this->cache = cache;
	return *this;
}

ComputePipelineBuilder& ComputePipelineBuilder::Layout(VulkanPipelineLayout* layout)
{
	pipelineInfo.layout = layout->layout;
	return *this;
}

ComputePipelineBuilder& ComputePipelineBuilder::ComputeShader(VulkanShader* shader)
{
	stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stageInfo.module = shader->module;
	stageInfo.pName = "main";

	pipelineInfo.stage = stageInfo;
	return *this;
}

std::unique_ptr<VulkanPipeline> ComputePipelineBuilder::Create(VulkanDevice* device)
{
	VkPipeline pipeline;
	vkCreateComputePipelines(device->device, cache ? cache->cache : VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline);
	auto obj = std::make_unique<VulkanPipeline>(device, pipeline);
	if (debugName)
		obj->SetDebugName(debugName);
	return obj;
}

/////////////////////////////////////////////////////////////////////////////

DescriptorSetLayoutBuilder::DescriptorSetLayoutBuilder()
{
}

DescriptorSetLayoutBuilder& DescriptorSetLayoutBuilder::Flags(VkDescriptorSetLayoutCreateFlags flags)
{
	layoutInfo.flags = flags;
	return *this;
}

DescriptorSetLayoutBuilder& DescriptorSetLayoutBuilder::AddBinding(int index, VkDescriptorType type, int arrayCount, VkShaderStageFlags stageFlags, VkDescriptorBindingFlags flags)
{
	VkDescriptorSetLayoutBinding binding = { };
	binding.binding = index;
	binding.descriptorType = type;
	binding.descriptorCount = arrayCount;
	binding.stageFlags = stageFlags;
	binding.pImmutableSamplers = nullptr;
	bindings.push_back(binding);
	bindingFlags.push_back(flags);

	layoutInfo.bindingCount = (uint32_t)bindings.size();
	layoutInfo.pBindings = bindings.data();

	bindingFlagsInfo.bindingCount = (uint32_t)bindings.size();
	bindingFlagsInfo.pBindingFlags = bindingFlags.data();

	if (flags != 0)
		layoutInfo.pNext = &bindingFlagsInfo;

	return *this;
}

std::unique_ptr<VulkanDescriptorSetLayout> DescriptorSetLayoutBuilder::Create(VulkanDevice* device)
{
	VkDescriptorSetLayout layout;
	VkResult result = vkCreateDescriptorSetLayout(device->device, &layoutInfo, nullptr, &layout);
	CheckVulkanError(result, "Could not create descriptor set layout");
	auto obj = std::make_unique<VulkanDescriptorSetLayout>(device, layout);
	if (debugName)
		obj->SetDebugName(debugName);
	return obj;
}

/////////////////////////////////////////////////////////////////////////////

DescriptorPoolBuilder::DescriptorPoolBuilder()
{
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.maxSets = 1;
	poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
}

DescriptorPoolBuilder& DescriptorPoolBuilder::Flags(VkDescriptorPoolCreateFlags flags)
{
	poolInfo.flags = flags | VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	return *this;
}

DescriptorPoolBuilder& DescriptorPoolBuilder::MaxSets(int value)
{
	poolInfo.maxSets = value;
	return *this;
}

DescriptorPoolBuilder& DescriptorPoolBuilder::AddPoolSize(VkDescriptorType type, int count)
{
	VkDescriptorPoolSize size;
	size.type = type;
	size.descriptorCount = count;
	poolSizes.push_back(size);

	poolInfo.poolSizeCount = (uint32_t)poolSizes.size();
	poolInfo.pPoolSizes = poolSizes.data();
	return *this;
}

std::unique_ptr<VulkanDescriptorPool> DescriptorPoolBuilder::Create(VulkanDevice* device)
{
	VkDescriptorPool descriptorPool;
	VkResult result = vkCreateDescriptorPool(device->device, &poolInfo, nullptr, &descriptorPool);
	CheckVulkanError(result, "Could not create descriptor pool");
	auto obj = std::make_unique<VulkanDescriptorPool>(device, descriptorPool);
	if (debugName)
		obj->SetDebugName(debugName);
	return obj;
}

/////////////////////////////////////////////////////////////////////////////

QueryPoolBuilder::QueryPoolBuilder()
{
	poolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
}

QueryPoolBuilder& QueryPoolBuilder::QueryType(VkQueryType type, int count, VkQueryPipelineStatisticFlags pipelineStatistics)
{
	poolInfo.queryType = type;
	poolInfo.queryCount = count;
	poolInfo.pipelineStatistics = pipelineStatistics;
	return *this;
}

std::unique_ptr<VulkanQueryPool> QueryPoolBuilder::Create(VulkanDevice* device)
{
	VkQueryPool queryPool;
	VkResult result = vkCreateQueryPool(device->device, &poolInfo, nullptr, &queryPool);
	CheckVulkanError(result, "Could not create query pool");
	auto obj = std::make_unique<VulkanQueryPool>(device, queryPool);
	if (debugName)
		obj->SetDebugName(debugName);
	return obj;
}

/////////////////////////////////////////////////////////////////////////////

FramebufferBuilder::FramebufferBuilder()
{
	framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
}

FramebufferBuilder& FramebufferBuilder::RenderPass(VulkanRenderPass* renderPass)
{
	framebufferInfo.renderPass = renderPass->renderPass;
	return *this;
}

FramebufferBuilder& FramebufferBuilder::AddAttachment(VulkanImageView* view)
{
	attachments.push_back(view->view);

	framebufferInfo.attachmentCount = (uint32_t)attachments.size();
	framebufferInfo.pAttachments = attachments.data();
	return *this;
}

FramebufferBuilder& FramebufferBuilder::AddAttachment(VkImageView view)
{
	attachments.push_back(view);

	framebufferInfo.attachmentCount = (uint32_t)attachments.size();
	framebufferInfo.pAttachments = attachments.data();
	return *this;
}

FramebufferBuilder& FramebufferBuilder::Size(int width, int height, int layers)
{
	framebufferInfo.width = width;
	framebufferInfo.height = height;
	framebufferInfo.layers = 1;
	return *this;
}

std::unique_ptr<VulkanFramebuffer> FramebufferBuilder::Create(VulkanDevice* device)
{
	VkFramebuffer framebuffer = 0;
	VkResult result = vkCreateFramebuffer(device->device, &framebufferInfo, nullptr, &framebuffer);
	CheckVulkanError(result, "Could not create framebuffer");
	auto obj = std::make_unique<VulkanFramebuffer>(device, framebuffer);
	if (debugName)
		obj->SetDebugName(debugName);
	return obj;
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
	viewportState.viewportCount = 1;
	viewportState.pViewports = &viewport;
	viewportState.scissorCount = 1;
	viewportState.pScissors = &scissor;

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

GraphicsPipelineBuilder& GraphicsPipelineBuilder::RasterizationSamples(VkSampleCountFlagBits samples)
{
	multisampling.rasterizationSamples = samples;
	return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::Cache(VulkanPipelineCache* cache)
{
	this->cache = cache;
	return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::Subpass(int subpass)
{
	pipelineInfo.subpass = subpass;
	return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::Layout(VulkanPipelineLayout* layout)
{
	pipelineInfo.layout = layout->layout;
	return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::RenderPass(VulkanRenderPass* renderPass)
{
	pipelineInfo.renderPass = renderPass->renderPass;
	return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::Topology(VkPrimitiveTopology topology)
{
	inputAssembly.topology = topology;
	return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::Viewport(float x, float y, float width, float height, float minDepth, float maxDepth)
{
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = width;
	viewport.height = height;
	viewport.minDepth = minDepth;
	viewport.maxDepth = maxDepth;
	return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::Scissor(int x, int y, int width, int height)
{
	scissor.offset.x = x;
	scissor.offset.y = y;
	scissor.extent.width = width;
	scissor.extent.height = height;
	return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::Cull(VkCullModeFlags cullMode, VkFrontFace frontFace)
{
	rasterizer.cullMode = cullMode;
	rasterizer.frontFace = frontFace;
	return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::DepthStencilEnable(bool test, bool write, bool stencil)
{
	depthStencil.depthTestEnable = test ? VK_TRUE : VK_FALSE;
	depthStencil.depthWriteEnable = write ? VK_TRUE : VK_FALSE;
	depthStencil.stencilTestEnable = stencil ? VK_TRUE : VK_FALSE;
	return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::Stencil(VkStencilOp failOp, VkStencilOp passOp, VkStencilOp depthFailOp, VkCompareOp compareOp, uint32_t compareMask, uint32_t writeMask, uint32_t reference)
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
	return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::DepthFunc(VkCompareOp func)
{
	depthStencil.depthCompareOp = func;
	return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::DepthClampEnable(bool value)
{
	rasterizer.depthClampEnable = value ? VK_TRUE : VK_FALSE;
	return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::DepthBias(bool enable, float biasConstantFactor, float biasClamp, float biasSlopeFactor)
{
	rasterizer.depthBiasEnable = enable ? VK_TRUE : VK_FALSE;
	rasterizer.depthBiasConstantFactor = biasConstantFactor;
	rasterizer.depthBiasClamp = biasClamp;
	rasterizer.depthBiasSlopeFactor = biasSlopeFactor;
	return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::ColorWriteMask(VkColorComponentFlags mask)
{
	colorBlendAttachment.colorWriteMask = mask;
	return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::AdditiveBlendMode()
{
	colorBlendAttachment.blendEnable = VK_TRUE;
	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
	return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::AlphaBlendMode()
{
	colorBlendAttachment.blendEnable = VK_TRUE;
	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
	return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::BlendMode(VkBlendOp op, VkBlendFactor src, VkBlendFactor dst)
{
	colorBlendAttachment.blendEnable = VK_TRUE;
	colorBlendAttachment.srcColorBlendFactor = src;
	colorBlendAttachment.dstColorBlendFactor = dst;
	colorBlendAttachment.colorBlendOp = op;
	colorBlendAttachment.srcAlphaBlendFactor = src;
	colorBlendAttachment.dstAlphaBlendFactor = dst;
	colorBlendAttachment.alphaBlendOp = op;
	return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::SubpassColorAttachmentCount(int count)
{
	colorBlendAttachments.resize(count, colorBlendAttachment);
	colorBlending.pAttachments = colorBlendAttachments.data();
	colorBlending.attachmentCount = (uint32_t)colorBlendAttachments.size();
	return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::AddVertexShader(VulkanShader* shader)
{
	VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};
	vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertShaderStageInfo.module = shader->module;
	vertShaderStageInfo.pName = "main";
	shaderStages.push_back(vertShaderStageInfo);

	pipelineInfo.stageCount = (uint32_t)shaderStages.size();
	pipelineInfo.pStages = shaderStages.data();
	return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::AddFragmentShader(VulkanShader* shader)
{
	VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
	fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragShaderStageInfo.module = shader->module;
	fragShaderStageInfo.pName = "main";
	shaderStages.push_back(fragShaderStageInfo);

	pipelineInfo.stageCount = (uint32_t)shaderStages.size();
	pipelineInfo.pStages = shaderStages.data();
	return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::AddVertexBufferBinding(int index, size_t stride)
{
	VkVertexInputBindingDescription desc = {};
	desc.binding = index;
	desc.stride = (uint32_t)stride;
	desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	vertexInputBindings.push_back(desc);

	vertexInputInfo.vertexBindingDescriptionCount = (uint32_t)vertexInputBindings.size();
	vertexInputInfo.pVertexBindingDescriptions = vertexInputBindings.data();
	return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::AddVertexAttribute(int location, int binding, VkFormat format, size_t offset)
{
	VkVertexInputAttributeDescription desc = { };
	desc.location = location;
	desc.binding = binding;
	desc.format = format;
	desc.offset = (uint32_t)offset;
	vertexInputAttributes.push_back(desc);

	vertexInputInfo.vertexAttributeDescriptionCount = (uint32_t)vertexInputAttributes.size();
	vertexInputInfo.pVertexAttributeDescriptions = vertexInputAttributes.data();
	return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::AddDynamicState(VkDynamicState state)
{
	dynamicStates.push_back(state);
	dynamicState.dynamicStateCount = (uint32_t)dynamicStates.size();
	dynamicState.pDynamicStates = dynamicStates.data();
	return *this;
}

std::unique_ptr<VulkanPipeline> GraphicsPipelineBuilder::Create(VulkanDevice* device)
{
	VkPipeline pipeline = 0;
	VkResult result = vkCreateGraphicsPipelines(device->device, cache ? cache->cache : VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline);
	CheckVulkanError(result, "Could not create graphics pipeline");
	auto obj = std::make_unique<VulkanPipeline>(device, pipeline);
	if (debugName)
		obj->SetDebugName(debugName);
	return obj;
}

/////////////////////////////////////////////////////////////////////////////

PipelineLayoutBuilder::PipelineLayoutBuilder()
{
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
}

PipelineLayoutBuilder& PipelineLayoutBuilder::AddSetLayout(VulkanDescriptorSetLayout* setLayout)
{
	setLayouts.push_back(setLayout->layout);
	pipelineLayoutInfo.setLayoutCount = (uint32_t)setLayouts.size();
	pipelineLayoutInfo.pSetLayouts = setLayouts.data();
	return *this;
}

PipelineLayoutBuilder& PipelineLayoutBuilder::AddPushConstantRange(VkShaderStageFlags stageFlags, size_t offset, size_t size)
{
	VkPushConstantRange range = { };
	range.stageFlags = stageFlags;
	range.offset = (uint32_t)offset;
	range.size = (uint32_t)size;
	pushConstantRanges.push_back(range);
	pipelineLayoutInfo.pushConstantRangeCount = (uint32_t)pushConstantRanges.size();
	pipelineLayoutInfo.pPushConstantRanges = pushConstantRanges.data();
	return *this;
}

std::unique_ptr<VulkanPipelineLayout> PipelineLayoutBuilder::Create(VulkanDevice* device)
{
	VkPipelineLayout pipelineLayout;
	VkResult result = vkCreatePipelineLayout(device->device, &pipelineLayoutInfo, nullptr, &pipelineLayout);
	CheckVulkanError(result, "Could not create pipeline layout");
	auto obj = std::make_unique<VulkanPipelineLayout>(device, pipelineLayout);
	if (debugName)
		obj->SetDebugName(debugName);
	return obj;
}

/////////////////////////////////////////////////////////////////////////////

PipelineCacheBuilder::PipelineCacheBuilder()
{
	pipelineCacheInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
}

PipelineCacheBuilder& PipelineCacheBuilder::InitialData(const void* data, size_t size)
{
	initData.resize(size);
	memcpy(initData.data(), data, size);
	return *this;
}

PipelineCacheBuilder& PipelineCacheBuilder::Flags(VkPipelineCacheCreateFlags flags)
{
	pipelineCacheInfo.flags = flags;
	return *this;
}

std::unique_ptr<VulkanPipelineCache> PipelineCacheBuilder::Create(VulkanDevice* device)
{
	pipelineCacheInfo.pInitialData = nullptr;
	pipelineCacheInfo.initialDataSize = 0;

	// Check if the saved cache data is compatible with our device:
	if (initData.size() >= sizeof(VkPipelineCacheHeaderVersionOne))
	{
		VkPipelineCacheHeaderVersionOne* header = (VkPipelineCacheHeaderVersionOne*)initData.data();
		if (header->headerVersion == VK_PIPELINE_CACHE_HEADER_VERSION_ONE &&
			header->vendorID == device->PhysicalDevice.Properties.Properties.vendorID &&
			header->deviceID == device->PhysicalDevice.Properties.Properties.deviceID &&
			memcmp(header->pipelineCacheUUID, device->PhysicalDevice.Properties.Properties.pipelineCacheUUID, VK_UUID_SIZE) == 0)
		{
			pipelineCacheInfo.pInitialData = initData.data();
			pipelineCacheInfo.initialDataSize = initData.size();
		}
	}

	VkPipelineCache pipelineCache;
	VkResult result = vkCreatePipelineCache(device->device, &pipelineCacheInfo, nullptr, &pipelineCache);
	CheckVulkanError(result, "Could not create pipeline cache");
	auto obj = std::make_unique<VulkanPipelineCache>(device, pipelineCache);
	if (debugName)
		obj->SetDebugName(debugName);
	return obj;
}

/////////////////////////////////////////////////////////////////////////////

RenderPassBuilder::RenderPassBuilder()
{
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
}

RenderPassBuilder& RenderPassBuilder::AddAttachment(VkFormat format, VkSampleCountFlagBits samples, VkAttachmentLoadOp load, VkAttachmentStoreOp store, VkImageLayout initialLayout, VkImageLayout finalLayout)
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
	return *this;
}

RenderPassBuilder& RenderPassBuilder::AddDepthStencilAttachment(VkFormat format, VkSampleCountFlagBits samples, VkAttachmentLoadOp load, VkAttachmentStoreOp store, VkAttachmentLoadOp stencilLoad, VkAttachmentStoreOp stencilStore, VkImageLayout initialLayout, VkImageLayout finalLayout)
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
	return *this;
}

RenderPassBuilder& RenderPassBuilder::AddExternalSubpassDependency(VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask)
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
	return *this;
}

RenderPassBuilder& RenderPassBuilder::AddSubpass()
{
	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

	subpasses.push_back(subpass);
	renderPassInfo.pSubpasses = subpasses.data();
	renderPassInfo.subpassCount = (uint32_t)subpasses.size();

	subpassData.push_back(std::make_unique<SubpassData>());
	return *this;
}

RenderPassBuilder& RenderPassBuilder::AddSubpassColorAttachmentRef(uint32_t index, VkImageLayout layout)
{
	VkAttachmentReference colorAttachmentRef = {};
	colorAttachmentRef.attachment = index;
	colorAttachmentRef.layout = layout;

	subpassData.back()->colorRefs.push_back(colorAttachmentRef);
	subpasses.back().pColorAttachments = subpassData.back()->colorRefs.data();
	subpasses.back().colorAttachmentCount = (uint32_t)subpassData.back()->colorRefs.size();
	return *this;
}

RenderPassBuilder& RenderPassBuilder::AddSubpassDepthStencilAttachmentRef(uint32_t index, VkImageLayout layout)
{
	VkAttachmentReference& depthAttachmentRef = subpassData.back()->depthRef;
	depthAttachmentRef.attachment = index;
	depthAttachmentRef.layout = layout;

	VkSubpassDescription& subpass = subpasses.back();
	subpass.pDepthStencilAttachment = &depthAttachmentRef;
	return *this;
}

std::unique_ptr<VulkanRenderPass> RenderPassBuilder::Create(VulkanDevice* device)
{
	VkRenderPass renderPass = 0;
	VkResult result = vkCreateRenderPass(device->device, &renderPassInfo, nullptr, &renderPass);
	CheckVulkanError(result, "Could not create render pass");
	auto obj = std::make_unique<VulkanRenderPass>(device, renderPass);
	if (debugName)
		obj->SetDebugName(debugName);
	return obj;
}

/////////////////////////////////////////////////////////////////////////////

PipelineBarrier& PipelineBarrier::AddMemory(VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask)
{
	VkMemoryBarrier barrier = { };
	barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
	barrier.srcAccessMask = srcAccessMask;
	barrier.dstAccessMask = dstAccessMask;
	memoryBarriers.push_back(barrier);
	return *this;
}

PipelineBarrier& PipelineBarrier::AddBuffer(VulkanBuffer* buffer, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask)
{
	return AddBuffer(buffer, 0, buffer->size, srcAccessMask, dstAccessMask);
}

PipelineBarrier& PipelineBarrier::AddBuffer(VulkanBuffer* buffer, VkDeviceSize offset, VkDeviceSize size, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask)
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
	return *this;
}

PipelineBarrier& PipelineBarrier::AddImage(VulkanImage* image, VkImageLayout oldLayout, VkImageLayout newLayout, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask, VkImageAspectFlags aspectMask, int baseMipLevel, int levelCount, int baseArrayLayer, int layerCount)
{
	return AddImage(image->image, oldLayout, newLayout, srcAccessMask, dstAccessMask, aspectMask, baseMipLevel, levelCount, baseArrayLayer, layerCount);
}

PipelineBarrier& PipelineBarrier::AddImage(VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask, VkImageAspectFlags aspectMask, int baseMipLevel, int levelCount, int baseArrayLayer, int layerCount)
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
	barrier.subresourceRange.baseArrayLayer = baseArrayLayer;
	barrier.subresourceRange.layerCount = layerCount;
	imageMemoryBarriers.push_back(barrier);
	return *this;
}

PipelineBarrier& PipelineBarrier::AddQueueTransfer(int srcFamily, int dstFamily, VulkanBuffer* buffer, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask)
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
	return *this;
}

PipelineBarrier& PipelineBarrier::AddQueueTransfer(int srcFamily, int dstFamily, VulkanImage* image, VkImageLayout layout, VkImageAspectFlags aspectMask, int baseMipLevel, int levelCount)
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
	return *this;
}

void PipelineBarrier::Execute(VulkanCommandBuffer* commandBuffer, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkDependencyFlags dependencyFlags)
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

QueueSubmit& QueueSubmit::AddCommandBuffer(VulkanCommandBuffer* buffer)
{
	commandBuffers.push_back(buffer->buffer);
	submitInfo.pCommandBuffers = commandBuffers.data();
	submitInfo.commandBufferCount = (uint32_t)commandBuffers.size();
	return *this;
}

QueueSubmit& QueueSubmit::AddWait(VkPipelineStageFlags waitStageMask, VulkanSemaphore* semaphore)
{
	waitStages.push_back(waitStageMask);
	waitSemaphores.push_back(semaphore->semaphore);

	submitInfo.pWaitDstStageMask = waitStages.data();
	submitInfo.pWaitSemaphores = waitSemaphores.data();
	submitInfo.waitSemaphoreCount = (uint32_t)waitSemaphores.size();
	return *this;
}

QueueSubmit& QueueSubmit::AddSignal(VulkanSemaphore* semaphore)
{
	signalSemaphores.push_back(semaphore->semaphore);
	submitInfo.pSignalSemaphores = signalSemaphores.data();
	submitInfo.signalSemaphoreCount = (uint32_t)signalSemaphores.size();
	return *this;
}

void QueueSubmit::Execute(VulkanDevice* device, VkQueue queue, VulkanFence* fence)
{
	VkResult result = vkQueueSubmit(device->GraphicsQueue, 1, &submitInfo, fence ? fence->fence : VK_NULL_HANDLE);
	CheckVulkanError(result, "Could not submit command buffer");
}

/////////////////////////////////////////////////////////////////////////////

WriteDescriptors& WriteDescriptors::AddBuffer(VulkanDescriptorSet* descriptorSet, int binding, VkDescriptorType type, VulkanBuffer* buffer)
{
	return AddBuffer(descriptorSet, binding, type, buffer, 0, buffer->size);
}

WriteDescriptors& WriteDescriptors::AddBuffer(VulkanDescriptorSet* descriptorSet, int binding, VkDescriptorType type, VulkanBuffer* buffer, size_t offset, size_t range)
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
	return *this;
}

WriteDescriptors& WriteDescriptors::AddStorageImage(VulkanDescriptorSet* descriptorSet, int binding, VulkanImageView* view, VkImageLayout imageLayout)
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
	return *this;
}

WriteDescriptors& WriteDescriptors::AddCombinedImageSampler(VulkanDescriptorSet* descriptorSet, int binding, VulkanImageView* view, VulkanSampler* sampler, VkImageLayout imageLayout)
{
	return AddCombinedImageSampler(descriptorSet, binding, 0, view, sampler, imageLayout);
}

WriteDescriptors& WriteDescriptors::AddCombinedImageSampler(VulkanDescriptorSet* descriptorSet, int binding, int arrayIndex, VulkanImageView* view, VulkanSampler* sampler, VkImageLayout imageLayout)
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
	descriptorWrite.dstArrayElement = arrayIndex;
	descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descriptorWrite.descriptorCount = 1;
	descriptorWrite.pImageInfo = &extra->imageInfo;
	writes.push_back(descriptorWrite);
	writeExtras.push_back(std::move(extra));
	return *this;
}

WriteDescriptors& WriteDescriptors::AddAccelerationStructure(VulkanDescriptorSet* descriptorSet, int binding, VulkanAccelerationStructure* accelStruct)
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
	return *this;
}

void WriteDescriptors::Execute(VulkanDevice* device)
{
	if (!writes.empty())
		vkUpdateDescriptorSets(device->device, (uint32_t)writes.size(), writes.data(), 0, nullptr);
}

/////////////////////////////////////////////////////////////////////////////

VulkanInstanceBuilder::VulkanInstanceBuilder()
{
	apiVersionsToTry = { VK_API_VERSION_1_2, VK_API_VERSION_1_1, VK_API_VERSION_1_0 };

	OptionalExtension(VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME);
	OptionalExtension(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
}

VulkanInstanceBuilder& VulkanInstanceBuilder::ApiVersionsToTry(const std::vector<uint32_t>& versions)
{
	apiVersionsToTry = versions;
	return *this;
}

VulkanInstanceBuilder& VulkanInstanceBuilder::RequireExtension(const std::string& extensionName)
{
	requiredExtensions.insert(extensionName);
	return *this;
}

VulkanInstanceBuilder& VulkanInstanceBuilder::RequireSurfaceExtensions(bool enable)
{
	if (enable)
	{
		RequireExtension(VK_KHR_SURFACE_EXTENSION_NAME);

#if defined(VK_USE_PLATFORM_WIN32_KHR)
		RequireExtension(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_MACOS_MVK)
		RequireExtension(VK_MVK_MACOS_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
		RequireExtension(VK_KHR_XLIB_SURFACE_EXTENSION_NAME);
#endif

		OptionalExtension(VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME); // For HDR support
	}
	return *this;
}

VulkanInstanceBuilder& VulkanInstanceBuilder::OptionalExtension(const std::string& extensionName)
{
	optionalExtensions.insert(extensionName);
	return *this;
}

VulkanInstanceBuilder& VulkanInstanceBuilder::DebugLayer(bool enable)
{
	debugLayer = enable;
	return *this;
}

std::shared_ptr<VulkanInstance> VulkanInstanceBuilder::Create()
{
	return std::make_shared<VulkanInstance>(apiVersionsToTry, requiredExtensions, optionalExtensions, debugLayer);
}

/////////////////////////////////////////////////////////////////////////////

#ifdef VK_USE_PLATFORM_WIN32_KHR

VulkanSurfaceBuilder::VulkanSurfaceBuilder()
{
}

VulkanSurfaceBuilder& VulkanSurfaceBuilder::Win32Window(HWND hwnd)
{
	this->hwnd = hwnd;
	return *this;
}

std::shared_ptr<VulkanSurface> VulkanSurfaceBuilder::Create(std::shared_ptr<VulkanInstance> instance)
{
	return std::make_shared<VulkanSurface>(std::move(instance), hwnd);
}

#endif

/////////////////////////////////////////////////////////////////////////////

VulkanDeviceBuilder::VulkanDeviceBuilder()
{
	OptionalExtension(VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME);
	OptionalExtension(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME);
}

VulkanDeviceBuilder& VulkanDeviceBuilder::RequireExtension(const std::string& extensionName)
{
	requiredDeviceExtensions.insert(extensionName);
	return *this;
}

VulkanDeviceBuilder& VulkanDeviceBuilder::OptionalExtension(const std::string& extensionName)
{
	optionalDeviceExtensions.insert(extensionName);
	return *this;
}

VulkanDeviceBuilder& VulkanDeviceBuilder::OptionalRayQuery()
{
	OptionalExtension(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
	OptionalExtension(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
	OptionalExtension(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
	OptionalExtension(VK_KHR_RAY_QUERY_EXTENSION_NAME);
	return *this;
}

VulkanDeviceBuilder& VulkanDeviceBuilder::OptionalDescriptorIndexing()
{
	OptionalExtension(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);
	return *this;
}

VulkanDeviceBuilder& VulkanDeviceBuilder::Surface(std::shared_ptr<VulkanSurface> surface)
{
	if (surface)
	{
		RequireExtension(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
#if defined(VK_USE_PLATFORM_WIN32_KHR)
		OptionalExtension(VK_EXT_FULL_SCREEN_EXCLUSIVE_EXTENSION_NAME);
#endif
	}
	this->surface = std::move(surface);
	return *this;
}

VulkanDeviceBuilder& VulkanDeviceBuilder::SelectDevice(int index)
{
	deviceIndex = index;
	return *this;
}

std::vector<VulkanCompatibleDevice> VulkanDeviceBuilder::FindDevices(const std::shared_ptr<VulkanInstance>& instance)
{
	std::vector<VulkanCompatibleDevice> supportedDevices;

	for (size_t idx = 0; idx < instance->PhysicalDevices.size(); idx++)
	{
		const auto& info = instance->PhysicalDevices[idx];

		// Check if all required extensions are there
		std::set<std::string> requiredExtensionSearch = requiredDeviceExtensions;
		for (const auto& ext : info.Extensions)
			requiredExtensionSearch.erase(ext.extensionName);
		if (!requiredExtensionSearch.empty())
			continue;

		// Check if all required features are there
		if (info.Features.Features.samplerAnisotropy != VK_TRUE ||
			info.Features.Features.fragmentStoresAndAtomics != VK_TRUE)
			continue;

		VulkanCompatibleDevice dev;
		dev.Device = &instance->PhysicalDevices[idx];
		dev.EnabledDeviceExtensions = requiredDeviceExtensions;

		// Enable optional extensions we are interested in, if they are available on this device
		for (const auto& ext : dev.Device->Extensions)
		{
			if (optionalDeviceExtensions.find(ext.extensionName) != optionalDeviceExtensions.end())
			{
				dev.EnabledDeviceExtensions.insert(ext.extensionName);
			}
		}

		// Enable optional features we are interested in, if they are available on this device
		auto& enabledFeatures = dev.EnabledFeatures;
		auto& deviceFeatures = dev.Device->Features;
		enabledFeatures.Features.samplerAnisotropy = deviceFeatures.Features.samplerAnisotropy;
		enabledFeatures.Features.fragmentStoresAndAtomics = deviceFeatures.Features.fragmentStoresAndAtomics;
		enabledFeatures.Features.depthClamp = deviceFeatures.Features.depthClamp;
		enabledFeatures.Features.shaderClipDistance = deviceFeatures.Features.shaderClipDistance;
		enabledFeatures.BufferDeviceAddress.bufferDeviceAddress = deviceFeatures.BufferDeviceAddress.bufferDeviceAddress;
		enabledFeatures.AccelerationStructure.accelerationStructure = deviceFeatures.AccelerationStructure.accelerationStructure;
		enabledFeatures.RayQuery.rayQuery = deviceFeatures.RayQuery.rayQuery;
		enabledFeatures.DescriptorIndexing.runtimeDescriptorArray = deviceFeatures.DescriptorIndexing.runtimeDescriptorArray;
		enabledFeatures.DescriptorIndexing.descriptorBindingPartiallyBound = deviceFeatures.DescriptorIndexing.descriptorBindingPartiallyBound;
		enabledFeatures.DescriptorIndexing.descriptorBindingSampledImageUpdateAfterBind = deviceFeatures.DescriptorIndexing.descriptorBindingSampledImageUpdateAfterBind;
		enabledFeatures.DescriptorIndexing.descriptorBindingVariableDescriptorCount = deviceFeatures.DescriptorIndexing.descriptorBindingVariableDescriptorCount;

		// Figure out which queue can present
		if (surface)
		{
			for (int i = 0; i < (int)info.QueueFamilies.size(); i++)
			{
				VkBool32 presentSupport = false;
				VkResult result = vkGetPhysicalDeviceSurfaceSupportKHR(info.Device, i, surface->Surface, &presentSupport);
				if (result == VK_SUCCESS && info.QueueFamilies[i].queueCount > 0 && presentSupport)
				{
					dev.PresentFamily = i;
					break;
				}
			}
		}

		// The vulkan spec states that graphics and compute queues can always do transfer.
		// Furthermore the spec states that graphics queues always can do compute.
		// Last, the spec makes it OPTIONAL whether the VK_QUEUE_TRANSFER_BIT is set for such queues, but they MUST support transfer.
		//
		// In short: pick the first graphics queue family for everything.
		for (int i = 0; i < (int)info.QueueFamilies.size(); i++)
		{
			const auto& queueFamily = info.QueueFamilies[i];
			if (queueFamily.queueCount > 0 && (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT))
			{
				dev.GraphicsFamily = i;
				dev.GraphicsTimeQueries = queueFamily.timestampValidBits != 0;
				break;
			}
		}

		// Only use device if we found the required graphics and present queues
		if (dev.GraphicsFamily != -1 && (!surface || dev.PresentFamily != -1))
		{
			supportedDevices.push_back(dev);
		}
	}

	// The device order returned by Vulkan can be anything. Prefer discrete > integrated > virtual gpu > cpu > other
	auto sortFunc = [&](const auto& a, const auto b)
	{
		// Sort by GPU type first. This will ensure the "best" device is most likely to map to vk_device 0
		static const int typeSort[] = { 4, 1, 0, 2, 3 };
		int sortA = a.Device->Properties.Properties.deviceType < 5 ? typeSort[a.Device->Properties.Properties.deviceType] : (int)a.Device->Properties.Properties.deviceType;
		int sortB = b.Device->Properties.Properties.deviceType < 5 ? typeSort[b.Device->Properties.Properties.deviceType] : (int)b.Device->Properties.Properties.deviceType;
		if (sortA != sortB)
			return sortA < sortB;

		// Then sort by the device's unique ID so that vk_device uses a consistent order
		int sortUUID = memcmp(a.Device->Properties.Properties.pipelineCacheUUID, b.Device->Properties.Properties.pipelineCacheUUID, VK_UUID_SIZE);
		return sortUUID < 0;
	};
	std::stable_sort(supportedDevices.begin(), supportedDevices.end(), sortFunc);

	return supportedDevices;
}

std::shared_ptr<VulkanDevice> VulkanDeviceBuilder::Create(std::shared_ptr<VulkanInstance> instance)
{
	if (instance->PhysicalDevices.empty())
		VulkanError("No Vulkan devices found. The graphics card may have no vulkan support or the driver may be too old.");

	std::vector<VulkanCompatibleDevice> supportedDevices = FindDevices(instance);
	if (supportedDevices.empty())
		VulkanError("No Vulkan device found supports the minimum requirements of this application");

	size_t selected = deviceIndex;
	if (selected >= supportedDevices.size())
		selected = 0;
	return std::make_shared<VulkanDevice>(instance, surface, supportedDevices[selected]);
}

/////////////////////////////////////////////////////////////////////////////

VulkanSwapChainBuilder::VulkanSwapChainBuilder()
{
}

std::shared_ptr<VulkanSwapChain> VulkanSwapChainBuilder::Create(VulkanDevice* device)
{
	return std::make_shared<VulkanSwapChain>(device);
}
