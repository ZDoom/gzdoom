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
#include "vk_streambuffer.h"
#include "vulkan/vk_renderdevice.h"
#include "vulkan/pipelines/vk_renderpass.h"
#include "vulkan/commands/vk_commandbuffer.h"
#include <zvulkan/vulkanbuilders.h>
#include "flatvertices.h"
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

/////////////////////////////////////////////////////////////////////////////

VkRSBuffers::VkRSBuffers(VulkanRenderDevice* fb)
{
	static const FVertexBufferAttribute format[] =
	{
		{ 0, VATTR_VERTEX, VFmt_Float3, (int)myoffsetof(FFlatVertex, x) },
		{ 0, VATTR_TEXCOORD, VFmt_Float2, (int)myoffsetof(FFlatVertex, u) },
		{ 0, VATTR_LIGHTMAP, VFmt_Float3, (int)myoffsetof(FFlatVertex, lu) },
	};

	Flatbuffer.VertexFormat = fb->GetRenderPassManager()->GetVertexFormat(1, 3, sizeof(FFlatVertex), format);

	Flatbuffer.VertexBuffer = BufferBuilder()
		.Usage(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_MEMORY_USAGE_UNKNOWN, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT)
		.MemoryType(
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
		.Size(Flatbuffer.BUFFER_SIZE * sizeof(FFlatVertex))
		.DebugName("Flatbuffer.VertexBuffer")
		.Create(fb->GetDevice());

	Flatbuffer.Vertices = (FFlatVertex*)Flatbuffer.VertexBuffer->Map(0, Flatbuffer.VertexBuffer->size);

	Flatbuffer.IndexBuffer = BufferBuilder()
		.Usage(VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY)
		.Size(16)
		.DebugName("Flatbuffer.IndexBuffer")
		.Create(fb->GetDevice());

	MatrixBuffer = std::make_unique<VkStreamBuffer>(fb, sizeof(MatricesUBO), 50000);
	StreamBuffer = std::make_unique<VkStreamBuffer>(fb, sizeof(StreamUBO), 300);

	Viewpoint.BlockAlign = (sizeof(HWViewpointUniforms) + fb->uniformblockalignment - 1) / fb->uniformblockalignment * fb->uniformblockalignment;

	Viewpoint.UBO = BufferBuilder()
		.Usage(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_UNKNOWN, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT)
		.MemoryType(
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
		.Size(Viewpoint.Count * Viewpoint.BlockAlign)
		.DebugName("Viewpoint.UBO")
		.Create(fb->GetDevice());

	Viewpoint.Data = Viewpoint.UBO->Map(0, Viewpoint.UBO->size);

	Lightbuffer.UBO = BufferBuilder()
		.Usage(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_UNKNOWN, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT)
		.MemoryType(
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
		.Size(Lightbuffer.Count * 4 * sizeof(FVector4))
		.DebugName("Lightbuffer.UBO")
		.Create(fb->GetDevice());

	Lightbuffer.Data = Lightbuffer.UBO->Map(0, Lightbuffer.UBO->size);

	Bonebuffer.SSO = BufferBuilder()
		.Usage(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_UNKNOWN, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT)
		.MemoryType(
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
		.Size(Bonebuffer.Count * sizeof(VSMatrix))
		.DebugName("Bonebuffer.SSO")
		.Create(fb->GetDevice());

	Bonebuffer.Data = Bonebuffer.SSO->Map(0, Bonebuffer.SSO->size);
}

VkRSBuffers::~VkRSBuffers()
{
	if (Flatbuffer.VertexBuffer)
		Flatbuffer.VertexBuffer->Unmap();
	Flatbuffer.VertexBuffer.reset();
	Flatbuffer.IndexBuffer.reset();

	if (Viewpoint.UBO)
		Viewpoint.UBO->Unmap();
	Viewpoint.UBO.reset();

	if (Lightbuffer.UBO)
		Lightbuffer.UBO->Unmap();
	Lightbuffer.UBO.reset();

	if (Bonebuffer.SSO)
		Bonebuffer.SSO->Unmap();
	Bonebuffer.SSO.reset();
}

/////////////////////////////////////////////////////////////////////////////

VkStreamBuffer::VkStreamBuffer(VulkanRenderDevice* fb, size_t structSize, size_t count)
{
	mBlockSize = static_cast<uint32_t>((structSize + fb->uniformblockalignment - 1) / fb->uniformblockalignment * fb->uniformblockalignment);

	UBO = BufferBuilder()
		.Usage(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_UNKNOWN, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT)
		.MemoryType(
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
		.Size(mBlockSize * count)
		.DebugName("VkStreamBuffer")
		.Create(fb->GetDevice());

	Data = UBO->Map(0, UBO->size);
}

VkStreamBuffer::~VkStreamBuffer()
{
	UBO->Unmap();
}

uint32_t VkStreamBuffer::NextStreamDataBlock()
{
	mStreamDataOffset += mBlockSize;
	if (mStreamDataOffset + (size_t)mBlockSize >= UBO->size)
	{
		mStreamDataOffset = 0;
		return 0xffffffff;
	}
	return mStreamDataOffset;
}
