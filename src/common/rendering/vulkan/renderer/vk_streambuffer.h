
#pragma once

#include "vulkan/system/vk_hwbuffer.h"
#include "vulkan/shaders/vk_shader.h"

class VkStreamBuffer;
class VkMatrixBuffer;

class VkStreamBufferWriter
{
public:
	VkStreamBufferWriter(VulkanFrameBuffer* fb);

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
	VkMatrixBufferWriter(VulkanFrameBuffer* fb);

	bool Write(const VSMatrix& modelMatrix, bool modelMatrixEnabled, const VSMatrix& textureMatrix, bool textureMatrixEnabled);
	void Reset();

	uint32_t Offset() const { return mOffset; }

private:
	VkStreamBuffer* mBuffer;
	MatricesUBO mMatrices = {};
	VSMatrix mIdentityMatrix;
	uint32_t mOffset = 0;
};
