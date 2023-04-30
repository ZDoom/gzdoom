#pragma once

#include "hwrenderer/data/buffers.h"
#include "zvulkan/vulkanobjects.h"
#include "tarray.h"
#include <list>

class VulkanRenderDevice;

class VkHardwareBuffer : public IBuffer
{
public:
	VkHardwareBuffer(VulkanRenderDevice* fb);
	~VkHardwareBuffer();

	void Reset();

	void SetData(size_t size, const void *data, BufferUsageType usage) override;
	void SetSubData(size_t offset, size_t size, const void *data) override;
	//void Resize(size_t newsize) override;

	void Map() override;
	void Unmap() override;

	void *Lock(unsigned int size) override;
	void Unlock() override;

	VulkanRenderDevice* fb = nullptr;
	std::list<VkHardwareBuffer*>::iterator it;

	VkBufferUsageFlags mBufferType = 0;
	std::unique_ptr<VulkanBuffer> mBuffer;
	std::unique_ptr<VulkanBuffer> mStaging;
	bool mPersistent = false;
	TArray<uint8_t> mStaticUpload;
};

class VkHardwareVertexBuffer : public VkHardwareBuffer
{
public:
	VkHardwareVertexBuffer(VulkanRenderDevice* fb, int vertexFormat) : VkHardwareBuffer(fb), VertexFormat(vertexFormat) { mBufferType = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT; }

	int VertexFormat = -1;
};

class VkHardwareIndexBuffer : public VkHardwareBuffer
{
public:
	VkHardwareIndexBuffer(VulkanRenderDevice* fb) : VkHardwareBuffer(fb) { mBufferType = VK_BUFFER_USAGE_INDEX_BUFFER_BIT; }
};

class VkHardwareDataBuffer : public VkHardwareBuffer
{
public:
	VkHardwareDataBuffer(VulkanRenderDevice* fb, bool ssbo, bool needresize) : VkHardwareBuffer(fb)
	{
		mBufferType = ssbo ? VK_BUFFER_USAGE_STORAGE_BUFFER_BIT : VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
		if (needresize)
		{
			mBufferType |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		}
	}
};
