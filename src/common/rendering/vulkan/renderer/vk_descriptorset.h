
#pragma once

#include "vulkan/system/vk_objects.h"
#include <list>

class VulkanFrameBuffer;
class VkMaterial;

class VkDescriptorSetManager
{
public:
	VkDescriptorSetManager(VulkanFrameBuffer* fb);
	~VkDescriptorSetManager();

	void Init();
	void UpdateFixedSet();
	void UpdateDynamicSet();
	void ResetHWTextureSets();

	VulkanDescriptorSetLayout* GetDynamicSetLayout() { return DynamicSetLayout.get(); }
	VulkanDescriptorSetLayout* GetFixedSetLayout() { return FixedSetLayout.get(); }
	VulkanDescriptorSetLayout* GetTextureSetLayout(int numLayers);

	VulkanDescriptorSet* GetDynamicDescriptorSet() { return DynamicSet.get(); }
	VulkanDescriptorSet* GetFixedDescriptorSet() { return FixedSet.get(); }
	VulkanDescriptorSet* GetNullTextureDescriptorSet();

	std::unique_ptr<VulkanDescriptorSet> AllocateTextureDescriptorSet(int numLayers);

	VulkanImageView* GetNullTextureView() { return NullTextureView.get(); }

	void AddMaterial(VkMaterial* texture);
	void RemoveMaterial(VkMaterial* texture);

private:
	void CreateDynamicSet();
	void CreateFixedSet();
	void CreateNullTexture();

	VulkanFrameBuffer* fb = nullptr;

	std::unique_ptr<VulkanDescriptorSetLayout> DynamicSetLayout;
	std::unique_ptr<VulkanDescriptorSetLayout> FixedSetLayout;
	std::vector<std::unique_ptr<VulkanDescriptorSetLayout>> TextureSetLayouts;

	std::unique_ptr<VulkanDescriptorPool> DynamicDescriptorPool;
	std::unique_ptr<VulkanDescriptorPool> FixedDescriptorPool;

	int TextureDescriptorSetsLeft = 0;
	int TextureDescriptorsLeft = 0;
	std::vector<std::unique_ptr<VulkanDescriptorPool>> TextureDescriptorPools;

	std::unique_ptr<VulkanDescriptorSet> DynamicSet;
	std::unique_ptr<VulkanDescriptorSet> FixedSet;
	std::unique_ptr<VulkanDescriptorSet> NullTextureDescriptorSet;

	std::unique_ptr<VulkanImage> NullTexture;
	std::unique_ptr<VulkanImageView> NullTextureView;

	std::list<VkMaterial*> Materials;
};
