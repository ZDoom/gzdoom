/*
**  Vulkan backend
**  Copyright (c) 2016-2020 Magnus Norddahl
**
**  This software is provided 'as-is', without any express or implied
**  warranty.  In no event will the authors be held liable for any damages
**  arising from the use of this software.
**
**  Permission is granted to anyone to use this software for any purpose,
**  including commercial applications, and to alter it and redistribute it
**  freely, subject to the following restrictions:
**
**  1. The origin of this software must not be misrepresented; you must not
**     claim that you wrote the original software. If you use this software
**     in a product, an acknowledgment in the product documentation would be
**     appreciated but is not required.
**  2. Altered source versions must be plainly marked as such, and must not be
**     misrepresented as being the original software.
**  3. This notice may not be removed or altered from any source distribution.
**
*/

#include "vk_descriptorset.h"
#include "vk_streambuffer.h"
#include "vk_raytrace.h"
#include "vulkan/shaders/vk_shader.h"
#include "vulkan/textures/vk_samplers.h"
#include "vulkan/textures/vk_renderbuffers.h"
#include "vulkan/textures/vk_hwtexture.h"
#include "vulkan/textures/vk_texture.h"
#include "vulkan/system/vk_builders.h"
#include "vulkan/system/vk_framebuffer.h"
#include "vulkan/system/vk_hwbuffer.h"
#include "vulkan/system/vk_commandbuffer.h"
#include "vulkan/system/vk_buffer.h"
#include "flatvertices.h"
#include "hw_viewpointuniforms.h"
#include "v_2ddrawer.h"

VkDescriptorSetManager::VkDescriptorSetManager(VulkanFrameBuffer* fb) : fb(fb)
{
	CreateHWBufferSetLayout();
	CreateFixedSetLayout();
	CreateHWBufferPool();
	CreateFixedSetPool();
}

VkDescriptorSetManager::~VkDescriptorSetManager()
{
	while (!Materials.empty())
		RemoveMaterial(Materials.back());
}

void VkDescriptorSetManager::Init()
{
	UpdateFixedSet();
	UpdateHWBufferSet();
}

void VkDescriptorSetManager::Deinit()
{
	while (!Materials.empty())
		RemoveMaterial(Materials.back());
}

void VkDescriptorSetManager::BeginFrame()
{
	UpdateFixedSet();
	UpdateHWBufferSet();
}

void VkDescriptorSetManager::UpdateHWBufferSet()
{
	fb->GetCommands()->DrawDeleteList->Add(std::move(HWBufferSet));

	HWBufferSet = HWBufferDescriptorPool->tryAllocate(HWBufferSetLayout.get());
	if (!HWBufferSet)
	{
		fb->GetCommands()->WaitForCommands(false);
		HWBufferSet = HWBufferDescriptorPool->allocate(HWBufferSetLayout.get());
	}

	WriteDescriptors update;
	update.addBuffer(HWBufferSet.get(), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, fb->GetBufferManager()->ViewpointUBO->mBuffer.get(), 0, sizeof(HWViewpointUniforms));
	update.addBuffer(HWBufferSet.get(), 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, fb->GetBufferManager()->MatrixBuffer->UniformBuffer->mBuffer.get(), 0, sizeof(MatricesUBO));
	update.addBuffer(HWBufferSet.get(), 2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, fb->GetBufferManager()->StreamBuffer->UniformBuffer->mBuffer.get(), 0, sizeof(StreamUBO));
	update.addBuffer(HWBufferSet.get(), 3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, fb->GetBufferManager()->LightBufferSSO->mBuffer.get());
	update.updateSets(fb->device);
}

void VkDescriptorSetManager::UpdateFixedSet()
{
	fb->GetCommands()->DrawDeleteList->Add(std::move(FixedSet));

	FixedSet = FixedDescriptorPool->tryAllocate(FixedSetLayout.get());
	if (!FixedSet)
	{
		fb->GetCommands()->WaitForCommands(false);
		FixedSet = FixedDescriptorPool->allocate(FixedSetLayout.get());
	}

	WriteDescriptors update;
	update.addCombinedImageSampler(FixedSet.get(), 0, fb->GetTextureManager()->Shadowmap.View.get(), fb->GetSamplerManager()->ShadowmapSampler.get(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	update.addCombinedImageSampler(FixedSet.get(), 1, fb->GetTextureManager()->Lightmap.View.get(), fb->GetSamplerManager()->LightmapSampler.get(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	if (fb->device->SupportsDeviceExtension(VK_KHR_RAY_QUERY_EXTENSION_NAME))
		update.addAccelerationStructure(FixedSet.get(), 2, fb->GetRaytrace()->GetAccelStruct());
	update.updateSets(fb->device);
}

void VkDescriptorSetManager::ResetHWTextureSets()
{
	for (auto mat : Materials)
		mat->DeleteDescriptors();

	auto deleteList = fb->GetCommands()->DrawDeleteList.get();
	for (auto& desc : TextureDescriptorPools)
	{
		deleteList->Add(std::move(desc));
	}
	deleteList->Add(std::move(NullTextureDescriptorSet));

	TextureDescriptorPools.clear();
	TextureDescriptorSetsLeft = 0;
	TextureDescriptorsLeft = 0;
}

VulkanDescriptorSet* VkDescriptorSetManager::GetNullTextureDescriptorSet()
{
	if (!NullTextureDescriptorSet)
	{
		NullTextureDescriptorSet = AllocateTextureDescriptorSet(SHADER_MIN_REQUIRED_TEXTURE_LAYERS);

		WriteDescriptors update;
		for (int i = 0; i < SHADER_MIN_REQUIRED_TEXTURE_LAYERS; i++)
		{
			update.addCombinedImageSampler(NullTextureDescriptorSet.get(), i, fb->GetTextureManager()->GetNullTextureView(), fb->GetSamplerManager()->Get(CLAMP_XY_NOMIP), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		}
		update.updateSets(fb->device);
	}

	return NullTextureDescriptorSet.get();
}

std::unique_ptr<VulkanDescriptorSet> VkDescriptorSetManager::AllocateTextureDescriptorSet(int numLayers)
{
	if (TextureDescriptorSetsLeft == 0 || TextureDescriptorsLeft < numLayers)
	{
		TextureDescriptorSetsLeft = 1000;
		TextureDescriptorsLeft = 2000;

		DescriptorPoolBuilder builder;
		builder.addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, TextureDescriptorsLeft);
		builder.setMaxSets(TextureDescriptorSetsLeft);
		TextureDescriptorPools.push_back(builder.create(fb->device));
		TextureDescriptorPools.back()->SetDebugName("VkDescriptorSetManager.TextureDescriptorPool");
	}

	TextureDescriptorSetsLeft--;
	TextureDescriptorsLeft -= numLayers;
	return TextureDescriptorPools.back()->allocate(GetTextureSetLayout(numLayers));
}

VulkanDescriptorSetLayout* VkDescriptorSetManager::GetTextureSetLayout(int numLayers)
{
	if (TextureSetLayouts.size() < (size_t)numLayers)
		TextureSetLayouts.resize(numLayers);

	auto& layout = TextureSetLayouts[numLayers - 1];
	if (layout)
		return layout.get();

	DescriptorSetLayoutBuilder builder;
	for (int i = 0; i < numLayers; i++)
	{
		builder.addBinding(i, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
	}
	layout = builder.create(fb->device);
	layout->SetDebugName("VkDescriptorSetManager.TextureSetLayout");
	return layout.get();
}

void VkDescriptorSetManager::AddMaterial(VkMaterial* texture)
{
	texture->it = Materials.insert(Materials.end(), texture);
}

void VkDescriptorSetManager::RemoveMaterial(VkMaterial* texture)
{
	texture->DeleteDescriptors();
	texture->fb = nullptr;
	Materials.erase(texture->it);
}

VulkanDescriptorSet* VkDescriptorSetManager::GetInput(VkPPRenderPassSetup* passSetup, const TArray<PPTextureInput>& textures, bool bindShadowMapBuffers)
{
	auto descriptors = AllocatePPDescriptorSet(passSetup->DescriptorLayout.get());
	descriptors->SetDebugName("VkPostprocess.descriptors");

	WriteDescriptors write;
	VkImageTransition imageTransition;

	for (unsigned int index = 0; index < textures.Size(); index++)
	{
		const PPTextureInput& input = textures[index];
		VulkanSampler* sampler = fb->GetSamplerManager()->Get(input.Filter, input.Wrap);
		VkTextureImage* tex = fb->GetTextureManager()->GetTexture(input.Type, input.Texture);

		write.addCombinedImageSampler(descriptors.get(), index, tex->DepthOnlyView ? tex->DepthOnlyView.get() : tex->View.get(), sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		imageTransition.addImage(tex, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, false);
	}

	if (bindShadowMapBuffers)
	{
		write.addBuffer(descriptors.get(), LIGHTNODES_BINDINGPOINT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, fb->GetBufferManager()->LightNodes->mBuffer.get());
		write.addBuffer(descriptors.get(), LIGHTLINES_BINDINGPOINT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, fb->GetBufferManager()->LightLines->mBuffer.get());
		write.addBuffer(descriptors.get(), LIGHTLIST_BINDINGPOINT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, fb->GetBufferManager()->LightList->mBuffer.get());
	}

	write.updateSets(fb->device);
	imageTransition.execute(fb->GetCommands()->GetDrawCommands());

	VulkanDescriptorSet* set = descriptors.get();
	fb->GetCommands()->DrawDeleteList->Add(std::move(descriptors));
	return set;
}

std::unique_ptr<VulkanDescriptorSet> VkDescriptorSetManager::AllocatePPDescriptorSet(VulkanDescriptorSetLayout* layout)
{
	if (PPDescriptorPool)
	{
		auto descriptors = PPDescriptorPool->tryAllocate(layout);
		if (descriptors)
			return descriptors;

		fb->GetCommands()->DrawDeleteList->Add(std::move(PPDescriptorPool));
	}

	DescriptorPoolBuilder builder;
	builder.addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 200);
	builder.addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4);
	builder.setMaxSets(100);
	PPDescriptorPool = builder.create(fb->device);
	PPDescriptorPool->SetDebugName("PPDescriptorPool");

	return PPDescriptorPool->allocate(layout);
}

void VkDescriptorSetManager::CreateHWBufferSetLayout()
{
	DescriptorSetLayoutBuilder builder;
	builder.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
	builder.addBinding(1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
	builder.addBinding(2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
	builder.addBinding(3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
	HWBufferSetLayout = builder.create(fb->device);
	HWBufferSetLayout->SetDebugName("VkDescriptorSetManager.HWBufferSetLayout");
}

void VkDescriptorSetManager::CreateFixedSetLayout()
{
	DescriptorSetLayoutBuilder builder;
	builder.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
	builder.addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
	if (fb->device->SupportsDeviceExtension(VK_KHR_RAY_QUERY_EXTENSION_NAME))
		builder.addBinding(2, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
	FixedSetLayout = builder.create(fb->device);
	FixedSetLayout->SetDebugName("VkDescriptorSetManager.FixedSetLayout");
}

void VkDescriptorSetManager::CreateHWBufferPool()
{
	DescriptorPoolBuilder poolbuilder;
	poolbuilder.addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 3 * maxSets);
	poolbuilder.addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 * maxSets);
	poolbuilder.setMaxSets(maxSets);
	HWBufferDescriptorPool = poolbuilder.create(fb->device);
	HWBufferDescriptorPool->SetDebugName("VkDescriptorSetManager.HWBufferDescriptorPool");
}

void VkDescriptorSetManager::CreateFixedSetPool()
{
	DescriptorPoolBuilder poolbuilder;
	poolbuilder.addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2 * maxSets);
	if (fb->device->SupportsDeviceExtension(VK_KHR_RAY_QUERY_EXTENSION_NAME))
		poolbuilder.addPoolSize(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1 * maxSets);
	poolbuilder.setMaxSets(maxSets);
	FixedDescriptorPool = poolbuilder.create(fb->device);
	FixedDescriptorPool->SetDebugName("VkDescriptorSetManager.FixedDescriptorPool");
}
