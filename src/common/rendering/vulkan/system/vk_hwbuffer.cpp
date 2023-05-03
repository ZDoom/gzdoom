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

#include "vk_hwbuffer.h"
#include "zvulkan/vulkanbuilders.h"
#include "vk_renderdevice.h"
#include "vk_commandbuffer.h"
#include "vk_buffer.h"
#include "vulkan/renderer/vk_renderstate.h"
#include "vulkan/renderer/vk_descriptorset.h"
#include "engineerrors.h"

VkHardwareBuffer::VkHardwareBuffer(VulkanRenderDevice* fb) : fb(fb)
{
	fb->GetBufferManager()->AddBuffer(this);
}

VkHardwareBuffer::~VkHardwareBuffer()
{
	if (fb)
		fb->GetBufferManager()->RemoveBuffer(this);
}

void VkHardwareBuffer::Reset()
{
	if (fb)
	{
		if (mBuffer && map)
		{
			mBuffer->Unmap();
			map = nullptr;
		}
		if (mBuffer)
			fb->GetCommands()->DrawDeleteList->Add(std::move(mBuffer));
		if (mStaging)
			fb->GetCommands()->TransferDeleteList->Add(std::move(mStaging));
	}
}

void VkHardwareBuffer::SetData(size_t size, const void *data, BufferUsageType usage)
{
	size_t bufsize = max(size, (size_t)16); // For supporting zero byte buffers

	// If SetData is called multiple times we have to keep the old buffers alive as there might still be draw commands referencing them
	if (mBuffer)
	{
		fb->GetCommands()->DrawDeleteList->Add(std::move(mBuffer));
	}
	if (mStaging)
	{
		fb->GetCommands()->TransferDeleteList->Add(std::move(mStaging));
	}

	if (usage == BufferUsageType::Static || usage == BufferUsageType::Stream)
	{
		// Note: we could recycle buffers here for the stream usage type to improve performance

		mPersistent = false;

		mBuffer = BufferBuilder()
			.Usage(VK_BUFFER_USAGE_TRANSFER_DST_BIT | mBufferType, VMA_MEMORY_USAGE_GPU_ONLY)
			.Size(bufsize)
			.DebugName(usage == BufferUsageType::Static ? "VkHardwareBuffer.Static" : "VkHardwareBuffer.Stream")
			.Create(fb->device.get());

		mStaging = BufferBuilder()
			.Usage(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY)
			.Size(bufsize)
			.DebugName(usage == BufferUsageType::Static ? "VkHardwareBuffer.Staging.Static" : "VkHardwareBuffer.Staging.Stream")
			.Create(fb->device.get());

		if (data)
		{
			void* dst = mStaging->Map(0, bufsize);
			memcpy(dst, data, size);
			mStaging->Unmap();
		}

		fb->GetCommands()->GetTransferCommands()->copyBuffer(mStaging.get(), mBuffer.get());
	}
	else if (usage == BufferUsageType::Persistent)
	{
		mPersistent = true;

		mBuffer = BufferBuilder()
			.Usage(mBufferType, VMA_MEMORY_USAGE_UNKNOWN, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT)
			.MemoryType(
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
			.Size(bufsize)
			.DebugName("VkHardwareBuffer.Persistent")
			.Create(fb->device.get());

		map = mBuffer->Map(0, bufsize);
		if (data)
			memcpy(map, data, size);
	}
	else if (usage == BufferUsageType::Mappable)
	{
		mPersistent = false;

		mBuffer = BufferBuilder()
			.Usage(mBufferType, VMA_MEMORY_USAGE_UNKNOWN, 0)
			.MemoryType(
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
			.Size(bufsize)
			.DebugName("VkHardwareBuffer.Mappable")
			.Create(fb->device.get());

		if (data)
		{
			void* dst = mBuffer->Map(0, bufsize);
			memcpy(dst, data, size);
			mBuffer->Unmap();
		}
	}

	buffersize = size;
}

void VkHardwareBuffer::SetSubData(size_t offset, size_t size, const void *data)
{
	size = max(size, (size_t)16); // For supporting zero byte buffers

	if (mStaging)
	{
		void *dst = mStaging->Map(offset, size);
		memcpy(dst, data, size);
		mStaging->Unmap();

		fb->GetCommands()->GetTransferCommands()->copyBuffer(mStaging.get(), mBuffer.get(), offset, offset, size);
	}
	else
	{
		void *dst = mBuffer->Map(offset, size);
		memcpy(dst, data, size);
		mBuffer->Unmap();
	}
}

void VkHardwareBuffer::Resize(size_t newsize)
{
	newsize = max(newsize, (size_t)16); // For supporting zero byte buffers

	// Grab old buffer
	size_t oldsize = buffersize;
	std::unique_ptr<VulkanBuffer> oldBuffer = std::move(mBuffer);
	oldBuffer->Unmap();
	map = nullptr;

	// Create new buffer
	mBuffer = BufferBuilder()
		.Usage(mBufferType, VMA_MEMORY_USAGE_UNKNOWN, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT)
		.MemoryType(
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
		.Size(newsize)
		.DebugName("VkHardwareBuffer.Resized")
		.Create(fb->device.get());
	buffersize = newsize;

	// Transfer data from old to new
	fb->GetCommands()->GetTransferCommands()->copyBuffer(oldBuffer.get(), mBuffer.get(), 0, 0, oldsize);
	fb->GetCommands()->TransferDeleteList->Add(std::move(oldBuffer));
	fb->GetCommands()->WaitForCommands(false);
	fb->GetDescriptorSetManager()->UpdateHWBufferSet(); // Old buffer may be part of the bound descriptor set

	// Fetch pointer to new buffer
	map = mBuffer->Map(0, newsize);
}

void VkHardwareBuffer::Map()
{
	if (!mPersistent)
		map = mBuffer->Map(0, mBuffer->size);
}

void VkHardwareBuffer::Unmap()
{
	if (!mPersistent)
	{
		mBuffer->Unmap();
		map = nullptr;
	}
}

void *VkHardwareBuffer::Lock(unsigned int size)
{
	size = max(size, (unsigned int)16); // For supporting zero byte buffers

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

void VkHardwareBuffer::Unlock()
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

void VkHardwareVertexBuffer::SetFormat(int numBindingPoints, int numAttributes, size_t stride, const FVertexBufferAttribute *attrs)
{
	VertexFormat = fb->GetRenderPassManager()->GetVertexFormat(numBindingPoints, numAttributes, stride, attrs);
}

/////////////////////////////////////////////////////////////////////////////


void VkHardwareDataBuffer::BindRange(FRenderState* state, size_t start, size_t length)
{
	static_cast<VkRenderState*>(state)->Bind(bindingpoint, (uint32_t)start);
}

