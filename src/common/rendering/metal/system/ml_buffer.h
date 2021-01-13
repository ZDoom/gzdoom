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

#import <Metal/Metal.h>
#import "simd/simd.h"
#include "utility/tarray.h"
#include "hwrenderer/data/buffers.h"

#ifdef _MSC_VER
// silence bogus warning C4250: 'VKVertexBuffer': inherits 'VKBuffer::VKBuffer::SetData' via dominance
// According to internet infos, the warning is erroneously emitted in this case.
#pragma warning(disable:4250)
#endif

namespace MetalRenderer
{
static struct MLVertexBufferAttribute
{
    int bindingpoint;
    int format;
    int size;
    int offset;
};

class MTLBuffer : virtual public IBuffer
{
public:
    MTLBuffer();
    ~MTLBuffer();

    //static void ResetAll();
    void Reset();

    void SetData(size_t size, const void *data, bool staticdata) override;
    void SetSubData(size_t offset, size_t size, const void *data) override;
    void Resize(size_t newsize) override;

    void Map() override;
    void Unmap() override;
    
    void* GetData();

    void *Lock(unsigned int size) override;
    void Unlock() override;
    
    MTLResourceOptions option;

    bool mPersistent = false;

    void*                 mBuffer;
private:
    //void*                 mBuffer;
    //uint                m_IndicesCount;
    //uint                indexSize;
};

class MTLVertexBuffer : public IVertexBuffer, public MTLBuffer
{
    int mNumBindingPoints;
public:
    MLVertexBufferAttribute mAttributeInfo[VATTR_MAX] = {};
    size_t mStride = 0;
    MTLVertexBuffer();
    ~MTLVertexBuffer();
    void SetFormat(int numBindingPoints, int numAttributes, size_t stride, const FVertexBufferAttribute *attrs) override;
    void Bind(int *offsets);

    int VertexFormat = -1;
    int sizeBuffer;
};

class MTLIndexBuffer : public IIndexBuffer, public MTLBuffer
{
public:
    MTLIndexBuffer() {option = MTLResourceStorageModeShared;}

    int bindingpoint;
};

class MTLDataBuffer : public IDataBuffer, public MTLBuffer
{
public:
    MTLDataBuffer(){};

    void BindRange(FRenderState *state, size_t start, size_t length) override;

    int bindingpoint;
};

static size_t getStrideForAttr(MLVertexBufferAttribute attr)
{
    switch (attr.format)
    {
        case MTLVertexFormatFloat:
            return sizeof(float) * attr.size;
            
        case MTLVertexFormatUInt:
            return sizeof(uint32_t) * attr.size;
            
        case MTLVertexFormatUInt1010102Normalized:
            return 32 * attr.size;
    }
    
    assert(true);
    return 0;
}

}
