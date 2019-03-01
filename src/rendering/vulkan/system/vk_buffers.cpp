
#include "vk_buffers.h"
#include "vk_builders.h"
#include "vk_framebuffer.h"
#include "vulkan/renderer/vk_renderstate.h"
#include "vulkan/renderer/vk_renderpass.h"
#include "doomerrors.h"

VKBuffer::~VKBuffer()
{
	if (map)
		mBuffer->Unmap();

	auto fb = GetVulkanFrameBuffer();
	if (fb && mBuffer)
		fb->mFrameDeleteList.push_back(std::move(mBuffer));
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

		fb->GetUploadCommands()->copyBuffer(mStaging.get(), mBuffer.get());
	}
	else
	{
		mPersistent = screen->BuffersArePersistent();

		BufferBuilder builder;
		builder.setUsage(mBufferType, VMA_MEMORY_USAGE_CPU_TO_GPU, mPersistent ? VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT : 0);
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

		fb->GetUploadCommands()->copyBuffer(mStaging.get(), mBuffer.get(), offset, offset, size);
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
	I_FatalError("VKBuffer::Resize not implemented\n");
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
	if (!mPersistent)
		map = mBuffer->Map(0, size);
	return map;
}

void VKBuffer::Unlock()
{
	if (!mPersistent)
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
