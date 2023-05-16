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

#include "vk_rsbuffers.h"
#include "vulkan/vk_renderstate.h"
#include "vulkan/vk_renderdevice.h"
#include "vulkan/buffers/vk_buffer.h"
#include <zvulkan/vulkanbuilders.h>
#include "flatvertices.h"
#include "cmdlib.h"

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

VkStreamBufferWriter::VkStreamBufferWriter(VkRSBuffers* rsbuffers)
{
	mBuffer = rsbuffers->StreamBuffer.get();
}

bool VkStreamBufferWriter::Write(const StreamData& data)
{
	mDataIndex++;
	if (mDataIndex == MAX_STREAM_DATA)
	{
		mDataIndex = 0;
		mStreamDataOffset = mBuffer->NextStreamDataBlock();
		if (mStreamDataOffset == 0xffffffff)
			return false;
	}
	uint8_t* ptr = (uint8_t*)mBuffer->Data;
	memcpy(ptr + mStreamDataOffset + sizeof(StreamData) * mDataIndex, &data, sizeof(StreamData));
	return true;
}

void VkStreamBufferWriter::Reset()
{
	mDataIndex = MAX_STREAM_DATA - 1;
	mStreamDataOffset = 0;
	mBuffer->Reset();
}

/////////////////////////////////////////////////////////////////////////////

VkMatrixBufferWriter::VkMatrixBufferWriter(VkRSBuffers* rsbuffers)
{
	mBuffer = rsbuffers->MatrixBuffer.get();
}

bool VkMatrixBufferWriter::Write(const MatricesUBO& matrices)
{
	mOffset = mBuffer->NextStreamDataBlock();
	if (mOffset == 0xffffffff)
		return false;

	uint8_t* ptr = (uint8_t*)mBuffer->Data;
	memcpy(ptr + mOffset, &matrices, sizeof(MatricesUBO));
	return true;
}

void VkMatrixBufferWriter::Reset()
{
	mOffset = 0;
	mBuffer->Reset();
}
