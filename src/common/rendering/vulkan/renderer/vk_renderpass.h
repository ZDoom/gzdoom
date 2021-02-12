
#pragma once

#include "vulkan/system/vk_objects.h"
#include "renderstyle.h"
#include "hwrenderer/data/buffers.h"
#include "hw_renderstate.h"
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
	VkRenderPassSetup(const VkRenderPassKey &key);

	VulkanRenderPass *GetRenderPass(int clearTargets);
	VulkanPipeline *GetPipeline(const VkPipelineKey &key);

	VkRenderPassKey PassKey;
	std::unique_ptr<VulkanRenderPass> RenderPasses[8];
	std::map<VkPipelineKey, std::unique_ptr<VulkanPipeline>> Pipelines;

private:
	std::unique_ptr<VulkanRenderPass> CreateRenderPass(int clearTargets);
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

	VkVertexFormat *GetVertexFormat(int index);

	std::unique_ptr<VulkanDescriptorSet> AllocateTextureDescriptorSet(int numLayers);
	VulkanPipelineLayout* GetPipelineLayout(int numLayers);

	VulkanDescriptorSet* GetNullTextureDescriptorSet();
	VulkanImageView* GetNullTextureView();

	std::unique_ptr<VulkanDescriptorSetLayout> DynamicSetLayout;
	std::map<VkRenderPassKey, std::unique_ptr<VkRenderPassSetup>> RenderPassSetup;

	std::unique_ptr<VulkanDescriptorSet> DynamicSet;

private:
	void CreateDynamicSetLayout();
	void CreateDescriptorPool();
	void CreateDynamicSet();
	void CreateNullTexture();

	VulkanDescriptorSetLayout *GetTextureSetLayout(int numLayers);

	int TextureDescriptorSetsLeft = 0;
	int TextureDescriptorsLeft = 0;
	std::vector<std::unique_ptr<VulkanDescriptorPool>> TextureDescriptorPools;
	std::unique_ptr<VulkanDescriptorPool> DynamicDescriptorPool;
	std::vector<std::unique_ptr<VulkanDescriptorSetLayout>> TextureSetLayouts;
	std::vector<std::unique_ptr<VulkanPipelineLayout>> PipelineLayouts;
	std::vector<VkVertexFormat> VertexFormats;

	std::unique_ptr<VulkanImage> NullTexture;
	std::unique_ptr<VulkanImageView> NullTextureView;
	std::unique_ptr<VulkanDescriptorSet> NullTextureDescriptorSet;
};
