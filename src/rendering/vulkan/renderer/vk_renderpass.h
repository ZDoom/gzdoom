
#pragma once

#include "vulkan/system/vk_objects.h"
#include "r_data/renderstyle.h"
#include "hwrenderer/data/buffers.h"
#include "hwrenderer/scene/hw_renderstate.h"
#include <string.h>
#include <map>

class VKDataBuffer;

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
	int Samples;
	int ClearTargets;
	int DrawBuffers;
	int NumTextureLayers;
	VkFormat DrawBufferFormat;

	bool UsesDepthStencil() const { return DepthTest || DepthWrite || StencilTest || (ClearTargets & (CT_Depth | CT_Stencil)); }

	bool operator<(const VkPipelineKey &other) const { return memcmp(this, &other, sizeof(VkPipelineKey)) < 0; }
	bool operator==(const VkPipelineKey &other) const { return memcmp(this, &other, sizeof(VkPipelineKey)) == 0; }
	bool operator!=(const VkPipelineKey &other) const { return memcmp(this, &other, sizeof(VkPipelineKey)) != 0; }
};

class VkRenderPassKey
{
public:
	VkRenderPassKey() = default;
	VkRenderPassKey(const VkPipelineKey &key)
	{
		DepthWrite = key.DepthWrite;
		DepthTest = key.DepthTest;
		StencilTest = key.StencilTest;
		Samples = key.Samples;
		ClearTargets = key.ClearTargets;
		DrawBuffers = key.DrawBuffers;
		DrawBufferFormat = key.DrawBufferFormat;
	}

	int DepthWrite;
	int DepthTest;
	int StencilTest;
	int Samples;
	int ClearTargets;
	int DrawBuffers;
	VkFormat DrawBufferFormat;

	bool UsesDepthStencil() const { return DepthTest || DepthWrite || StencilTest || (ClearTargets & (CT_Depth | CT_Stencil)); }

	bool operator<(const VkRenderPassKey &other) const { return memcmp(this, &other, sizeof(VkRenderPassKey)) < 0; }
	bool operator==(const VkRenderPassKey &other) const { return memcmp(this, &other, sizeof(VkRenderPassKey)) == 0; }
	bool operator!=(const VkRenderPassKey &other) const { return memcmp(this, &other, sizeof(VkRenderPassKey)) != 0; }
};

class VkRenderPassSetup
{
public:
	VkRenderPassSetup(const VkRenderPassKey &key);

	VulkanPipeline *GetPipeline(const VkPipelineKey &key);

	std::unique_ptr<VulkanRenderPass> RenderPass;
	std::map<VkPipelineKey, std::unique_ptr<VulkanPipeline>> Pipelines;
	std::map<VkImageView, std::unique_ptr<VulkanFramebuffer>> Framebuffer;

private:
	void CreateRenderPass(const VkRenderPassKey &key);
	std::unique_ptr<VulkanPipeline> CreatePipeline(const VkPipelineKey &key);
};

class VkVertexFormat
{
public:
	int NumBindingPoints;
	size_t Stride;
	std::vector<FVertexBufferAttribute> Attrs;
	int UseVertexData;
};

class VkRenderPassManager
{
public:
	VkRenderPassManager();
	~VkRenderPassManager();

	void Init();
	void RenderBuffersReset();
	void UpdateDynamicSet();
	void TextureSetPoolReset();

	VkRenderPassSetup *GetRenderPass(const VkRenderPassKey &key);
	int GetVertexFormat(int numBindingPoints, int numAttributes, size_t stride, const FVertexBufferAttribute *attrs);

	std::unique_ptr<VulkanDescriptorSet> AllocateTextureDescriptorSet(int numLayers);
	VulkanPipelineLayout* GetPipelineLayout(int numLayers);

	std::unique_ptr<VulkanDescriptorSetLayout> DynamicSetLayout;
	std::map<VkRenderPassKey, std::unique_ptr<VkRenderPassSetup>> RenderPassSetup;

	std::unique_ptr<VulkanDescriptorSet> DynamicSet;

	std::vector<VkVertexFormat> VertexFormats;

private:
	void CreateDynamicSetLayout();
	void CreateDescriptorPool();
	void CreateDynamicSet();

	VulkanDescriptorSetLayout *GetTextureSetLayout(int numLayers);

	int TextureDescriptorSetsLeft = 0;
	int TextureDescriptorsLeft = 0;
	std::vector<std::unique_ptr<VulkanDescriptorPool>> TextureDescriptorPools;
	std::unique_ptr<VulkanDescriptorPool> DynamicDescriptorPool;
	std::vector<std::unique_ptr<VulkanDescriptorSetLayout>> TextureSetLayouts;
	std::vector<std::unique_ptr<VulkanPipelineLayout>> PipelineLayouts;
};
