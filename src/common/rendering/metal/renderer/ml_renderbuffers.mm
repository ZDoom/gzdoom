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

#include "ml_renderbuffers.h"
#include "metal/system/ml_framebuffer.h"
#include "hwrenderer/utility/hw_cvars.h"
#include "metal/renderer/ml_renderstate.h"
#import <Metal/Metal.h>

MetalCocoaView* GetMacWindow();

namespace MetalRenderer
{


MTLRenderBuffers::MTLRenderBuffers()
{
    mDrawable = nil;
}

MTLRenderBuffers::~MTLRenderBuffers()
{
    [mSceneMultisampleTex release];
    [mSceneDepthStencilTex release];
    [mSceneFogTex release];
    [mSceneNormalTex release];
    [mSceneMultisampleBuf release];
    [mSceneDepthStencilBuf release];
    [mSceneFogBuf release];
    [mSceneNormalBuf release];
    [mSceneFB release];
    [mSceneDataFB release];
    [PipelineImage[0] release];
    [PipelineImage[1] release];
    [mDitherTexture release];
}

void MTLRenderBuffers::BeginFrame(int width, int height, int sceneWidth, int sceneHeight)
{
    
}

void MTLRenderBuffers::CreatePipeline(int width, int height)
{
    auto fb = GetMetalFrameBuffer();

    for (int i = 0; i < NumPipelineImages; i++)
    {
        MTLTextureDescriptor *desc = [MTLTextureDescriptor new];
        desc.width = fb->GetClientWidth();
        desc.height = fb->GetClientHeight();
        desc.pixelFormat = MTLPixelFormatRGBA16Float;//MTLPixelFormatRGBA16Float;
        desc.usage = MTLTextureUsageShaderWrite | MTLTextureUsageShaderRead;
        PipelineImage[i] = [device newTextureWithDescriptor:desc];
        
        [desc release];
    }
}

void MTLRenderBuffers::BindDitherTexture(int texunit)
{
    if (!mDitherTexture)
    {
        static const float data[64] =
        {
             .0078125, .2578125, .1328125, .3828125, .0234375, .2734375, .1484375, .3984375,
             .7578125, .5078125, .8828125, .6328125, .7734375, .5234375, .8984375, .6484375,
             .0703125, .3203125, .1953125, .4453125, .0859375, .3359375, .2109375, .4609375,
             .8203125, .5703125, .9453125, .6953125, .8359375, .5859375, .9609375, .7109375,
             .0390625, .2890625, .1640625, .4140625, .0546875, .3046875, .1796875, .4296875,
             .7890625, .5390625, .9140625, .6640625, .8046875, .5546875, .9296875, .6796875,
             .1015625, .3515625, .2265625, .4765625, .1171875, .3671875, .2421875, .4921875,
             .8515625, .6015625, .9765625, .7265625, .8671875, .6171875, .9921875, .7421875,
        };
        
        MTLTextureDescriptor *desc = [MTLTextureDescriptor new];
        desc.width = 8;
        desc.height = 8;
        desc.pixelFormat = MTLPixelFormatRG32Float;
        desc.usage = MTLTextureUsageShaderWrite | MTLTextureUsageShaderRead;
        
        mDitherTexture = [device newTextureWithDescriptor:desc];
        
        [desc release];
        
        MTLRegion region = MTLRegionMake2D(0, 0, 8, 8);
        [mDitherTexture replaceRegion:region mipmapLevel:0 withBytes:data bytesPerRow:(8*8)];
    }

    MLRenderer->mSamplerManager->SetRepeatAddressMode(true);
}

OBJC_ID(MTLTexture) MTLRenderBuffers::Create2DTexture(const char *name, MTLPixelFormat format, int width, int height, const void* data /*= nullptr*/)
{
    MTLTextureDescriptor *desc = [MTLTextureDescriptor new];
    desc.width = width;
    desc.height = height;
    desc.pixelFormat = format;
    desc.storageMode = MTLStorageModePrivate;
    desc.usage = MTLTextureUsageShaderWrite | MTLTextureUsageShaderRead;
   
    OBJC_ID(MTLTexture) dummy = [device newTextureWithDescriptor:desc];
    
    [desc release];
    return dummy;
}

OBJC_ID(MTLTexture) MTLRenderBuffers::Create2DMultisampleTexture(const char *name, MTLPixelFormat format, int width, int height, int samples, bool fixedSampleLocations)
{
    MTLTextureDescriptor *desc = [MTLTextureDescriptor new];
    desc.width = width;
    desc.height = height;
    desc.pixelFormat = format;
    desc.storageMode = MTLStorageModePrivate;
    desc.usage = MTLTextureUsageShaderWrite | MTLTextureUsageShaderRead;
    desc.textureType = MTLTextureType2DMultisample;
    desc.sampleCount = samples;
    OBJC_ID(MTLTexture) dummy = [device newTextureWithDescriptor:desc];
    [desc release];
    return dummy;
}

OBJC_ID(MTLTexture) MTLRenderBuffers::CreateFrameBuffer(const char *name, OBJC_ID(MTLTexture) colorbuffer)
{
    MTLTextureDescriptor *desc = [MTLTextureDescriptor new];
    desc.usage = MTLTextureUsageRenderTarget;
    desc.storageMode = MTLStorageModePrivate;
       
    OBJC_ID(MTLTexture) dummy = [device newTextureWithDescriptor:desc];
    [desc release];
    return dummy;
}

static OBJC_ID(MTLTexture) CreateRenderBuffer(const char *name, int width, int height, const void* data /* = nullptr */)
{
    MTLTextureDescriptor *desc = [MTLTextureDescriptor new];
    desc.width = width;
    desc.height = height;
    desc.textureType = MTLTextureType2D;
    desc.storageMode = MTLStorageModePrivate;
    desc.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
    desc.pixelFormat = MTLPixelFormatRGBA16Float;
    
    OBJC_ID(MTLTexture) dummy = [device newTextureWithDescriptor:desc];
    [desc release];
    return dummy;
}

OBJC_ID(MTLTexture) MTLRenderBuffers::CreateDepthTexture(const char *name, MTLPixelFormat format, int width, int height, int samples, bool fixedSampleLocations)
{
    MTLTextureDescriptor *desc = [MTLTextureDescriptor new];
    desc.width = width;
    desc.height = height;
    desc.pixelFormat = format;
    desc.storageMode = MTLStorageModePrivate;
    desc.usage = MTLTextureUsageRenderTarget;
    desc.textureType = MTLTextureType2D;
    
    OBJC_ID(MTLTexture) dummy = [device newTextureWithDescriptor:desc];
    [desc release];
    return dummy;
}

void MTLRenderBuffers::ClearScene()
{

}

void MTLRenderBuffers::CreateScene(int width, int height, int samples, bool needsSceneTextures)
{
    ClearScene(); 
    auto fb = GetMetalFrameBuffer();
    
    mSceneDepthStencilTex = CreateDepthTexture("mSceneDepthTex", MTLPixelFormatDepth32Float_Stencil8, fb->GetClientWidth(), fb->GetClientHeight(), 0, 0);
    mSceneFogTex = Create2DTexture("SceneFog", MTLPixelFormatRGBA8Unorm, width, height);
    mSceneNormalTex = Create2DTexture("SceneNormal", MTLPixelFormatRGB10A2Unorm, width, height);
    mSceneDepthStencilBuf = Create2DTexture("SceneDepthStencil", MTLPixelFormatDepth32Float_Stencil8, width, height);
    MetalCocoaView* const window = GetMacWindow();
    mSceneDataFB = Create2DTexture("SceneGBufferFB", MTLPixelFormatRGBA8Unorm, width, height);
}

void MTLRenderBuffers::CreateSceneColor(int width, int height)
{

    MTLTextureDescriptor *desc = [MTLTextureDescriptor new];
    desc.width = width;
    desc.height = height;
    desc.pixelFormat = MTLPixelFormatBGRA8Unorm;
    SceneColor = [device newTextureWithDescriptor:desc];
    
    [desc release];
}

void MTLRenderBuffers::CreateSceneDepthStencil(int width, int height)//, VkSampleCountFlagBits samples)
{
    MTLDepthStencilDescriptor *desc = [MTLDepthStencilDescriptor new];
    desc.depthCompareFunction = MTLCompareFunctionLess;
    desc.depthWriteEnabled = YES;
    desc.frontFaceStencil.stencilCompareFunction = MTLCompareFunctionAlways;
    desc.backFaceStencil.stencilCompareFunction = MTLCompareFunctionAlways;
    
    
    SceneDepthStencil = [device newDepthStencilStateWithDescriptor:desc];
    
    [desc release];
}

void MTLRenderBuffers::Setup(int width, int height, int sceneWidth, int sceneHeight)
{
    if (width <= 0 || height <= 0)
        I_FatalError("Requested invalid render buffer sizes: screen = %dx%d", width, height);

    int samples = 1;//clamp((int)gl_multisample, 0, mMaxSamples); mMaxSamples = 16;
    bool needsSceneTextures = false;//(gl_ssao != 0);

    if (width != mWidth || height != mHeight)
        CreatePipeline(width, height);

    if (width != mWidth || height != mHeight || mSamples != samples || mSceneUsesTextures != needsSceneTextures)
        CreateScene(width, height, samples, needsSceneTextures);

    auto fb = GetMetalFrameBuffer();
    
    mWidth = width;
    mHeight = height;
    mSamples = samples;
    mSceneUsesTextures = needsSceneTextures;
    mSceneWidth = sceneWidth;
    mSceneHeight = sceneHeight;


    if (/*FailedCreate*/0)
    {
        [mSceneDepthStencilTex release];
        [mSceneFogTex release];
        [mSceneNormalTex release];
        
        for (int i = 0; i < NumPipelineImages; i++)
            [PipelineImage[i] release];
        
        mWidth = 0;
        mHeight = 0;
        mSamples = 0;
        mSceneWidth = 0;
        mSceneHeight = 0;
        I_FatalError("Unable to create render buffers.");
    }
}

void MTLPPRenderState::Draw()
{
    raise(SIGTRAP);
}


}
