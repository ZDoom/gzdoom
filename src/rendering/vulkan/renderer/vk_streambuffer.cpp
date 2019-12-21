
#include "vk_renderstate.h"
#include "vulkan/system/vk_framebuffer.h"
#include "vulkan/system/vk_builders.h"
#include "vulkan/renderer/vk_streambuffer.h"

VkStreamBuffer::VkStreamBuffer(size_t structSize, size_t count)
{
	mBlockSize = static_cast<uint32_t>((structSize + screen->uniformblockalignment - 1) / screen->uniformblockalignment * screen->uniformblockalignment);

	UniformBuffer = (VKDataBuffer*)GetVulkanFrameBuffer()->CreateDataBuffer(-1, false, false);
	UniformBuffer->SetData(mBlockSize * count, nullptr, false);
}

VkStreamBuffer::~VkStreamBuffer()
{
	delete UniformBuffer;
}

uint32_t VkStreamBuffer::NextStreamDataBlock()
{
	mStreamDataOffset += mBlockSize;
	if (mStreamDataOffset + (size_t)mBlockSize >= UniformBuffer->Size())
	{
		mStreamDataOffset = 0;
		return 0xffffffff;
	}
	return mStreamDataOffset;
}

/////////////////////////////////////////////////////////////////////////////

VkStreamBufferWriter::VkStreamBufferWriter()
{
	mStreamDataSize = GetVulkanFrameBuffer()->GetRenderState()->GetUniformDataSize(UniformFamily::Normal);
	mMaxStreamData = 65536 / mStreamDataSize;
	mBuffer = new VkStreamBuffer(mStreamDataSize * mMaxStreamData, 300);
	mDataIndex = mMaxStreamData - 1;
	mStreamDataOffset = 0;
}

VkStreamBufferWriter::~VkStreamBufferWriter()
{
	delete mBuffer;
}

bool VkStreamBufferWriter::Write(const void* data)
{
	mDataIndex++;
	if (mDataIndex == mMaxStreamData)
	{
		mDataIndex = 0;
		mStreamDataOffset = mBuffer->NextStreamDataBlock();
		if (mStreamDataOffset == 0xffffffff)
			return false;
	}
	uint8_t* ptr = (uint8_t*)mBuffer->UniformBuffer->Memory();
	memcpy(ptr + mStreamDataOffset + mStreamDataSize * mDataIndex, data, mStreamDataSize);
	return true;
}

void VkStreamBufferWriter::Reset()
{
	mDataIndex = mMaxStreamData - 1;
	mStreamDataOffset = 0;
	mBuffer->Reset();
}

/////////////////////////////////////////////////////////////////////////////

VkMatrixBufferWriter::VkMatrixBufferWriter()
{
	auto fb = GetVulkanFrameBuffer();
	mBuffer = new VkStreamBuffer(sizeof(MatricesUBO), 50000);
	mIdentityMatrix.loadIdentity();
}

VkMatrixBufferWriter::~VkMatrixBufferWriter()
{
	delete mBuffer;
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

bool VkMatrixBufferWriter::Write(const VSMatrix& modelMatrix, bool modelMatrixEnabled, const VSMatrix& textureMatrix, bool textureMatrixEnabled)
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
		BufferedSet(modified, mMatrices.TextureMatrix, textureMatrix);
	}
	else
	{
		BufferedSet(modified, mMatrices.TextureMatrix, mIdentityMatrix);
	}

	if (modified)
	{
		mOffset = mBuffer->NextStreamDataBlock();
		if (mOffset == 0xffffffff)
			return false;

		uint8_t* ptr = (uint8_t*)mBuffer->UniformBuffer->Memory();
		memcpy(ptr + mOffset, &mMatrices, sizeof(MatricesUBO));
	}

	return true;
}

void VkMatrixBufferWriter::Reset()
{
	mOffset = 0;
	mBuffer->Reset();
}
