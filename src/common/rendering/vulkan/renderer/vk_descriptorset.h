
#pragma once

#include "vulkan/system/vk_objects.h"

class VkDescriptorSetManager
{
public:
	VkDescriptorSetManager();
	~VkDescriptorSetManager();

	void Init();
	void UpdateFixedSet();
	void UpdateDynamicSet();
	void TextureSetPoolReset();
	void FilterModeChanged();

	VulkanDescriptorSetLayout* GetDynamicSetLayout() { return DynamicSetLayout.get(); }
	VulkanDescriptorSetLayout* GetFixedSetLayout() { return FixedSetLayout.get(); }
	VulkanDescriptorSetLayout* GetTextureSetLayout(int numLayers);

	VulkanDescriptorSet* GetDynamicDescriptorSet() { return DynamicSet.get(); }
	VulkanDescriptorSet* GetFixedDescriptorSet() { return FixedSet.get(); }
	VulkanDescriptorSet* GetNullTextureDescriptorSet();

	std::unique_ptr<VulkanDescriptorSet> AllocateTextureDescriptorSet(int numLayers);

	VulkanImageView* GetNullTextureView() { return NullTextureView.get(); }

private:
	void CreateDynamicSet();
	void CreateFixedSet();
	void CreateNullTexture();

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
};
