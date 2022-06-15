
#pragma once

#include "vulkan/system/vk_objects.h"
#include <list>

class VulkanFrameBuffer;
class VkMaterial;
class PPTextureInput;
class VkPPRenderPassSetup;

class VkDescriptorSetManager
{
public:
	VkDescriptorSetManager(VulkanFrameBuffer* fb);
	~VkDescriptorSetManager();

	void Init();
	void Deinit();
	void BeginFrame();
	void UpdateFixedSet();
	void UpdateHWBufferSet();
	void ResetHWTextureSets();

	VulkanDescriptorSetLayout* GetHWBufferSetLayout() { return HWBufferSetLayout.get(); }
	VulkanDescriptorSetLayout* GetFixedSetLayout() { return FixedSetLayout.get(); }
	VulkanDescriptorSetLayout* GetTextureSetLayout(int numLayers);

	VulkanDescriptorSet* GetHWBufferDescriptorSet() { return HWBufferSet.get(); }
	VulkanDescriptorSet* GetFixedDescriptorSet() { return FixedSet.get(); }
	VulkanDescriptorSet* GetNullTextureDescriptorSet();

	std::unique_ptr<VulkanDescriptorSet> AllocateTextureDescriptorSet(int numLayers);

	VulkanDescriptorSet* GetInput(VkPPRenderPassSetup* passSetup, const TArray<PPTextureInput>& textures, bool bindShadowMapBuffers);

	void AddMaterial(VkMaterial* texture);
	void RemoveMaterial(VkMaterial* texture);

private:
	void CreateHWBufferSetLayout();
	void CreateFixedSetLayout();
	void CreateHWBufferPool();
	void CreateFixedSetPool();

	std::unique_ptr<VulkanDescriptorSet> AllocatePPDescriptorSet(VulkanDescriptorSetLayout* layout);

	VulkanFrameBuffer* fb = nullptr;

	std::unique_ptr<VulkanDescriptorSetLayout> HWBufferSetLayout;
	std::unique_ptr<VulkanDescriptorSetLayout> FixedSetLayout;
	std::vector<std::unique_ptr<VulkanDescriptorSetLayout>> TextureSetLayouts;

	std::unique_ptr<VulkanDescriptorPool> HWBufferDescriptorPool;
	std::unique_ptr<VulkanDescriptorPool> FixedDescriptorPool;

	std::unique_ptr<VulkanDescriptorPool> PPDescriptorPool;

	int TextureDescriptorSetsLeft = 0;
	int TextureDescriptorsLeft = 0;
	std::vector<std::unique_ptr<VulkanDescriptorPool>> TextureDescriptorPools;

	std::unique_ptr<VulkanDescriptorSet> HWBufferSet;
	std::unique_ptr<VulkanDescriptorSet> FixedSet;
	std::unique_ptr<VulkanDescriptorSet> NullTextureDescriptorSet;

	std::list<VkMaterial*> Materials;

	static const int maxSets = 10;
};
