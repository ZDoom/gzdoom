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

	WriteDescriptors()
		.AddBuffer(HWBufferSet.get(), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, fb->GetBufferManager()->ViewpointUBO->mBuffer.get(), 0, sizeof(HWViewpointUniforms))
		.AddBuffer(HWBufferSet.get(), 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, fb->GetBufferManager()->MatrixBuffer->UniformBuffer->mBuffer.get(), 0, sizeof(MatricesUBO))
		.AddBuffer(HWBufferSet.get(), 2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, fb->GetBufferManager()->StreamBuffer->UniformBuffer->mBuffer.get(), 0, sizeof(StreamUBO))
		.AddBuffer(HWBufferSet.get(), 3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, fb->GetBufferManager()->LightBufferSSO->mBuffer.get())
		.Execute(fb->device);
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
	update.AddCombinedImageSampler(FixedSet.get(), 0, fb->GetTextureManager()->Shadowmap.View.get(), fb->GetSamplerManager()->ShadowmapSampler.get(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	update.AddCombinedImageSampler(FixedSet.get(), 1, fb->GetTextureManager()->Lightmap.View.get(), fb->GetSamplerManager()->LightmapSampler.get(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	if (fb->RaytracingEnabled())
		update.AddAccelerationStructure(FixedSet.get(), 2, fb->GetRaytrace()->GetAccelStruct());
	update.Execute(fb->device);
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
			update.AddCombinedImageSampler(NullTextureDescriptorSet.get(), i, fb->GetTextureManager()->GetNullTextureView(), fb->GetSamplerManager()->Get(CLAMP_XY_NOMIP), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		}
		update.Execute(fb->device);
	}

	return NullTextureDescriptorSet.get();
}

std::unique_ptr<VulkanDescriptorSet> VkDescriptorSetManager::AllocateTextureDescriptorSet(int numLayers)
{
	if (TextureDescriptorSetsLeft == 0 || TextureDescriptorsLeft < numLayers)
	{
		TextureDescriptorSetsLeft = 1000;
		TextureDescriptorsLeft = 2000;

		TextureDescriptorPools.push_back(DescriptorPoolBuilder()
			.AddPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, TextureDescriptorsLeft)
			.MaxSets(TextureDescriptorSetsLeft)
			.DebugName("VkDescriptorSetManager.TextureDescriptorPool")
			.Create(fb->device));
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
		builder.AddBinding(i, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
	}
	builder.DebugName("VkDescriptorSetManager.TextureSetLayout");
	layout = builder.Create(fb->device);
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

		write.AddCombinedImageSampler(descriptors.get(), index, tex->DepthOnlyView ? tex->DepthOnlyView.get() : tex->View.get(), sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		imageTransition.AddImage(tex, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, false);
	}

	if (bindShadowMapBuffers)
	{
		write.AddBuffer(descriptors.get(), LIGHTNODES_BINDINGPOINT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, fb->GetBufferManager()->LightNodes->mBuffer.get());
		write.AddBuffer(descriptors.get(), LIGHTLINES_BINDINGPOINT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, fb->GetBufferManager()->LightLines->mBuffer.get());
		write.AddBuffer(descriptors.get(), LIGHTLIST_BINDINGPOINT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, fb->GetBufferManager()->LightList->mBuffer.get());
	}

	write.Execute(fb->device);
	imageTransition.Execute(fb->GetCommands()->GetDrawCommands());

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

	PPDescriptorPool = DescriptorPoolBuilder()
		.AddPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 200)
		.AddPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4)
		.MaxSets(100)
		.DebugName("PPDescriptorPool")
		.Create(fb->device);

	return PPDescriptorPool->allocate(layout);
}

void VkDescriptorSetManager::CreateHWBufferSetLayout()
{
	HWBufferSetLayout = DescriptorSetLayoutBuilder()
		.AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
		.AddBinding(1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
		.AddBinding(2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
		.AddBinding(3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT)
		.DebugName("VkDescriptorSetManager.HWBufferSetLayout")
		.Create(fb->device);
}

void VkDescriptorSetManager::CreateFixedSetLayout()
{
	DescriptorSetLayoutBuilder builder;
	builder.AddBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
	builder.AddBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
	if (fb->RaytracingEnabled())
		builder.AddBinding(2, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
	builder.DebugName("VkDescriptorSetManager.FixedSetLayout");
	FixedSetLayout = builder.Create(fb->device);
}

void VkDescriptorSetManager::CreateHWBufferPool()
{
	HWBufferDescriptorPool = DescriptorPoolBuilder()
		.AddPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 3 * maxSets)
		.AddPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 * maxSets)
		.MaxSets(maxSets)
		.DebugName("VkDescriptorSetManager.HWBufferDescriptorPool")
		.Create(fb->device);
}

void VkDescriptorSetManager::CreateFixedSetPool()
{
	DescriptorPoolBuilder poolbuilder;
	poolbuilder.AddPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2 * maxSets);
	if (fb->RaytracingEnabled())
		poolbuilder.AddPoolSize(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1 * maxSets);
	poolbuilder.MaxSets(maxSets);
	poolbuilder.DebugName("VkDescriptorSetManager.FixedDescriptorPool");
	FixedDescriptorPool = poolbuilder.Create(fb->device);
}
