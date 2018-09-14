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
#include "g_game.h"
#include "swrenderer/r_swscene.h"
#include "hwrenderer/utility/hw_clock.h"

#include "gl_load/gl_interface.h"
#include "gl/system/gl_framebuffer.h"
#include "hwrenderer/utility/hw_cvars.h"
#include "gl/scene/gl_portal.h"
#include "gl/system/gl_debug.h"
#include "gl/renderer/gl_renderer.h"
#include "gl/renderer/gl_lightdata.h"
#include "gl/renderer/gl_renderstate.h"
#include "gl/renderer/gl_renderbuffers.h"
#include "gl/data/gl_vertexbuffer.h"
#include "gl/scene/gl_drawinfo.h"
#include "hwrenderer/utility/hw_vrmodes.h"
#include "hwrenderer/postprocessing/hw_presentshader.h"
#include "hwrenderer/postprocessing/hw_present3dRowshader.h"
#include "hwrenderer/postprocessing/hw_shadowmapshader.h"
#include "gl/shaders/gl_postprocessshaderinstance.h"
#include "gl/textures/gl_samplers.h"
#include "gl/dynlights/gl_lightbuffer.h"
#include "gl/data/gl_viewpointbuffer.h"
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
}

void FGLRenderer::Initialize(int width, int height)
{
	mScreenBuffers = new FGLRenderBuffers();
	mSaveBuffers = new FGLRenderBuffers();
	mBuffers = mScreenBuffers;
	mPresentShader = new FPresentShader();
	mPresent3dCheckerShader = new FPresent3DCheckerShader();
	mPresent3dColumnShader = new FPresent3DColumnShader();
	mPresent3dRowShader = new FPresent3DRowShader();
	mShadowMapShader = new FShadowMapShader();
	mCustomPostProcessShaders = new FCustomPostProcessShaders();

	// needed for the core profile, because someone decided it was a good idea to remove the default VAO.
	glGenQueries(1, &PortalQueryObject);

	glGenVertexArrays(1, &mVAOID);
	glBindVertexArray(mVAOID);
	FGLDebug::LabelObject(GL_VERTEX_ARRAY, mVAOID, "FGLRenderer.mVAOID");

	mVBO = new FFlatVertexBuffer(width, height);
	mSkyVBO = new FSkyVertexBuffer;
	mLights = new FLightBuffer();
	mViewpoints = new GLViewpointBuffer;
	gl_RenderState.SetVertexBuffer(mVBO);
	mFBID = 0;
	mOldFBID = 0;

	SetupLevel();
	mShaderManager = new FShaderManager;
	mSamplerManager = new FSamplerManager;
}

FGLRenderer::~FGLRenderer() 
{
	FlushModels();
	AActor::DeleteAllAttachedLights();
	FMaterial::FlushAll();
	if (mShaderManager != nullptr) delete mShaderManager;
	if (mSamplerManager != nullptr) delete mSamplerManager;
	if (mVBO != nullptr) delete mVBO;
	if (mSkyVBO != nullptr) delete mSkyVBO;
	if (mLights != nullptr) delete mLights;
	if (mViewpoints != nullptr) delete mViewpoints;
	if (mFBID != 0) glDeleteFramebuffers(1, &mFBID);
	if (mVAOID != 0)
	{
		glBindVertexArray(0);
		glDeleteVertexArrays(1, &mVAOID);
	}
	if (PortalQueryObject != 0) glDeleteQueries(1, &PortalQueryObject);

	if (swdrawer) delete swdrawer;
	if (mBuffers) delete mBuffers;
	if (mSaveBuffers) delete mSaveBuffers;
	if (mPresentShader) delete mPresentShader;
	if (mPresent3dCheckerShader) delete mPresent3dCheckerShader;
	if (mPresent3dColumnShader) delete mPresent3dColumnShader;
	if (mPresent3dRowShader) delete mPresent3dRowShader;
	if (mShadowMapShader) delete mShadowMapShader;
	delete mCustomPostProcessShaders;
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

		mLights->Clear();
		mViewpoints->Clear();

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

		mShadowMap.Update();
		retsec = RenderViewpoint(r_viewpoint, player->camera, NULL, r_viewpoint.FieldOfView.Degrees, ratio, fovratio, true, true);
	}
	All.Unclock();
	return retsec;
}

//===========================================================================
//
//
//
//===========================================================================

void FGLRenderer::BindToFrameBuffer(FMaterial *mat)
{
	auto BaseLayer = static_cast<FHardwareTexture*>(mat->GetLayer(0));

	if (BaseLayer == nullptr)
	{
		// must create the hardware texture first
		BaseLayer->BindOrCreate(mat->sourcetex, 0, 0, 0, 0);
		FHardwareTexture::Unbind(0);
		gl_RenderState.ClearLastMaterial();
	}
	BaseLayer->BindToFrameBuffer(mat->GetWidth(), mat->GetHeight());
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

	StartOffscreen();
	BindToFrameBuffer(gltex);

	IntRect bounds;
	bounds.left = bounds.top = 0;
	bounds.width = FHardwareTexture::GetTexDimension(gltex->GetWidth());
	bounds.height = FHardwareTexture::GetTexDimension(gltex->GetHeight());

	FRenderViewpoint texvp;
	RenderViewpoint(texvp, Viewpoint, &bounds, FOV, (float)width / height, (float)width / height, false, false);

	EndOffscreen();

	tex->SetUpdated();
}

//===========================================================================
//
// Render the view to a savegame picture
//
//===========================================================================

void FGLRenderer::WriteSavePic (player_t *player, FileWriter *file, int width, int height)
{
    IntRect bounds;
    bounds.left = 0;
    bounds.top = 0;
    bounds.width = width;
    bounds.height = height;
    
    // if mVBO is persistently mapped we must be sure the GPU finished reading from it before we fill it with new data.
    glFinish();
    
    // Switch to render buffers dimensioned for the savepic
    mBuffers = mSaveBuffers;
    
    P_FindParticleSubsectors();    // make sure that all recently spawned particles have a valid subsector.
    gl_RenderState.SetVertexBuffer(mVBO);
    mVBO->Reset();
    mLights->Clear();
	mViewpoints->Clear();

    // This shouldn't overwrite the global viewpoint even for a short time.
    FRenderViewpoint savevp;
    sector_t *viewsector = RenderViewpoint(savevp, players[consoleplayer].camera, &bounds, r_viewpoint.FieldOfView.Degrees, 1.6f, 1.6f, true, false);
    glDisable(GL_STENCIL_TEST);
    gl_RenderState.SetSoftLightLevel(-1);
    CopyToBackbuffer(&bounds, false);
    
    // strictly speaking not needed as the glReadPixels should block until the scene is rendered, but this is to safeguard against shitty drivers
    glFinish();
    
    uint8_t * scr = (uint8_t *)M_Malloc(width * height * 3);
    glReadPixels(0,0,width, height,GL_RGB,GL_UNSIGNED_BYTE,scr);
    M_CreatePNG (file, scr + ((height-1) * width * 3), NULL, SS_RGB, width, height, -width * 3, Gamma);
    M_Free(scr);
    
    // Switch back the screen render buffers
    screen->SetViewportRects(nullptr);
    mBuffers = mScreenBuffers;
}

//===========================================================================
//
//
//
//===========================================================================

void FGLRenderer::BeginFrame()
{
	mScreenBuffers->Setup(screen->mScreenViewport.width, screen->mScreenViewport.height, screen->mSceneViewport.width, screen->mSceneViewport.height);
	mSaveBuffers->Setup(SAVEPICWIDTH, SAVEPICHEIGHT, SAVEPICWIDTH, SAVEPICHEIGHT);
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

CVAR(Bool, gl_aalines, false, CVAR_ARCHIVE)

void FGLRenderer::Draw2D(F2DDrawer *drawer)
{
	twoD.Clock();
	FGLDebug::PushGroup("Draw2D");
	if (VRMode::GetVRMode(true)->mEyeCount == 1)
		mBuffers->BindCurrentFB();
	const auto &mScreenViewport = screen->mScreenViewport;
	glViewport(mScreenViewport.left, mScreenViewport.top, mScreenViewport.width, mScreenViewport.height);
	GLRenderer->mViewpoints->Set2D(screen->GetWidth(), screen->GetHeight());

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
		gl_RenderState.EnableFog(2);	// Special 2D mode 'fog'.

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

		if (cmd.mSpecialColormap[0].a != 0)
		{
			gl_RenderState.SetTextureMode(TM_FIXEDCOLORMAP);
			gl_RenderState.SetObjectColor(cmd.mSpecialColormap[0]);
			gl_RenderState.SetObjectColor2(cmd.mSpecialColormap[1]);
		}
		gl_RenderState.SetFog(cmd.mColor1, 0);
		gl_RenderState.SetColor(1, 1, 1, 1, cmd.mDesaturate); 

		gl_RenderState.AlphaFunc(GL_GEQUAL, 0.f);

		if (cmd.mTexture != nullptr)
		{
			auto mat = FMaterial::ValidateTexture(cmd.mTexture, false);
			if (mat == nullptr) continue;

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
			if (cmd.mFlags & F2DDrawer::DTF_Burn)
			{
				gl_RenderState.SetEffect(EFF_BURN);
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
			break;

		case F2DDrawer::DrawTypeLines:
			glDrawArrays(GL_LINES, cmd.mVertIndex, cmd.mVertCount);
			break;

		case F2DDrawer::DrawTypePoints:
			glDrawArrays(GL_POINTS, cmd.mVertIndex, cmd.mVertCount);
			break;

		}
		gl_RenderState.SetObjectColor(0xffffffff);
		gl_RenderState.SetObjectColor2(0);
		gl_RenderState.EnableTextureMatrix(false);
		gl_RenderState.SetEffect(EFF_NONE);

	}
	glDisable(GL_SCISSOR_TEST);

	gl_RenderState.BlendEquation(GL_FUNC_ADD);
	gl_RenderState.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	gl_RenderState.SetVertexBuffer(mVBO);
	gl_RenderState.EnableTexture(true);
	gl_RenderState.EnableBrightmap(true);
	gl_RenderState.SetTextureMode(TM_MODULATE);
	gl_RenderState.EnableFog(false);
	gl_RenderState.ResetColor();
	gl_RenderState.Apply();
	delete vb;
	FGLDebug::PopGroup();
	twoD.Unclock();
}
