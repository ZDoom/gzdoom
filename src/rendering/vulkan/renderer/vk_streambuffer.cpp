
#include "vk_renderstate.h"
#include "vulkan/system/vk_framebuffer.h"
#include "vulkan/system/vk_builders.h"
#include "vulkan/renderer/vk_streambuffer.h"

template<typename T>
int UniformBufferAlignedSize(int count) { return ((sizeof(T) + screen->uniformblockalignment - 1) / screen->uniformblockalignment * screen->uniformblockalignment) * count; }

VkStreamBuffer::VkStreamBuffer()
{
	auto fb = GetVulkanFrameBuffer();
	UniformBuffer = (VKDataBuffer*)fb->CreateDataBuffer(-1, false, false);
	UniformBuffer->SetData(UniformBufferAlignedSize<StreamUBO>(200), nullptr, false);
}

VkStreamBuffer::~VkStreamBuffer()
{
	delete UniformBuffer;
}

uint32_t VkStreamBuffer::NextStreamDataBlock()
{
	mStreamDataOffset += sizeof(StreamUBO);
	if (mStreamDataOffset + sizeof(StreamUBO) >= UniformBuffer->Size())
	{
		mStreamDataOffset = 0;
		return 0xffffffff;
	}
	return mStreamDataOffset;
}

/////////////////////////////////////////////////////////////////////////////

VkStreamBufferWriter::VkStreamBufferWriter()
{
	mBuffer = GetVulkanFrameBuffer()->StreamBuffer;
}

bool VkStreamBufferWriter::Write(const StreamData& data)
{
	mDataIndex++;
	if (mDataIndex == MAX_STREAM_DATA)
	{
		mDataIndex = 0;
		mStreamDataOffset = mBuffer->NextStreamDataBlock();
		if (mStreamDataOffset == 0xffffffff)
			return false;
	}
	uint8_t* ptr = (uint8_t*)mBuffer->UniformBuffer->Memory();
	memcpy(ptr + mStreamDataOffset + sizeof(StreamData) * mDataIndex, &data, sizeof(StreamData));
	return true;
}

void VkStreamBufferWriter::Reset()
{
	mDataIndex = MAX_STREAM_DATA - 1;
	mStreamDataOffset = 0;
	mBuffer->Reset();
}

/////////////////////////////////////////////////////////////////////////////

VkMatrixBuffer::VkMatrixBuffer()
{
	mIdentityMatrix.loadIdentity();

	auto fb = GetVulkanFrameBuffer();
	UniformBuffer = (VKDataBuffer*)fb->CreateDataBuffer(-1, false, false);
	UniformBuffer->SetData(UniformBufferAlignedSize<MatricesUBO>(50000), nullptr, false);
}

VkMatrixBuffer::~VkMatrixBuffer()
{
	delete UniformBuffer;
}

template<typename T>
static void BufferedSet(bool& modified, T& dst, const T& src)
{
	if (dst == src)
		return;
	dst = src;
	modified = true;
}

static void BufferedSet(bool& modified, VSMatrix& dst, const VSMatrix& src)
{
	if (memcmp(dst.get(), src.get(), sizeof(FLOATTYPE) * 16) == 0)
		return;
	dst = src;
	modified = true;
}

bool VkMatrixBuffer::Write(const VSMatrix& modelMatrix, bool modelMatrixEnabled, const VSMatrix& textureMatrix, bool textureMatrixEnabled)
{
	bool modified = (mOffset == 0); // always modified first call

	if (modelMatrixEnabled)
	{
		BufferedSet(modified, mMatrices.ModelMatrix, modelMatrix);
		if (modified)
			mMatrices.NormalModelMatrix.computeNormalMatrix(modelMatrix);
	}
	else
	{
		BufferedSet(modified, mMatrices.ModelMatrix, mIdentityMatrix);
		BufferedSet(modified, mMatrices.NormalModelMatrix, mIdentityMatrix);
	}

	if (textureMatrixEnabled)
	{
		BufferedSet(modified, mMatrices.TextureMatrix, textureMatrixEnabled);
	}
	else
	{
		BufferedSet(modified, mMatrices.TextureMatrix, mIdentityMatrix);
	}

	if (modified)
	{
		if (mOffset + (size_t)UniformBufferAlignedSize<MatricesUBO>(2) >= UniformBuffer->Size())
			return false;

		mOffset += UniformBufferAlignedSize<MatricesUBO>(1);
		memcpy(static_cast<uint8_t*>(UniformBuffer->Memory()) + mOffset, &mMatrices, sizeof(MatricesUBO));
	}

	return true;
}

void VkMatrixBuffer::Reset()
{
	mOffset = 0;
}
