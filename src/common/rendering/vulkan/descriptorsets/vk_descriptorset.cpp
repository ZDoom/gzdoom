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
#include "vulkan/vk_renderdevice.h"
#include "vulkan/accelstructs/vk_raytrace.h"
#include "vulkan/shaders/vk_shader.h"
#include "vulkan/samplers/vk_samplers.h"
#include "vulkan/textures/vk_renderbuffers.h"
#include "vulkan/textures/vk_hwtexture.h"
#include "vulkan/textures/vk_texture.h"
#include "vulkan/buffers/vk_hwbuffer.h"
#include "vulkan/buffers/vk_buffer.h"
#include "vulkan/buffers/vk_rsbuffers.h"
#include "vulkan/commands/vk_commandbuffer.h"
#include "vulkan/pipelines/vk_pprenderpass.h"
#include <zvulkan/vulkanbuilders.h>
#include "flatvertices.h"
#include "hw_viewpointuniforms.h"
#include "v_2ddrawer.h"

VkDescriptorSetManager::VkDescriptorSetManager(VulkanRenderDevice* fb) : fb(fb)
{
	CreateRSBufferSetLayout();
	CreateFixedSetLayout();
	CreateRSBufferPool();
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

	for (int threadIndex = 0; threadIndex < fb->MaxThreads; threadIndex++)
	{
		auto rsbuffers = fb->GetBufferManager()->GetRSBuffers(threadIndex);
		auto rsbufferset = RSBufferDescriptorPool->allocate(RSBufferSetLayout.get());

		WriteDescriptors()
			.AddBuffer(rsbufferset.get(), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, rsbuffers->Viewpoint.UBO.get(), 0, sizeof(HWViewpointUniforms))
			.AddBuffer(rsbufferset.get(), 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, rsbuffers->MatrixBuffer->UBO.get(), 0, sizeof(MatricesUBO))
			.AddBuffer(rsbufferset.get(), 2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, rsbuffers->StreamBuffer->UBO.get(), 0, sizeof(StreamUBO))
			.AddBuffer(rsbufferset.get(), 3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, rsbuffers->Lightbuffer.UBO.get(), 0, sizeof(LightBufferUBO))
			.AddBuffer(rsbufferset.get(), 4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, rsbuffers->Bonebuffer.SSO.get())
			.Execute(fb->GetDevice());

		RSBufferSets.push_back(std::move(rsbufferset));
	}
}

void VkDescriptorSetManager::Deinit()
{
	while (!Materials.empty())
		RemoveMaterial(Materials.back());
}

void VkDescriptorSetManager::BeginFrame()
{
	UpdateFixedSet();
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
	if (fb->GetDevice()->SupportsExtension(VK_KHR_RAY_QUERY_EXTENSION_NAME))
	{
		update.AddAccelerationStructure(FixedSet.get(), 2, fb->GetRaytrace()->GetAccelStruct());
	}
	else
	{
		update.AddBuffer(FixedSet.get(), 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, fb->GetRaytrace()->GetNodeBuffer());
		update.AddBuffer(FixedSet.get(), 3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, fb->GetRaytrace()->GetVertexBuffer());
		update.AddBuffer(FixedSet.get(), 4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, fb->GetRaytrace()->GetIndexBuffer());
	}
	update.Execute(fb->GetDevice());
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
		update.Execute(fb->GetDevice());
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
			.Create(fb->GetDevice()));
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
	layout = builder.Create(fb->GetDevice());
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
		write.AddBuffer(descriptors.get(), 4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, fb->GetBufferManager()->Shadowmap.Nodes->mBuffer.get());
		write.AddBuffer(descriptors.get(), 5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, fb->GetBufferManager()->Shadowmap.Lines->mBuffer.get());
		write.AddBuffer(descriptors.get(), 6, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, fb->GetBufferManager()->Shadowmap.Lights->mBuffer.get());
	}

	write.Execute(fb->GetDevice());
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
		.Create(fb->GetDevice());

	return PPDescriptorPool->allocate(layout);
}

void VkDescriptorSetManager::CreateRSBufferSetLayout()
{
	RSBufferSetLayout = DescriptorSetLayoutBuilder()
		.AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
		.AddBinding(1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
		.AddBinding(2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
		.AddBinding(3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
		.AddBinding(4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT)
		.DebugName("VkDescriptorSetManager.RSBufferSetLayout")
		.Create(fb->GetDevice());
}

void VkDescriptorSetManager::CreateFixedSetLayout()
{
	DescriptorSetLayoutBuilder builder;
	builder.AddBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
	builder.AddBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
	if (fb->GetDevice()->SupportsExtension(VK_KHR_RAY_QUERY_EXTENSION_NAME))
	{
		builder.AddBinding(2, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
	}
	else
	{
		builder.AddBinding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
		builder.AddBinding(3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
		builder.AddBinding(4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
	}
	builder.DebugName("VkDescriptorSetManager.FixedSetLayout");
	FixedSetLayout = builder.Create(fb->GetDevice());
}

void VkDescriptorSetManager::CreateRSBufferPool()
{
	RSBufferDescriptorPool = DescriptorPoolBuilder()
		.AddPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 4 * fb->MaxThreads)
		.AddPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 * fb->MaxThreads)
		.MaxSets(fb->MaxThreads)
		.DebugName("VkDescriptorSetManager.RSBufferDescriptorPool")
		.Create(fb->GetDevice());
}

void VkDescriptorSetManager::CreateFixedSetPool()
{
	DescriptorPoolBuilder poolbuilder;
	poolbuilder.AddPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2 * maxSets);
	if (fb->GetDevice()->SupportsExtension(VK_KHR_RAY_QUERY_EXTENSION_NAME))
	{
		poolbuilder.AddPoolSize(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1 * maxSets);
	}
	else
	{
		poolbuilder.AddPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3 * maxSets);
	}
	poolbuilder.MaxSets(maxSets);
	poolbuilder.DebugName("VkDescriptorSetManager.FixedDescriptorPool");
	FixedDescriptorPool = poolbuilder.Create(fb->GetDevice());
}
