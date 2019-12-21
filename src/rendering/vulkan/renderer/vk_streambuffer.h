
#pragma once

#include "vulkan/system/vk_buffers.h"
#include "vulkan/shaders/vk_shader.h"

class VKDataBuffer;

class VkStreamBuffer
{
public:
	VkStreamBuffer(size_t structSize, size_t count);
	~VkStreamBuffer();

	uint32_t NextStreamDataBlock();
	void Reset() { mStreamDataOffset = 0; }

	VKDataBuffer* UniformBuffer = nullptr;
	uint32_t mBlockSize = 0;

private:
	uint32_t mStreamDataOffset = 0;
};

class VkStreamBufferWriter
{
public:
	VkStreamBufferWriter();
	~VkStreamBufferWriter();

	bool Write(const void* data);
	void Reset();

	uint32_t DataIndex() const { return mDataIndex; }
	uint32_t StreamDataOffset() const { return mStreamDataOffset; }
	VKDataBuffer* Buffer() const { return mBuffer->UniformBuffer; }
	uint32_t BlockSize() const { return mBuffer->mBlockSize; }
	uint32_t MaxStreamData() const { return mMaxStreamData; }

private:
	VkStreamBuffer* mBuffer;
	uint32_t mStreamDataSize;
	uint32_t mMaxStreamData;
	uint32_t mDataIndex;
	uint32_t mStreamDataOffset;
};

class VkMatrixBufferWriter
{
public:
	VkMatrixBufferWriter();
	~VkMatrixBufferWriter();

	bool Write(const VSMatrix& modelMatrix, bool modelMatrixEnabled, const VSMatrix& textureMatrix, bool textureMatrixEnabled);
	void Reset();

	uint32_t Offset() const { return mOffset; }
	VKDataBuffer* Buffer() const { return mBuffer->UniformBuffer; }

private:
	VkStreamBuffer* mBuffer;
	MatricesUBO mMatrices = {};
	VSMatrix mIdentityMatrix;
	uint32_t mOffset = 0;
};
