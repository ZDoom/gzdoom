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
#include "r_defs.h"
#include "v_video.h"
#include "vectors.h"
#include "swrenderer/r_renderer.h"
#include "matrix.h"
#include "metal/renderer/ml_renderbuffers.h"
#include "metal/renderer/ml_renderstate.h"
#include "metal/system/ml_framebuffer.h"
#include "hwrenderer/scene/hw_portal.h"
#include "hwrenderer/dynlights/hw_shadowmap.h"
#include "hwrenderer/postprocessing/hw_postprocess.h"
#include <functional>

#ifdef _MSC_VER
#pragma warning(disable:4244)
#endif

struct particle_t;
class FCanvasTexture;
class FFlatVertexBuffer;
class FSkyVertexBuffer;
class FShaderManager;
class HWPortal;
class FLightBuffer;
class DPSprite;
class FGLRenderBuffers;
class FGL2DDrawer;
class FHardwareTexture;
class SWSceneDrawer;
class HWViewpointBuffer;
struct FRenderViewpoint;

namespace MetalRenderer
{
class MetalFrameBuffer;
class MTLRenderState;

struct mtlHWViewpointUniforms
{
    mtlHWViewpointUniforms()
    {
        mProjectionMatrix = matrix_float4x4{0};
        mViewMatrix = matrix_float4x4{0};
        mNormalViewMatrix = matrix_float4x4{0};
        mCameraPos = vector_float4{0.f,0.f,0.f,0.f};
        mClipLine  = vector_float4{0.f,0.f,0.f,0.f};
        
        mGlobVis = 1.f;
        mPalLightLevels = 0;
        mViewHeight = 0;
        mClipHeight = 0.f;
        mClipHeightDirection = 0.f;
        mShadowmapFilter = 1;
    }
    ~mtlHWViewpointUniforms() = default;
    
    matrix_float4x4 mProjectionMatrix;
    matrix_float4x4 mViewMatrix;
    matrix_float4x4 mNormalViewMatrix;
    vector_float4   mCameraPos;
    vector_float4   mClipLine;

    float   mGlobVis;
    int     mPalLightLevels;
    int     mViewHeight;
    float   mClipHeight;
    float   mClipHeightDirection;
    int     mShadowmapFilter;
};

class MTLRenderer
{
public:

    MetalFrameBuffer *framebuffer;
    int mMirrorCount = 0;
    int mPlaneMirrorCount = 0;
    MTLShaderManager *mShaderManager = nullptr;
    MTLSamplerManager *mSamplerManager = nullptr;
    unsigned int mFBID;
    unsigned int mVAOID;
    unsigned int PortalQueryObject;
    unsigned int mStencilValue = 0;
    dispatch_semaphore_t semaphore;
    
    MTLRenderState *ml_RenderState;
    
    mtlHWViewpointUniforms *mHWViewpointUniforms;

    int mOldFBID;

    MTLRenderBuffers *mBuffers = nullptr;
    MTLRenderBuffers *mScreenBuffers = nullptr;
    MTLRenderBuffers *mSaveBuffers = nullptr;
    PresentUniforms *mPresentShader = nullptr;;
    MTLShaderProgram *mPresent3dCheckerShader = nullptr;
    MTLShaderProgram *mPresent3dColumnShader = nullptr;
    MTLShaderProgram *mPresent3dRowShader = nullptr;
    bool loadDepthStencil : 1;

    //FRotator mAngles;

    SWSceneDrawer *swdrawer = nullptr;

    MTLRenderer(MetalFrameBuffer *fb);
    ~MTLRenderer();

    void Initialize(int width, int height, OBJC_ID(MTLDevice) device);

    void ClearBorders();

    void ResetSWScene();

    void PresentStereo() {raise(SIGTRAP);};
    void RenderScreenQuad();
    void PostProcessScene(int fixedcm, const std::function<void()> &afterBloomDrawEndScene2D);
    void AmbientOccludeScene(float m5);
    void ClearTonemapPalette();
    void BlurScene(float gameinfobluramount);
    void CopyToBackbuffer(const IntRect *bounds, bool applyGamma);
    void DrawPresentTexture(const IntRect &box, bool applyGamma);
    void Flush();
    void Draw2D(F2DDrawer *data);
    void RenderTextureView(FCanvasTexture *tex, AActor *Viewpoint, double FOV);
    void WriteSavePic(player_t *player, FileWriter *file, int width, int height);
    sector_t *RenderView(player_t *player);
    void BeginFrame();
    
    sector_t *RenderViewpoint (FRenderViewpoint &mainvp, AActor * camera, IntRect * bounds, float fov, float ratio, float fovratio, bool mainview, bool toscreen);


    bool StartOffscreen();
    void EndOffscreen();
    void UpdateShadowMap();

    void BindToFrameBuffer(FMaterial *mat);

private:

    void DrawScene(HWDrawInfo *di, int drawmode);
    bool QuadStereoCheckInitialRenderContextState();
    void PresentAnaglyph(bool r, bool g, bool b);
    void PresentSideBySide();
    void PresentTopBottom();
    void PresentColumnInterleaved();
    void PresentRowInterleaved();
    void PresentCheckerInterleaved();
    void PresentQuadStereo();

};

struct TexFilter_s
{
    int minfilter;
    int magfilter;
    bool mipmapping;
} ;

extern MTLRenderer *MLRenderer;
}

