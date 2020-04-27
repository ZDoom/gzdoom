/*
**  Vulkan backend
**  Copyright (c) 2016-2020 Magnus Norddahl
**
**  This software is provided 'as-is', without any express or implied
**  warranty.  In no event will the authors be held liable for any damages
**  arising from the use of this software.
**
**  Permission is granted to anyone to use this software for any purpose,
**  including commercial applications, and to alter it and redistribute it
**  freely, subject to the following restrictions:
**
**  1. The origin of this software must not be misrepresented; you must not
**     claim that you wrote the original software. If you use this software
**     in a product, an acknowledgment in the product documentation would be
**     appreciated but is not required.
**  2. Altered source versions must be plainly marked as such, and must not be
**     misrepresented as being the original software.
**  3. This notice may not be removed or altered from any source distribution.
**
*/

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

VkMatrixBufferWriter::VkMatrixBufferWriter()
{
	mBuffer = GetVulkanFrameBuffer()->MatrixBuffer;
	mIdentityMatrix.loadIdentity();
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
