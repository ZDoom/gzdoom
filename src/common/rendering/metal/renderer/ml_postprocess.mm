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

#include "m_png.h"
#include "r_utility.h"
#include "d_player.h"
#include "metal/system/ml_buffer.h"
#include "metal/system/ml_framebuffer.h"
#include "hwrenderer/utility/hw_cvars.h"
#include "metal/renderer/ml_renderstate.h"
#include "metal/renderer/ml_renderbuffers.h"
#include "metal/renderer/ml_renderer.h"
#include "metal/renderer/ml_postprocessstate.h"
#include "hwrenderer/postprocessing/hw_postprocess.h"
#include "hwrenderer/postprocessing/hw_postprocess_cvars.h"
#include "hwrenderer/utility/hw_vrmodes.h"
#include "hwrenderer/data/flatvertices.h"
#include "r_videoscale.h"

extern bool vid_hdr_active;

CVAR(Int, ml_dither_bpc, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)

namespace MetalRenderer
{

//void MTLRenderer::RenderScreenQuad()
//{
//    auto buffer = static_cast<GLVertexBuffer *>(screen->mVertexData->GetBufferObjects().first);
//    buffer->Bind(nullptr);
//    glDrawArrays(GL_TRIANGLE_STRIP, FFlatVertexBuffer::PRESENT_INDEX, 4);
//}

//-----------------------------------------------------------------------------
//
// Adds ambient occlusion to the scene
//
//-----------------------------------------------------------------------------

void MTLRenderer::AmbientOccludeScene(float m5)
{
    int sceneWidth = mBuffers->GetSceneWidth();
    int sceneHeight = mBuffers->GetSceneHeight();

    MTLPPRenderState renderstate(mBuffers);
    hw_postprocess.ssao.Render(&renderstate, m5, sceneWidth, sceneHeight);
}

void MTLRenderer::BlurScene(float gameinfobluramount)
{
    int sceneWidth = mBuffers->GetSceneWidth();
    int sceneHeight = mBuffers->GetSceneHeight();

    MTLPPRenderState renderstate(mBuffers);

    auto vrmode = VRMode::GetVRMode(true);
    int eyeCount = vrmode->mEyeCount;
    for (int i = 0; i < eyeCount; ++i)
    {
        hw_postprocess.bloom.RenderBlur(&renderstate, sceneWidth, sceneHeight, gameinfobluramount);
        if (eyeCount - i > 1) mBuffers->NextEye(eyeCount);
    }
}

void MTLRenderer::ClearTonemapPalette()
{
    hw_postprocess.tonemap.ClearTonemapPalette();
}

//-----------------------------------------------------------------------------
//
// Copies the rendered screen to its final destination
//
//-----------------------------------------------------------------------------

void MTLRenderer::Flush()
{
    auto vrmode = VRMode::GetVRMode(true);
    if (vrmode->mEyeCount == 1)
    {
        CopyToBackbuffer(nullptr, true);
    }
    else
    {
        // Render 2D to eye textures
        int eyeCount = vrmode->mEyeCount;
        for (int eye_ix = 0; eye_ix < eyeCount; ++eye_ix)
        {
            screen->Draw2D();
            if (eyeCount - eye_ix > 1)
                mBuffers->NextEye(eyeCount);
        }

        screen->Clear2D();
        PresentStereo();
    }
}

//-----------------------------------------------------------------------------
//
// Gamma correct while copying to frame buffer
//
//-----------------------------------------------------------------------------

void MTLRenderer::CopyToBackbuffer(const IntRect *bounds, bool applyGamma)
{
    screen->Draw2D();    // draw all pending 2D stuff before copying the buffer
    screen->Clear2D();

    MTLPPRenderState renderstate(mBuffers);
    hw_postprocess.customShaders.Run(&renderstate, "screen");


    IntRect box;
    if (bounds)
    {
        box = *bounds;
    }
    else
    {
        ClearBorders();
        box = screen->mOutputLetterbox;
    }

    DrawPresentTexture(box, applyGamma);
}

void MTLRenderer::DrawPresentTexture(const IntRect &box, bool applyGamma)
{
    if (!applyGamma || framebuffer->IsHWGammaActive())
    {
        mPresentShader->InvGamma = 1.0f;
        mPresentShader->Contrast = 1.0f;
        mPresentShader->Brightness = 0.0f;
        mPresentShader->Saturation = 1.0f;
    }
    else
    {
        mPresentShader->InvGamma = 1.0f / clamp<float>(Gamma, 0.1f, 4.f);
        mPresentShader->Contrast = clamp<float>(vid_contrast, 0.1f, 3.f);
        mPresentShader->Brightness = clamp<float>(vid_brightness, -0.8f, 0.8f);
        mPresentShader->Saturation = clamp<float>(vid_saturation, -15.0f, 15.f);
        mPresentShader->GrayFormula = static_cast<int>(gl_satformula);
    }
    if (vid_hdr_active && framebuffer->IsFullscreen())
    {
        // Full screen exclusive mode treats a rgba16f frame buffer as linear.
        // It probably will eventually in desktop mode too, but the DWM doesn't seem to support that.
        mPresentShader->HdrMode = 1;
        mPresentShader->ColorScale = (ml_dither_bpc == -1) ? 1023.0f : (float)((1 << ml_dither_bpc) - 1);
    }
    else
    {
        mPresentShader->HdrMode = 0;
        mPresentShader->ColorScale = (ml_dither_bpc == -1) ? 255.0f : (float)((1 << ml_dither_bpc) - 1);
    }
    mPresentShader->Scale = { screen->mScreenViewport.width / (float)mBuffers->GetWidth(), screen->mScreenViewport.height / (float)mBuffers->GetHeight() };
    mPresentShader->Offset = { 0.0f, 0.0f };
    RenderScreenQuad();
}

//-----------------------------------------------------------------------------
//
// Fills the black bars around the screen letterbox
//
//-----------------------------------------------------------------------------

void MTLRenderer::ClearBorders()
{
    const auto &box = screen->mOutputLetterbox;

    int clientWidth = framebuffer->GetClientWidth();
    int clientHeight = framebuffer->GetClientHeight();
    if (clientWidth == 0 || clientHeight == 0)
        return;
}

}

