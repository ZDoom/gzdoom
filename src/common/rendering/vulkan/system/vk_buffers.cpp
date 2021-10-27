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

#include "vk_buffers.h"
#include "vk_builders.h"
#include "vk_framebuffer.h"
#include "vulkan/renderer/vk_renderstate.h"
#include "vulkan/renderer/vk_renderpass.h"
#include "engineerrors.h"

VKBuffer *VKBuffer::First = nullptr;

VKBuffer::VKBuffer()
{
	Next = First;
	First = this;
	if (Next) Next->Prev = this;
}

VKBuffer::~VKBuffer()
{
	if (Next) Next->Prev = Prev;
	if (Prev) Prev->Next = Next;
	else First = Next;

	if (mBuffer && map)
		mBuffer->Unmap();

	auto fb = GetVulkanFrameBuffer();
	if (fb && mBuffer)
		fb->FrameDeleteList.Buffers.push_back(std::move(mBuffer));
}

void VKBuffer::ResetAll()
{
	for (VKBuffer *cur = VKBuffer::First; cur; cur = cur->Next)
		cur->Reset();
}

void VKBuffer::Reset()
{
	if (mBuffer && map)
		mBuffer->Unmap();
	mBuffer.reset();
	mStaging.reset();
}

void VKBuffer::SetData(size_t size, const void *data, BufferUsageType usage)
{
	auto fb = GetVulkanFrameBuffer();

	size_t bufsize = std::max(size, (size_t)16); // For supporting zero byte buffers

	if (usage == BufferUsageType::Static || usage == BufferUsageType::Stream)
	{
		// Note: we could recycle buffers here for the stream usage type to improve performance

		mPersistent = false;

		BufferBuilder builder;
		builder.setUsage(VK_BUFFER_USAGE_TRANSFER_DST_BIT | mBufferType, VMA_MEMORY_USAGE_GPU_ONLY);
		builder.setSize(bufsize);
		mBuffer = builder.create(fb->device);

		BufferBuilder builder2;
		builder2.setUsage(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
		builder2.setSize(bufsize);
		mStaging = builder2.create(fb->device);

		if (data)
		{
			void* dst = mStaging->Map(0, bufsize);
			memcpy(dst, data, size);
			mStaging->Unmap();
		}

		fb->GetTransferCommands()->copyBuffer(mStaging.get(), mBuffer.get());
	}
	else if (usage == BufferUsageType::Persistent)
	{
		mPersistent = true;

		BufferBuilder builder;
		builder.setUsage(mBufferType, VMA_MEMORY_USAGE_UNKNOWN, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT);
		builder.setMemoryType(
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		builder.setSize(bufsize);
		mBuffer = builder.create(fb->device);

		map = mBuffer->Map(0, bufsize);
		if (data)
			memcpy(map, data, size);
	}
	else if (usage == BufferUsageType::Mappable)
	{
		mPersistent = false;

		BufferBuilder builder;
		builder.setUsage(mBufferType, VMA_MEMORY_USAGE_UNKNOWN, 0);
		builder.setMemoryType(
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		builder.setSize(bufsize);
		mBuffer = builder.create(fb->device);

		if (data)
		{
			void* dst = mBuffer->Map(0, bufsize);
			memcpy(dst, data, size);
			mBuffer->Unmap();
		}
	}

	buffersize = size;
}

void VKBuffer::SetSubData(size_t offset, size_t size, const void *data)
{
	size = std::max(size, (size_t)16); // For supporting zero byte buffers

	auto fb = GetVulkanFrameBuffer();
	if (mStaging)
	{
		void *dst = mStaging->Map(offset, size);
		memcpy(dst, data, size);
		mStaging->Unmap();

		fb->GetTransferCommands()->copyBuffer(mStaging.get(), mBuffer.get(), offset, offset, size);
	}
	else
	{
		void *dst = mBuffer->Map(offset, size);
		memcpy(dst, data, size);
		mBuffer->Unmap();
	}
}

void VKBuffer::Resize(size_t newsize)
{
	newsize = std::max(newsize, (size_t)16); // For supporting zero byte buffers

	auto fb = GetVulkanFrameBuffer();

	// Grab old buffer
	size_t oldsize = buffersize;
	std::unique_ptr<VulkanBuffer> oldBuffer = std::move(mBuffer);
	oldBuffer->Unmap();
	map = nullptr;

	// Create new buffer
	BufferBuilder builder;
	builder.setUsage(mBufferType, VMA_MEMORY_USAGE_UNKNOWN, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT);
	builder.setMemoryType(
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	builder.setSize(newsize);
	mBuffer = builder.create(fb->device);
	buffersize = newsize;

	// Transfer data from old to new
	fb->GetTransferCommands()->copyBuffer(oldBuffer.get(), mBuffer.get(), 0, 0, oldsize);
	fb->WaitForCommands(false);

	// Fetch pointer to new buffer
	map = mBuffer->Map(0, newsize);

	// Old buffer may be part of the dynamic set
	fb->GetRenderPassManager()->UpdateDynamicSet();
}

void VKBuffer::Map()
{
	if (!mPersistent)
		map = mBuffer->Map(0, mBuffer->size);
}

void VKBuffer::Unmap()
{
	if (!mPersistent)
	{
		mBuffer->Unmap();
		map = nullptr;
	}
}

void *VKBuffer::Lock(unsigned int size)
{
	size = std::max(size, (unsigned int)16); // For supporting zero byte buffers

	if (!mBuffer)
	{
		// The model mesh loaders lock multiple non-persistent buffers at the same time. This is not allowed in vulkan.
		// VkDeviceMemory can only be mapped once and multiple non-persistent buffers may come from the same device memory object.
		mStaticUpload.Resize(size);
		map = mStaticUpload.Data();
	}
	else if (!mPersistent)
	{
		map = mBuffer->Map(0, size);
	}
	return map;
}

void VKBuffer::Unlock()
{
	if (!mBuffer)
	{
		map = nullptr;
		SetData(mStaticUpload.Size(), mStaticUpload.Data(), BufferUsageType::Static);
		mStaticUpload.Clear();
	}
	else if (!mPersistent)
	{
		mBuffer->Unmap();
		map = nullptr;
	}
}

/////////////////////////////////////////////////////////////////////////////

void VKVertexBuffer::SetFormat(int numBindingPoints, int numAttributes, size_t stride, const FVertexBufferAttribute *attrs)
{
	VertexFormat = GetVulkanFrameBuffer()->GetRenderPassManager()->GetVertexFormat(numBindingPoints, numAttributes, stride, attrs);
}

/////////////////////////////////////////////////////////////////////////////


void VKDataBuffer::BindRange(FRenderState* state, size_t start, size_t length)
{
	static_cast<VkRenderState*>(state)->Bind(bindingpoint, (uint32_t)start);
}

