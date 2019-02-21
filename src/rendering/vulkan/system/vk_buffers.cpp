
#include "vk_buffers.h"
#include "vk_builders.h"
#include "vk_framebuffer.h"
#include "doomerrors.h"

void VKBuffer::SetData(size_t size, const void *data, bool staticdata)
{
	auto fb = GetVulkanFrameBuffer();

	mPersistent = screen->BuffersArePersistent() && !staticdata;

	if (staticdata)
	{
		BufferBuilder builder;
		builder.setUsage(VK_BUFFER_USAGE_TRANSFER_DST_BIT | mBufferType, VMA_MEMORY_USAGE_GPU_ONLY);
		builder.setSize(size);
		mBuffer = builder.create(fb->device);

		builder.setUsage(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
		mStaging = builder.create(fb->device);

		void *dst = mStaging->Map(0, size);
		memcpy(dst, data, size);
		mStaging->Unmap();

		fb->GetUploadCommands()->copyBuffer(mStaging.get(), mBuffer.get());
	}
	else
	{
		BufferBuilder builder;
		builder.setUsage(mBufferType, VMA_MEMORY_USAGE_CPU_TO_GPU);
		builder.setSize(size);
		mBuffer = builder.create(fb->device);

		if (mPersistent)
		{
			map = mBuffer->Map(0, size);
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
}

/////////////////////////////////////////////////////////////////////////////

void VKDataBuffer::BindRange(size_t start, size_t length)
{
}

void VKDataBuffer::BindBase()
{
}
