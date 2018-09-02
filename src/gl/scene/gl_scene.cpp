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
#include "hwrenderer/dynlights/hw_dynlightdata.h"

#include "gl/dynlights/gl_lightbuffer.h"
#include "gl_load/gl_interface.h"
#include "gl/system/gl_framebuffer.h"
#include "gl/system/gl_debug.h"
#include "hwrenderer/utility/hw_cvars.h"
#include "gl/renderer/gl_lightdata.h"
#include "gl/renderer/gl_renderstate.h"
#include "gl/renderer/gl_renderbuffers.h"
#include "gl/data/gl_vertexbuffer.h"
#include "gl/data/gl_viewpointbuffer.h"
#include "hwrenderer/scene/hw_clipper.h"
#include "hwrenderer/scene/hw_portal.h"
#include "gl/scene/gl_drawinfo.h"
#include "gl/scene/gl_portal.h"
#include "hwrenderer/utility/hw_vrmodes.h"

//==========================================================================
//
// CVARs
//
//==========================================================================
CVAR(Bool, gl_texture, true, 0)
CVAR(Bool, gl_no_skyclear, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR(Float, gl_mask_threshold, 0.5f,CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR(Float, gl_mask_sprite_threshold, 0.5f,CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR(Bool, gl_sort_textures, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)

EXTERN_CVAR (Bool, cl_capfps)
EXTERN_CVAR (Bool, r_deathcamera)
EXTERN_CVAR (Float, r_visibility)
EXTERN_CVAR (Bool, r_drawvoxels)


void FDrawInfo::ApplyVPUniforms()
{
	VPUniforms.CalcDependencies();
	vpIndex = GLRenderer->mViewpoints->SetViewpoint(&VPUniforms);
}


//-----------------------------------------------------------------------------
//
// CreateScene
//
// creates the draw lists for the current scene
//
//-----------------------------------------------------------------------------

void FDrawInfo::CreateScene()
{
	const auto &vp = Viewpoint;
	angle_t a1 = FrustumAngle();
	mClipper->SafeAddClipRangeRealAngles(vp.Angles.Yaw.BAMs() + a1, vp.Angles.Yaw.BAMs() - a1);

	// reset the portal manager
	GLRenderer->mPortalState.StartFrame();
	PO_LinkToSubsectors();

	ProcessAll.Clock();

	// clip the scene and fill the drawlists
	Bsp.Clock();
	GLRenderer->mVBO->Map();
	GLRenderer->mLights->Begin();

	// Give the DrawInfo the viewpoint in fixed point because that's what the nodes are.
	viewx = FLOAT2FIXED(vp.Pos.X);
	viewy = FLOAT2FIXED(vp.Pos.Y);

	validcount++;	// used for processing sidedefs only once by the renderer.
	 
	mShadowMap = &GLRenderer->mShadowMap;

	RenderBSPNode (level.HeadNode());
	PreparePlayerSprites(vp.sector, in_area);

	// Process all the sprites on the current portal's back side which touch the portal.
	if (mCurrentPortal != nullptr) mCurrentPortal->RenderAttached(this);
	Bsp.Unclock();

	// And now the crappy hacks that have to be done to avoid rendering anomalies.
	// These cannot be multithreaded when the time comes because all these depend
	// on the global 'validcount' variable.

	HandleMissingTextures(in_area);	// Missing upper/lower textures
	HandleHackedSubsectors();	// open sector hacks for deep water
	ProcessSectorStacks(in_area);		// merge visplanes of sector stacks
	GLRenderer->mLights->Finish();
	GLRenderer->mVBO->Unmap();

	ProcessAll.Unclock();

}

//-----------------------------------------------------------------------------
//
// RenderScene
//
// Draws the current draw lists for the non GLSL renderer
//
//-----------------------------------------------------------------------------

void FDrawInfo::RenderScene(int recursion)
{
	const auto &vp = Viewpoint;
	RenderAll.Clock();

	glDepthMask(true);
 	if (!gl_no_skyclear) GLRenderer->mPortalState.RenderFirstSkyPortal(recursion, this);

	gl_RenderState.EnableFog(true);
	gl_RenderState.BlendFunc(GL_ONE,GL_ZERO);

	if (gl_sort_textures)
	{
		drawlists[GLDL_PLAINWALLS].SortWalls();
		drawlists[GLDL_PLAINFLATS].SortFlats();
		drawlists[GLDL_MASKEDWALLS].SortWalls();
		drawlists[GLDL_MASKEDFLATS].SortFlats();
		drawlists[GLDL_MASKEDWALLSOFS].SortWalls();
	}

	// if we don't have a persistently mapped buffer, we have to process all the dynamic lights up front,
	// so that we don't have to do repeated map/unmap calls on the buffer.
	if (gl.lightmethod == LM_DEFERRED && level.HasDynamicLights && !isFullbrightScene())
	{
		GLRenderer->mLights->Begin();
		drawlists[GLDL_PLAINFLATS].DrawFlats(this, GLPASS_LIGHTSONLY);
		drawlists[GLDL_MASKEDFLATS].DrawFlats(this, GLPASS_LIGHTSONLY);
		drawlists[GLDL_TRANSLUCENTBORDER].Draw(this, GLPASS_LIGHTSONLY);
		drawlists[GLDL_TRANSLUCENT].Draw(this, GLPASS_LIGHTSONLY, true);
		GLRenderer->mLights->Finish();
	}

	// Part 1: solid geometry. This is set up so that there are no transparent parts
	glDepthFunc(GL_LESS);
	gl_RenderState.AlphaFunc(GL_GEQUAL, 0.f);
	glDisable(GL_POLYGON_OFFSET_FILL);

	int pass = GLPASS_ALL;

	gl_RenderState.EnableTexture(gl_texture);
	gl_RenderState.EnableBrightmap(true);
	drawlists[GLDL_PLAINWALLS].DrawWalls(this, pass);
	drawlists[GLDL_PLAINFLATS].DrawFlats(this, pass);


	// Part 2: masked geometry. This is set up so that only pixels with alpha>gl_mask_threshold will show
	if (!gl_texture) 
	{
		gl_RenderState.EnableTexture(true);
		gl_RenderState.SetTextureMode(TM_MASK);
	}
	gl_RenderState.AlphaFunc(GL_GEQUAL, gl_mask_threshold);
	drawlists[GLDL_MASKEDWALLS].DrawWalls(this, pass);
	drawlists[GLDL_MASKEDFLATS].DrawFlats(this, pass);

	// Part 3: masked geometry with polygon offset. This list is empty most of the time so only waste time on it when in use.
	if (drawlists[GLDL_MASKEDWALLSOFS].Size() > 0)
	{
		glEnable(GL_POLYGON_OFFSET_FILL);
		glPolygonOffset(-1.0f, -128.0f);
		drawlists[GLDL_MASKEDWALLSOFS].DrawWalls(this, pass);
		glDisable(GL_POLYGON_OFFSET_FILL);
		glPolygonOffset(0, 0);
	}

	drawlists[GLDL_MODELS].Draw(this, pass);

	gl_RenderState.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// Part 4: Draw decals (not a real pass)
	glDepthFunc(GL_LEQUAL);
	glEnable(GL_POLYGON_OFFSET_FILL);
	glPolygonOffset(-1.0f, -128.0f);
	glDepthMask(false);
	DrawDecals();

	gl_RenderState.SetTextureMode(TM_MODULATE);

	glDepthMask(true);


	// Push bleeding floor/ceiling textures back a little in the z-buffer
	// so they don't interfere with overlapping mid textures.
	glPolygonOffset(1.0f, 128.0f);

	// Part 5: flood all the gaps with the back sector's flat texture
	// This will always be drawn like GLDL_PLAIN, depending on the fog settings
	
	glDepthMask(false);							// don't write to Z-buffer!
	gl_RenderState.EnableFog(true);
	gl_RenderState.AlphaFunc(GL_GEQUAL, 0.f);
	gl_RenderState.BlendFunc(GL_ONE,GL_ZERO);
	DrawUnhandledMissingTextures();
	glDepthMask(true);

	glPolygonOffset(0.0f, 0.0f);
	glDisable(GL_POLYGON_OFFSET_FILL);
	RenderAll.Unclock();

}

//-----------------------------------------------------------------------------
//
// RenderTranslucent
//
// Draws the current draw lists for the non GLSL renderer
//
//-----------------------------------------------------------------------------

void FDrawInfo::RenderTranslucent()
{
	RenderAll.Clock();

	// final pass: translucent stuff
	gl_RenderState.AlphaFunc(GL_GEQUAL, gl_mask_sprite_threshold);
	gl_RenderState.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	gl_RenderState.EnableBrightmap(true);
	drawlists[GLDL_TRANSLUCENTBORDER].Draw(this, GLPASS_TRANSLUCENT);
	glDepthMask(false);
	DrawSorted(GLDL_TRANSLUCENT);
	gl_RenderState.EnableBrightmap(false);


	gl_RenderState.AlphaFunc(GL_GEQUAL, 0.5f);
	glDepthMask(true);

	RenderAll.Unclock();
}


//-----------------------------------------------------------------------------
//
// gl_drawscene - this function renders the scene from the current
// viewpoint, including mirrors and skyboxes and other portals
// It is assumed that the GLPortal::EndFrame returns with the 
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

	RenderScene(recursion);

	if (applySSAO && gl_RenderState.GetPassType() == GBUFFER_PASS)
	{
		gl_RenderState.EnableDrawBuffers(1);
		GLRenderer->AmbientOccludeScene(VPUniforms.mProjectionMatrix.get()[5]);
		glViewport(screen->mSceneViewport.left, screen->mSceneViewport.top, screen->mSceneViewport.width, screen->mSceneViewport.height);
		GLRenderer->mBuffers->BindSceneFB(true);
		gl_RenderState.EnableDrawBuffers(gl_RenderState.GetPassDrawBufferCount());
		gl_RenderState.Apply();
		GLRenderer->mViewpoints->Bind(vpIndex);
	}

	// Handle all portals after rendering the opaque objects but before
	// doing all translucent stuff
	recursion++;
	GLRenderer->mPortalState.EndFrame(this);
	recursion--;
	RenderTranslucent();
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
		DrawPlayerSprites(true);
	}

	glDisable(GL_STENCIL_TEST);
    glViewport(screen->mScreenViewport.left, screen->mScreenViewport.top, screen->mScreenViewport.width, screen->mScreenViewport.height);

	// Restore standard rendering state
	gl_RenderState.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
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
	GLRenderer->mViewpoints->SetViewpoint(&vp);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_MULTISAMPLE);


 	DrawPlayerSprites(false);

	gl_RenderState.SetSoftLightLevel(-1);

	// Restore standard rendering state
	gl_RenderState.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
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
	GLRenderer->mPortalState.BeginScene();

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
    glClearColor(mSceneClearColor[0], mSceneClearColor[1], mSceneClearColor[2], 1.0f);
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

		FDrawInfo *di = FDrawInfo::StartDrawInfo(mainvp, nullptr);
		auto &vp = di->Viewpoint;
		di->SetViewArea();
		auto cm =  di->SetFullbrightFlags(mainview ? vp.camera->player : nullptr);
		di->Viewpoint.FieldOfView = fov;	// Set the real FOV for the current scene (it's not necessarily the same as the global setting in r_viewpoint)

		// Stereo mode specific perspective projection
		di->VPUniforms.mProjectionMatrix = eye.GetProjection(fov, ratio, fovratio);
		// Stereo mode specific viewpoint adjustment
		vp.Pos += eye.GetViewShift(vp.HWAngles.Yaw.Degrees);
		di->SetupView(vp.Pos.X, vp.Pos.Y, vp.Pos.Z, false, false);

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

