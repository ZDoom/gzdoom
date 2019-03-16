
#pragma once

#include "vulkan/system/vk_objects.h"
#include "r_data/renderstyle.h"
#include "hwrenderer/data/buffers.h"
#include "hwrenderer/scene/hw_renderstate.h"
#include <string.h>
#include <map>

class VKDataBuffer;

class VkRenderPassKey
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

	bool UsesDepthStencil() const { return DepthTest || DepthWrite || StencilTest || (ClearTargets & (CT_Depth | CT_Stencil)); }

	bool operator<(const VkRenderPassKey &other) const { return memcmp(this, &other, sizeof(VkRenderPassKey)) < 0; }
	bool operator==(const VkRenderPassKey &other) const { return memcmp(this, &other, sizeof(VkRenderPassKey)) == 0; }
	bool operator!=(const VkRenderPassKey &other) const { return memcmp(this, &other, sizeof(VkRenderPassKey)) != 0; }
};

class VkRenderPassSetup
{
public:
	VkRenderPassSetup(const VkRenderPassKey &key);

	std::unique_ptr<VulkanRenderPass> RenderPass;
	std::unique_ptr<VulkanPipeline> Pipeline;
	std::map<VkImageView, std::unique_ptr<VulkanFramebuffer>> Framebuffer;

private:
	void CreatePipeline(const VkRenderPassKey &key);
	void CreateRenderPass(const VkRenderPassKey &key);
};

class VkVertexFormat
{
public:
	int NumBindingPoints;
	size_t Stride;
	std::vector<FVertexBufferAttribute> Attrs;
	bool UseVertexData;
};

class VkRenderPassManager
{
public:
	VkRenderPassManager();

	void Init();
	void RenderBuffersReset();
	void UpdateDynamicSet();

	VkRenderPassSetup *GetRenderPass(const VkRenderPassKey &key);
	int GetVertexFormat(int numBindingPoints, int numAttributes, size_t stride, const FVertexBufferAttribute *attrs);

	std::unique_ptr<VulkanDescriptorSetLayout> DynamicSetLayout;
	std::unique_ptr<VulkanDescriptorSetLayout> TextureSetLayout;
	std::unique_ptr<VulkanPipelineLayout> PipelineLayout;
	std::unique_ptr<VulkanDescriptorPool> DescriptorPool;
	std::map<VkRenderPassKey, std::unique_ptr<VkRenderPassSetup>> RenderPassSetup;

	std::unique_ptr<VulkanDescriptorSet> DynamicSet;

	std::vector<VkVertexFormat> VertexFormats;

private:
	void CreateDynamicSetLayout();
	void CreateTextureSetLayout();
	void CreatePipelineLayout();
	void CreateDescriptorPool();
	void CreateDynamicSet();

};
