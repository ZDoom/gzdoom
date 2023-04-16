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

VkBufferManager::VkBufferManager(VulkanRenderDevice* fb) : fb(fb)
{
}

VkBufferManager::~VkBufferManager()
{
}

void VkBufferManager::Init()
{
	MatrixBuffer.reset(new VkStreamBuffer(this, sizeof(MatricesUBO), 50000));
	StreamBuffer.reset(new VkStreamBuffer(this, sizeof(StreamUBO), 300));

	CreateFanToTrisIndexBuffer();
}

void VkBufferManager::Deinit()
{
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

	for (VkHardwareDataBuffer** knownbuf : { &ViewpointUBO, &LightBufferSSO, &LightNodes, &LightLines, &LightList, &BoneBufferSSO })
	{
		if (buffer == *knownbuf) *knownbuf = nullptr;
	}
}

IBuffer* VkBufferManager::CreateVertexBuffer(int numBindingPoints, int numAttributes, size_t stride, const FVertexBufferAttribute* attrs)
{
	return new VkHardwareVertexBuffer(fb, fb->GetRenderPassManager()->GetVertexFormat(numBindingPoints, numAttributes, stride, attrs));
}

IBuffer* VkBufferManager::CreateIndexBuffer()
{
	return new VkHardwareIndexBuffer(fb);
}

IBuffer* VkBufferManager::CreateLightBuffer()
{
	LightBufferSSO = new VkHardwareDataBuffer(fb, true, false);
	return LightBufferSSO;
}

IBuffer* VkBufferManager::CreateBoneBuffer()
{
	BoneBufferSSO = new VkHardwareDataBuffer(fb, true, false);
	return BoneBufferSSO;
}

IBuffer* VkBufferManager::CreateViewpointBuffer()
{
	ViewpointUBO = new VkHardwareDataBuffer(fb, false, true);
	return ViewpointUBO;
}

IBuffer* VkBufferManager::CreateShadowmapNodesBuffer()
{
	LightNodes = new VkHardwareDataBuffer(fb, true, false);
	return LightNodes;
}

IBuffer* VkBufferManager::CreateShadowmapLinesBuffer()
{
	LightLines = new VkHardwareDataBuffer(fb, true, false);
	return LightLines;
}

IBuffer* VkBufferManager::CreateShadowmapLightsBuffer()
{
	LightList = new VkHardwareDataBuffer(fb, true, false);
	return LightList;
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

VkStreamBuffer::VkStreamBuffer(VkBufferManager* buffers, size_t structSize, size_t count)
{
	mBlockSize = static_cast<uint32_t>((structSize + buffers->fb->uniformblockalignment - 1) / buffers->fb->uniformblockalignment * buffers->fb->uniformblockalignment);

	UniformBuffer = new VkHardwareDataBuffer(buffers->fb, false, false);
	UniformBuffer->SetData(mBlockSize * count, nullptr, BufferUsageType::Persistent);
}

VkStreamBuffer::~VkStreamBuffer()
{
	delete UniformBuffer;
}

uint32_t VkStreamBuffer::NextStreamDataBlock()
{
	mStreamDataOffset += mBlockSize;
	if (mStreamDataOffset + (size_t)mBlockSize >= UniformBuffer->Size())
	{
		mStreamDataOffset = 0;
		return 0xffffffff;
	}
	return mStreamDataOffset;
}
