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

#include "gl/system/gl_system.h"
#include "i_time.h"
#include "gi.h"
#include "m_png.h"
#include "m_random.h"
#include "st_stuff.h"
#include "dobject.h"
#include "doomstat.h"
#include "g_level.h"
#include "r_data/r_interpolate.h"
#include "r_utility.h"
#include "d_player.h"
#include "p_effect.h"
#include "sbar.h"
#include "po_man.h"
#include "r_utility.h"
#include "p_local.h"
#include "gl/gl_functions.h"
#include "serializer.h"
#include "g_levellocals.h"
#include "events.h"

#include "gl/dynlights/gl_lightbuffer.h"
#include "gl/system/gl_interface.h"
#include "gl/system/gl_framebuffer.h"
#include "gl/system/gl_cvars.h"
#include "gl/renderer/gl_lightdata.h"
#include "gl/renderer/gl_renderstate.h"
#include "gl/renderer/gl_renderbuffers.h"
#include "gl/data/gl_data.h"
#include "gl/data/gl_vertexbuffer.h"
#include "gl/dynlights/gl_dynlight.h"
#include "gl/models/gl_models.h"
#include "gl/scene/gl_clipper.h"
#include "gl/scene/gl_drawinfo.h"
#include "gl/scene/gl_portal.h"
#include "gl/scene/gl_scenedrawer.h"
#include "gl/shaders/gl_shader.h"
#include "gl/stereo3d/gl_stereo3d.h"
#include "gl/stereo3d/scoped_view_shifter.h"
#include "gl/textures/gl_material.h"
#include "gl/utility/gl_clock.h"
#include "gl/utility/gl_convert.h"
#include "gl/utility/gl_templates.h"

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
EXTERN_CVAR (Float, underwater_fade_scalar)
EXTERN_CVAR (Float, r_visibility)
EXTERN_CVAR (Bool, gl_legacy_mode)
EXTERN_CVAR (Bool, r_drawvoxels)

extern bool NoInterpolateView;

area_t			in_area;
TArray<uint8_t> currentmapsection;
int camtexcount;

//-----------------------------------------------------------------------------
//
// R_FrustumAngle
//
//-----------------------------------------------------------------------------
angle_t GLSceneDrawer::FrustumAngle()
{
	float tilt = fabs(GLRenderer->mAngles.Pitch.Degrees);

	// If the pitch is larger than this you can look all around at a FOV of 90°
	if (tilt > 46.0f) return 0xffffffff;

	// ok, this is a gross hack that barely works...
	// but at least it doesn't overestimate too much...
	double floatangle = 2.0 + (45.0 + ((tilt / 1.9)))*GLRenderer->mCurrentFoV*48.0 / AspectMultiplier(r_viewwindow.WidescreenRatio) / 90.0;
	angle_t a1 = DAngle(floatangle).BAMs();
	if (a1 >= ANGLE_180) return 0xffffffff;
	return a1;
}

//-----------------------------------------------------------------------------
//
// Sets the area the camera is in
//
//-----------------------------------------------------------------------------
void GLSceneDrawer::SetViewArea()
{
	// The render_sector is better suited to represent the current position in GL
	r_viewpoint.sector = R_PointInSubsector(r_viewpoint.Pos)->render_sector;

	// Get the heightsec state from the render sector, not the current one!
	if (r_viewpoint.sector->heightsec && !(r_viewpoint.sector->heightsec->MoreFlags & SECF_IGNOREHEIGHTSEC))
	{
		in_area = r_viewpoint.Pos.Z <= r_viewpoint.sector->heightsec->floorplane.ZatPoint(r_viewpoint.Pos) ? area_below :
				(r_viewpoint.Pos.Z > r_viewpoint.sector->heightsec->ceilingplane.ZatPoint(r_viewpoint.Pos) &&
				!(r_viewpoint.sector->heightsec->MoreFlags&SECF_FAKEFLOORONLY)) ? area_above : area_normal;
	}
	else
	{
		in_area = area_default;	// depends on exposed lower sectors
	}
}

//-----------------------------------------------------------------------------
//
// resets the 3D viewport
//
//-----------------------------------------------------------------------------

void GLSceneDrawer::Reset3DViewport()
{
	glViewport(GLRenderer->mScreenViewport.left, GLRenderer->mScreenViewport.top, GLRenderer->mScreenViewport.width, GLRenderer->mScreenViewport.height);
}

//-----------------------------------------------------------------------------
//
// sets 3D viewport and initial state
//
//-----------------------------------------------------------------------------

void GLSceneDrawer::Set3DViewport(bool mainview)
{
	if (mainview && GLRenderer->mBuffers->Setup(GLRenderer->mScreenViewport.width, GLRenderer->mScreenViewport.height, GLRenderer->mSceneViewport.width, GLRenderer->mSceneViewport.height))
	{
		bool useSSAO = (gl_ssao != 0);
		GLRenderer->mBuffers->BindSceneFB(useSSAO);
		gl_RenderState.SetPassType(useSSAO ? GBUFFER_PASS : NORMAL_PASS);
		gl_RenderState.EnableDrawBuffers(gl_RenderState.GetPassDrawBufferCount());
		gl_RenderState.Apply();
	}

	// Always clear all buffers with scissor test disabled.
	// This is faster on newer hardware because it allows the GPU to skip
	// reading from slower memory where the full buffers are stored.
	glDisable(GL_SCISSOR_TEST);
	glClearColor(GLRenderer->mSceneClearColor[0], GLRenderer->mSceneClearColor[1], GLRenderer->mSceneClearColor[2], 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

	const auto &bounds = GLRenderer->mSceneViewport;
	glViewport(bounds.left, bounds.top, bounds.width, bounds.height);
	glScissor(bounds.left, bounds.top, bounds.width, bounds.height);

	glEnable(GL_SCISSOR_TEST);

	glEnable(GL_MULTISAMPLE);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_STENCIL_TEST);
	glStencilFunc(GL_ALWAYS,0,~0);	// default stencil
	glStencilOp(GL_KEEP,GL_KEEP,GL_REPLACE);
}

//-----------------------------------------------------------------------------
//
// Setup the camera position
//
//-----------------------------------------------------------------------------

void GLSceneDrawer::SetViewAngle(DAngle viewangle)
{
	GLRenderer->mAngles.Yaw = float(270.0-viewangle.Degrees);
	DVector2 v = r_viewpoint.Angles.Yaw.ToVector();
	GLRenderer->mViewVector.X = v.X;
	GLRenderer->mViewVector.Y = v.Y;

	R_SetViewAngle(r_viewpoint, r_viewwindow);
}
	

//-----------------------------------------------------------------------------
//
// SetProjection
// sets projection matrix
//
//-----------------------------------------------------------------------------

void GLSceneDrawer::SetProjection(VSMatrix matrix)
{
	gl_RenderState.mProjectionMatrix.loadIdentity();
	gl_RenderState.mProjectionMatrix.multMatrix(matrix);
}

//-----------------------------------------------------------------------------
//
// Setup the modelview matrix
//
//-----------------------------------------------------------------------------

void GLSceneDrawer::SetViewMatrix(float vx, float vy, float vz, bool mirror, bool planemirror)
{
	float mult = mirror? -1:1;
	float planemult = planemirror? -level.info->pixelstretch : level.info->pixelstretch;

	gl_RenderState.mViewMatrix.loadIdentity();
	gl_RenderState.mViewMatrix.rotate(GLRenderer->mAngles.Roll.Degrees,  0.0f, 0.0f, 1.0f);
	gl_RenderState.mViewMatrix.rotate(GLRenderer->mAngles.Pitch.Degrees, 1.0f, 0.0f, 0.0f);
	gl_RenderState.mViewMatrix.rotate(GLRenderer->mAngles.Yaw.Degrees,   0.0f, mult, 0.0f);
	gl_RenderState.mViewMatrix.translate(vx * mult, -vz * planemult , -vy);
	gl_RenderState.mViewMatrix.scale(-mult, planemult, 1);
}


//-----------------------------------------------------------------------------
//
// SetupView
// Setup the view rotation matrix for the given viewpoint
//
//-----------------------------------------------------------------------------
void GLSceneDrawer::SetupView(float vx, float vy, float vz, DAngle va, bool mirror, bool planemirror)
{
	SetViewAngle(va);
	SetViewMatrix(vx, vy, vz, mirror, planemirror);
	gl_RenderState.ApplyMatrices();
}

//-----------------------------------------------------------------------------
//
// CreateScene
//
// creates the draw lists for the current scene
//
//-----------------------------------------------------------------------------

void GLSceneDrawer::CreateScene()
{
	angle_t a1 = FrustumAngle();
	InitClipper(r_viewpoint.Angles.Yaw.BAMs() + a1, r_viewpoint.Angles.Yaw.BAMs() - a1);

	// reset the portal manager
	GLPortal::StartFrame();
	PO_LinkToSubsectors();

	ProcessAll.Clock();

	// clip the scene and fill the drawlists
	for(unsigned i=0;i<glSectorPortals.Size(); i++) glSectorPortals[i]->glportal = NULL;
	GLRenderer->gl_spriteindex=0;
	Bsp.Clock();
	GLRenderer->mVBO->Map();
	SetView();
	validcount++;	// used for processing sidedefs only once by the renderer.
	RenderBSPNode (level.HeadNode());
	if (GLRenderer->mCurrentPortal != NULL) GLRenderer->mCurrentPortal->RenderAttached();
	Bsp.Unclock();

	// And now the crappy hacks that have to be done to avoid rendering anomalies:

	gl_drawinfo->HandleMissingTextures();	// Missing upper/lower textures
	gl_drawinfo->HandleHackedSubsectors();	// open sector hacks for deep water
	gl_drawinfo->ProcessSectorStacks();		// merge visplanes of sector stacks
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

void GLSceneDrawer::RenderScene(int recursion)
{
	RenderAll.Clock();

	glDepthMask(true);
	if (!gl_no_skyclear) GLPortal::RenderFirstSkyPortal(recursion);

	gl_RenderState.SetCameraPos(r_viewpoint.Pos.X, r_viewpoint.Pos.Y, r_viewpoint.Pos.Z);

	gl_RenderState.EnableFog(true);
	gl_RenderState.BlendFunc(GL_ONE,GL_ZERO);

	if (gl_sort_textures)
	{
		gl_drawinfo->drawlists[GLDL_PLAINWALLS].SortWalls();
		gl_drawinfo->drawlists[GLDL_PLAINFLATS].SortFlats();
		gl_drawinfo->drawlists[GLDL_MASKEDWALLS].SortWalls();
		gl_drawinfo->drawlists[GLDL_MASKEDFLATS].SortFlats();
		gl_drawinfo->drawlists[GLDL_MASKEDWALLSOFS].SortWalls();
	}

	// if we don't have a persistently mapped buffer, we have to process all the dynamic lights up front,
	// so that we don't have to do repeated map/unmap calls on the buffer.
	bool haslights = GLRenderer->mLightCount > 0 && FixedColormap == CM_DEFAULT && gl_lights;
	if (gl.lightmethod == LM_DEFERRED && haslights)
	{
		GLRenderer->mLights->Begin();
		gl_drawinfo->drawlists[GLDL_PLAINWALLS].DrawWalls(GLPASS_LIGHTSONLY);
		gl_drawinfo->drawlists[GLDL_PLAINFLATS].DrawFlats(GLPASS_LIGHTSONLY);
		gl_drawinfo->drawlists[GLDL_MASKEDWALLS].DrawWalls(GLPASS_LIGHTSONLY);
		gl_drawinfo->drawlists[GLDL_MASKEDFLATS].DrawFlats(GLPASS_LIGHTSONLY);
		gl_drawinfo->drawlists[GLDL_MASKEDWALLSOFS].DrawWalls(GLPASS_LIGHTSONLY);
		gl_drawinfo->drawlists[GLDL_TRANSLUCENTBORDER].Draw(GLPASS_LIGHTSONLY);
		gl_drawinfo->drawlists[GLDL_TRANSLUCENT].Draw(GLPASS_LIGHTSONLY, true);
		GLRenderer->mLights->Finish();
	}

	// Part 1: solid geometry. This is set up so that there are no transparent parts
	glDepthFunc(GL_LESS);
	gl_RenderState.AlphaFunc(GL_GEQUAL, 0.f);
	glDisable(GL_POLYGON_OFFSET_FILL);

	int pass;

	if (!haslights || gl.lightmethod == LM_DEFERRED)
	{
		pass = GLPASS_PLAIN;
	}
	else if (gl.lightmethod == LM_DIRECT)
	{
		pass = GLPASS_ALL;
	}
	else // GL 2.x legacy mode
	{
		// process everything that needs to handle textured dynamic lights.
		if (haslights) RenderMultipassStuff();

		// The remaining lists which are unaffected by dynamic lights are just processed as normal.
		pass = GLPASS_PLAIN;
	}

	gl_RenderState.EnableTexture(gl_texture);
	gl_RenderState.EnableBrightmap(true);
	gl_drawinfo->drawlists[GLDL_PLAINWALLS].DrawWalls(pass);
	gl_drawinfo->drawlists[GLDL_PLAINFLATS].DrawFlats(pass);


	// Part 2: masked geometry. This is set up so that only pixels with alpha>gl_mask_threshold will show
	if (!gl_texture) 
	{
		gl_RenderState.EnableTexture(true);
		gl_RenderState.SetTextureMode(TM_MASK);
	}
	gl_RenderState.AlphaFunc(GL_GEQUAL, gl_mask_threshold);
	gl_drawinfo->drawlists[GLDL_MASKEDWALLS].DrawWalls(pass);
	gl_drawinfo->drawlists[GLDL_MASKEDFLATS].DrawFlats(pass);

	// Part 3: masked geometry with polygon offset. This list is empty most of the time so only waste time on it when in use.
	if (gl_drawinfo->drawlists[GLDL_MASKEDWALLSOFS].Size() > 0)
	{
		glEnable(GL_POLYGON_OFFSET_FILL);
		glPolygonOffset(-1.0f, -128.0f);
		gl_drawinfo->drawlists[GLDL_MASKEDWALLSOFS].DrawWalls(pass);
		glDisable(GL_POLYGON_OFFSET_FILL);
		glPolygonOffset(0, 0);
	}

	gl_drawinfo->drawlists[GLDL_MODELS].Draw(pass);

	gl_RenderState.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// Part 4: Draw decals (not a real pass)
	glDepthFunc(GL_LEQUAL);
	glEnable(GL_POLYGON_OFFSET_FILL);
	glPolygonOffset(-1.0f, -128.0f);
	glDepthMask(false);

	// this is the only geometry type on which decals can possibly appear
	gl_drawinfo->drawlists[GLDL_PLAINWALLS].DrawDecals();
	if (gl.legacyMode)
	{
		// also process the render lists with walls and dynamic lights
        gl_drawinfo->dldrawlists[GLLDL_WALLS_PLAIN].DrawDecals();
        gl_drawinfo->dldrawlists[GLLDL_WALLS_FOG].DrawDecals();
	}

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
	gl_drawinfo->DrawUnhandledMissingTextures();
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

void GLSceneDrawer::RenderTranslucent()
{
	RenderAll.Clock();

	glDepthMask(false);
	gl_RenderState.SetCameraPos(r_viewpoint.Pos.X, r_viewpoint.Pos.Y, r_viewpoint.Pos.Z);

	// final pass: translucent stuff
	gl_RenderState.AlphaFunc(GL_GEQUAL, gl_mask_sprite_threshold);
	gl_RenderState.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	gl_RenderState.EnableBrightmap(true);
	gl_drawinfo->drawlists[GLDL_TRANSLUCENTBORDER].Draw(GLPASS_TRANSLUCENT);
	gl_drawinfo->drawlists[GLDL_TRANSLUCENT].DrawSorted();
	gl_RenderState.EnableBrightmap(false);

	glDepthMask(true);

	gl_RenderState.AlphaFunc(GL_GEQUAL, 0.5f);
	RenderAll.Unclock();
}


//-----------------------------------------------------------------------------
//
// gl_drawscene - this function renders the scene from the current
// viewpoint, including mirrors and skyboxes and other glSectorPortals
// It is assumed that the GLPortal::EndFrame returns with the 
// stencil, z-buffer and the projection matrix intact!
//
//-----------------------------------------------------------------------------

void GLSceneDrawer::DrawScene(int drawmode)
{
	static int recursion=0;
	static int ssao_portals_available = 0;

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

	if (r_viewpoint.camera != nullptr)
	{
		ActorRenderFlags savedflags = r_viewpoint.camera->renderflags;
		CreateScene();
		r_viewpoint.camera->renderflags = savedflags;
	}
	else
	{
		CreateScene();
	}
	GLRenderer->mClipPortal = NULL;	// this must be reset before any portal recursion takes place.

	RenderScene(recursion);

	if (applySSAO && gl_RenderState.GetPassType() == GBUFFER_PASS)
	{
		gl_RenderState.EnableDrawBuffers(1);
		GLRenderer->AmbientOccludeScene();
		GLRenderer->mBuffers->BindSceneFB(true);
		gl_RenderState.EnableDrawBuffers(gl_RenderState.GetPassDrawBufferCount());
		gl_RenderState.Apply();
		gl_RenderState.ApplyMatrices();
	}

	// Handle all glSectorPortals after rendering the opaque objects but before
	// doing all translucent stuff
	recursion++;
	GLPortal::EndFrame();
	recursion--;
	RenderTranslucent();
}


void gl_FillScreen()
{
	gl_RenderState.AlphaFunc(GL_GEQUAL, 0.f);
	gl_RenderState.EnableTexture(false);
	gl_RenderState.Apply();
	// The fullscreen quad is stored at index 4 in the main vertex buffer.
	GLRenderer->mVBO->RenderArray(GL_TRIANGLE_STRIP, FFlatVertexBuffer::FULLSCREEN_INDEX, 4);
}

//==========================================================================
//
// Draws a blend over the entire view
//
//==========================================================================
void GLSceneDrawer::DrawBlend(sector_t * viewsector)
{
	float blend[4]={0,0,0,0};
	PalEntry blendv=0;
	float extra_red;
	float extra_green;
	float extra_blue;
	player_t *player = NULL;

	if (players[consoleplayer].camera != NULL)
	{
		player=players[consoleplayer].camera->player;
	}

	// don't draw sector based blends when an invulnerability colormap is active
	if (!FixedColormap)
	{
		if (!viewsector->e->XFloor.ffloors.Size())
		{
			if (viewsector->heightsec && !(viewsector->MoreFlags&SECF_IGNOREHEIGHTSEC))
			{
				switch (in_area)
				{
				default:
				case area_normal: blendv = viewsector->heightsec->midmap; break;
				case area_above: blendv = viewsector->heightsec->topmap; break;
				case area_below: blendv = viewsector->heightsec->bottommap; break;
				}
			}
		}
		else
		{
			TArray<lightlist_t> & lightlist = viewsector->e->XFloor.lightlist;

			for (unsigned int i = 0; i < lightlist.Size(); i++)
			{
				double lightbottom;
				if (i < lightlist.Size() - 1)
					lightbottom = lightlist[i + 1].plane.ZatPoint(r_viewpoint.Pos);
				else
					lightbottom = viewsector->floorplane.ZatPoint(r_viewpoint.Pos);

				if (lightbottom < r_viewpoint.Pos.Z && (!lightlist[i].caster || !(lightlist[i].caster->flags&FF_FADEWALLS)))
				{
					// 3d floor 'fog' is rendered as a blending value
					blendv = lightlist[i].blend;
					// If this is the same as the sector's it doesn't apply!
					if (blendv == viewsector->Colormap.FadeColor) blendv = 0;
					// a little hack to make this work for Legacy maps.
					if (blendv.a == 0 && blendv != 0) blendv.a = 128;
					break;
				}
			}
		}

		if (blendv.a == 0)
		{
			blendv = R_BlendForColormap(blendv);
		}

		if (blendv.a == 255)
		{

			extra_red = blendv.r / 255.0f;
			extra_green = blendv.g / 255.0f;
			extra_blue = blendv.b / 255.0f;

			// If this is a multiplicative blend do it separately and add the additive ones on top of it.
			blendv = 0;

			// black multiplicative blends are ignored
			if (extra_red || extra_green || extra_blue)
			{
				gl_RenderState.BlendFunc(GL_DST_COLOR, GL_ZERO);
				gl_RenderState.SetColor(extra_red, extra_green, extra_blue, 1.0f);
				gl_FillScreen();
			}
		}
		else if (blendv.a)
		{
			// [Nash] allow user to set blend intensity
			int cnt = blendv.a;
			cnt = (int)(cnt * underwater_fade_scalar);

			V_AddBlend(blendv.r / 255.f, blendv.g / 255.f, blendv.b / 255.f, cnt / 255.0f, blend);
		}
	}

	// This mostly duplicates the code in shared_sbar.cpp
	// When I was writing this the original was called too late so that I
	// couldn't get the blend in time. However, since then I made some changes
	// here that would get lost if I switched back so I won't do it.

	if (player)
	{
		V_AddPlayerBlend(player, blend, 0.5, 175);
	}
	
	if (players[consoleplayer].camera != NULL)
	{
		// except for fadeto effects
		player_t *player = (players[consoleplayer].camera->player != NULL) ? players[consoleplayer].camera->player : &players[consoleplayer];
		V_AddBlend (player->BlendR, player->BlendG, player->BlendB, player->BlendA, blend);
	}

	gl_RenderState.SetTextureMode(TM_MODULATE);
	gl_RenderState.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	if (blend[3]>0.0f)
	{
		gl_RenderState.SetColor(blend[0], blend[1], blend[2], blend[3]);
		gl_FillScreen();
	}
	gl_RenderState.ResetColor();
	gl_RenderState.EnableTexture(true);
}


//-----------------------------------------------------------------------------
//
// Draws player sprites and color blend
//
//-----------------------------------------------------------------------------


void GLSceneDrawer::EndDrawScene(sector_t * viewsector)
{
	gl_RenderState.EnableFog(false);

	// [BB] HUD models need to be rendered here. Make sure that
	// DrawPlayerSprites is only called once. Either to draw
	// HUD models or to draw the weapon sprites.
	const bool renderHUDModel = gl_IsHUDModelForPlayerAvailable( players[consoleplayer].camera->player );
	if ( renderHUDModel )
	{
		// [BB] The HUD model should be drawn over everything else already drawn.
		glClear(GL_DEPTH_BUFFER_BIT);
		DrawPlayerSprites (viewsector, true);
	}

	glDisable(GL_STENCIL_TEST);

	GLRenderer->framebuffer->Begin2D(false);

	Reset3DViewport();

	// Delay drawing psprites until after bloom has been applied, if enabled.
	if (!FGLRenderBuffers::IsEnabled() || !gl_bloom || FixedColormap != CM_DEFAULT)
	{
		DrawEndScene2D(viewsector);
	}
	else
	{
		// Restore standard rendering state
		gl_RenderState.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		gl_RenderState.ResetColor();
		gl_RenderState.EnableTexture(true);
		glDisable(GL_SCISSOR_TEST);
	}
}

void GLSceneDrawer::DrawEndScene2D(sector_t * viewsector)
{
	const bool renderHUDModel = gl_IsHUDModelForPlayerAvailable(players[consoleplayer].camera->player);

	// [BB] Only draw the sprites if we didn't render a HUD model before.
	if (renderHUDModel == false)
	{
		DrawPlayerSprites(viewsector, false);
	}
	if (gl.legacyMode)
	{
		gl_RenderState.DrawColormapOverlay();
	}

	gl_RenderState.SetFixedColormap(CM_DEFAULT);
	gl_RenderState.SetSoftLightLevel(-1);
	DrawTargeterSprites();
	if (!FGLRenderBuffers::IsEnabled())
	{
		DrawBlend(viewsector);
	}

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

void GLSceneDrawer::ProcessScene(bool toscreen)
{
	FDrawInfo::StartDrawInfo(this);
	iter_dlightf = iter_dlight = draw_dlight = draw_dlightf = 0;
	GLPortal::BeginScene();

	int mapsection = R_PointInSubsector(r_viewpoint.Pos)->mapsection;
	memset(&currentmapsection[0], 0, currentmapsection.Size());
	currentmapsection[mapsection>>3] |= 1 << (mapsection & 7);
	DrawScene(toscreen ? DM_MAINVIEW : DM_OFFSCREEN);
	FDrawInfo::EndDrawInfo();

}

//-----------------------------------------------------------------------------
//
// gl_SetFixedColormap
//
//-----------------------------------------------------------------------------

void GLSceneDrawer::SetFixedColormap (player_t *player)
{
	FixedColormap=CM_DEFAULT;

	// check for special colormaps
	player_t * cplayer = player->camera->player;
	if (cplayer) 
	{
		if (cplayer->extralight == INT_MIN)
		{
			FixedColormap=CM_FIRSTSPECIALCOLORMAP + INVERSECOLORMAP;
			r_viewpoint.extralight=0;
		}
		else if (cplayer->fixedcolormap != NOFIXEDCOLORMAP)
		{
			FixedColormap = CM_FIRSTSPECIALCOLORMAP + cplayer->fixedcolormap;
		}
		else if (cplayer->fixedlightlevel != -1)
		{
			auto torchtype = PClass::FindActor(NAME_PowerTorch);
			auto litetype = PClass::FindActor(NAME_PowerLightAmp);
			for(AInventory * in = cplayer->mo->Inventory; in; in = in->Inventory)
			{
				PalEntry color = in->CallGetBlend ();

				// Need special handling for light amplifiers 
				if (in->IsKindOf(torchtype))
				{
					FixedColormap = cplayer->fixedlightlevel + CM_TORCH;
				}
				else if (in->IsKindOf(litetype))
				{
					FixedColormap = CM_LITE;
				}
			}
		}
	}
	gl_RenderState.SetFixedColormap(FixedColormap);
}

//-----------------------------------------------------------------------------
//
// Renders one viewpoint in a scene
//
//-----------------------------------------------------------------------------

sector_t * GLSceneDrawer::RenderViewpoint (AActor * camera, GL_IRECT * bounds, float fov, float ratio, float fovratio, bool mainview, bool toscreen)
{       
	sector_t * lviewsector;
	GLRenderer->mSceneClearColor[0] = 0.0f;
	GLRenderer->mSceneClearColor[1] = 0.0f;
	GLRenderer->mSceneClearColor[2] = 0.0f;
	R_SetupFrame (r_viewpoint, r_viewwindow, camera);
	SetViewArea();

	GLRenderer->mGlobVis = R_GetGlobVis(r_viewwindow, r_visibility);

	// We have to scale the pitch to account for the pixel stretching, because the playsim doesn't know about this and treats it as 1:1.
	double radPitch = r_viewpoint.Angles.Pitch.Normalized180().Radians();
	double angx = cos(radPitch);
	double angy = sin(radPitch) * level.info->pixelstretch;
	double alen = sqrt(angx*angx + angy*angy);

	GLRenderer->mAngles.Pitch = (float)RAD2DEG(asin(angy / alen));
	GLRenderer->mAngles.Roll.Degrees = r_viewpoint.Angles.Roll.Degrees;

	// Scroll the sky
	GLRenderer->mSky1Pos = (double)fmod((double)screen->FrameTime * (double)level.skyspeed1, 1024.f) * 90./256.;
	GLRenderer->mSky2Pos = (double)fmod((double)screen->FrameTime * (double)level.skyspeed2, 1024.f) * 90./256.;



	if (camera->player && camera->player-players==consoleplayer &&
		((camera->player->cheats & CF_CHASECAM) || (r_deathcamera && camera->health <= 0)) && camera==camera->player->mo)
	{
		GLRenderer->mViewActor=NULL;
	}
	else
	{
		GLRenderer->mViewActor=camera;
	}

	// 'viewsector' will not survive the rendering so it cannot be used anymore below.
	lviewsector = r_viewpoint.sector;

	// Render (potentially) multiple views for stereo 3d
	float viewShift[3];
	const s3d::Stereo3DMode& stereo3dMode = mainview && toscreen? s3d::Stereo3DMode::getCurrentMode() : s3d::Stereo3DMode::getMonoMode();
	stereo3dMode.SetUp();
	for (int eye_ix = 0; eye_ix < stereo3dMode.eye_count(); ++eye_ix)
	{
		if (eye_ix > 0 && camera->player)
			SetFixedColormap(camera->player); // reiterate color map for each eye, so night vision goggles work in both eyes
		const s3d::EyePose * eye = stereo3dMode.getEyePose(eye_ix);
		eye->SetUp();
		GLRenderer->SetOutputViewport(bounds);
		Set3DViewport(mainview);
		GLRenderer->mDrawingScene2D = true;
		GLRenderer->mCurrentFoV = fov;
		// Stereo mode specific perspective projection
		SetProjection( eye->GetProjection(fov, ratio, fovratio) );
		// SetProjection(fov, ratio, fovratio);	// switch to perspective mode and set up clipper
		SetViewAngle(r_viewpoint.Angles.Yaw);
		// Stereo mode specific viewpoint adjustment - temporarily shifts global ViewPos
		eye->GetViewShift(GLRenderer->mAngles.Yaw.Degrees, viewShift);
		s3d::ScopedViewShifter viewShifter(viewShift);
		SetViewMatrix(r_viewpoint.Pos.X, r_viewpoint.Pos.Y, r_viewpoint.Pos.Z, false, false);
		gl_RenderState.ApplyMatrices();

		ProcessScene(toscreen);
		if (mainview && toscreen) EndDrawScene(lviewsector); // do not call this for camera textures.
		if (mainview && FGLRenderBuffers::IsEnabled())
		{
			GLRenderer->PostProcessScene(FixedColormap, [&]() { if (gl_bloom && FixedColormap == CM_DEFAULT) DrawEndScene2D(lviewsector); });

			// This should be done after postprocessing, not before.
			GLRenderer->mBuffers->BindCurrentFB();
			glViewport(GLRenderer->mScreenViewport.left, GLRenderer->mScreenViewport.top, GLRenderer->mScreenViewport.width, GLRenderer->mScreenViewport.height);

			if (!toscreen)
			{
				gl_RenderState.mViewMatrix.loadIdentity();
				gl_RenderState.mProjectionMatrix.ortho(GLRenderer->mScreenViewport.left, GLRenderer->mScreenViewport.width, GLRenderer->mScreenViewport.height, GLRenderer->mScreenViewport.top, -1.0f, 1.0f);
				gl_RenderState.ApplyMatrices();
			}

			DrawBlend(lviewsector);
		}
		GLRenderer->mDrawingScene2D = false;
		if (!stereo3dMode.IsMono() && FGLRenderBuffers::IsEnabled())
			GLRenderer->mBuffers->BlitToEyeTexture(eye_ix);
		eye->TearDown();
	}
	stereo3dMode.TearDown();

	interpolator.RestoreInterpolations ();
	return lviewsector;
}

//-----------------------------------------------------------------------------
//
// renders the view
//
//-----------------------------------------------------------------------------

void FGLRenderer::RenderView (player_t* player)
{
	checkBenchActive();

	gl_RenderState.SetVertexBuffer(mVBO);
	mVBO->Reset();

	// reset statistics counters
	ResetProfilingData();

	// Get this before everything else
	if (cl_capfps || r_NoInterpolate) r_viewpoint.TicFrac = 1.;
	else r_viewpoint.TicFrac = I_GetTimeFrac ();

	P_FindParticleSubsectors ();

	if (!gl.legacyMode) mLights->Clear();

	// NoInterpolateView should have no bearing on camera textures, but needs to be preserved for the main view below.
	bool saved_niv = NoInterpolateView;
	NoInterpolateView = false;
	// prepare all camera textures that have been used in the last frame
	FCanvasTextureInfo::UpdateAll();
	NoInterpolateView = saved_niv;


	// now render the main view
	float fovratio;
	float ratio = r_viewwindow.WidescreenRatio;
	if (r_viewwindow.WidescreenRatio >= 1.3f)
	{
		fovratio = 1.333333f;
	}
	else
	{
		fovratio = ratio;
	}
	GLSceneDrawer drawer;

	drawer.SetFixedColormap (player);

	// Check if there's some lights. If not some code can be skipped.
	TThinkerIterator<ADynamicLight> it(STAT_DLIGHT);
	mLightCount = ((it.Next()) != NULL);

	mShadowMap.Update();
	sector_t * viewsector = drawer.RenderViewpoint(player->camera, NULL, r_viewpoint.FieldOfView.Degrees, ratio, fovratio, true, true);

	All.Unclock();
}

//===========================================================================
//
// Render the view to a savegame picture
//
//===========================================================================

void GLSceneDrawer::WriteSavePic (player_t *player, FileWriter *file, int width, int height)
{
	GL_IRECT bounds;

	P_FindParticleSubsectors();	// make sure that all recently spawned particles have a valid subsector.
	bounds.left=0;
	bounds.top=0;
	bounds.width=width;
	bounds.height=height;
	glFlush();
	SetFixedColormap(player);
	gl_RenderState.SetVertexBuffer(GLRenderer->mVBO);
	GLRenderer->mVBO->Reset();
	if (!gl.legacyMode) GLRenderer->mLights->Clear();

	// Check if there's some lights. If not some code can be skipped.
	TThinkerIterator<ADynamicLight> it(STAT_DLIGHT);
	GLRenderer->mLightCount = ((it.Next()) != NULL);

	sector_t *viewsector = RenderViewpoint(players[consoleplayer].camera, &bounds,
								r_viewpoint.FieldOfView.Degrees, 1.6f, 1.6f, true, false);
	glDisable(GL_STENCIL_TEST);
	gl_RenderState.SetFixedColormap(CM_DEFAULT);
	gl_RenderState.SetSoftLightLevel(-1);
	screen->Begin2D(false);
	if (!FGLRenderBuffers::IsEnabled())
	{
		DrawBlend(viewsector);
	}
	GLRenderer->CopyToBackbuffer(&bounds, false);
	glFlush();

	uint8_t * scr = (uint8_t *)M_Malloc(width * height * 3);
	glReadPixels(0,0,width, height,GL_RGB,GL_UNSIGNED_BYTE,scr);
	M_CreatePNG (file, scr + ((height-1) * width * 3), NULL, SS_RGB, width, height, -width * 3, Gamma);
	M_Free(scr);
}


//===========================================================================
//
//
//
//===========================================================================

struct FGLInterface : public FRenderer
{
	void Precache(uint8_t *texhitlist, TMap<PClassActor*, bool> &actorhitlist) override;
	void RenderView(player_t *player) override;
	void WriteSavePic (player_t *player, FileWriter *file, int width, int height) override;
	void StartSerialize(FSerializer &arc) override;
	void EndSerialize(FSerializer &arc) override;
	void RenderTextureView (FCanvasTexture *self, AActor *viewpoint, double fov) override;
	void PreprocessLevel() override;
	void CleanLevelData() override;
	bool RequireGLNodes() override;

	int GetMaxViewPitch(bool down) override;
	void SetClearColor(int color) override;
	void Init() override;
	uint32_t GetCaps() override;
};

//==========================================================================
//
// DFrameBuffer :: Precache
//
//==========================================================================
void gl_PrecacheTexture(uint8_t *texhitlist, TMap<PClassActor*, bool> &actorhitlist);

void FGLInterface::Precache(uint8_t *texhitlist, TMap<PClassActor*, bool> &actorhitlist)
{	
	gl_PrecacheTexture(texhitlist, actorhitlist);
}

//===========================================================================
//
// notify the renderer that serialization of the curent level is about to start/end
//
//===========================================================================

void FGLInterface::StartSerialize(FSerializer &arc)
{
}

void FGLInterface::EndSerialize(FSerializer &arc)
{
	if (arc.isReading())
	{
		// The portal data needs to be recreated after reading a savegame.
		gl_InitPortals();
	}
}

//===========================================================================
//
// Get max. view angle (renderer specific information so it goes here now)
//
//===========================================================================

EXTERN_CVAR(Float, maxviewpitch)

int FGLInterface::GetMaxViewPitch(bool down)
{
	return int(maxviewpitch);
}

//===========================================================================
//
// 
//
//===========================================================================

void FGLInterface::SetClearColor(int color)
{
	PalEntry pe = GPalette.BaseColors[color];
	GLRenderer->mSceneClearColor[0] = pe.r / 255.f;
	GLRenderer->mSceneClearColor[1] = pe.g / 255.f;
	GLRenderer->mSceneClearColor[2] = pe.b / 255.f;
}

//===========================================================================
//
// Render the view to a savegame picture
//
//===========================================================================

void FGLInterface::WriteSavePic (player_t *player, FileWriter *file, int width, int height)
{
	GLSceneDrawer drawer;
	drawer.WriteSavePic(player, file, width, height);
}

//===========================================================================
//
// 
//
//===========================================================================

void FGLInterface::RenderView(player_t *player)
{
	GLRenderer->RenderView(player);
}

//===========================================================================
//
// 
//
//===========================================================================

void FGLInterface::Init()
{
	gl_InitData();
}

//===========================================================================
//
// Camera texture rendering
//
//===========================================================================
CVAR(Bool, gl_usefb, false , CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
extern TexFilter_s TexFilter[];

void FGLInterface::RenderTextureView (FCanvasTexture *tex, AActor *Viewpoint, double FOV)
{
	FMaterial * gltex = FMaterial::ValidateTexture(tex, false);

	int width = gltex->TextureWidth();
	int height = gltex->TextureHeight();

	if (gl.legacyMode)
	{
		// In legacy mode, fail if the requested texture is too large.
		if (gltex->GetWidth() > screen->GetWidth() || gltex->GetHeight() > screen->GetHeight()) return;
		glFlush();
	}
	else
	{
		GLRenderer->StartOffscreen();
		gltex->BindToFrameBuffer();
	}

	GL_IRECT bounds;
	bounds.left=bounds.top=0;
	bounds.width=FHardwareTexture::GetTexDimension(gltex->GetWidth());
	bounds.height=FHardwareTexture::GetTexDimension(gltex->GetHeight());

	GLSceneDrawer drawer;
	drawer.FixedColormap = CM_DEFAULT;
	gl_RenderState.SetFixedColormap(CM_DEFAULT);
	drawer.RenderViewpoint(Viewpoint, &bounds, FOV, (float)width/height, (float)width/height, false, false);

	if (gl.legacyMode)
	{
		glFlush();
		gl_RenderState.SetMaterial(gltex, 0, 0, -1, false);
		glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, bounds.width, bounds.height);
	}
	else
	{
		GLRenderer->EndOffscreen();
	}

	tex->SetUpdated();
	camtexcount++;
}

//===========================================================================
//
// 
//
//===========================================================================

void FGLInterface::PreprocessLevel() 
{
	gl_PreprocessLevel();
}

void FGLInterface::CleanLevelData() 
{
	gl_CleanLevelData();
}

bool FGLInterface::RequireGLNodes() 
{ 
	return true; 
}

uint32_t FGLInterface::GetCaps()
{
	// describe our basic feature set
	ActorRenderFeatureFlags FlagSet = RFF_FLATSPRITES | RFF_MODELS | RFF_SLOPE3DFLOORS |
		RFF_TILTPITCH | RFF_ROLLSPRITES | RFF_POLYGONAL;
	if (r_drawvoxels)
		FlagSet |= RFF_VOXELS;
	if (gl_legacy_mode)
	{
		// legacy mode always has truecolor because palette tonemap is not available
		FlagSet |= RFF_TRUECOLOR;
	}
	else if (!(FGLRenderBuffers::IsEnabled()))
	{
		// truecolor is always available when renderbuffers are unavailable because palette tonemap is not possible
		FlagSet |= RFF_TRUECOLOR | RFF_MATSHADER | RFF_BRIGHTMAP;
	}
	else
	{
		if (gl_tonemap != 5) // not running palette tonemap shader
			FlagSet |= RFF_TRUECOLOR;
		FlagSet |= RFF_MATSHADER | RFF_POSTSHADER | RFF_BRIGHTMAP;
	}
	return (uint32_t)FlagSet;
}
//===========================================================================
//
// 
//
//===========================================================================

FRenderer *gl_CreateInterface()
{
	return new FGLInterface;
}


