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

#include "metal/textures/ml_samplers.h"
#include "metal/textures/ml_hwtexture.h"
#include "hwrenderer/postprocessing/hw_postprocess.h"
#include <QuartzCore/QuartzCore.h>

namespace MetalRenderer
{

static OBJC_ID(MTLTexture) CreateRenderBuffer(const char *name, MTLPixelFormat format, int width, int height, const void* data = nullptr);

class MTLHardwareTexture;

class MTLRenderBuffers
{
public:
    MTLRenderBuffers();
    ~MTLRenderBuffers();

    void BeginFrame(int width, int height, int sceneWidth, int sceneHeight);

    int GetWidth() const { return mWidth; }
    int GetHeight() const { return mHeight; }
    int GetSceneWidth() const { return mSceneWidth; }
    int GetSceneHeight() const { return mSceneHeight; }
    int GetSceneSamples() const { return mSamples; }
    int & CurrentEye() { return mCurrentEye; }
    int NextEye(int eyeCount) {raise(SIGTRAP);};
    void Setup(int width, int height, int sceneWidth, int sceneHeight);
    void BindDitherTexture(int texunit);
    OBJC_ID(MTLTexture) CreateDepthTexture(const char *name, MTLPixelFormat format, int width, int height, int samples, bool fixedSampleLocations);
    void ClearScene();
    
    OBJC_ID(MTLTexture) SceneColor;
    OBJC_ID(MTLDepthStencilState) SceneDepthStencil;
    OBJC_ID(MTLTexture) SceneNormal;
    OBJC_ID(MTLTexture) SceneFog;

    void BlitSceneToTexture();

    static const int NumPipelineImages = 2;
    OBJC_ID(MTLTexture) PipelineImage[NumPipelineImages];
    
    int mCurrentEye = 0;

    OBJC_ID(MTLTexture) Shadowmap;
    // Buffers for the scene
    OBJC_ID(MTLTexture) mSceneMultisampleTex = nil;
    OBJC_ID(MTLTexture) mSceneDepthStencilTex = nil;
    OBJC_ID(MTLTexture) mSceneDepthTex = nil;
    OBJC_ID(MTLTexture) mSceneFogTex = nil;
    OBJC_ID(MTLTexture) mSceneNormalTex = nil;
    OBJC_ID(MTLTexture) mSceneMultisampleBuf = nil;
    OBJC_ID(MTLTexture) mSceneDepthStencilBuf = nil;
    OBJC_ID(MTLTexture) mSceneFogBuf = nil;
    OBJC_ID(MTLTexture) mSceneNormalBuf = nil;
    OBJC_ID(MTLTexture) mSceneFB = nil;
    OBJC_ID(CAMetalDrawable) mDrawable;
    OBJC_ID(MTLTexture) mSceneDataFB = nil;
    bool mSceneUsesTextures = false;
    OBJC_ID(MTLTexture) mDitherTexture = nil;

private:
    void CreatePipeline(int width, int height);
    void CreateScene(int width, int height, int samples, bool needsSceneTextures);
    void CreateSceneColor(int width, int height);
    void CreateSceneDepthStencil(int width, int height);
    void CreateSceneFog(int width, int height) {};
    void CreateSceneNormal(int width, int height) {};
    void CreateShadowmap() {raise(SIGTRAP);};
    int  GetBestSampleCount() {raise(SIGTRAP);};
    OBJC_ID(MTLTexture) Create2DTexture(const char *name, MTLPixelFormat format, int width, int height, const void* data = nullptr);
    OBJC_ID(MTLTexture) Create2DMultisampleTexture(const char *name, MTLPixelFormat format, int width, int height, int samples, bool fixedSampleLocations);
    OBJC_ID(MTLTexture) CreateFrameBuffer(const char *name, OBJC_ID(MTLTexture) colorbuffer);

    int mWidth = 0;
    int mHeight = 0;
    int mSceneWidth = 0;
    int mSceneHeight = 0;
    int mSamples = 0;
    
    OBJC_ID(MTLTexture) mPipelineTexture[2];
    OBJC_ID(MTLTexture) mPipelineFB[2];
    
    bool                m_VsyncEnabled;
    bool                m_TripleBufferEnabled;
    OBJC_ID(MTLTexture)      m_DisplayBuffers[2];
    uint                m_CurrentBufferId;
};

class MTLPPRenderState : public PPRenderState
{
public:
    MTLPPRenderState(MTLRenderBuffers *buffers) : buffers(buffers) { }

    void PushGroup(const FString &name) override {raise(SIGTRAP);};
    void PopGroup() override {raise(SIGTRAP);};
    void Draw() override;

private:
    OBJC_ID(MTLTexture) *GetMLTexture(PPTexture *texture);
    MTLRenderBuffers *buffers;
};
}
