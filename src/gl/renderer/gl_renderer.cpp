// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2005-2016 Christoph Oelckers
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
** gl1_renderer.cpp
** Renderer interface
**
*/

#include "gl_load/gl_system.h"
#include "files.h"
#include "v_video.h"
#include "m_png.h"
#include "w_wad.h"
#include "doomstat.h"
#include "i_time.h"
#include "p_effect.h"
#include "d_player.h"
#include "a_dynlight.h"
#include "swrenderer/r_swscene.h"
#include "hwrenderer/utility/hw_clock.h"

#include "gl_load/gl_interface.h"
#include "gl/system/gl_framebuffer.h"
#include "hwrenderer/utility/hw_cvars.h"
#include "gl/system/gl_debug.h"
#include "gl/renderer/gl_renderer.h"
#include "gl/renderer/gl_lightdata.h"
#include "gl/renderer/gl_renderstate.h"
#include "gl/renderer/gl_renderbuffers.h"
#include "gl/data/gl_vertexbuffer.h"
#include "gl/scene/gl_drawinfo.h"
#include "gl/scene/gl_scenedrawer.h"
#include "gl/shaders/gl_ambientshader.h"
#include "gl/shaders/gl_bloomshader.h"
#include "gl/shaders/gl_blurshader.h"
#include "gl/shaders/gl_tonemapshader.h"
#include "gl/shaders/gl_colormapshader.h"
#include "gl/shaders/gl_lensshader.h"
#include "gl/shaders/gl_fxaashader.h"
#include "gl/shaders/gl_presentshader.h"
#include "gl/shaders/gl_present3dRowshader.h"
#include "gl/shaders/gl_shadowmapshader.h"
#include "gl/shaders/gl_postprocessshaderinstance.h"
#include "gl/stereo3d/gl_stereo3d.h"
#include "gl/textures/gl_samplers.h"
#include "gl/dynlights/gl_lightbuffer.h"
#include "r_videoscale.h"

EXTERN_CVAR(Int, screenblocks)
EXTERN_CVAR(Bool, cl_capfps)

extern bool NoInterpolateView;

//===========================================================================
// 
// Renderer interface
//
//===========================================================================

//-----------------------------------------------------------------------------
//
// Initialize
//
//-----------------------------------------------------------------------------

FGLRenderer::FGLRenderer(OpenGLFrameBuffer *fb) 
{
	framebuffer = fb;
	mCurrentPortal = nullptr;
	mMirrorCount = 0;
	mPlaneMirrorCount = 0;
	mAngles = FRotator(0.f, 0.f, 0.f);
	mViewVector = FVector2(0,0);
	mVBO = nullptr;
	mSkyVBO = nullptr;
	mShaderManager = nullptr;
	mLights = nullptr;
	mTonemapPalette = nullptr;
	mBuffers = nullptr;
	mPresentShader = nullptr;
	mPresent3dCheckerShader = nullptr;
	mPresent3dColumnShader = nullptr;
	mPresent3dRowShader = nullptr;
	mBloomExtractShader = nullptr;
	mBloomCombineShader = nullptr;
	mExposureExtractShader = nullptr;
	mExposureAverageShader = nullptr;
	mExposureCombineShader = nullptr;
	mBlurShader = nullptr;
	mTonemapShader = nullptr;
	mTonemapPalette = nullptr;
	mColormapShader = nullptr;
	mLensShader = nullptr;
	mLinearDepthShader = nullptr;
	mDepthBlurShader = nullptr;
	mSSAOShader = nullptr;
	mSSAOCombineShader = nullptr;
	mFXAAShader = nullptr;
	mFXAALumaShader = nullptr;
	mShadowMapShader = nullptr;
	mCustomPostProcessShaders = nullptr;
}

void FGLRenderer::Initialize(int width, int height)
{
	mBuffers = new FGLRenderBuffers();
	mLinearDepthShader = new FLinearDepthShader();
	mDepthBlurShader = new FDepthBlurShader();
	mSSAOShader = new FSSAOShader();
	mSSAOCombineShader = new FSSAOCombineShader();
	mBloomExtractShader = new FBloomExtractShader();
	mBloomCombineShader = new FBloomCombineShader();
	mExposureExtractShader = new FExposureExtractShader();
	mExposureAverageShader = new FExposureAverageShader();
	mExposureCombineShader = new FExposureCombineShader();
	mBlurShader = new FBlurShader();
	mTonemapShader = new FTonemapShader();
	mColormapShader = new FColormapShader();
	mTonemapPalette = nullptr;
	mLensShader = new FLensShader();
	mFXAAShader = new FFXAAShader;
	mFXAALumaShader = new FFXAALumaShader;
	mPresentShader = new FPresentShader();
	mPresent3dCheckerShader = new FPresent3DCheckerShader();
	mPresent3dColumnShader = new FPresent3DColumnShader();
	mPresent3dRowShader = new FPresent3DRowShader();
	mShadowMapShader = new FShadowMapShader();
	mCustomPostProcessShaders = new FCustomPostProcessShaders();

	if (gl.legacyMode)
	{
		legacyShaders = new LegacyShaderContainer;
	}

	// needed for the core profile, because someone decided it was a good idea to remove the default VAO.
	if (!gl.legacyMode)
	{
		glGenVertexArrays(1, &mVAOID);
		glBindVertexArray(mVAOID);
		FGLDebug::LabelObject(GL_VERTEX_ARRAY, mVAOID, "FGLRenderer.mVAOID");
	}
	else mVAOID = 0;

	mVBO = new FFlatVertexBuffer(width, height);
	mSkyVBO = new FSkyVertexBuffer;
	if (!gl.legacyMode) mLights = new FLightBuffer();
	else mLights = NULL;
	gl_RenderState.SetVertexBuffer(mVBO);
	mFBID = 0;
	mOldFBID = 0;

	SetupLevel();
	mShaderManager = new FShaderManager;
	mSamplerManager = new FSamplerManager;

	GLPortal::Initialize();
}

FGLRenderer::~FGLRenderer() 
{
	GLPortal::Shutdown();

	FlushModels();
	AActor::DeleteAllAttachedLights();
	FMaterial::FlushAll();
	if (legacyShaders) delete legacyShaders;
	if (mShaderManager != NULL) delete mShaderManager;
	if (mSamplerManager != NULL) delete mSamplerManager;
	if (mVBO != NULL) delete mVBO;
	if (mSkyVBO != NULL) delete mSkyVBO;
	if (mLights != NULL) delete mLights;
	if (mFBID != 0) glDeleteFramebuffers(1, &mFBID);
	if (mVAOID != 0)
	{
		glBindVertexArray(0);
		glDeleteVertexArrays(1, &mVAOID);
	}
	if (swdrawer) delete swdrawer;
	if (mBuffers) delete mBuffers;
	if (mPresentShader) delete mPresentShader;
	if (mLinearDepthShader) delete mLinearDepthShader;
	if (mDepthBlurShader) delete mDepthBlurShader;
	if (mSSAOShader) delete mSSAOShader;
	if (mSSAOCombineShader) delete mSSAOCombineShader;
	if (mPresent3dCheckerShader) delete mPresent3dCheckerShader;
	if (mPresent3dColumnShader) delete mPresent3dColumnShader;
	if (mPresent3dRowShader) delete mPresent3dRowShader;
	if (mBloomExtractShader) delete mBloomExtractShader;
	if (mBloomCombineShader) delete mBloomCombineShader;
	if (mExposureExtractShader) delete mExposureExtractShader;
	if (mExposureAverageShader) delete mExposureAverageShader;
	if (mExposureCombineShader) delete mExposureCombineShader;
	if (mBlurShader) delete mBlurShader;
	if (mTonemapShader) delete mTonemapShader;
	if (mTonemapPalette) delete mTonemapPalette;
	if (mColormapShader) delete mColormapShader;
	if (mLensShader) delete mLensShader;
	if (mShadowMapShader) delete mShadowMapShader;
	delete mCustomPostProcessShaders;
	delete mFXAAShader;
	delete mFXAALumaShader;
}

//===========================================================================
// 
//
//
//===========================================================================

void FGLRenderer::ResetSWScene()
{
	// force recreation of the SW scene drawer to ensure it gets a new set of resources.
	if (swdrawer != nullptr) delete swdrawer;
	swdrawer = nullptr;
}

void FGLRenderer::SetupLevel()
{
	mVBO->CreateVBO();
}

//===========================================================================
// 
//
//
//===========================================================================

void FGLRenderer::FlushTextures()
{
	FMaterial::FlushAll();
}

//===========================================================================
// 
//
//
//===========================================================================

bool FGLRenderer::StartOffscreen()
{
	bool firstBind = (mFBID == 0);
	if (mFBID == 0)
		glGenFramebuffers(1, &mFBID);
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &mOldFBID);
	glBindFramebuffer(GL_FRAMEBUFFER, mFBID);
	if (firstBind)
		FGLDebug::LabelObject(GL_FRAMEBUFFER, mFBID, "OffscreenFB");
	return true;
}

//===========================================================================
// 
//
//
//===========================================================================

void FGLRenderer::EndOffscreen()
{
	glBindFramebuffer(GL_FRAMEBUFFER, mOldFBID); 
}

//-----------------------------------------------------------------------------
//
// renders the view
//
//-----------------------------------------------------------------------------

sector_t *FGLRenderer::RenderView(player_t* player)
{
	gl_RenderState.SetVertexBuffer(mVBO);
	mVBO->Reset();
	sector_t *retsec;

	if (!V_IsHardwareRenderer())
	{
		if (swdrawer == nullptr) swdrawer = new SWSceneDrawer;
		retsec = swdrawer->RenderView(player);
	}
	else
	{
		checkBenchActive();

		// reset statistics counters
		ResetProfilingData();

		// Get this before everything else
		if (cl_capfps || r_NoInterpolate) r_viewpoint.TicFrac = 1.;
		else r_viewpoint.TicFrac = I_GetTimeFrac();

		P_FindParticleSubsectors();

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

		drawer.SetFixedColormap(player);

		mShadowMap.Update();
		retsec = drawer.RenderViewpoint(player->camera, NULL, r_viewpoint.FieldOfView.Degrees, ratio, fovratio, true, true);
	}
	All.Unclock();
	return retsec;
}

//===========================================================================
//
// Camera texture rendering
//
//===========================================================================

void FGLRenderer::RenderTextureView(FCanvasTexture *tex, AActor *Viewpoint, double FOV)
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
		StartOffscreen();
		gltex->BindToFrameBuffer();
	}

	IntRect bounds;
	bounds.left = bounds.top = 0;
	bounds.width = FHardwareTexture::GetTexDimension(gltex->GetWidth());
	bounds.height = FHardwareTexture::GetTexDimension(gltex->GetHeight());

	GLSceneDrawer drawer;
	drawer.FixedColormap = CM_DEFAULT;
	gl_RenderState.SetFixedColormap(CM_DEFAULT);
	drawer.RenderViewpoint(Viewpoint, &bounds, FOV, (float)width / height, (float)width / height, false, false);

	if (gl.legacyMode)
	{
		glFlush();
		gl_RenderState.SetMaterial(gltex, 0, 0, -1, false);
		glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, bounds.width, bounds.height);
	}
	else
	{
		EndOffscreen();
	}

	tex->SetUpdated();
}

void FGLRenderer::WriteSavePic(player_t *player, FileWriter *file, int width, int height)
{
	// Todo: This needs to call the software renderer and process the returned image, if so desired.
	// This also needs to take out parts of the scene drawer so they can be shared between renderers.
	GLSceneDrawer drawer;
	drawer.WriteSavePic(player, file, width, height);
}

void FGLRenderer::BeginFrame()
{
	buffersActive = GLRenderer->mBuffers->Setup(screen->mScreenViewport.width, screen->mScreenViewport.height, screen->mSceneViewport.width, screen->mSceneViewport.height);
}

//===========================================================================
// 
// Vertex buffer for 2D drawer
//
//===========================================================================

class F2DVertexBuffer : public FSimpleVertexBuffer
{
	uint32_t ibo_id;

	// Make sure we can build upon FSimpleVertexBuffer.
	static_assert(offsetof(FSimpleVertex, x) == offsetof(F2DDrawer::TwoDVertex, x), "x not aligned");
	static_assert(offsetof(FSimpleVertex, u) == offsetof(F2DDrawer::TwoDVertex, u), "u not aligned");
	static_assert(offsetof(FSimpleVertex, color) == offsetof(F2DDrawer::TwoDVertex, color0), "color not aligned");

public:

	F2DVertexBuffer()
	{
		glGenBuffers(1, &ibo_id);
	}
	~F2DVertexBuffer()
	{
		if (ibo_id != 0)
		{
			glDeleteBuffers(1, &ibo_id);
		}
	}
	void UploadData(F2DDrawer::TwoDVertex *vertices, int vertcount, int *indices, int indexcount)
	{
		glBindBuffer(GL_ARRAY_BUFFER, vbo_id);
		glBufferData(GL_ARRAY_BUFFER, vertcount * sizeof(vertices[0]), vertices, GL_STREAM_DRAW);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_id);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexcount * sizeof(indices[0]), indices, GL_STREAM_DRAW);
	}

	void BindVBO() override
	{
		FSimpleVertexBuffer::BindVBO();
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_id);
	}
};

//===========================================================================
// 
// Draws the 2D stuff. This is the version for OpenGL 3 and later.
//
//===========================================================================

void LegacyColorOverlay(F2DDrawer *drawer, F2DDrawer::RenderCommand & cmd);
int LegacyDesaturation(F2DDrawer::RenderCommand &cmd);
CVAR(Bool, gl_aalines, false, CVAR_ARCHIVE)

void FGLRenderer::Draw2D(F2DDrawer *drawer)
{
	twoD.Clock();
	if (buffersActive)
	{
		mBuffers->BindCurrentFB();
	}
	const auto &mScreenViewport = screen->mScreenViewport;
	glViewport(mScreenViewport.left, mScreenViewport.top, mScreenViewport.width, mScreenViewport.height);

	gl_RenderState.mViewMatrix.loadIdentity();
	gl_RenderState.mProjectionMatrix.ortho(0, screen->GetWidth(), screen->GetHeight(), 0, -1.0f, 1.0f);
	gl_RenderState.ApplyMatrices();

	glDisable(GL_DEPTH_TEST);

	// Korshun: ENABLE AUTOMAP ANTIALIASING!!!
	if (gl_aalines)
		glEnable(GL_LINE_SMOOTH);
	else
	{
		glDisable(GL_MULTISAMPLE);
		glDisable(GL_LINE_SMOOTH);
		glLineWidth(1.0);
	}


	auto &vertices = drawer->mVertices;
	auto &indices = drawer->mIndices;
	auto &commands = drawer->mData;

	if (commands.Size() == 0)
	{
		twoD.Unclock();
		return;
	}

	for (auto &v : vertices)
	{
		// Change from BGRA to RGBA
		std::swap(v.color0.r, v.color0.b);
	}
	auto vb = new F2DVertexBuffer;
	vb->UploadData(&vertices[0], vertices.Size(), &indices[0], indices.Size());
	gl_RenderState.SetVertexBuffer(vb);
	gl_RenderState.SetFixedColormap(CM_DEFAULT);
	gl_RenderState.EnableFog(false);

	for(auto &cmd : commands)
	{

		int gltrans = -1;
		int tm, sb, db, be;
		// The texture mode being returned here cannot be used, because the higher level code 
		// already manipulated the data so that some cases will not be handled correctly.
		// Since we already get a proper mode from the calling code this doesn't really matter.
		gl_GetRenderStyle(cmd.mRenderStyle, false, false, &tm, &sb, &db, &be);
		gl_RenderState.BlendEquation(be); 
		gl_RenderState.BlendFunc(sb, db);
		gl_RenderState.EnableBrightmap(!(cmd.mRenderStyle.Flags & STYLEF_ColorIsFixed));

		// Rather than adding remapping code, let's enforce that the constants here are equal.
		static_assert(int(F2DDrawer::DTM_Normal) == int(TM_MODULATE), "DTM_Normal != TM_MODULATE");
		static_assert(int(F2DDrawer::DTM_Opaque) == int(TM_OPAQUE), "DTM_Opaque != TM_OPAQUE");
		static_assert(int(F2DDrawer::DTM_Invert) == int(TM_INVERSE), "DTM_Invert != TM_INVERSE");
		static_assert(int(F2DDrawer::DTM_InvertOpaque) == int(TM_INVERTOPAQUE), "DTM_InvertOpaque != TM_INVERTOPAQUE");
		static_assert(int(F2DDrawer::DTM_Stencil) == int(TM_MASK), "DTM_Stencil != TM_MASK");
		static_assert(int(F2DDrawer::DTM_AlphaTexture) == int(TM_REDTOALPHA), "DTM_AlphaTexture != TM_REDTOALPHA");
		gl_RenderState.SetTextureMode(cmd.mDrawMode);
		if (cmd.mFlags & F2DDrawer::DTF_Scissor)
		{
			glEnable(GL_SCISSOR_TEST);
			// scissor test doesn't use the current viewport for the coordinates, so use real screen coordinates
			// Note that the origin here is the lower left corner!
			auto sciX = screen->ScreenToWindowX(cmd.mScissor[0]);
			auto sciY = screen->ScreenToWindowY(cmd.mScissor[3]);
			auto sciW = screen->ScreenToWindowX(cmd.mScissor[2]) - sciX;
			auto sciH = screen->ScreenToWindowY(cmd.mScissor[1]) - sciY;
			glScissor(sciX, sciY, sciW, sciH);
		}
		else glDisable(GL_SCISSOR_TEST);

		if (cmd.mSpecialColormap != nullptr)
		{
			auto index = cmd.mSpecialColormap - &SpecialColormaps[0];
			if (index < 0 || (unsigned)index >= SpecialColormaps.Size()) index = 0;	// if it isn't in the table FBitmap cannot use it. Shouldn't happen anyway.
			if (!gl.legacyMode || cmd.mTexture->UseType == ETextureType::SWCanvas)
			{ 
				gl_RenderState.SetFixedColormap(CM_FIRSTSPECIALCOLORMAPFORCED + int(index));
			}
			else
			{
				// map the special colormap to a translation for the legacy renderer.
				// This only gets used on the software renderer's weapon sprite.
				gltrans = STRange_Specialcolormap + index;
			}
		}
		else
		{
			if (!gl.legacyMode)
			{
				gl_RenderState.Set2DOverlayColor(cmd.mColor1);
				gl_RenderState.SetFixedColormap(CM_PLAIN2D);
			}
			else if (cmd.mDesaturate > 0)
			{
				gltrans = LegacyDesaturation(cmd);
			}
		}

		gl_RenderState.SetColor(1, 1, 1, 1, cmd.mDesaturate); 

		gl_RenderState.AlphaFunc(GL_GEQUAL, 0.f);

		if (cmd.mTexture != nullptr)
		{
			auto mat = FMaterial::ValidateTexture(cmd.mTexture, false);
			if (mat == nullptr) continue;

			// This requires very special handling
			if (gl.legacyMode && cmd.mTexture->UseType == ETextureType::SWCanvas)
			{
				gl_RenderState.SetTextureMode(TM_SWCANVAS);
			}

			if (gltrans == -1 && cmd.mTranslation != nullptr) gltrans = cmd.mTranslation->GetUniqueIndex();
			gl_RenderState.SetMaterial(mat, cmd.mFlags & F2DDrawer::DTF_Wrap ? CLAMP_NONE : CLAMP_XY_NOMIP, -gltrans, -1, cmd.mDrawMode == F2DDrawer::DTM_AlphaTexture);
			gl_RenderState.EnableTexture(true);

			// Canvas textures are stored upside down
			if (cmd.mTexture->bHasCanvas)
			{
				gl_RenderState.mTextureMatrix.loadIdentity();
				gl_RenderState.mTextureMatrix.scale(1.f, -1.f, 1.f);
				gl_RenderState.mTextureMatrix.translate(0.f, 1.f, 0.0f);
				gl_RenderState.EnableTextureMatrix(true);
			}
		}
		else
		{
			gl_RenderState.EnableTexture(false);
		}
		gl_RenderState.Apply();

		switch (cmd.mType)
		{
		case F2DDrawer::DrawTypeTriangles:
			glDrawElements(GL_TRIANGLES, cmd.mIndexCount, GL_UNSIGNED_INT, (const void *)(cmd.mIndexIndex * sizeof(unsigned int)));
			if (gl.legacyMode && cmd.mColor1 != 0)
			{
				// Draw the overlay as a separate operation.
				LegacyColorOverlay(drawer, cmd);
			}
			break;

		case F2DDrawer::DrawTypeLines:
			glDrawArrays(GL_LINES, cmd.mVertIndex, cmd.mVertCount);
			break;

		case F2DDrawer::DrawTypePoints:
			glDrawArrays(GL_POINTS, cmd.mVertIndex, cmd.mVertCount);
			break;

		}
		gl_RenderState.SetEffect(EFF_NONE);
		gl_RenderState.EnableTextureMatrix(false);
	}
	glDisable(GL_SCISSOR_TEST);

	gl_RenderState.SetVertexBuffer(GLRenderer->mVBO);
	gl_RenderState.EnableTexture(true);
	gl_RenderState.EnableBrightmap(true);
	gl_RenderState.SetTextureMode(TM_MODULATE);
	gl_RenderState.SetFixedColormap(CM_DEFAULT);
	gl_RenderState.ResetColor();
	gl_RenderState.Apply();
	delete vb;
	twoD.Unclock();
}
