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

#include "vk_streambuffer.h"
#include "vulkan/vk_renderstate.h"
#include "vulkan/vk_renderdevice.h"
#include "vulkan/buffers/vk_buffer.h"
#include <zvulkan/vulkanbuilders.h>

VkStreamBufferWriter::VkStreamBufferWriter(VulkanRenderDevice* fb)
{
	mBuffer = fb->GetBufferManager()->StreamBuffer.get();
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
	uint8_t* ptr = (uint8_t*)mBuffer->Data;
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

VkMatrixBufferWriter::VkMatrixBufferWriter(VulkanRenderDevice* fb)
{
	mBuffer = fb->GetBufferManager()->MatrixBuffer.get();
}

bool VkMatrixBufferWriter::Write(const MatricesUBO& matrices)
{
	mOffset = mBuffer->NextStreamDataBlock();
	if (mOffset == 0xffffffff)
		return false;

	uint8_t* ptr = (uint8_t*)mBuffer->Data;
	memcpy(ptr + mOffset, &matrices, sizeof(MatricesUBO));
	return true;
}

void VkMatrixBufferWriter::Reset()
{
	mOffset = 0;
	mBuffer->Reset();
}
