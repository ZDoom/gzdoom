// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2004-2016 Christoph Oelckers
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
/*
** gl_scene.cpp
** manages the rendering of the player's view
**
*/

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
#include "r_data/models/models.h"
#include "hwrenderer/dynlights/hw_dynlightdata.h"
#include "hwrenderer/utility/hw_clock.h"
#include "hwrenderer/data/flatvertices.h"

#include "hwrenderer/dynlights/hw_lightbuffer.h"
#include "gl_load/gl_interface.h"
#include "gl/system/gl_framebuffer.h"
#include "gl/system/gl_debug.h"
#include "hwrenderer/utility/hw_cvars.h"
#include "gl/renderer/gl_renderstate.h"
#include "gl/renderer/gl_renderbuffers.h"
#include "hwrenderer/data/hw_viewpointbuffer.h"
#include "hwrenderer/scene/hw_clipper.h"
#include "hwrenderer/scene/hw_portal.h"
#include "gl/scene/gl_drawinfo.h"
#include "hwrenderer/utility/hw_vrmodes.h"
#include "gl/renderer/gl_renderer.h"

//==========================================================================
//
// CVARs
//
//==========================================================================
CVAR(Bool, gl_texture, true, 0)
CVAR(Bool, gl_no_skyclear, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR(Float, gl_mask_threshold, 0.5f,CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR(Float, gl_mask_sprite_threshold, 0.5f,CVAR_ARCHIVE|CVAR_GLOBALCONFIG)

EXTERN_CVAR (Bool, cl_capfps)
EXTERN_CVAR (Bool, r_deathcamera)
EXTERN_CVAR (Float, r_visibility)
EXTERN_CVAR (Bool, r_drawvoxels)


//-----------------------------------------------------------------------------
//
// gl_drawscene - this function renders the scene from the current
// viewpoint, including mirrors and skyboxes and other portals
// It is assumed that the HWPortal::EndFrame returns with the 
// stencil, z-buffer and the projection matrix intact!
//
//-----------------------------------------------------------------------------

void FDrawInfo::DrawScene(int drawmode)
{
	static int recursion=0;
	static int ssao_portals_available = 0;
	const auto &vp = Viewpoint;

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
		CreateScene();
		vp.camera->renderflags = savedflags;
	}
	else
	{
		CreateScene();
	}

	glDepthMask(true);
	if (!gl_no_skyclear) screen->mPortalState->RenderFirstSkyPortal(recursion, this, gl_RenderState);

	RenderScene(gl_RenderState);

	if (applySSAO && gl_RenderState.GetPassType() == GBUFFER_PASS)
	{
		gl_RenderState.EnableDrawBuffers(1);
		GLRenderer->AmbientOccludeScene(VPUniforms.mProjectionMatrix.get()[5]);
		glViewport(screen->mSceneViewport.left, screen->mSceneViewport.top, screen->mSceneViewport.width, screen->mSceneViewport.height);
		GLRenderer->mBuffers->BindSceneFB(true);
		gl_RenderState.EnableDrawBuffers(gl_RenderState.GetPassDrawBufferCount());
		gl_RenderState.Apply();
		screen->mViewpoints->Bind(gl_RenderState, vpIndex);
	}

	// Handle all portals after rendering the opaque objects but before
	// doing all translucent stuff
	recursion++;
	screen->mPortalState->EndFrame(this, gl_RenderState);
	recursion--;
	RenderTranslucent(gl_RenderState);
}

//-----------------------------------------------------------------------------
//
// Draws player sprites and color blend
//
//-----------------------------------------------------------------------------


void FDrawInfo::EndDrawScene(sector_t * viewsector)
{
	gl_RenderState.EnableFog(false);

	// [BB] HUD models need to be rendered here. 
	const bool renderHUDModel = IsHUDModelForPlayerAvailable( players[consoleplayer].camera->player );
	if ( renderHUDModel )
	{
		// [BB] The HUD model should be drawn over everything else already drawn.
		glClear(GL_DEPTH_BUFFER_BIT);
		DrawPlayerSprites(true, gl_RenderState);
	}

	glDisable(GL_STENCIL_TEST);
    glViewport(screen->mScreenViewport.left, screen->mScreenViewport.top, screen->mScreenViewport.width, screen->mScreenViewport.height);

	// Restore standard rendering state
	gl_RenderState.SetRenderStyle(STYLE_Translucent);
	gl_RenderState.ResetColor();
	gl_RenderState.EnableTexture(true);
	glDisable(GL_SCISSOR_TEST);
}

void FDrawInfo::DrawEndScene2D(sector_t * viewsector)
{
	const bool renderHUDModel = IsHUDModelForPlayerAvailable(players[consoleplayer].camera->player);
	auto vrmode = VRMode::GetVRMode(true);

	HWViewpointUniforms vp = VPUniforms;
	vp.mViewMatrix.loadIdentity();
	vp.mProjectionMatrix = vrmode->GetHUDSpriteProjection();
	screen->mViewpoints->SetViewpoint(gl_RenderState, &vp);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_MULTISAMPLE);


 	DrawPlayerSprites(false, gl_RenderState);

	gl_RenderState.SetSoftLightLevel(-1);

	// Restore standard rendering state
	gl_RenderState.SetRenderStyle(STYLE_Translucent);
	gl_RenderState.ResetColor();
	gl_RenderState.EnableTexture(true);
	glDisable(GL_SCISSOR_TEST);
}

//-----------------------------------------------------------------------------
//
// R_RenderView - renders one view - either the screen or a camera texture
//
//-----------------------------------------------------------------------------

void FDrawInfo::ProcessScene(bool toscreen)
{
	iter_dlightf = iter_dlight = draw_dlight = draw_dlightf = 0;
	screen->mPortalState->BeginScene();

	int mapsection = R_PointInSubsector(Viewpoint.Pos)->mapsection;
	CurrentMapSections.Set(mapsection);
	DrawScene(toscreen ? DM_MAINVIEW : DM_OFFSCREEN);

}

//-----------------------------------------------------------------------------
//
// sets 3D viewport and initial state
//
//-----------------------------------------------------------------------------

void FGLRenderer::Set3DViewport()
{
    // Always clear all buffers with scissor test disabled.
    // This is faster on newer hardware because it allows the GPU to skip
    // reading from slower memory where the full buffers are stored.
    glDisable(GL_SCISSOR_TEST);
	glClearColor(screen->mSceneClearColor[0], screen->mSceneClearColor[1], screen->mSceneClearColor[2], screen->mSceneClearColor[3]);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    
    const auto &bounds = screen->mSceneViewport;
    glViewport(bounds.left, bounds.top, bounds.width, bounds.height);
    glScissor(bounds.left, bounds.top, bounds.width, bounds.height);
    
    glEnable(GL_SCISSOR_TEST);
    
    glEnable(GL_MULTISAMPLE);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_ALWAYS,0,~0);    // default stencil
    glStencilOp(GL_KEEP,GL_KEEP,GL_REPLACE);
}

//-----------------------------------------------------------------------------
//
// Renders one viewpoint in a scene
//
//-----------------------------------------------------------------------------

sector_t * FGLRenderer::RenderViewpoint (FRenderViewpoint &mainvp, AActor * camera, IntRect * bounds, float fov, float ratio, float fovratio, bool mainview, bool toscreen)
{
	R_SetupFrame (mainvp, r_viewwindow, camera);

    // Render (potentially) multiple views for stereo 3d
	// Fixme. The view offsetting should be done with a static table and not require setup of the entire render state for the mode.
	auto vrmode = VRMode::GetVRMode(mainview && toscreen);
	for (int eye_ix = 0; eye_ix < vrmode->mEyeCount; ++eye_ix)
	{
		const auto &eye = vrmode->mEyes[eye_ix];
		screen->SetViewportRects(bounds);

		if (mainview) // Bind the scene frame buffer and turn on draw buffers used by ssao
		{
			FGLDebug::PushGroup("MainView");

			bool useSSAO = (gl_ssao != 0);
			mBuffers->BindSceneFB(useSSAO);
			gl_RenderState.SetPassType(useSSAO ? GBUFFER_PASS : NORMAL_PASS);
			gl_RenderState.EnableDrawBuffers(gl_RenderState.GetPassDrawBufferCount());
			gl_RenderState.Apply();
		}

		Set3DViewport();

		FDrawInfo *di = static_cast<FDrawInfo*>(HWDrawInfo::StartDrawInfo(mainvp, nullptr));
		auto &vp = di->Viewpoint;
		di->SetViewArea();
		auto cm =  di->SetFullbrightFlags(mainview ? vp.camera->player : nullptr);
		di->Viewpoint.FieldOfView = fov;	// Set the real FOV for the current scene (it's not necessarily the same as the global setting in r_viewpoint)

		// Stereo mode specific perspective projection
		di->VPUniforms.mProjectionMatrix = eye.GetProjection(fov, ratio, fovratio);
		// Stereo mode specific viewpoint adjustment
		vp.Pos += eye.GetViewShift(vp.HWAngles.Yaw.Degrees);
		di->SetupView(gl_RenderState, vp.Pos.X, vp.Pos.Y, vp.Pos.Z, false, false);

		di->ProcessScene(toscreen);

		if (mainview)
		{
			PostProcess.Clock();
			if (toscreen) di->EndDrawScene(mainvp.sector); // do not call this for camera textures.

			if (gl_RenderState.GetPassType() == GBUFFER_PASS) // Turn off ssao draw buffers
			{
				gl_RenderState.SetPassType(NORMAL_PASS);
				gl_RenderState.EnableDrawBuffers(1);
			}

			mBuffers->BlitSceneToTexture(); // Copy the resulting scene to the current post process texture

			FGLDebug::PopGroup(); // MainView

			PostProcessScene(cm, [&]() { di->DrawEndScene2D(mainvp.sector); });
			PostProcess.Unclock();
		}
		di->EndDrawInfo();
		if (vrmode->mEyeCount > 1)
			mBuffers->BlitToEyeTexture(eye_ix);
	}

	interpolator.RestoreInterpolations ();
	return mainvp.sector;
}

