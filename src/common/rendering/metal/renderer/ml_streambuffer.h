//
//---------------------------------------------------------------------------
//
// Copyright(C) 2020-2021 Eugene Grigorchuk
// All rights reserved.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//--------------------------------------------------------------------------
//

#pragma once

#include "metal/system/ml_buffer.h"
#include "metal/shaders/ml_shader.h"

namespace MetalRenderer
{
class MTLStreamBuffer
{
public:
    MTLStreamBuffer(size_t structSize, size_t count);
    ~MTLStreamBuffer();

    uint32_t NextStreamDataBlock();
    void Reset() { mStreamDataOffset = 0; }

    MTLDataBuffer* UniformBuffer = nullptr;

private:
    uint32_t mBlockSize = 0;
    uint32_t mStreamDataOffset = 0;
};

class MTLStreamBufferWriter
{
public:
	MTLStreamBufferWriter();

	bool Write(const StreamData& data);
	void Reset();

	uint32_t DataIndex() const { return mDataIndex; }
	uint32_t StreamDataOffset() const { return mStreamDataOffset; }

private:
	MTLStreamBuffer* mBuffer;
	uint32_t mDataIndex = 255;
	uint32_t mStreamDataOffset = 0;
};

class MTLMatrixBufferWriter
{
public:
	MTLMatrixBufferWriter();

	bool Write(const VSMatrix& modelMatrix, bool modelMatrixEnabled, const VSMatrix& textureMatrix, bool textureMatrixEnabled);
	void Reset();

	uint32_t Offset() const { return mOffset; }

private:
	MTLStreamBuffer* mBuffer;
	MatricesUBO mMatrices = {};
	VSMatrix mIdentityMatrix;
	uint32_t mOffset = 0;
};

}
