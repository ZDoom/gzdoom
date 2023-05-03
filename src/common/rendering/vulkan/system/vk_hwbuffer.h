#pragma once

#include "hwrenderer/data/buffers.h"
#include "zvulkan/vulkanobjects.h"
#include "tarray.h"
#include <list>

#ifdef _MSC_VER
// silence bogus warning C4250: 'VkHardwareVertexBuffer': inherits 'VkHardwareBuffer::VkHardwareBuffer::SetData' via dominance
// According to internet infos, the warning is erroneously emitted in this case.
#pragma warning(disable:4250) 
#endif

class VulkanRenderDevice;

class VkHardwareBuffer : virtual public IBuffer
{
public:
	VkHardwareBuffer(VulkanRenderDevice* fb);
	~VkHardwareBuffer();

	void Reset();

	void SetData(size_t size, const void *data, BufferUsageType usage) override;
	void SetSubData(size_t offset, size_t size, const void *data) override;
	void Resize(size_t newsize) override;

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

class VkHardwareVertexBuffer : public IVertexBuffer, public VkHardwareBuffer
{
public:
	VkHardwareVertexBuffer(VulkanRenderDevice* fb) : VkHardwareBuffer(fb) { mBufferType = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT; }
	void SetFormat(int numBindingPoints, int numAttributes, size_t stride, const FVertexBufferAttribute *attrs) override;

	int VertexFormat = -1;
};

class VkHardwareIndexBuffer : public IIndexBuffer, public VkHardwareBuffer
{
public:
	VkHardwareIndexBuffer(VulkanRenderDevice* fb) : VkHardwareBuffer(fb) { mBufferType = VK_BUFFER_USAGE_INDEX_BUFFER_BIT; }
};

class VkHardwareDataBuffer : public IDataBuffer, public VkHardwareBuffer
{
public:
	VkHardwareDataBuffer(VulkanRenderDevice* fb, int bindingpoint, bool ssbo, bool needresize) : VkHardwareBuffer(fb), bindingpoint(bindingpoint)
	{
		mBufferType = ssbo ? VK_BUFFER_USAGE_STORAGE_BUFFER_BIT : VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
		if (needresize)
		{
			mBufferType |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		}
	}

	void BindRange(FRenderState *state, size_t start, size_t length) override;

	int bindingpoint;
};
