
#pragma once

#include "zvulkan/vulkanobjects.h"
#include "renderstyle.h"
#include "hwrenderer/data/buffers.h"
#include "hwrenderer/postprocessing/hw_postprocess.h"
#include "hw_renderstate.h"
#include <string.h>
#include <map>

class VulkanRenderDevice;
class VkPPShader;
class GraphicsPipelineBuilder;
class ColorBlendAttachmentBuilder;

class VkPipelineKey
{
public:
	FRenderStyle RenderStyle;
	int SpecialEffect;
	int EffectState;
	int AlphaTest;
	int DepthWrite;
	int DepthTest;
	int DepthFunc;
	int DepthClamp;
	int DepthBias;
	int StencilTest;
	int StencilPassOp;
	int ColorMask;
	int CullMode;
	int VertexFormat;
	int DrawType;
	int NumTextureLayers;

	bool operator<(const VkPipelineKey &other) const { return memcmp(this, &other, sizeof(VkPipelineKey)) < 0; }
	bool operator==(const VkPipelineKey &other) const { return memcmp(this, &other, sizeof(VkPipelineKey)) == 0; }
	bool operator!=(const VkPipelineKey &other) const { return memcmp(this, &other, sizeof(VkPipelineKey)) != 0; }
};

class VkRenderPassKey
{
public:
	int DepthStencil;
	int Samples;
	int DrawBuffers;
	VkFormat DrawBufferFormat;

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

enum class WhichDepthStencil;

class VkPPRenderPassKey
{
public:
	VkPPShader* Shader;
	int Uniforms;
	int InputTextures;
	PPBlendMode BlendMode;
	VkFormat OutputFormat;
	int SwapChain;
	int ShadowMapBuffers;
	WhichDepthStencil StencilTest;
	VkSampleCountFlagBits Samples;

	bool operator<(const VkPPRenderPassKey& other) const { return memcmp(this, &other, sizeof(VkPPRenderPassKey)) < 0; }
	bool operator==(const VkPPRenderPassKey& other) const { return memcmp(this, &other, sizeof(VkPPRenderPassKey)) == 0; }
	bool operator!=(const VkPPRenderPassKey& other) const { return memcmp(this, &other, sizeof(VkPPRenderPassKey)) != 0; }
};

class VkPPRenderPassSetup
{
public:
	VkPPRenderPassSetup(VulkanRenderDevice* fb, const VkPPRenderPassKey& key);

	std::unique_ptr<VulkanDescriptorSetLayout> DescriptorLayout;
	std::unique_ptr<VulkanPipelineLayout> PipelineLayout;
	std::unique_ptr<VulkanRenderPass> RenderPass;
	std::unique_ptr<VulkanPipeline> Pipeline;

private:
	void CreateDescriptorLayout(const VkPPRenderPassKey& key);
	void CreatePipelineLayout(const VkPPRenderPassKey& key);
	void CreatePipeline(const VkPPRenderPassKey& key);
	void CreateRenderPass(const VkPPRenderPassKey& key);

	VulkanRenderDevice* fb = nullptr;
};

ColorBlendAttachmentBuilder& BlendMode(ColorBlendAttachmentBuilder& builder, const FRenderStyle& style);

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
