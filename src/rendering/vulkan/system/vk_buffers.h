#pragma once

#include "hwrenderer/data/buffers.h"
#include "vk_objects.h"
#include "utility/tarray.h"

#ifdef _MSC_VER
// silence bogus warning C4250: 'VKVertexBuffer': inherits 'VKBuffer::VKBuffer::SetData' via dominance
// According to internet infos, the warning is erroneously emitted in this case.
#pragma warning(disable:4250) 
#endif

class VKBuffer : virtual public IBuffer
{
public:
	~VKBuffer();

	void SetData(size_t size, const void *data, bool staticdata) override;
	void SetSubData(size_t offset, size_t size, const void *data) override;
	void Resize(size_t newsize) override;

	void Map() override;
	void Unmap() override;

	void *Lock(unsigned int size) override;
	void Unlock() override;

	VkBufferUsageFlags mBufferType = 0;
	std::unique_ptr<VulkanBuffer> mBuffer;
	std::unique_ptr<VulkanBuffer> mStaging;
	bool mPersistent = false;
	TArray<uint8_t> mStaticUpload;
};

class VKVertexBuffer : public IVertexBuffer, public VKBuffer
{
public:
	VKVertexBuffer() { mBufferType = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT; }
	void SetFormat(int numBindingPoints, int numAttributes, size_t stride, const FVertexBufferAttribute *attrs) override;

	int VertexFormat = -1;
};

class VKIndexBuffer : public IIndexBuffer, public VKBuffer
{
public:
	VKIndexBuffer() { mBufferType = VK_BUFFER_USAGE_INDEX_BUFFER_BIT; }
};

class VKDataBuffer : public IDataBuffer, public VKBuffer
{
public:
	VKDataBuffer(int bindingpoint, bool ssbo) : bindingpoint(bindingpoint) { mBufferType = ssbo ? VK_BUFFER_USAGE_STORAGE_BUFFER_BIT : VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT; }

	void BindRange(size_t start, size_t length) override;
	void BindBase() override;

	int bindingpoint;
};
