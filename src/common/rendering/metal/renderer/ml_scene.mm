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

#include "gl_load/gl_system.h"
#include "gi.h"
#include "m_png.h"
#include "doomstat.h"
#include "g_level.h"
#include "r_data/r_interpolate.h"
#include "r_utility.h"
#include "d_player.h"
#include "p_effect.h"
#include "sbar.h"
#include "po_man.h"
#include "p_local.h"
#include "serializer.h"
#include "g_levellocals.h"
#include "actorinlines.h"
#include "r_data/models/models.h"
#include "hwrenderer/dynlights/hw_dynlightdata.h"
#include "hwrenderer/utility/hw_clock.h"
#include "hwrenderer/data/flatvertices.h"

#include "hwrenderer/dynlights/hw_lightbuffer.h"
#include "metal/system/ml_framebuffer.h"
#include "hwrenderer/utility/hw_cvars.h"
#include "metal/renderer/ml_RenderState.h"
#include "metal/renderer/ml_renderbuffers.h"
#include "hwrenderer/data/hw_viewpointbuffer.h"
#include "hwrenderer/scene/hw_clipper.h"
#include "hwrenderer/scene/hw_portal.h"
#include "hwrenderer/utility/hw_vrmodes.h"
#include "metal/renderer/ml_renderer.h"
#include "metal/system/ml_buffer.h"

CVAR(Bool, ml_no_skyclear, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)

namespace MetalRenderer
{
    MTLRenderer *mlRenderer;
//-----------------------------------------------------------------------------
//
// gl_drawscene - this function renders the scene from the current
// viewpoint, including mirrors and skyboxes and other portals
// It is assumed that the HWPortal::EndFrame returns with the
// stencil, z-buffer and the projection matrix intact!
//
//-----------------------------------------------------------------------------

void MTLRenderer::DrawScene(HWDrawInfo *di, int drawmode)
{
    static int recursion=0;
    static int ssao_portals_available = 0;
    const auto &vp = di->Viewpoint;

    bool applySSAO = false;
    if (drawmode == DM_MAINVIEW)
    {
        ssao_portals_available = gl_ssao_portals;
        applySSAO = true;
    }
    else if (drawmode == DM_OFFSCREEN)
    {
        ssao_portals_available = 0;
    }
    else if (drawmode == DM_PORTAL && ssao_portals_available > 0)
    {
        applySSAO = true;
        ssao_portals_available--;
    }

    if (vp.camera != nullptr)
    {
        ActorRenderFlags savedflags = vp.camera->renderflags;
        di->CreateScene(drawmode == DM_MAINVIEW);
        vp.camera->renderflags = savedflags;
    }
    else
    {
        di->CreateScene(false);
    }

    ml_RenderState->EnableDepthTest(true);
    if (!ml_no_skyclear)
    {
        screen->mPortalState->RenderFirstSkyPortal(recursion, di, *ml_RenderState);
        //ml_RenderState->EndFrame();
    }

    di->RenderScene(*ml_RenderState);

    if (applySSAO && ml_RenderState->GetPassType() == GBUFFER_PASS)
    {
        ml_RenderState->EnableDrawBuffers(1);
        mlRenderer->AmbientOccludeScene(di->VPUniforms.mProjectionMatrix.get()[5]);
       // glViewport(screen->mSceneViewport.left, screen->mSceneViewport.top, screen->mSceneViewport.width, screen->mSceneViewport.height);
      //mlRenderer->mBuffers->BindSceneFB(true);
        ml_RenderState->EnableDrawBuffers(ml_RenderState->GetPassDrawBufferCount());
        ml_RenderState->Apply();
        screen->mViewpoints->Bind(*ml_RenderState, di->vpIndex);
    }

    // Handle all portals after rendering the opaque objects but before
    // doing all translucent stuff
    recursion++;
    //ml_RenderState->EndFrame();
    screen->mPortalState->EndFrame(di, *ml_RenderState);
    recursion--;
    di->RenderTranslucent(*ml_RenderState);
}

//-----------------------------------------------------------------------------
//
// Renders one viewpoint in a scene
//
//-----------------------------------------------------------------------------

sector_t * MTLRenderer::RenderViewpoint (FRenderViewpoint &mainvp, AActor * camera, IntRect * bounds, float fov, float ratio, float fovratio, bool mainview, bool toscreen)
{
    R_SetupFrame (mainvp, r_viewwindow, camera);

    if (mainview && toscreen)
        UpdateShadowMap();

    // Update the attenuation flag of all light defaults for each viewpoint.
    // This function will only do something if the setting differs.
    FLightDefaults::SetAttenuationForLevel(!!(camera->Level->flags3 & LEVEL3_ATTENUATE));

    // Render (potentially) multiple views for stereo 3d
    // Fixme. The view offsetting should be done with a static table and not require setup of the entire render state for the mode.
    auto vrmode = VRMode::GetVRMode(mainview && toscreen);
    const int eyeCount = vrmode->mEyeCount;
    mBuffers->CurrentEye() = 0;  // always begin at zero, in case eye count changed
    for (int eye_ix = 0; eye_ix < eyeCount; ++eye_ix)
    {
        const auto &eye = vrmode->mEyes[mBuffers->CurrentEye()];
        screen->SetViewportRects(bounds);

        if (mainview) // Bind the scene frame buffer and turn on draw buffers used by ssao
        {
            bool useSSAO = (gl_ssao != 0);
            //mBuffers->BindSceneFB(useSSAO);
            ml_RenderState->SetPassType(useSSAO ? GBUFFER_PASS : NORMAL_PASS);
            ml_RenderState->EnableDrawBuffers(ml_RenderState->GetPassDrawBufferCount());
            ml_RenderState->Apply();
        }

        auto di = HWDrawInfo::StartDrawInfo(mainvp.ViewLevel, nullptr, mainvp, nullptr);
        auto &vp = di->Viewpoint;

        di->Set3DViewport(*ml_RenderState);
        di->SetViewArea();
        auto cm =  di->SetFullbrightFlags(mainview ? vp.camera->player : nullptr);
        di->Viewpoint.FieldOfView = fov;    // Set the real FOV for the current scene (it's not necessarily the same as the global setting in r_viewpoint)

        // Stereo mode specific perspective projection
        di->VPUniforms.mProjectionMatrix = eye.GetProjection(fov, ratio, fovratio);
        // Stereo mode specific viewpoint adjustment
        vp.Pos += eye.GetViewShift(vp.HWAngles.Yaw.Degrees);
        di->SetupView(*ml_RenderState, vp.Pos.X, vp.Pos.Y, vp.Pos.Z, false, false);

        // std::function until this can be done better in a cross-API fashion.
        di->ProcessScene(toscreen, [&](HWDrawInfo *di, int mode) {
            DrawScene(di, mode);
        });

        if (mainview)
        {
            PostProcess.Clock();
            if (toscreen) di->EndDrawScene(mainvp.sector, *ml_RenderState); // do not call this for camera textures.

            if (ml_RenderState->GetPassType() == GBUFFER_PASS) // Turn off ssao draw buffers
            {
                ml_RenderState->SetPassType(NORMAL_PASS);
                ml_RenderState->EnableDrawBuffers(1);
            }

            //mBuffers->BlitSceneToTexture(); // Copy the resulting scene to the current post process texture

            PostProcessScene(cm, [&]() { di->DrawEndScene2D(mainvp.sector, *ml_RenderState); });
            PostProcess.Unclock();
        }
        di->EndDrawInfo();
        //if (eyeCount - eye_ix > 1)
        //    mBuffers->NextEye(eyeCount);
    }

    return mainvp.sector;
}

}
