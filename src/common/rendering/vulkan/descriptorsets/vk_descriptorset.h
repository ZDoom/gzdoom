
#pragma once

#include "zvulkan/vulkanobjects.h"
#include "zvulkan/vulkanbuilders.h"
#include <list>
#include "tarray.h"

class VulkanRenderDevice;
class VkMaterial;
class PPTextureInput;
class VkPPRenderPassSetup;

class VkDescriptorSetManager
{
public:
	VkDescriptorSetManager(VulkanRenderDevice* fb);
	~VkDescriptorSetManager();

	void Init();
	void Deinit();
	void BeginFrame();
	void ResetHWTextureSets();

	VulkanDescriptorSetLayout* GetRSBufferSetLayout() { return RSBufferSetLayout.get(); }
	VulkanDescriptorSetLayout* GetFixedSetLayout() { return FixedSetLayout.get(); }
	VulkanDescriptorSetLayout* GetTextureSetLayout(int numLayers);
	VulkanDescriptorSetLayout* GetBindlessSetLayout() { return BindlessDescriptorSetLayout.get(); }

	VulkanDescriptorSet* GetRSBufferDescriptorSet(int threadIndex) { return RSBufferSets[threadIndex].get(); }
	VulkanDescriptorSet* GetFixedDescriptorSet() { return FixedSet.get(); }
	VulkanDescriptorSet* GetNullTextureDescriptorSet();
	VulkanDescriptorSet* GetBindlessDescriptorSet() { return BindlessDescriptorSet.get(); }

	std::unique_ptr<VulkanDescriptorSet> AllocateTextureDescriptorSet(int numLayers);

	VulkanDescriptorSet* GetInput(VkPPRenderPassSetup* passSetup, const TArray<PPTextureInput>& textures, bool bindShadowMapBuffers);

	void AddMaterial(VkMaterial* texture);
	void RemoveMaterial(VkMaterial* texture);

	void UpdateBindlessDescriptorSet();
	int AddBindlessTextureIndex(VulkanImageView* imageview, VulkanSampler* sampler);

private:
	void CreateRSBufferSetLayout();
	void CreateFixedSetLayout();
	void CreateRSBufferPool();
	void CreateFixedSetPool();
	void CreateBindlessDescriptorSet();
	void UpdateFixedSet();

	std::unique_ptr<VulkanDescriptorSet> AllocatePPDescriptorSet(VulkanDescriptorSetLayout* layout);

	VulkanRenderDevice* fb = nullptr;

	std::unique_ptr<VulkanDescriptorSetLayout> RSBufferSetLayout;
	std::unique_ptr<VulkanDescriptorSetLayout> FixedSetLayout;
	std::vector<std::unique_ptr<VulkanDescriptorSetLayout>> TextureSetLayouts;

	std::unique_ptr<VulkanDescriptorPool> RSBufferDescriptorPool;
	std::unique_ptr<VulkanDescriptorPool> FixedDescriptorPool;

	std::unique_ptr<VulkanDescriptorPool> PPDescriptorPool;

	int TextureDescriptorSetsLeft = 0;
	int TextureDescriptorsLeft = 0;
	std::vector<std::unique_ptr<VulkanDescriptorPool>> TextureDescriptorPools;

	std::vector<std::unique_ptr<VulkanDescriptorSet>> RSBufferSets;
	std::unique_ptr<VulkanDescriptorSet> FixedSet;
	std::unique_ptr<VulkanDescriptorSet> NullTextureDescriptorSet;

	std::list<VkMaterial*> Materials;

	std::unique_ptr<VulkanDescriptorPool> BindlessDescriptorPool;
	std::unique_ptr<VulkanDescriptorSet> BindlessDescriptorSet;
	std::unique_ptr<VulkanDescriptorSetLayout> BindlessDescriptorSetLayout;
	WriteDescriptors WriteBindless;
	int NextBindlessIndex = 0;

	static const int maxSets = 100;
	static const int MaxBindlessTextures = 16536;
};
