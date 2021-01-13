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

#include "templates.h"
#include "doomstat.h"
#include "r_data/colormaps.h"
#include "gl_load/gl_system.h"
#include "gl_load/gl_interface.h"
#include "hwrenderer/utility/hw_cvars.h"
#include "hwrenderer/data/flatvertices.h"
#include "hwrenderer/textures/hw_material.h"
#include "hwrenderer/scene/hw_skydome.h"
#include "metal/shaders/ml_shader.h"
#include "metal/renderer/ml_renderer.h"
#include "hwrenderer/dynlights/hw_lightbuffer.h"
#include "metal/renderer/ml_renderbuffers.h"
#include "metal/renderer/ml_RenderState.h"
#include "metal/textures/ml_hwtexture.h"
#include "metal/system/ml_buffer.h"
#include "hwrenderer/utility/hw_clock.h"
#include "hwrenderer/data/hw_viewpointbuffer.h"
#include <simd/simd.h>
#include <math.h>

namespace MetalRenderer
{

typedef struct
{
    matrix_float4x4 ModelMatrix;
    matrix_float4x4 TextureMatrix;
    matrix_float4x4 NormalModelMatrix;
} PerView;

static VSMatrix identityMatrix(1);

//static void matrixToGL(const VSMatrix &mat, int loc)
//{
//    glUniformMatrix4fv(loc, 1, false, (float*)&mat);
//}

//==========================================================================
//
// This only gets called once upon setup.
// With OpenGL the state is persistent and cannot be cleared, once set up.
//
//==========================================================================

void MTLRenderState::Reset()
{
    FRenderState::Reset();
    mGlossiness = 0.0f;
    mSpecularLevel = 0.0f;
    mShaderTimer = 0.0f;
    mCurrentVertexOffsets[0] = mVertexOffsets[0] = 0;
    stSrcBlend = stDstBlend = -1;
    stBlendEquation = -1;
    stAlphaTest = 0;
    mLastDepthClamp = true;
    mEffectState = 0;
    
    stRenderStyle = DefaultRenderStyle();

    if (mCurrentVertexBuffer)
    {
        delete mCurrentVertexBuffer;
        mCurrentVertexBuffer = nullptr;
    }
    if (mVertexBuffer)
    {
        delete mVertexBuffer;
        mVertexBuffer = nullptr;
    }
    if (mCurrentIndexBuffer)
    {
        delete mCurrentIndexBuffer;
        mCurrentIndexBuffer = nullptr;
    }
    
    if (activeShader)
    {
        delete activeShader;
        activeShader = nullptr;
    }
    
    if (needDeleteVB)
    {
        [mtl_vertexBuffer[0] release];
        [mtl_vertexBuffer[1] release];
        [mtl_vertexBuffer[2] release];
    }
    
    if  (needDeleteIB)
    {
        [mtl_indexBuffer[0] release];
        [mtl_indexBuffer[1] release];
        [mtl_indexBuffer[2] release];
    }
    
}


//==========================================================================
//
// Apply State
//
//==========================================================================

void MTLRenderState::ApplyState()
{
    if (mRenderStyle != stRenderStyle)
    {
        ApplyBlendMode();
        stRenderStyle = mRenderStyle;
    }

    if (mSplitEnabled != stSplitEnabled)
    {
        stSplitEnabled = mSplitEnabled;
    }

    if (mMaterial.mChanged)
    {
        ApplyMaterial(mMaterial.mMaterial, mMaterial.mClampMode, mMaterial.mTranslation, mMaterial.mOverrideShader);
        mMaterial.mChanged = false;
    }
    
    //commandBuffer = [commandQueue commandBuffer];
    //commandBuffer.label = @"RenderStateCommandBuffer";

    //Is this need or not?
    if (mBias.mChanged)
    {
        //if (mBias.mFactor == 0 && mBias.mUnits == 0)
        //{
        //    glDisable(GL_POLYGON_OFFSET_FILL);
        //}
        //else
        //{
        //    glEnable(GL_POLYGON_OFFSET_FILL);
        //}
        [renderCommandEncoder setDepthBias:mBias.mFactor slopeScale:mBias.mUnits clamp:300];
        //glPolygonOffset(mBias.mFactor, mBias.mUnits);
        mBias.mChanged = false;
    }
}
    

typedef struct
{
    float uDesaturationFactor;
    vector_float4 uCameraPos;
    float uGlobVis;
    int uPalLightLevels;
    int uFogEnabled;
    vector_float4 uGlowTopColor;
    vector_float4 uGlowBottomColor;
    int uTextureMode;
    vector_float4 uFogColor;
    vector_float4 uObjectColor;
    vector_float4 uObjectColor2;
    vector_float4 uAddColor;
    vector_float4 uDynLightColor;
    float timer;
    bool mTextureEnabled;
    
    //#define uLightLevel uLightAttr.a
    //#define uFogDensity uLightAttr.b
    //#define uLightFactor uLightAttr.g
    //#define uLightDist uLightAttr.r
    vector_float4 uLightAttr;
    vector_float4 aColor;
} Uniforms;

typedef struct
{
    vector_float2 uClipSplit;
    vector_float4 uSplitTopPlane;
    float         uInterpolationFactor;
    vector_float4 uGlowTopColor;
    vector_float4 uGlowTopPlane;
    vector_float4 uGlowBottomPlane;
    vector_float4 uObjectColor2;
    vector_float4 uSplitBottomPlane;
    vector_float4 uGradientBottomPlane;
    vector_float4 uGlowBottomColor;
    vector_float4 uGradientTopPlane;
    vector_float4 aNormal;
} VSUniforms;

typedef struct
{
    vector_float4   uClipLine;
    float           uClipHeight;
    float           uClipHeightDirection;
    simd::float4x4  ProjectionMatrix;
    simd::float4x4  ViewMatrix;
    simd::float4x4  NormalViewMatrix;
} ViewpointUBO;

matrix_float4x4 matrix_perspective_right_hand(float fovyRadians, float aspect, float nearZ, float farZ)
{
    float ys = 1 / tanf(fovyRadians * 0.5);
    float xs = ys / aspect;
    float zs = farZ / (nearZ - farZ);
    
    return (matrix_float4x4) {{
        { xs,   0,          0,  0 },
        {  0,  ys,          0,  0 },
        {  0,   0,         zs, -1 },
        {  0,   0, nearZ * zs,  0 }
    }};
}

matrix_float4x4 matrix_perspective_left_hand(float fovyRadians, float aspect, float nearZ, float farZ)
{
        const float yScale = 1.0f / std::tan((float)fovyRadians / 2.0f);
        const float xScale = yScale / aspect;
        const float farNearDiff = farZ - nearZ;

        //Left handed projection matrix
    return matrix_float4x4{vector_float4{xScale, 0.0f, 0.0f, 0.0f},
                           vector_float4{0.0f, yScale, 0.0f, 0.0f},
                           vector_float4{0.0f, 0.0f, 1.f / farNearDiff, 1.0f},
                           vector_float4{0.0f, 0.0f, (-nearZ * farZ) / farNearDiff, 0.0f}};
}

void MTLRenderState::InitialaziState()
{
    Reset();
    NSError* error = nil;
    if (defaultLibrary == nil) defaultLibrary = [device newLibraryWithFile: @"/Users/egrigorchuk/git/MetalGZDoom/metalShaders/doomMetallib.metallib"
                                                                     error:&error];
    if (VShader == nil)        VShader = [defaultLibrary newFunctionWithName:@"VertexMainSimple"];
    if (FShader  == nil)       FShader = [defaultLibrary newFunctionWithName:@"FragmentMainSimple"];
    if (commandQueue == nil)   commandQueue = [device newCommandQueueWithMaxCommandBufferCount:512];
    needCpyBuffer = true;
    if(error)
    {
        NSLog(@"Failed to created pipeline state, error %@", error);
        assert(true);
    }
    
    AllocDesc();
    CreateRenderPipelineState();
}

void MTLRenderState::CreateRenderPipelineState()
{
    //##########################ATTRIBUTE 0#########################
    vertexDesc[0].attributes[0].format = MTLVertexFormatFloat3;
    vertexDesc[0].attributes[0].offset = 0;
    vertexDesc[0].attributes[0].bufferIndex = 0;
    //##########################ATTRIBUTE 1#########################
    vertexDesc[0].attributes[1].format = MTLVertexFormatFloat2;
    vertexDesc[0].attributes[1].offset = 12;
    vertexDesc[0].attributes[1].bufferIndex = 0;
    //##########################ATTRIBUTE 2#########################
    vertexDesc[0].attributes[2].format = MTLVertexFormatUChar4Normalized_BGRA;
    vertexDesc[0].attributes[2].offset = 20;
    vertexDesc[0].attributes[2].bufferIndex = 0;
    //##########################LAYOUTS#############################
    vertexDesc[0].layouts[0].stride = 24;
    vertexDesc[0].layouts[0].stepRate = 1;
    vertexDesc[0].layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;
    //##############################################################
    
    //##########################ATTRIBUTE 0#########################
    vertexDesc[1].attributes[0].format = MTLVertexFormatFloat3;
    vertexDesc[1].attributes[0].offset = 0;
    vertexDesc[1].attributes[0].bufferIndex = 0;
    //##########################ATTRIBUTE 1#########################
    vertexDesc[1].attributes[1].format = MTLVertexFormatFloat2;
    vertexDesc[1].attributes[1].offset = 12;
    vertexDesc[1].attributes[1].bufferIndex = 0;
    //##########################LAYOUTS#############################
    vertexDesc[1].layouts[0].stride = 20;
    vertexDesc[1].layouts[0].stepRate = 1;
    vertexDesc[1].layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;
    //##############################################################
        
    renderPipelineDesc.colorAttachments[0].pixelFormat = MTLPixelFormatRGBA16Float;//MTLPixelFormatBGRA8Unorm_sRGB;
    renderPipelineDesc.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float_Stencil8;
    renderPipelineDesc.stencilAttachmentPixelFormat = MTLPixelFormatDepth32Float_Stencil8;

    renderPipelineDesc.colorAttachments[0].alphaBlendOperation         = MTLBlendOperationAdd;
    renderPipelineDesc.colorAttachments[0].sourceAlphaBlendFactor      = MTLBlendFactorSourceAlpha;
    renderPipelineDesc.colorAttachments[0].blendingEnabled = YES;

    renderPipelineDesc.colorAttachments[0].destinationRGBBlendFactor   = MTLBlendFactorOneMinusSourceAlpha;
    renderPipelineDesc.colorAttachments[0].rgbBlendOperation           = MTLBlendOperationAdd;
    
    // fix it
    renderPipelineDesc.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    
    renderPipelineDesc.label = @"USED 3 ATTRIBUTES";
    renderPipelineDesc.vertexFunction = VShader;
    renderPipelineDesc.fragmentFunction = FShader;
    renderPipelineDesc.vertexDescriptor = vertexDesc[0];
    renderPipelineDesc.sampleCount = 1;
    
    
    MTLBlendFactor    styles[] =
    { MTLBlendFactorZero,                MTLBlendFactorOne,         MTLBlendFactorSourceAlpha,
      MTLBlendFactorOneMinusSourceAlpha, MTLBlendFactorSourceColor, MTLBlendFactorOneMinusSourceColor,
      MTLBlendFactorDestinationColor,    MTLBlendFactorOneMinusDestinationColor};
    
    MTLColorWriteMask mask[] = {MTLColorWriteMaskAll, MTLColorWriteMaskAlpha};
    NSError* error = nil;
    int index = 0;
    for (int j = 0; j < 2; j++)
    {
        renderPipelineDesc.colorAttachments[0].writeMask = mask[j];
        for(int i = 0; i < 7; i++)
        {
            renderPipelineDesc.colorAttachments[0].sourceRGBBlendFactor = styles[i];
            pipelineState[index] = [device newRenderPipelineStateWithDescriptor:renderPipelineDesc  error:&error];
            if(error)
            {
                NSLog(@"Failed to created pipeline state, error %@", error);
                assert(pipelineState);
            }
            renderPipeline[index].colorMask = mask[j];
            renderPipeline[index].sourceRGBBlendFactor = styles[i];
            renderPipeline[index].sizeAttr = 3;
            index++;
        }
    }
    renderPipelineDesc.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorSourceAlpha;
    for (int j = 0; j < 2; j++)
    {
        renderPipelineDesc.colorAttachments[0].writeMask = mask[j];
        for(int i = 0; i < 7; i++)
        {
            renderPipelineDesc.colorAttachments[0].sourceRGBBlendFactor = styles[i];
            pipelineState[index] = [device newRenderPipelineStateWithDescriptor:renderPipelineDesc  error:&error];
            if(error)
            {
                NSLog(@"Failed to created pipeline state, error %@", error);
                assert(pipelineState);
            }
            renderPipeline[index].colorMask = mask[j];
            renderPipeline[index].sourceRGBBlendFactor = styles[i];
            renderPipeline[index].sizeAttr = 3;
            index++;
        }
    }
    VShader = [defaultLibrary newFunctionWithName:@"VertexMainSimpleWithoutColor"];
    FShader = [defaultLibrary newFunctionWithName:@"FragmentMainSimpleWithoutColor"];
    renderPipelineDesc.label = @"USED 2 ATTRIBUTES";
    renderPipelineDesc.vertexFunction = VShader;
    renderPipelineDesc.fragmentFunction = FShader;
    renderPipelineDesc.vertexDescriptor = vertexDesc[1];
    renderPipelineDesc.sampleCount = 1;
    
    for (int j = 0; j < 2; j++)
    {
        renderPipelineDesc.colorAttachments[0].writeMask = mask[j];
        for(int i = 0; i < 7; i++)
        {
            renderPipelineDesc.colorAttachments[0].sourceRGBBlendFactor = styles[i];
            pipelineState[index] = [device newRenderPipelineStateWithDescriptor:renderPipelineDesc  error:&error];
            if(error)
            {
                NSLog(@"Failed to created pipeline state, error %@", error);
                assert(pipelineState);
            }
            renderPipeline[index].colorMask = mask[j];
            renderPipeline[index].sourceRGBBlendFactor = styles[i];
            renderPipeline[index].sizeAttr = 2;
            index++;
        }
    }
    renderPipelineDesc.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    for (int j = 0; j < 2; j++)
    {
        renderPipelineDesc.colorAttachments[0].writeMask = mask[j];
        for(int i = 0; i < 7; i++)
        {
            renderPipelineDesc.colorAttachments[0].sourceRGBBlendFactor = styles[i];
            pipelineState[index] = [device newRenderPipelineStateWithDescriptor:renderPipelineDesc  error:&error];
            if(error)
            {
                NSLog(@"Failed to created pipeline state, error %@", error);
                assert(pipelineState);
            }
            renderPipeline[index].colorMask = mask[j];
            renderPipeline[index].sourceRGBBlendFactor = styles[i];
            renderPipeline[index].sizeAttr = 2;
            index++;
        }
    }
    
    pipelineState[PIPELINE_STATE] = nil;
}

bool MTLRenderState::ApplyShader()
{
    @autoreleasepool
    {
    static const float nulvec[] = { 0.f, 0.f, 0.f, 0.f };
    MTLVertexBuffer* vertexBuffer = static_cast<MTLVertexBuffer*>(mVertexBuffer);
    
    if (vertexBuffer == nullptr /*|| VertexBufferAttributeWasChange(vertexBuffer->mAttributeInfo)*/ || !updated)
    {
        bool isVertexBufferNull = vertexBuffer == nullptr;
        if (!isVertexBufferNull)
            CopyVertexBufferAttribute(vertexBuffer->mAttributeInfo);
        
        int sizeAttr = 0;
        for (int i = 0; i < 6; i++)
        {
            if (vertexBuffer->mAttributeInfo[i].size == 0)
                break;
            
            sizeAttr++;
        }
        
        for(int i = 0; i < PIPELINE_STATE; i++)
        {
            if (renderPipeline[i].colorMask == colorMask && renderPipeline[i].sizeAttr == sizeAttr && renderPipeline[i].sourceRGBBlendFactor == srcblend)
            {
                currentState = i;
                break;
            }
        }
    }
    
    [renderCommandEncoder setRenderPipelineState:pipelineState[currentState]];
    //if (needCreateDepthState && 1)
    {
        [renderCommandEncoder setDepthStencilState:  depthState[FindDepthIndex(depthStateDesc)]];
    }
    
    int fogset = 0;
    if (mFogEnabled)
    {
        if (mFogEnabled == 2)
        {
            fogset = -3;    // 2D rendering with 'foggy' overlay.
        }
        else if ((GetFogColor() & 0xffffff) == 0)
        {
            fogset = gl_fogmode;
        }
        else
        {
            fogset = -gl_fogmode;
        }
    }
    
    activeShader->muDesaturation.Set(mStreamData.uDesaturationFactor);
    activeShader->muFogEnabled.Set(fogset);
    activeShader->muTextureMode.Set(mTextureMode == TM_NORMAL && mTempTM == TM_OPAQUE ? TM_OPAQUE : mTextureMode);
    activeShader->muLightParms.Set(vector_float4{mLightParms[0],mLightParms[1],mLightParms[2],mLightParms[3]});
    activeShader->muFogColor.Set(mStreamData.uFogColor);
    activeShader->muObjectColor.Set(mStreamData.uObjectColor);
    activeShader->muDynLightColor.Set(mStreamData.uDynLightColor.X);
    activeShader->muInterpolationFactor.Set(mStreamData.uInterpolationFactor);
    activeShader->muTimer.Set((double)(screen->FrameTime - firstFrame) * (double)mShaderTimer / 1000.);
    activeShader->muAlphaThreshold.Set(mAlphaThreshold);
    activeShader->muLightIndex.Set(-1);
    activeShader->muClipSplit.Set(vector_float2{mClipSplit[0],mClipSplit[1]});
    activeShader->muSpecularMaterial.Set(vector_float2{mGlossiness, mSpecularLevel});
    activeShader->muAddColor.Set(mStreamData.uAddColor);

    if (mGlowEnabled || activeShader->currentglowstate)
    {
        activeShader->muGlowTopColor.Set(vector_float4{mStreamData.uGlowTopColor.X,mStreamData.uGlowTopColor.Y,mStreamData.uGlowTopColor.Z,mStreamData.uGlowTopColor.W});
        activeShader->muGlowBottomColor.Set(vector_float4{mStreamData.uGlowBottomColor.X,mStreamData.uGlowBottomColor.Y,mStreamData.uGlowBottomColor.Z,mStreamData.uGlowBottomColor.W});
        activeShader->muGlowTopPlane.Set(vector_float4{mStreamData.uGlowTopPlane.X,mStreamData.uGlowTopPlane.Y,mStreamData.uGlowTopPlane.Z,mStreamData.uGlowTopPlane.W});
        activeShader->muGlowBottomPlane.Set(vector_float4{mStreamData.uGlowBottomPlane.X,mStreamData.uGlowBottomPlane.Y,mStreamData.uGlowBottomPlane.Z,mStreamData.uGlowBottomPlane.W});
        activeShader->currentglowstate = mGlowEnabled;
    }

    if (mGradientEnabled || activeShader->currentgradientstate)
    {
        activeShader->muObjectColor2.Set(mStreamData.uObjectColor2);
        activeShader->muGradientTopPlane.Set(vector_float4{mStreamData.uGradientTopPlane.X,
                                                           mStreamData.uGradientTopPlane.Y,
                                                           mStreamData.uGradientTopPlane.Z,
                                                           mStreamData.uGradientTopPlane.W});
        activeShader->muGradientBottomPlane.Set(vector_float4{mStreamData.uGradientBottomPlane.X, mStreamData.uGradientBottomPlane.Y, mStreamData.uGradientBottomPlane.Z, mStreamData.uGradientBottomPlane.W});
        activeShader->currentgradientstate = mGradientEnabled;
    }

    if (mSplitEnabled || activeShader->currentsplitstate)
    {
        activeShader->muSplitTopPlane.Set(vector_float4{mStreamData.uSplitTopPlane.X, mStreamData.uSplitTopPlane.Y, mStreamData.uSplitTopPlane.Z, mStreamData.uSplitTopPlane.W});
        activeShader->muSplitBottomPlane.Set(vector_float4{mStreamData.uSplitBottomPlane.X, mStreamData.uSplitBottomPlane.Y, mStreamData.uSplitBottomPlane.Z, mStreamData.uSplitBottomPlane.W});
        activeShader->currentsplitstate = mSplitEnabled;
    }

    if (mTextureMatrixEnabled)
    {
        activeShader->texturematrix.matrixToMetal(mTextureMatrix);
        activeShader->currentTextureMatrixState = true;
    }
    else if (activeShader->currentTextureMatrixState)
    {
        activeShader->currentTextureMatrixState = false;
        activeShader->texturematrix.matrixToMetal(identityMatrix);
    }

    if (mModelMatrixEnabled)
    {
        activeShader->modelmatrix.matrixToMetal(mModelMatrix);
        VSMatrix norm;
        norm.computeNormalMatrix(mModelMatrix);
        activeShader->normalmodelmatrix.matrixToMetal(norm);
        activeShader->currentModelMatrixState = true;
    }
    else if (activeShader->currentModelMatrixState)
    {
        activeShader->currentModelMatrixState = false;
        activeShader->modelmatrix.matrixToMetal(identityMatrix);
        activeShader->normalmodelmatrix.matrixToMetal(identityMatrix);
    }
    
    MTLDataBuffer* dataBuffer = (MTLDataBuffer*)screen->mViewpoints;
    
    PerView data;
    data.ModelMatrix       = (activeShader->modelmatrix.mat);
    data.TextureMatrix     = (activeShader->texturematrix.mat);
    data.NormalModelMatrix = (activeShader->normalmodelmatrix.mat);
    auto fb = GetMetalFrameBuffer();
    
    [renderCommandEncoder setVertexBytes:&data length:sizeof(data) atIndex:3];
    
    int index = mLightIndex;
    // Mess alert for crappy AncientGL!
    if (!screen->mLights->GetBufferType() && index >= 0)
    {
        size_t start, size;
        index = screen->mLights->GetBinding(index, &start, &size);

        if (start != mLastMappedLightIndex)
        {
            mLastMappedLightIndex = start;
            //static_cast<GLDataBuffer*>(screen->mLights->GetBuffer())->BindRange(nullptr, start, size);
        }
    }
    Uniforms uniforms;
    uniforms.timer = activeShader->muTimer.val;
    uniforms.uAddColor = {activeShader->muAddColor.mBuffer.r,activeShader->muAddColor.mBuffer.g,activeShader->muAddColor.mBuffer.b,activeShader->muAddColor.mBuffer.a};
    uniforms.uDesaturationFactor = mStreamData.uDesaturationFactor;
    uniforms.uDynLightColor = activeShader->muDynLightColor.val;
    uniforms.uFogColor = {activeShader->muFogColor.mBuffer.r,activeShader->muFogColor.mBuffer.g,activeShader->muFogColor.mBuffer.b,activeShader->muFogColor.mBuffer.a};
    uniforms.uFogEnabled = activeShader->muFogEnabled.val;
    uniforms.uGlowBottomColor = activeShader->muGlowBottomColor.val;
    uniforms.uGlowTopColor = activeShader->muGlowTopColor.val;
    uniforms.uObjectColor = {activeShader->muObjectColor.mBuffer.r,activeShader->muObjectColor.mBuffer.g,activeShader->muObjectColor.mBuffer.b,activeShader->muObjectColor.mBuffer.a};
    uniforms.uObjectColor2 = {activeShader->muObjectColor2.mBuffer.r,activeShader->muObjectColor2.mBuffer.g,activeShader->muObjectColor2.mBuffer.b,activeShader->muObjectColor2.mBuffer.a};
    uniforms.uTextureMode = activeShader->muTextureMode.val;
    uniforms.uLightAttr = activeShader->muLightParms.val;
    uniforms.aColor = vector_float4{mStreamData.uVertexColor.X,mStreamData.uVertexColor.Y,mStreamData.uVertexColor.Z,mStreamData.uVertexColor.W};
    uniforms.mTextureEnabled = mTextureEnabled;
    //glVertexAttrib4fv(VATTR_COLOR, &mStreamData.uVertexColor.X);
    //glVertexAttrib4fv(VATTR_NORMAL, &mStreamData.uVertexNormal.X);
    
    [renderCommandEncoder setFragmentBytes:&uniforms length:sizeof(Uniforms) atIndex:2];
    
    
    if(MLRenderer->mHWViewpointUniforms != nullptr)
        [renderCommandEncoder setVertexBytes:MLRenderer->mHWViewpointUniforms length:sizeof(mtlHWViewpointUniforms) atIndex:4];
    
    
    VSUniforms VSUniform;
    VSUniform.uClipSplit = activeShader->muClipSplit.val;
    VSUniform.uSplitTopPlane = activeShader->muSplitTopPlane.val;
    VSUniform.uInterpolationFactor = activeShader->muInterpolationFactor.val;
    VSUniform.uGlowTopColor = activeShader->muGlowTopColor.val;
    VSUniform.uGlowTopPlane = activeShader->muGlowTopPlane.val;
    VSUniform.uGlowBottomPlane = activeShader->muGlowBottomPlane.val;
    VSUniform.uObjectColor2 = {activeShader->muObjectColor2.mBuffer.r,activeShader->muObjectColor2.mBuffer.g,activeShader->muObjectColor2.mBuffer.b,activeShader->muObjectColor2.mBuffer.a};
    VSUniform.uSplitBottomPlane = activeShader->muSplitBottomPlane.val;
    VSUniform.uGradientBottomPlane = activeShader->muGradientBottomPlane.val;
    VSUniform.uGlowBottomColor = activeShader->muGlowBottomColor.val;
    VSUniform.uGradientTopPlane = activeShader->muGradientTopPlane.val;
    VSUniform.aNormal = vector_float4{mStreamData.uVertexNormal.X,mStreamData.uVertexNormal.Y,mStreamData.uVertexNormal.Z,mStreamData.uVertexNormal.W};
    
    
    [renderCommandEncoder setVertexBytes:&VSUniform length:sizeof(VSUniform) atIndex:5];
    
    activeShader->muLightIndex.Set(index);
    }
    return true;
}

void MTLRenderState::ApplyBuffers()
{
    if (mVertexBuffer != mCurrentVertexBuffer || mVertexOffsets[0] != mCurrentVertexOffsets[0] || mVertexOffsets[1] != mCurrentVertexOffsets[1])
    {
        if (mVertexBuffer != nullptr)
        {
            MTLVertexBuffer* mtlBuffer = static_cast<MTLVertexBuffer*>(mVertexBuffer);
            //float* val = (float*)mtl_vertexBuffer[currentIndexVB].contents;
            //memcpy(val, (float*)mtlBuffer->mBuffer, mtlBuffer->Size());
            
            needCpyBuffer = true;
            
            //printf("SIZE = %zu\n",  mtlBuffer->Size());
            mCurrentVertexBuffer = mVertexBuffer;
            mCurrentVertexOffsets[0] = mVertexOffsets[0];
            mCurrentVertexOffsets[1] = mVertexOffsets[1];
        }
        if (mIndexBuffer != nullptr)
        {
            if (mIndexBuffer)
            {
                MTLVertexBuffer* mtlBuffer = static_cast<MTLVertexBuffer*>(mVertexBuffer);
                MTLIndexBuffer* IndxBuffer = static_cast<MTLIndexBuffer*>(mIndexBuffer);
                int* arrIndexes = (int*)IndxBuffer->mBuffer;
                float* buff = (float*)mtlBuffer->mBuffer;
                float* mtlbuff = (float*)mtl_indexBuffer[currentIndexVB].contents;
                int val = 0;
                for (int i = 0; i < IndxBuffer->Size() / sizeof(int); i++)
                {
                    if (arrIndexes[i] > val)
                        val = arrIndexes[i];
                }
                
                memcpy((char*)mtl_indexBuffer[currentIndexVB].contents +    offsetIB[1], mtlBuffer->mBuffer,  mtlBuffer->mStride * (val + 1));
                memcpy((char*)mtl_index[currentIndexVB].contents       +    indexOffset[1], IndxBuffer->mBuffer, mIndexBuffer->Size());
                
                //printf("REALLY SIZE = %lu\n", (mtlBuffer->mStride * (val + 1)));
                //printf("offsetIB[1] = %lu\n", offsetIB[1]);
                offsetIB[0]    = offsetIB[1];
                indexOffset[0] = indexOffset[1];
                //printf("offsetIB[0] = %lu\n", offsetIB[0]);
                offsetIB[1] += mtlBuffer->mStride * (val + 1);
                indexOffset[1] += mIndexBuffer->Size();
            }
            //printf("Size index buffer = %lu\n", mIndexBuffer->Size());
            mCurrentIndexBuffer = mIndexBuffer;
        }
    }
}

void MTLRenderState::Apply()
{
    ApplyShader();
    ApplyState();
    ApplyBuffers();
    MLRenderer->mSamplerManager->BindToShader(renderCommandEncoder);
}

void MTLRenderState::CreateRenderState(MTLRenderPassDescriptor * renderPassDescriptor)
{
    commandBuffer = [commandQueue commandBuffer];
    renderCommandEncoder = [commandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor];
    renderCommandEncoder.label = @"SceneFB";
    [renderCommandEncoder setFrontFacingWinding:MTLWindingClockwise];
    [renderCommandEncoder setCullMode:MTLCullModeNone];
    [renderCommandEncoder setViewport:(MTLViewport)
    {   0.0, 0.0,
        (double)GetMetalFrameBuffer()->GetClientWidth(), (double)GetMetalFrameBuffer()->GetClientHeight(),
        0.0, 1.0 }];
    //CreateFanToTrisIndexBuffer();
    //needSetUniforms = true;
    
    if (mtl_vertexBuffer[0].length == 0)
    {
        mtl_vertexBuffer[0] = [device newBufferWithLength:100000 options:MTLResourceStorageModeShared];
        mtl_vertexBuffer[1] = [device newBufferWithLength:100000 options:MTLResourceStorageModeShared];
        mtl_vertexBuffer[2] = [device newBufferWithLength:100000 options:MTLResourceStorageModeShared];
        
        needDeleteVB = true;
    }
    
    if (mtl_indexBuffer[0].length == 0)
    {
        mtl_indexBuffer[0] = [device newBufferWithLength:400000 options:MTLResourceStorageModeShared];
        mtl_indexBuffer[1] = [device newBufferWithLength:400000 options:MTLResourceStorageModeShared];
        mtl_indexBuffer[2] = [device newBufferWithLength:400000 options:MTLResourceStorageModeShared];
        
        needDeleteIB = true;
    }
    
    if (mtl_index[0].length == 0)
    {
        mtl_index[0] = [device newBufferWithLength:400000 options:MTLResourceStorageModeShared];
        mtl_index[1] = [device newBufferWithLength:400000 options:MTLResourceStorageModeShared];
        mtl_index[2] = [device newBufferWithLength:400000 options:MTLResourceStorageModeShared];
        
        needDeleteIB = true;
    }
    
    if (activeShader == nullptr)
    {
        activeShader = new MTLShader();
    }
}

void MTLRenderState::CopyVertexBufferAttribute(const MLVertexBufferAttribute *attr)
{
    memcpy(prevAttributeInfo, attr, sizeof(MLVertexBufferAttribute)*6);
}

int MTLRenderState::FindDepthIndex(MTLDepthStencilDescriptor* desc)
{
    MTLStencilOperation stencil = desc.backFaceStencil.depthStencilPassOperation;
    MTLCompareFunction     comp = desc.depthCompareFunction;
    bool             writeDepth = desc.depthWriteEnabled;
    
    for (int i = 0; i < SIZE_DEPTH_DESC; i++)
    {
        if (depthIndex[i].compareFunction == comp && depthIndex[i].stencilOperation == stencil && depthIndex[i].writeDepth == writeDepth)
            return depthIndex[i].ind;
    }
    
    return -1;
}

bool MTLRenderState::VertexBufferAttributeWasChange(const MLVertexBufferAttribute *attr)
{
    for (int i = 0; i < 6; i++)
    {
        if (prevAttributeInfo[i].size != attr[i].size)
            return true;
    }
    return false;
}

void MTLRenderState::setVertexBuffer(OBJC_ID(MTLBuffer) buffer, size_t index, size_t offset /*= 0*/)
{
    [renderCommandEncoder setVertexBuffer:buffer offset:offset atIndex:index];
}

void MTLRenderState::EndFrame()
{
    if (MLRenderer->mScreenBuffers->mDrawable)
        [commandBuffer presentDrawable:MLRenderer->mScreenBuffers->mDrawable];
    
    [renderCommandEncoder endEncoding];
    [commandBuffer commit];
    //[commandBuffer waitUntilCompleted];
    currentIndexVB = currentIndexVB == 2 ? 0 : currentIndexVB + 1;
    offsetVB[0] = offsetVB[1] = 0;
    offsetIB[0] = offsetIB[1] = 0;
    indexOffset[0] = indexOffset[1] = 0;
    //printf("EndFrame\n");
    needCpyBuffer = true;
}

//===========================================================================
//
//    Binds a texture to the renderer
//
//====================================================me=======================

void MTLRenderState::ApplyMaterial(FMaterial *mat, int clampmode, int translation, int overrideshader)
{
    if (mat->tex->isHardwareCanvas())
    {
        mTempTM = TM_OPAQUE;
    }
    else
    {
        mTempTM = TM_NORMAL;
    }
    mEffectState = overrideshader >= 0 ? overrideshader : mat->GetShaderIndex();
    mShaderTimer = mat->tex->shaderspeed;
    SetSpecular(mat->tex->Glossiness, mat->tex->SpecularLevel);

    auto tex = mat->tex;
    if (tex->UseType == ETextureType::SWCanvas) clampmode = CLAMP_NOFILTER;
    if (tex->isHardwareCanvas()) clampmode = CLAMP_CAMTEX;
    else if ((tex->isWarped() || tex->shaderindex >= FIRST_USER_SHADER) && clampmode <= CLAMP_XY) clampmode = CLAMP_NONE;
    
    // avoid rebinding the same texture multiple times.
   // if (mat == lastMaterial && lastClamp == clampmode && translation == lastTranslation) return;
    lastMaterial = mat;
    lastClamp = clampmode;
    lastTranslation = translation;

    int usebright = false;
    int maxbound = 0;

    // Textures that are already scaled in the texture lump will not get replaced by hires textures.
    int flags = mat->isExpanded() ? CTF_Expand : (gl_texture_usehires && !tex->isScaled() && clampmode <= CLAMP_XY) ? CTF_CheckHires : 0;
    int numLayers = mat->GetLayers();
    auto base = static_cast<MTLHardwareTexture*>(mat->GetLayer(0, translation));

    if (base->BindOrCreate(tex, tex->GetID().GetIndex(), clampmode, translation, flags, renderCommandEncoder))
    {
        for (int i = 1; i<numLayers; i++)
        {
            FTexture *layer;
            auto systex = static_cast<MTLHardwareTexture*>(mat->GetLayer(i, 0, &layer));
            systex->BindOrCreate(layer, i, clampmode, 0, mat->isExpanded() ? CTF_Expand : 0, renderCommandEncoder);
            maxbound = i;
        }
    }
    // unbind everything from the last texture that's still active
    for (int i = maxbound + 1; i <= maxBoundMaterial; i++)
    {
    //    MTLHardwareTexture::Unbind(i);
        maxBoundMaterial = maxbound;
    }
}

//==========================================================================
//
// Apply blend mode from RenderStyle
//
//==========================================================================

void MTLRenderState::ApplyBlendMode()
{
    static MTLBlendFactor    blendstyles[] =
      { MTLBlendFactorZero,                MTLBlendFactorSourceAlpha,         MTLBlendFactorSourceAlpha,
        MTLBlendFactorOneMinusSourceAlpha, MTLBlendFactorSourceColor, MTLBlendFactorOneMinusSourceColor,
        MTLBlendFactorDestinationColor,    MTLBlendFactorOneMinusDestinationColor};
    static int renderops[] = { 0, MTLBlendOperationAdd, MTLBlendOperationSubtract, MTLBlendOperationReverseSubtract, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1 };
    useBlendMode = true;

    srcblend = blendstyles[mRenderStyle.SrcAlpha%STYLEALPHA_MAX];
    dstblend = blendstyles[mRenderStyle.DestAlpha%STYLEALPHA_MAX];
    blendequation = renderops[mRenderStyle.BlendOp & 15];

    if (blendequation == -1)    // This was a fuzz style.
    {
        srcblend = MTLBlendFactorDestinationColor;
        dstblend = MTLBlendFactorOneMinusSourceAlpha;
        blendequation = MTLBlendOperationAdd;
    }

    // Checks must be disabled until all draw code has been converted.
    //if (srcblend != stSrcBlend || dstblend != stDstBlend)
    {
        stSrcBlend = srcblend;
        stDstBlend = dstblend;
       // glBlendFunc(srcblend, dstblend);
    }
    //if (blendequation != stBlendEquation)
    {
        stBlendEquation = blendequation;
       // glBlendEquation(blendequation);
    }
    updated = false;
}

//==========================================================================
//
// API dependent draw calls
//
//==========================================================================

static MTLPrimitiveType dt2ml[] = { MTLPrimitiveTypePoint, MTLPrimitiveTypeLine, MTLPrimitiveTypeTriangle, MTLPrimitiveTypeTriangle, MTLPrimitiveTypeTriangleStrip };
//static int dt2gl[] =            { GL_POINTS,             GL_LINES,             GL_TRIANGLES,             GL_TRIANGLE_FAN,          GL_TRIANGLE_STRIP };

void MTLRenderState::Draw(int dt, int index, int count, bool apply)
{
    @autoreleasepool
    {
        if (apply)
        {
            Apply();
        }
        
        drawcalls.Clock();
        MTLVertexBuffer* mtlBuffer = static_cast<MTLVertexBuffer*>(mVertexBuffer);
        float* buffer = &(((float*)(mtlBuffer->mBuffer))[(mtlBuffer->mStride * index) / 4]);
        
        //printf("Draw = %lu\n", mtlBuffer->mStride * count);
        
        if (dt == 3)
        {
            [renderCommandEncoder setVertexBytes:buffer length:count * mtlBuffer->mStride atIndex:0];
            [renderCommandEncoder drawIndexedPrimitives:dt2ml[dt]
                                             indexCount:(count - 2) * 3
                                              indexType:MTLIndexTypeUInt32
                                            indexBuffer:fanIndexBuffer indexBufferOffset:0];
        }
        else
        {
            memcpy((char*)mtl_vertexBuffer[currentIndexVB].contents + offsetVB[1],
                   (char*)mtlBuffer->mBuffer + mtlBuffer->mStride * index,
                   mtlBuffer->mStride * count);
            
            [renderCommandEncoder setVertexBuffer:mtl_vertexBuffer[currentIndexVB]
                                           offset:offsetVB[1]
                                          atIndex:0];
            
            //printf("REALLY SIZE = %lu\n", (mtlBuffer->mStride * count));
            //printf("offsetVB[1] = %lu\n", offsetVB[1]);
            offsetVB[1] += mtlBuffer->mStride * count;
            [renderCommandEncoder drawPrimitives:dt2ml[dt] vertexStart:0 vertexCount:count];
        }
        [renderCommandEncoder popDebugGroup];
        
        drawcalls.Unclock();
    }
}

void MTLRenderState::CreateFanToTrisIndexBuffer()
{
   TArray<uint32_t> data;
    for (int i = 2; i < 1000; i++)
    {
        data.Push(0);
        data.Push(i - 1);
        data.Push(i);
    }
    fanIndexBuffer = [device newBufferWithBytes:data.Data() length:sizeof(uint32_t) * data.Size() options:MTLResourceStorageModeShared];
}

void MTLRenderState::DrawIndexed(int dt, int index, int count, bool apply)
{
    @autoreleasepool
    {
        if (apply)
        {
            Apply();
        }
        drawcalls.Clock();
        //MTLIndexBuffer*  IndexBuffer    = static_cast<MTLIndexBuffer*>(mIndexBuffer);
        //OBJC_ID(MTLBuffer)   indexBuffer    = [device newBufferWithBytes:(float*)IndexBuffer->mBuffer  length:IndexBuffer->Size()  options:MTLResourceStorageModeShared];
        
        //int *val = (int*)IndexBuffer->mBuffer;
        
        MTLVertexBuffer* mtlBuffer = static_cast<MTLVertexBuffer*>(mVertexBuffer);
        
        
        [renderCommandEncoder setVertexBuffer:mtl_indexBuffer[currentIndexVB]
                                       offset:offsetIB[0]
                                      atIndex:0];
        
        
        [renderCommandEncoder drawIndexedPrimitives:dt2ml[dt]
                                         indexCount:count
                                          indexType:MTLIndexTypeUInt32
                                        indexBuffer:mtl_index[currentIndexVB]
                                  indexBufferOffset:(index * sizeof(uint32_t)) + indexOffset[0]];
        [renderCommandEncoder popDebugGroup];

        drawcalls.Unclock();
    }
}

void MTLRenderState::SetDepthMask(bool on)
{
    EnableDepthTest(on);
    if(!on)
    {
        //depthStateDesc.depthWriteEnabled                          = on;
        //depthStateDesc.depthCompareFunction                       = MTLCompareFunctionNever;
        //depthStateDesc.frontFaceStencil.stencilCompareFunction    = MTLCompareFunctionNever;
        //depthStateDesc.frontFaceStencil.stencilFailureOperation   = MTLStencilOperationZero;
        //depthStateDesc.frontFaceStencil.depthStencilPassOperation = MTLStencilOperationZero;
        //depthStateDesc.backFaceStencil.stencilCompareFunction     = MTLCompareFunctionNever;
        //depthStateDesc.backFaceStencil.stencilFailureOperation    = MTLStencilOperationZero;
        //depthStateDesc.backFaceStencil.depthStencilPassOperation  = MTLStencilOperationZero;
    }
}

void MTLRenderState::SetDepthFunc(int func)
{
    static MTLCompareFunction df2ml[] = { MTLCompareFunctionLess, MTLCompareFunctionLessEqual, MTLCompareFunctionAlways };
    //                                  {         GL_LESS,                GL_LEQUAL,                   GL_ALWAYS };
    depthCompareFunc = df2ml[func];
    depthStateDesc.depthCompareFunction = depthCompareFunc;
    needCreateDepthState = true;
}

void MTLRenderState::SetDepthRange(float min, float max)
{
    //glDepthRange(min, max);
}

void MTLRenderState::SetColorMask(bool r, bool g, bool b, bool a)
{
    colorMask = 0;
    if (r)
        colorMask |= MTLColorWriteMaskRed;
    if (g)
        colorMask |= MTLColorWriteMaskGreen;
    if (b)
        colorMask |= MTLColorWriteMaskBlue;
    if (a)
        colorMask |= MTLColorWriteMaskAlpha;
    
    updated = false;
}

void MTLRenderState::SetStencil(int offs, int op, int flags = -1)
{
    static MTLStencilOperation op2ml[] = { MTLStencilOperationKeep, MTLStencilOperationIncrementClamp, MTLStencilOperationDecrementClamp };
    //                                          { GL_KEEP,                        GL_INCR,                          GL_DECR };
    depthStateDesc.frontFaceStencil.stencilCompareFunction    = MTLCompareFunctionEqual;
    depthStateDesc.frontFaceStencil.stencilFailureOperation   = MTLStencilOperationKeep;
    depthStateDesc.frontFaceStencil.depthStencilPassOperation = op2ml[op];
    depthStateDesc.backFaceStencil.stencilCompareFunction    = MTLCompareFunctionEqual;
    depthStateDesc.backFaceStencil.stencilFailureOperation   = MTLStencilOperationKeep;
    depthStateDesc.backFaceStencil.depthStencilPassOperation = op2ml[op];
    
    
    [renderCommandEncoder setStencilReferenceValue:screen->stencilValue + offs ];
    needCreateDepthState = true;
    
    //MLRenderer->loadDepthStencil = true;
    //glStencilFunc(GL_EQUAL, screen->stencilValue + offs, ~0);        // draw sky into stencil
    //glStencilOp(GL_KEEP, GL_KEEP, op2gl[op]);        // this stage doesn't modify the stencil

    if (flags != -1)
    {
        bool cmon = !(flags & SF_ColorMaskOff);
        if (cmon)
            colorMask = MTLColorWriteMaskAll;
        else
            colorMask = MTLColorWriteMaskNone;
        
        updated = false;
        //glColorMask(cmon, cmon, cmon, cmon);                        // don't write to the graphics buffer
        //glDepthMask(!(flags & SF_DepthMaskOff));
    }
}

void MTLRenderState::ToggleState(int state, bool on)
{
    if (on)
    {
    //    glEnable(state);
    }
    //else
    //{
    //    glDisable(state);
    //}
}

void MTLRenderState::SetCulling(int mode)
{
    //if (mode != Cull_None)
    //{
    //    glEnable(GL_CULL_FACE);
    //    glFrontFace(mode == Cull_CCW ? GL_CCW : GL_CW);
    //}
    //else
    //{
    //    glDisable(GL_CULL_FACE);
    //}
}

void MTLRenderState::EnableClipDistance(int num, bool state)
{
    // Update the viewpoint-related clip plane setting.
    //if (!(gl.flags & RFL_NO_CLIP_PLANES))
    //{
    //    ToggleState(GL_CLIP_DISTANCE0 + num, state);
    //}
}

void MTLRenderState::Clear(int targets)
{
    // This always clears to default values.
    int gltarget = 0;
    colorMask = MTLColorWriteMaskAll;
    if (targets)
    {
        
    }
    //{
    //    gltarget |= GL_DEPTH_BUFFER_BIT;
    //    glClearDepth(1);
    //}
    //if (targets & CT_Stencil)
    //{
    //    gltarget |= GL_STENCIL_BUFFER_BIT;
    //    glClearStencil(0);
    //}
    //if (targets & CT_Color)
    //{
    //    gltarget |= GL_COLOR_BUFFER_BIT;
    //    glClearColor(screen->mSceneClearColor[0], screen->mSceneClearColor[1], screen->mSceneClearColor[2], screen->mSceneClearColor[3]);
    //}
    //glClear(gltarget);
}

void MTLRenderState::EnableStencil(bool on)
{
    if (!on)
        depthStateDesc.frontFaceStencil.stencilCompareFunction = MTLCompareFunctionNever;
}

void MTLRenderState::SetScissor(int x, int y, int w, int h)
{
    mScissorX = x;
    mScissorY = y;
    mScissorWidth = w;
    mScissorHeight = h;
    
    if (w > -1)
    {
        //glScissor(x, y, w, h);
        [renderCommandEncoder setScissorRect:{static_cast<NSUInteger>(x),
                                              static_cast<NSUInteger>(y),
                                              static_cast<NSUInteger>(w),
                                              static_cast<NSUInteger>(h - y)}];
    }
}

void MTLRenderState::SetViewport(int x, int y, int w, int h)
{
    //m_Viewport.originX = x;
    //m_Viewport.originY = y;
    //m_Viewport.height = h;
    //m_Viewport.width = w;
    //m_Viewport.znear = 0.0f;
    //m_Viewport.zfar = 1.0f;
    //
    //[renderCommandEncoder setViewport:m_Viewport];
}

void MTLRenderState::EnableDepthTest(bool on)
{
    updated = false;
    depthStateDesc.depthWriteEnabled = on;
}

void MTLRenderState::EnableMultisampling(bool on)
{
    //ToggleState(GL_MULTISAMPLE, on);
}

void MTLRenderState::EnableLineSmooth(bool on)
{
    //ToggleState(GL_LINE_SMOOTH, on);
}

//==========================================================================
//
//
//
//==========================================================================
void MTLRenderState::ClearScreen()
{
    bool multi = !!glIsEnabled(GL_MULTISAMPLE);

    screen->mViewpoints->Set2D(*this, SCREENWIDTH, SCREENHEIGHT);
    SetColor(0, 0, 0);
    Apply();

    glDisable(GL_MULTISAMPLE);
    glDisable(GL_DEPTH_TEST);

    glDrawArrays(GL_TRIANGLE_STRIP, FFlatVertexBuffer::FULLSCREEN_INDEX, 4);

    glEnable(GL_DEPTH_TEST);
    if (multi) glEnable(GL_MULTISAMPLE);
}



//==========================================================================
//
// Below are less frequently altrered state settings which do not get
// buffered by the state object, but set directly instead.
//
//==========================================================================

bool MTLRenderState::SetDepthClamp(bool on)
{
    bool res = mLastDepthClamp;
    //if (!on) glDisable(GL_DEPTH_CLAMP);
    //else glEnable(GL_DEPTH_CLAMP);
    mLastDepthClamp = on;
    return res;
}

}
