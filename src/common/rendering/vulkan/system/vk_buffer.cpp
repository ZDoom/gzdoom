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
#include "vulkan/renderer/vk_streambuffer.h"
#include "hwrenderer/data/shaderuniforms.h"

VkBufferManager::VkBufferManager(VulkanFrameBuffer* fb) : fb(fb)
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

	for (VkHardwareDataBuffer** knownbuf : { &ViewpointUBO, &LightBufferSSO, &LightNodes, &LightLines, &LightList })
	{
		if (buffer == *knownbuf) *knownbuf = nullptr;
	}
}

IVertexBuffer* VkBufferManager::CreateVertexBuffer()
{
	return new VkHardwareVertexBuffer(fb);
}

IIndexBuffer* VkBufferManager::CreateIndexBuffer()
{
	return new VkHardwareIndexBuffer(fb);
}

IDataBuffer* VkBufferManager::CreateDataBuffer(int bindingpoint, bool ssbo, bool needsresize)
{
	auto buffer = new VkHardwareDataBuffer(fb, bindingpoint, ssbo, needsresize);

	switch (bindingpoint)
	{
	case LIGHTBUF_BINDINGPOINT: LightBufferSSO = buffer; break;
	case VIEWPOINT_BINDINGPOINT: ViewpointUBO = buffer; break;
	case LIGHTNODES_BINDINGPOINT: LightNodes = buffer; break;
	case LIGHTLINES_BINDINGPOINT: LightLines = buffer; break;
	case LIGHTLIST_BINDINGPOINT: LightList = buffer; break;
	case POSTPROCESS_BINDINGPOINT: break;
	default: break;
	}

	return buffer;
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
	mBlockSize = static_cast<uint32_t>((structSize + screen->uniformblockalignment - 1) / screen->uniformblockalignment * screen->uniformblockalignment);

	UniformBuffer = (VkHardwareDataBuffer*)buffers->CreateDataBuffer(-1, false, false);
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
