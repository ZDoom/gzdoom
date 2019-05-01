
#include "vk_buffers.h"
#include "vk_builders.h"
#include "vk_framebuffer.h"
#include "vulkan/renderer/vk_renderstate.h"
#include "vulkan/renderer/vk_renderpass.h"
#include "doomerrors.h"

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

void VKBuffer::SetData(size_t size, const void *data, bool staticdata)
{
	auto fb = GetVulkanFrameBuffer();

	if (staticdata)
	{
		mPersistent = false;

		{
			BufferBuilder builder;
			builder.setUsage(VK_BUFFER_USAGE_TRANSFER_DST_BIT | mBufferType, VMA_MEMORY_USAGE_GPU_ONLY);
			builder.setSize(size);
			mBuffer = builder.create(fb->device);
		}

		{
			BufferBuilder builder;
			builder.setUsage(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
			builder.setSize(size);
			mStaging = builder.create(fb->device);
		}

		void *dst = mStaging->Map(0, size);
		memcpy(dst, data, size);
		mStaging->Unmap();

		fb->GetTransferCommands()->copyBuffer(mStaging.get(), mBuffer.get());
	}
	else
	{
		mPersistent = screen->BuffersArePersistent();

		BufferBuilder builder;
		builder.setUsage(mBufferType, VMA_MEMORY_USAGE_UNKNOWN, mPersistent ? VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT : 0);
		builder.setMemoryType(
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		builder.setSize(size);
		mBuffer = builder.create(fb->device);

		if (mPersistent)
		{
			map = mBuffer->Map(0, size);
			if (data)
				memcpy(map, data, size);
		}
		else if (data)
		{
			void *dst = mBuffer->Map(0, size);
			memcpy(dst, data, size);
			mBuffer->Unmap();
		}
	}
	buffersize = size;
}

void VKBuffer::SetSubData(size_t offset, size_t size, const void *data)
{
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
		SetData(mStaticUpload.Size(), mStaticUpload.Data(), true);
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

void VKDataBuffer::BindRange(size_t start, size_t length)
{
	GetVulkanFrameBuffer()->GetRenderState()->Bind(bindingpoint, (uint32_t)start);
}

void VKDataBuffer::BindBase()
{
	GetVulkanFrameBuffer()->GetRenderState()->Bind(bindingpoint, 0);
}
