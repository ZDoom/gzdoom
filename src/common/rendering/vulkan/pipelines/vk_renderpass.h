
#pragma once

#include "zvulkan/vulkanobjects.h"
#include "renderstyle.h"
#include "hwrenderer/data/buffers.h"
#include "hwrenderer/postprocessing/hw_postprocess.h"
#include "hw_renderstate.h"
#include "common/rendering/vulkan/shaders/vk_shader.h"
#include <string.h>
#include <map>

class VulkanRenderDevice;
class GraphicsPipelineBuilder;
class VkPPShader;
class VkPPRenderPassKey;
class VkPPRenderPassSetup;

class VkPipelineKey
{
public:
	union
	{
		struct
		{
			uint64_t DrawType : 3;
			uint64_t CullMode : 2;
			uint64_t ColorMask : 4;
			uint64_t DepthWrite : 1;
			uint64_t DepthTest : 1;
			uint64_t DepthClamp : 1;
			uint64_t DepthBias : 1;
			uint64_t DepthFunc : 2;
			uint64_t StencilTest : 1;
			uint64_t StencilPassOp : 2;
			uint64_t Unused : 46;
		};
		uint64_t AsQWORD = 0;
	};

	int VertexFormat = 0;
	int NumTextureLayers = 0;

	VkShaderKey ShaderKey;
	FRenderStyle RenderStyle;

	int Padding = 0; // for 64 bit alignment

	bool operator<(const VkPipelineKey &other) const { return memcmp(this, &other, sizeof(VkPipelineKey)) < 0; }
	bool operator==(const VkPipelineKey &other) const { return memcmp(this, &other, sizeof(VkPipelineKey)) == 0; }
	bool operator!=(const VkPipelineKey &other) const { return memcmp(this, &other, sizeof(VkPipelineKey)) != 0; }
};

static_assert(sizeof(FRenderStyle) == 4, "sizeof(FRenderStyle) is not its expected size!");
static_assert(sizeof(VkShaderKey) == 16, "sizeof(VkShaderKey) is not its expected size!");
static_assert(sizeof(VkPipelineKey) == 16 + 16 + 8, "sizeof(VkPipelineKey) is not its expected size!"); // If this assert fails, the flags union no longer adds up to 64 bits. Or there are gaps in the class so the memcmp doesn't work.

class VkRenderPassKey
{
public:
	int DepthStencil = 0;
	int Samples = 0;
	int DrawBuffers = 0;
	VkFormat DrawBufferFormat = VK_FORMAT_UNDEFINED;

	bool operator<(const VkRenderPassKey &other) const { return memcmp(this, &other, sizeof(VkRenderPassKey)) < 0; }
	bool operator==(const VkRenderPassKey &other) const { return memcmp(this, &other, sizeof(VkRenderPassKey)) == 0; }
	bool operator!=(const VkRenderPassKey &other) const { return memcmp(this, &other, sizeof(VkRenderPassKey)) != 0; }
};

class VkRenderPassSetup
{
public:
	VkRenderPassSetup(VulkanRenderDevice* fb, const VkRenderPassKey &key);

	VulkanRenderPass *GetRenderPass(int clearTargets);
	VulkanPipeline *GetPipeline(const VkPipelineKey &key);

	VkRenderPassKey PassKey;
	std::unique_ptr<VulkanRenderPass> RenderPasses[8];
	std::map<VkPipelineKey, std::unique_ptr<VulkanPipeline>> Pipelines;

private:
	std::unique_ptr<VulkanRenderPass> CreateRenderPass(int clearTargets);
	std::unique_ptr<VulkanPipeline> CreatePipeline(const VkPipelineKey &key);

	VulkanRenderDevice* fb = nullptr;
};

class VkVertexFormat
{
public:
	int NumBindingPoints;
	size_t Stride;
	std::vector<FVertexBufferAttribute> Attrs;
	int UseVertexData;
};

GraphicsPipelineBuilder& BlendMode(GraphicsPipelineBuilder& builder, const FRenderStyle& style);

class VkRenderPassManager
{
public:
	VkRenderPassManager(VulkanRenderDevice* fb);
	~VkRenderPassManager();

	void RenderBuffersReset();

	VkRenderPassSetup *GetRenderPass(const VkRenderPassKey &key);
	int GetVertexFormat(int numBindingPoints, int numAttributes, size_t stride, const FVertexBufferAttribute *attrs);
	VkVertexFormat *GetVertexFormat(int index);
	VulkanPipelineLayout* GetPipelineLayout(int numLayers);

	VkPPRenderPassSetup* GetPPRenderPass(const VkPPRenderPassKey& key);

	VulkanPipelineCache* GetCache() { return PipelineCache.get(); }

private:
	VulkanRenderDevice* fb = nullptr;

	std::map<VkRenderPassKey, std::unique_ptr<VkRenderPassSetup>> RenderPassSetup;
	std::vector<std::unique_ptr<VulkanPipelineLayout>> PipelineLayouts;
	std::vector<VkVertexFormat> VertexFormats;

	std::map<VkPPRenderPassKey, std::unique_ptr<VkPPRenderPassSetup>> PPRenderPassSetup;

	FString CacheFilename;
	std::unique_ptr<VulkanPipelineCache> PipelineCache;
};
