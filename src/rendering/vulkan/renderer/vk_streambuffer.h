
#pragma once

#include "vulkan/system/vk_buffers.h"
#include "vulkan/shaders/vk_shader.h"

class VkStreamBuffer;
class VkMatrixBuffer;

class VkStreamBufferWriter
{
public:
	VkStreamBufferWriter();

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
	VkMatrixBufferWriter();

	bool Write(const VSMatrix& modelMatrix, bool modelMatrixEnabled, const VSMatrix& textureMatrix, bool textureMatrixEnabled);
	void Reset();

	uint32_t Offset() const { return mOffset; }

private:
	VkStreamBuffer* mBuffer;
	MatricesUBO mMatrices = {};
	VSMatrix mIdentityMatrix;
	uint32_t mOffset = 0;
};

class VkStreamBuffer
{
public:
	VkStreamBuffer(size_t structSize, size_t count);
	~VkStreamBuffer();

	uint32_t NextStreamDataBlock();
	void Reset() { mStreamDataOffset = 0; }

	VKDataBuffer* UniformBuffer = nullptr;

private:
	uint32_t mBlockSize = 0;
	uint32_t mStreamDataOffset = 0;
};
