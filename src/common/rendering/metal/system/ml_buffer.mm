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

#include "v_video.h"
#include "ml_buffer.h"
#include "ml_framebuffer.h"
#include "metal/renderer/ml_renderstate.h"
#include "metal/renderer/ml_renderer.h"


namespace MetalRenderer
{

static inline void InvalidateBufferState()
{
    if (MLRenderer && MLRenderer->ml_RenderState)
        MLRenderer->ml_RenderState->ResetVertexBuffer();    // force rebinding of buffers on next Apply call.
}

MTLBuffer::MTLBuffer()
{
    mBuffer = nullptr;
}

MTLBuffer::~MTLBuffer()
{
    Reset();
}

void MTLBuffer::Reset()
{
   if (mBuffer)
    free(mBuffer);
    
    mBuffer = nullptr;
}

void MTLBuffer::SetData(size_t size, const void *data, bool staticdata)
{
    if (staticdata)
    {
        if (data != nullptr)
        {
            mBuffer = malloc(size);
            memcpy(mBuffer, data, size);
        }
        else
        {
            mPersistent = screen->BuffersArePersistent() && !staticdata;
            mBuffer = malloc(size);
            map = mBuffer;
        }
    }
    else
    {
        if (data != nullptr)
        {
            mBuffer = malloc(size);
            memcpy(mBuffer, data, size);
        }
        else
        {
            mPersistent = screen->BuffersArePersistent() && !staticdata;
            mBuffer = malloc(size);
            map = mBuffer;
        }
    }
    buffersize = size;
    InvalidateBufferState();
}

void MTLBuffer::SetSubData(size_t offset, size_t size, const void *data)
{
    void * buff = mBuffer;
    if (buff != nullptr)
    {
        memcpy((&buff + offset), data, size);
        buffersize = size;
    }
}

void MTLBuffer::Resize(size_t newsize)
{
    buffersize = newsize;
}

void MTLBuffer::Map()
{
    if (!mPersistent)
    {
        map = mBuffer;
        InvalidateBufferState();
    }
}

void MTLBuffer::Unmap()
{
    if (!mPersistent)
    {
        InvalidateBufferState();
        map = nullptr;
    }
}

void* MTLBuffer::GetData()
{
    return mBuffer;
}

void *MTLBuffer::Lock(unsigned int size)
{
    return mBuffer;
}

void MTLBuffer::Unlock()
{
    InvalidateBufferState();
}

/////////////////////////////////////////////////////////////////////////////


void MTLVertexBuffer::SetFormat(int numBindingPoints, int numAttributes, size_t stride, const FVertexBufferAttribute *attrs)
{
    static int VFmtToGLFmt[] = { MTLVertexFormatFloat, MTLVertexFormatFloat, MTLVertexFormatFloat, MTLVertexFormatFloat, MTLVertexFormatUInt, MTLVertexFormatUInt1010102Normalized };
    static uint8_t VFmtToSize[] = {4, 3, 2, 1, 4, 4};
    sizeBuffer = 0;
    
    mStride = stride;
    mNumBindingPoints = numBindingPoints;
    
    for(int i = 0; i < numAttributes; i++)
    {
        if (attrs[i].location >= 0 && attrs[i].location < VATTR_MAX)
        {
            auto & attrinf = mAttributeInfo[attrs[i].location];
            attrinf.format = VFmtToGLFmt[attrs[i].format];
            attrinf.size = VFmtToSize[attrs[i].format];
            sizeBuffer += attrinf.size;
            attrinf.offset = attrs[i].offset;
            attrinf.bindingpoint = attrs[i].binding;
        }
    }
}

MTLVertexBuffer::MTLVertexBuffer()
{
    option = MTLResourceStorageModeShared;
}

MTLVertexBuffer::~MTLVertexBuffer()
{

}

void MTLVertexBuffer::Bind(int *offsets)
{
 
    for(auto &attrinf : mAttributeInfo)
    {
        if (attrinf.size != 0)
        {
            size_t ofs = offsets == nullptr ? attrinf.offset : attrinf.offset + mStride * offsets[attrinf.bindingpoint];
        }
        
    }
}


/////////////////////////////////////////////////////////////////////////////


void MTLDataBuffer::BindRange(FRenderState* state, size_t start, size_t length)
{
    HWViewpointUniforms *uniforms = ((HWViewpointUniforms*)((float*)mBuffer + (start/4)));
    //HWViewpointUniforms *uniforms1 = ((HWViewpointUniforms*)malloc(length));
    //HWViewpointUniforms *uniforms = (HWViewpointUniforms*)malloc(length);
    size_t index = 0;//start/256;
    //state->Pro
    mtlHWViewpointUniforms *hwUniforms = MLRenderer->mHWViewpointUniforms;
    float* proj = uniforms[index].mProjectionMatrix.Get();
    hwUniforms->mProjectionMatrix = matrix_float4x4{vector_float4{proj[0] , proj[1] , proj[2] , proj[3]},
                                                    vector_float4{proj[4] , proj[5] , proj[6] , proj[7]},
                                                    vector_float4{proj[8] , proj[9] , proj[10], proj[11]},
                                                    vector_float4{proj[12], proj[13], proj[14], proj[15]}};
    
    proj = uniforms[index].mViewMatrix.Get();
    hwUniforms->mViewMatrix = matrix_float4x4{vector_float4{proj[0] , proj[1] , proj[2] , proj[3]},
                                              vector_float4{proj[4] , proj[5] , proj[6] , proj[7]},
                                              vector_float4{proj[8] , proj[9] , proj[10], proj[11]},
                                              vector_float4{proj[12], proj[13], proj[14], proj[15]}};
    
    proj = uniforms[index].mNormalViewMatrix.Get();
    hwUniforms->mNormalViewMatrix = matrix_float4x4{vector_float4{proj[0] , proj[1] , proj[2] , proj[3]},
                                                    vector_float4{proj[4] , proj[5] , proj[6] , proj[7]},
                                                    vector_float4{proj[8] , proj[9] , proj[10], proj[11]},
                                                    vector_float4{proj[12], proj[13], proj[14], proj[15]}};
    
    hwUniforms->mClipLine  = vector_float4{uniforms->mClipLine.X,uniforms->mClipLine.Y,uniforms->mClipLine.Z,uniforms->mClipLine.W};
    hwUniforms->mCameraPos = vector_float4{uniforms->mCameraPos.X,uniforms->mCameraPos.Y,uniforms->mCameraPos.Z,uniforms->mCameraPos.W};
    hwUniforms->mGlobVis = uniforms->mGlobVis;
    hwUniforms->mPalLightLevels = uniforms->mPalLightLevels;
    hwUniforms->mViewHeight = uniforms[index].mViewHeight;
    hwUniforms->mClipHeight = uniforms[index].mClipHeight;
    hwUniforms->mClipHeightDirection = uniforms[index].mClipHeightDirection;
    hwUniforms->mShadowmapFilter = uniforms[index].mShadowmapFilter;

    //OBJC_ID(MTLBuffer) buff = [device newBufferWithBytes:hwUniforms length:sizeof(hwUniforms) options:MTLResourceStorageModeShared];
    //ml_RenderState.setVertexBuffer(buff, 4);
}

}
