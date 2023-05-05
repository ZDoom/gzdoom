
#pragma once

#include "vulkan/buffers/vk_hwbuffer.h"
#include "vulkan/shaders/vk_shader.h"

class VkStreamBuffer;

class VkStreamBufferWriter
{
public:
	VkStreamBufferWriter(VulkanRenderDevice* fb);

	bool Write(const StreamData& data);
	void Reset();

	uint32_t DataIndex() const { return mDataIndex; }
	uint32_t StreamDataOffset() const { return mStreamDataOffset; }

private:
	VkStreamBuffer* mBuffer;
	uint32_t mDataIndex = MAX_STREAM_DATA - 1;
	uint32_t mStreamDataOffset = 0;
};

class VkMatrixBufferWriter
{
public:
	VkMatrixBufferWriter(VulkanRenderDevice* fb);

	bool Write(const MatricesUBO& matrices);
	void Reset();

	uint32_t Offset() const { return mOffset; }

private:
	VkStreamBuffer* mBuffer;
	uint32_t mOffset = 0;
};
