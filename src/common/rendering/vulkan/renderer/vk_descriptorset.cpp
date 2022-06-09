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
#include "vulkan/system/vk_builders.h"
#include "vulkan/system/vk_framebuffer.h"
#include "vulkan/system/vk_buffers.h"
#include "vulkan/system/vk_commandbuffer.h"
#include "flatvertices.h"
#include "hw_viewpointuniforms.h"
#include "v_2ddrawer.h"

VkDescriptorSetManager::VkDescriptorSetManager()
{
}

VkDescriptorSetManager::~VkDescriptorSetManager()
{
}

void VkDescriptorSetManager::Init()
{
	CreateFixedSet();
	CreateDynamicSet();
	CreateNullTexture();
}

void VkDescriptorSetManager::CreateDynamicSet()
{
	DescriptorSetLayoutBuilder builder;
	builder.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
	builder.addBinding(1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
	builder.addBinding(2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
	DynamicSetLayout = builder.create(GetVulkanFrameBuffer()->device);
	DynamicSetLayout->SetDebugName("VkDescriptorSetManager.DynamicSetLayout");

	DescriptorPoolBuilder poolbuilder;
	poolbuilder.addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 3);
	poolbuilder.setMaxSets(1);
	DynamicDescriptorPool = poolbuilder.create(GetVulkanFrameBuffer()->device);
	DynamicDescriptorPool->SetDebugName("VkDescriptorSetManager.DynamicDescriptorPool");

	DynamicSet = DynamicDescriptorPool->allocate(DynamicSetLayout.get());
	if (!DynamicSet)
		I_FatalError("CreateDynamicSet failed.\n");
}

void VkDescriptorSetManager::UpdateDynamicSet()
{
	auto fb = GetVulkanFrameBuffer();

	WriteDescriptors update;
	update.addBuffer(DynamicSet.get(), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, fb->ViewpointUBO->mBuffer.get(), 0, sizeof(HWViewpointUniforms));
	update.addBuffer(DynamicSet.get(), 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, fb->MatrixBuffer->UniformBuffer->mBuffer.get(), 0, sizeof(MatricesUBO));
	update.addBuffer(DynamicSet.get(), 2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, fb->StreamBuffer->UniformBuffer->mBuffer.get(), 0, sizeof(StreamUBO));
	update.updateSets(fb->device);
}

void VkDescriptorSetManager::CreateFixedSet()
{
	DescriptorSetLayoutBuilder builder;
	builder.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
	builder.addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
	builder.addBinding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
	if (GetVulkanFrameBuffer()->device->SupportsDeviceExtension(VK_KHR_RAY_QUERY_EXTENSION_NAME))
		builder.addBinding(3, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
	FixedSetLayout = builder.create(GetVulkanFrameBuffer()->device);
	FixedSetLayout->SetDebugName("VkDescriptorSetManager.FixedSetLayout");

	DescriptorPoolBuilder poolbuilder;
	poolbuilder.addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1);
	poolbuilder.addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2);
	if (GetVulkanFrameBuffer()->device->SupportsDeviceExtension(VK_KHR_RAY_QUERY_EXTENSION_NAME))
		poolbuilder.addPoolSize(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1);
	poolbuilder.setMaxSets(1);
	FixedDescriptorPool = poolbuilder.create(GetVulkanFrameBuffer()->device);
	FixedDescriptorPool->SetDebugName("VkDescriptorSetManager.FixedDescriptorPool");

	FixedSet = FixedDescriptorPool->allocate(FixedSetLayout.get());
	if (!FixedSet)
		I_FatalError("CreateFixedSet failed.\n");
}

void VkDescriptorSetManager::UpdateFixedSet()
{
	auto fb = GetVulkanFrameBuffer();

	WriteDescriptors update;
	update.addCombinedImageSampler(FixedSet.get(), 0, fb->GetBuffers()->Shadowmap.View.get(), fb->GetBuffers()->ShadowmapSampler.get(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	update.addCombinedImageSampler(FixedSet.get(), 1, fb->GetBuffers()->Lightmap.View.get(), fb->GetBuffers()->LightmapSampler.get(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	update.addBuffer(FixedSet.get(), 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, fb->LightBufferSSO->mBuffer.get());
	if (fb->device->SupportsDeviceExtension(VK_KHR_RAY_QUERY_EXTENSION_NAME))
		update.addAccelerationStructure(FixedSet.get(), 3, fb->GetRaytrace()->GetAccelStruct());
	update.updateSets(fb->device);
}

void VkDescriptorSetManager::TextureSetPoolReset()
{
	if (auto fb = GetVulkanFrameBuffer())
	{
		auto& deleteList = fb->GetCommands()->FrameDeleteList;

		for (auto& desc : TextureDescriptorPools)
		{
			deleteList.DescriptorPools.push_back(std::move(desc));
		}
		deleteList.Descriptors.push_back(std::move(NullTextureDescriptorSet));
	}
	NullTextureDescriptorSet.reset();
	TextureDescriptorPools.clear();
	TextureDescriptorSetsLeft = 0;
	TextureDescriptorsLeft = 0;
}

void VkDescriptorSetManager::CreateNullTexture()
{
	auto fb = GetVulkanFrameBuffer();

	ImageBuilder imgbuilder;
	imgbuilder.setFormat(VK_FORMAT_R8G8B8A8_UNORM);
	imgbuilder.setSize(1, 1);
	imgbuilder.setUsage(VK_IMAGE_USAGE_SAMPLED_BIT);
	NullTexture = imgbuilder.create(fb->device);
	NullTexture->SetDebugName("VkDescriptorSetManager.NullTexture");

	ImageViewBuilder viewbuilder;
	viewbuilder.setImage(NullTexture.get(), VK_FORMAT_R8G8B8A8_UNORM);
	NullTextureView = viewbuilder.create(fb->device);
	NullTextureView->SetDebugName("VkDescriptorSetManager.NullTextureView");

	PipelineBarrier barrier;
	barrier.addImage(NullTexture.get(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
	barrier.execute(fb->GetCommands()->GetTransferCommands(), VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
}

VulkanDescriptorSet* VkDescriptorSetManager::GetNullTextureDescriptorSet()
{
	if (!NullTextureDescriptorSet)
	{
		NullTextureDescriptorSet = AllocateTextureDescriptorSet(SHADER_MIN_REQUIRED_TEXTURE_LAYERS);

		auto fb = GetVulkanFrameBuffer();
		WriteDescriptors update;
		for (int i = 0; i < SHADER_MIN_REQUIRED_TEXTURE_LAYERS; i++)
		{
			update.addCombinedImageSampler(NullTextureDescriptorSet.get(), i, NullTextureView.get(), fb->GetSamplerManager()->Get(CLAMP_XY_NOMIP), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
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
		TextureDescriptorPools.push_back(builder.create(GetVulkanFrameBuffer()->device));
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
	layout = builder.create(GetVulkanFrameBuffer()->device);
	layout->SetDebugName("VkDescriptorSetManager.TextureSetLayout");
	return layout.get();
}
