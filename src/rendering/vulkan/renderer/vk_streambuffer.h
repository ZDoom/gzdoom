
#pragma once

#include "vulkan/system/vk_buffers.h"
#include "vulkan/shaders/vk_shader.h"

class VkStreamBuffer;

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

class VkStreamBuffer
{
public:
	VkStreamBuffer();
	~VkStreamBuffer();

	uint32_t NextStreamDataBlock();
	void Reset() { mStreamDataOffset = 0; }

	VKDataBuffer* UniformBuffer = nullptr;

private:
	uint32_t mStreamDataOffset = 0;
};

class VkMatrixBuffer
{
public:
	VkMatrixBuffer();
	~VkMatrixBuffer();

	bool Write(const VSMatrix &modelMatrix, bool modelMatrixEnabled, const VSMatrix& textureMatrix, bool textureMatrixEnabled);
	void Reset();

	uint32_t Offset() const { return mOffset; }

	VKDataBuffer* UniformBuffer = nullptr;

private:
	MatricesUBO mMatrices = {};
	VSMatrix mIdentityMatrix;
	uint32_t mOffset = 0;
};
