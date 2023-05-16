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

#include "vk_buffer.h"
#include "vk_hwbuffer.h"
#include "vk_rsbuffers.h"
#include "vulkan/vk_renderdevice.h"
#include "vulkan/pipelines/vk_renderpass.h"
#include "vulkan/commands/vk_commandbuffer.h"
#include <zvulkan/vulkanbuilders.h>
#include "cmdlib.h"

VkBufferManager::VkBufferManager(VulkanRenderDevice* fb) : fb(fb)
{
}

VkBufferManager::~VkBufferManager()
{
}

void VkBufferManager::Init()
{
	for (int threadIndex = 0; threadIndex < fb->MaxThreads; threadIndex++)
	{
		RSBuffers.push_back(std::make_unique<VkRSBuffers>(fb));
	}

	Shadowmap.Nodes.reset(new VkHardwareDataBuffer(fb, true, false));
	Shadowmap.Lines.reset(new VkHardwareDataBuffer(fb, true, false));
	Shadowmap.Lights.reset(new VkHardwareDataBuffer(fb, true, false));

	CreateFanToTrisIndexBuffer();
}

void VkBufferManager::Deinit()
{
	RSBuffers.clear();

	Shadowmap.Nodes.reset();
	Shadowmap.Lines.reset();
	Shadowmap.Lights.reset();

	while (!Buffers.empty())
		RemoveBuffer(Buffers.back());
}

void VkBufferManager::AddBuffer(VkHardwareBuffer* buffer)
{
	buffer->it = Buffers.insert(Buffers.end(), buffer);
}

void VkBufferManager::RemoveBuffer(VkHardwareBuffer* buffer)
{
	buffer->Reset();
	buffer->fb = nullptr;
	Buffers.erase(buffer->it);
}

IBuffer* VkBufferManager::CreateVertexBuffer(int numBindingPoints, int numAttributes, size_t stride, const FVertexBufferAttribute* attrs)
{
	return new VkHardwareVertexBuffer(fb, fb->GetRenderPassManager()->GetVertexFormat(numBindingPoints, numAttributes, stride, attrs));
}

IBuffer* VkBufferManager::CreateIndexBuffer()
{
	return new VkHardwareIndexBuffer(fb);
}

void VkBufferManager::CreateFanToTrisIndexBuffer()
{
	TArray<uint32_t> data;
	for (int i = 2; i < 1000; i++)
	{
		data.Push(0);
		data.Push(i - 1);
		data.Push(i);
	}

	FanToTrisIndexBuffer.reset(CreateIndexBuffer());
	FanToTrisIndexBuffer->SetData(sizeof(uint32_t) * data.Size(), data.Data(), BufferUsageType::Static);
}
