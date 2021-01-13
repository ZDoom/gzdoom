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

#import "Metal/Metal.h"
#import "MetalKit/MetalKit.h"

#include <string.h>
#include "matrix.h"
#include "hwrenderer/scene/hw_drawstructs.h"
#include "hwrenderer/scene/hw_renderstate.h"
#include "hwrenderer/textures/hw_material.h"
#include "c_cvars.h"
#include "r_defs.h"
#include "r_data/r_translate.h"
#include "g_levellocals.h"
#include "metal/shaders/ml_shader.h"
#include "metal/system/MetalCocoaView.h"

#define SIZE_DEPTH_DESC 18
#define PIPELINE_STATE 57

namespace MetalRenderer
{

enum
{
    VATTR_VERTEX,
    VATTR_TEXCOORD,
    VATTR_COLOR,
    VATTR_VERTEX2,
    VATTR_NORMAL,
    VATTR_NORMAL2,
    
    VATTR_MAX
};

class MTLRenderBuffers;
class MTLShader;
struct HWSectorPlane;
static OBJC_ID(MTLDevice) device = MTLCreateSystemDefaultDevice();
class MTLRenderState : public FRenderState
{
    uint8_t mLastDepthClamp : 1;

    float mGlossiness, mSpecularLevel;
    float mShaderTimer;
    MLVertexBufferAttribute prevAttributeInfo[VATTR_MAX] = {};

    int mEffectState;
    int mTempTM = TM_NORMAL;
    
    bool needCreateDepthState : 1;

    FRenderStyle stRenderStyle;
    int stSrcBlend, stDstBlend;
    bool stAlphaTest;
    bool stSplitEnabled;
    int stBlendEquation;
    MTLRenderPassDescriptor* DefRenderPassDescriptor;
    bool needCreateDepthStare : 1;
    bool depthWriteEnabled : 1;
    int mNumDrawBuffers = 1;
    MTLColorWriteMask colorMask = MTLColorWriteMaskAll;
    bool updated : 1;

    bool ApplyShader();
    void ApplyState();

    // Texture binding state
    FMaterial *lastMaterial = nullptr;
    int lastClamp = 0;
    int lastTranslation = 0;
    int maxBoundMaterial = -1;
    size_t mLastMappedLightIndex = SIZE_MAX;
    int mScissorX;
    int mScissorY;
    int mScissorWidth;
    int mScissorHeight;
    
    struct RenderPipeline
    {
        int sizeAttr;
        MTLBlendFactor sourceRGBBlendFactor;
        MTLColorWriteMask colorMask;
    };

    IVertexBuffer *mCurrentVertexBuffer;
    int mCurrentVertexOffsets[2];    // one per binding point
    IIndexBuffer *mCurrentIndexBuffer;
    MTLRenderBuffers *buffers;
    MTLViewport m_Viewport;
    OBJC_ID(MTLFunction) VShader;
    OBJC_ID(MTLFunction) FShader;
    MTLVertexDescriptor *vertexDesc[VATTR_MAX];
    RenderPipeline renderPipeline[PIPELINE_STATE];
    int currentState = 0;

    
    struct DepthIndex
    {
        DepthIndex(MTLStencilOperation _stencil, MTLCompareFunction _compare, int _ind, bool _writeDepth)
        {
            stencilOperation = _stencil;
            compareFunction  = _compare;
            ind              = _ind;
            writeDepth       = _writeDepth;
        }
        ~DepthIndex() = default;
        DepthIndex()  = default;
        MTLStencilOperation stencilOperation;
        MTLCompareFunction  compareFunction;
        int                 ind;
        bool                writeDepth;
    };
    
    void CreateRenderPipelineState();
    
    DepthIndex depthIndex[SIZE_DEPTH_DESC];
    OBJC_ID(MTLDepthStencilState) depthState[SIZE_DEPTH_DESC];
    MTLRenderPipelineDescriptor * renderPipelineDesc;
    MTLDepthStencilDescriptor *depthStateDesc;
    MTLCompareFunction depthCompareFunc;
    bool needSetUniforms : 1;


public:
    OBJC_ID(MTLRenderPipelineState) pipelineState[PIPELINE_STATE];
    OBJC_ID(MTLLibrary) defaultLibrary;
    MTLShader *activeShader;
    int val = 0;
    OBJC_ID(MTLCommandQueue) commandQueue;
    OBJC_ID(MTLCommandBuffer) commandBuffer;
    OBJC_ID(MTLRenderCommandEncoder) renderCommandEncoder;
    int currentIndexVB = 0;
    OBJC_ID(MTLBuffer) mtl_vertexBuffer [3];
    OBJC_ID(MTLBuffer) mtl_indexBuffer [3];
    OBJC_ID(MTLBuffer) mtl_index [3];
    bool needDeleteVB = false;
    bool needDeleteIB = false;
    MTLBlendFactor srcblend = MTLBlendFactorSourceAlpha;
    MTLBlendFactor dstblend = MTLBlendFactorZero;
    int blendequation = 0;
    bool useBlendMode = false;
    
    size_t offsetVB[2];
    size_t offsetIB[2];
    size_t indexOffset[2];
    bool needCpyBuffer : 1;
    void CreateRenderState(MTLRenderPassDescriptor * renderPassDescriptor);
    void setVertexBuffer(OBJC_ID(MTLBuffer) buffer, size_t index, size_t offset = 0);
    void CopyVertexBufferAttribute(const MLVertexBufferAttribute* attr);
    bool VertexBufferAttributeWasChange(const MLVertexBufferAttribute* attr);
    int  FindDepthIndex (MTLDepthStencilDescriptor* desc);
    void AllocDesc()
    {
        CreateFanToTrisIndexBuffer();
        if (vertexDesc[0] == nil)      vertexDesc[0] = [[MTLVertexDescriptor alloc] init];
        if (vertexDesc[1] == nil)      vertexDesc[1] = [[MTLVertexDescriptor alloc] init];
        if (renderPipelineDesc == nil) renderPipelineDesc = [[MTLRenderPipelineDescriptor alloc] init];
        if (depthStateDesc == nil)     depthStateDesc = [[MTLDepthStencilDescriptor alloc] init];
        
        depthWriteEnabled = false;
        MTLStencilOperation op2ml[] = { MTLStencilOperationKeep, MTLStencilOperationIncrementClamp, MTLStencilOperationDecrementClamp };
        //                                          { GL_KEEP,                        GL_INCR,                          GL_DECR };
        MTLCompareFunction  df2ml[] = { MTLCompareFunctionLess, MTLCompareFunctionLessEqual, MTLCompareFunctionAlways };
        //                                  {         GL_LESS,                GL_LEQUAL,                   GL_ALWAYS };
        
        int val = 0;
        for (int i = 0; i < 3; i++)
        {
            for (int j = 0; j < 3; j++)
            {
                depthStateDesc.frontFaceStencil.stencilCompareFunction    = MTLCompareFunctionEqual;
                depthStateDesc.frontFaceStencil.stencilFailureOperation   = MTLStencilOperationKeep;
                depthStateDesc.frontFaceStencil.depthStencilPassOperation = op2ml[i];
                depthStateDesc.backFaceStencil.stencilCompareFunction     = MTLCompareFunctionEqual;
                depthStateDesc.backFaceStencil.stencilFailureOperation    = MTLStencilOperationKeep;
                depthStateDesc.backFaceStencil.depthStencilPassOperation  = op2ml[i];
                depthStateDesc.depthCompareFunction =  df2ml[j];
                depthStateDesc.depthWriteEnabled = YES;
                
                depthState[val] = [device newDepthStencilStateWithDescriptor: depthStateDesc];
                depthIndex[val] = {op2ml[i], df2ml[j], val, true};
                val++;
                
                depthStateDesc.depthWriteEnabled = NO;
                depthState[val] = [device newDepthStencilStateWithDescriptor: depthStateDesc];
                depthIndex[val] = {op2ml[i], df2ml[j], val, false};
                val++;
            }
        }
        offsetVB[0] = offsetVB[1] = 0;
        offsetIB[0] = offsetIB[1] = 0;
        indexOffset[0] = indexOffset[1] = 0;
        needCpyBuffer = false;
    }
    
    MTLRenderState() = default;
    
    MTLRenderState(MTLRenderBuffers *buffers) : buffers(buffers)
    {
    }
    
    virtual ~MTLRenderState()
    {
        Reset();
    }
    
    void InitialaziState();
    
    void Reset();

    void ClearLastMaterial()
    {
        lastMaterial = nullptr;
    }

    void ApplyMaterial(FMaterial *mat, int clampmode, int translation, int overrideshader);

    void EndFrame();
    void Apply();
    void ApplyBuffers();
    void ApplyBlendMode();
    
    void ResetVertexBuffer()
    {
        // forces rebinding with the next 'apply' call.
        mCurrentVertexBuffer = nullptr;
        mCurrentIndexBuffer = nullptr;
    }

    void SetSpecular(float glossiness, float specularLevel)
    {
        mGlossiness = glossiness;
        mSpecularLevel = specularLevel;
    }

    void EnableDrawBuffers(int count) override
    {
        count = MIN(count, 3);
        if (mNumDrawBuffers != count)
        {
            mNumDrawBuffers = count;
        }
    }

    void ToggleState(int state, bool on);

    void ClearScreen() override;
    void Draw(int dt, int index, int count, bool apply = true) override;
    void DrawIndexed(int dt, int index, int count, bool apply = true) override;
    void CreateFanToTrisIndexBuffer();
    OBJC_ID(MTLBuffer) fanIndexBuffer;

    bool SetDepthClamp(bool on) override;
    void SetDepthMask(bool on) override;
    void SetDepthFunc(int func) override;
    void SetDepthRange(float min, float max) override;
    void SetColorMask(bool r, bool g, bool b, bool a) override;
    void SetStencil(int offs, int op, int flags) override;
    void SetCulling(int mode) override;
    void EnableClipDistance(int num, bool state) override;
    void Clear(int targets) override;
    void EnableStencil(bool on) override;
    void SetScissor(int x, int y, int w, int h) override;
    void SetViewport(int x, int y, int w, int h) override;
    void EnableDepthTest(bool on) override;
    void EnableMultisampling(bool on) override;
    void EnableLineSmooth(bool on) override;

};

//extern MTLRenderState* ml_RenderState;
static MetalCocoaView* m_view;

}
