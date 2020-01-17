/*
**  Softpoly backend
**  Copyright (c) 2016-2020 Magnus Norddahl
**
**  This software is provided 'as-is', without any express or implied
**  warranty.  In no event will the authors be held liable for any damages
**  arising from the use of this software.
**
**  Permission is granted to anyone to use this software for any purpose,
**  including commercial applications, and to alter it and redistribute it
**  freely, subject to the following restrictions:
**
**  1. The origin of this software must not be misrepresented; you must not
**     claim that you wrote the original software. If you use this software
**     in a product, an acknowledgment in the product documentation would be
**     appreciated but is not required.
**  2. Altered source versions must be plainly marked as such, and must not be
**     misrepresented as being the original software.
**  3. This notice may not be removed or altered from any source distribution.
**
*/

#include "v_video.h"
#include "m_png.h"
#include "templates.h"
#include "r_videoscale.h"
#include "actor.h"
#include "i_time.h"
#include "g_game.h"
#include "gamedata/fonts/v_text.h"

#include "rendering/i_video.h"

#include "hwrenderer/utility/hw_clock.h"
#include "hwrenderer/utility/hw_vrmodes.h"
#include "hwrenderer/utility/hw_cvars.h"
#include "hwrenderer/models/hw_models.h"
#include "hwrenderer/scene/hw_skydome.h"
#include "hwrenderer/scene/hw_fakeflat.h"
#include "hwrenderer/scene/hw_drawinfo.h"
#include "hwrenderer/scene/hw_portal.h"
#include "hwrenderer/data/hw_viewpointbuffer.h"
#include "hwrenderer/data/flatvertices.h"
#include "hwrenderer/data/shaderuniforms.h"
#include "hwrenderer/dynlights/hw_lightbuffer.h"
#include "hwrenderer/postprocessing/hw_postprocess.h"

#include "swrenderer/r_swscene.h"

#include "poly_framebuffer.h"
#include "poly_buffers.h"
#include "poly_renderstate.h"
#include "poly_hwtexture.h"
#include "doomerrors.h"

void Draw2D(F2DDrawer *drawer, FRenderState &state);
void DoWriteSavePic(FileWriter *file, ESSType ssformat, uint8_t *scr, int width, int height, sector_t *viewsector, bool upsidedown);

EXTERN_CVAR(Bool, r_drawvoxels)
EXTERN_CVAR(Int, gl_tonemap)
EXTERN_CVAR(Int, screenblocks)
EXTERN_CVAR(Bool, cl_capfps)
EXTERN_CVAR(Bool, gl_no_skyclear)

extern bool NoInterpolateView;
extern int rendered_commandbuffers;
extern int current_rendered_commandbuffers;

extern bool gpuStatActive;
extern bool keepGpuStatActive;
extern FString gpuStatOutput;

PolyFrameBuffer::PolyFrameBuffer(void *hMonitor, bool fullscreen) : Super(hMonitor, fullscreen) 
{
	I_PolyPresentInit();
}

PolyFrameBuffer::~PolyFrameBuffer()
{
	// screen is already null at this point, but PolyHardwareTexture::ResetAll needs it during clean up. Is there a better way we can do this?
	auto tmp = screen;
	screen = this;

	PolyHardwareTexture::ResetAll();
	PolyBuffer::ResetAll();
	PPResource::ResetAll();

	delete mScreenQuad.VertexBuffer;
	delete mScreenQuad.IndexBuffer;

	delete mVertexData;
	delete mSkyData;
	delete mViewpoints;
	delete mLights;
	mShadowMap.Reset();

	screen = tmp;

	I_PolyPresentDeinit();
}

void PolyFrameBuffer::InitializeState()
{
	vendorstring = "Poly";
	hwcaps = RFL_SHADER_STORAGE_BUFFER | RFL_BUFFER_STORAGE;
	glslversion = 4.50f;
	uniformblockalignment = 1;
	maxuniformblock = 0x7fffffff;

	mRenderState.reset(new PolyRenderState());

	mVertexData = new FFlatVertexBuffer(GetWidth(), GetHeight());
	mSkyData = new FSkyVertexBuffer;
	mViewpoints = new HWViewpointBuffer;
	mLights = new FLightBuffer();

	static const FVertexBufferAttribute format[] =
	{
		{ 0, VATTR_VERTEX, VFmt_Float3, (int)myoffsetof(ScreenQuadVertex, x) },
		{ 0, VATTR_TEXCOORD, VFmt_Float2, (int)myoffsetof(ScreenQuadVertex, u) },
		{ 0, VATTR_COLOR, VFmt_Byte4, (int)myoffsetof(ScreenQuadVertex, color0) }
	};

	uint32_t indices[6] = { 0, 1, 2, 1, 3, 2 };

	mScreenQuad.VertexBuffer = screen->CreateVertexBuffer();
	mScreenQuad.VertexBuffer->SetFormat(1, 3, sizeof(ScreenQuadVertex), format);

	mScreenQuad.IndexBuffer = screen->CreateIndexBuffer();
	mScreenQuad.IndexBuffer->SetData(6 * sizeof(uint32_t), indices, false);

	CheckCanvas();
}

void PolyFrameBuffer::CheckCanvas()
{
	if (!mCanvas || mCanvas->GetWidth() != GetWidth() || mCanvas->GetHeight() != GetHeight())
	{
		FlushDrawCommands();
		DrawerThreads::WaitForWorkers();

		mCanvas.reset(new DCanvas(0, 0, true));
		mCanvas->Resize(GetWidth(), GetHeight(), false);
		mDepthStencil.reset();
		mDepthStencil.reset(new PolyDepthStencil(GetWidth(), GetHeight()));

		mRenderState->SetRenderTarget(GetCanvas(), GetDepthStencil(), true);
	}
}

PolyCommandBuffer *PolyFrameBuffer::GetDrawCommands()
{
	if (!mDrawCommands)
	{
		mDrawCommands.reset(new PolyCommandBuffer(&mFrameMemory));
		mDrawCommands->SetLightBuffer(mLightBuffer->Memory());
	}
	return mDrawCommands.get();
}

void PolyFrameBuffer::FlushDrawCommands()
{
	mRenderState->EndRenderPass();
	if (mDrawCommands)
	{
		mDrawCommands->Submit();
		mDrawCommands.reset();
	}
}

void PolyFrameBuffer::Update()
{
	twoD.Reset();
	Flush3D.Reset();

	Flush3D.Clock();

	Draw2D();
	Clear2D();

	Flush3D.Unclock();

	FlushDrawCommands();

	if (mCanvas)
	{
		int w = mCanvas->GetWidth();
		int h = mCanvas->GetHeight();
		int pixelsize = 4;
		const uint8_t *src = (const uint8_t*)mCanvas->GetPixels();
		int pitch = 0;
		uint8_t *dst = I_PolyPresentLock(w, h, cur_vsync, pitch);
		if (dst)
		{
#if 1
			auto copyqueue = std::make_shared<DrawerCommandQueue>(&mFrameMemory);
			copyqueue->Push<MemcpyCommand>(dst, pitch / pixelsize, src, w, h, w, pixelsize);
			DrawerThreads::Execute(copyqueue);
#else
			for (int y = 0; y < h; y++)
			{
				memcpy(dst + y * pitch, src + y * w * pixelsize, w * pixelsize);
			}
#endif

			DrawerThreads::WaitForWorkers();
			I_PolyPresentUnlock(mOutputLetterbox.left, mOutputLetterbox.top, mOutputLetterbox.width, mOutputLetterbox.height);
		}
		FPSLimit();
	}

	DrawerThreads::WaitForWorkers();
	mFrameMemory.Clear();
	FrameDeleteList.Buffers.clear();
	FrameDeleteList.Images.clear();

	CheckCanvas();

	Super::Update();
}


void PolyFrameBuffer::WriteSavePic(player_t *player, FileWriter *file, int width, int height)
{
	if (!V_IsHardwareRenderer())
	{
		Super::WriteSavePic(player, file, width, height);
	}
	else
	{
	}
}

sector_t *PolyFrameBuffer::RenderView(player_t *player)
{
	// To do: this is virtually identical to FGLRenderer::RenderView and should be merged.

	mRenderState->SetVertexBuffer(mVertexData);
	mVertexData->Reset();

	sector_t *retsec;
	if (!V_IsHardwareRenderer())
	{
		if (!swdrawer) swdrawer.reset(new SWSceneDrawer);
		retsec = swdrawer->RenderView(player);
	}
	else
	{
		hw_ClearFakeFlat();

		iter_dlightf = iter_dlight = draw_dlight = draw_dlightf = 0;

		checkBenchActive();

		// reset statistics counters
		ResetProfilingData();

		// Get this before everything else
		if (cl_capfps || r_NoInterpolate) r_viewpoint.TicFrac = 1.;
		else r_viewpoint.TicFrac = I_GetTimeFrac();

		mLights->Clear();
		mViewpoints->Clear();

		// NoInterpolateView should have no bearing on camera textures, but needs to be preserved for the main view below.
		bool saved_niv = NoInterpolateView;
		NoInterpolateView = false;

		// Shader start time does not need to be handled per level. Just use the one from the camera to render from.
		GetRenderState()->CheckTimer(player->camera->Level->ShaderStartTime);
		// prepare all camera textures that have been used in the last frame.
		// This must be done for all levels, not just the primary one!
		for (auto Level : AllLevels())
		{
			Level->canvasTextureInfo.UpdateAll([&](AActor *camera, FCanvasTexture *camtex, double fov)
			{
				RenderTextureView(camtex, camera, fov);
			});
		}
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

		retsec = RenderViewpoint(r_viewpoint, player->camera, NULL, r_viewpoint.FieldOfView.Degrees, ratio, fovratio, true, true);
	}
	All.Unclock();
	return retsec;
}

sector_t *PolyFrameBuffer::RenderViewpoint(FRenderViewpoint &mainvp, AActor * camera, IntRect * bounds, float fov, float ratio, float fovratio, bool mainview, bool toscreen)
{
	// To do: this is virtually identical to FGLRenderer::RenderViewpoint and should be merged.

	R_SetupFrame(mainvp, r_viewwindow, camera);

	if (mainview && toscreen)
		UpdateShadowMap();

	// Update the attenuation flag of all light defaults for each viewpoint.
	// This function will only do something if the setting differs.
	FLightDefaults::SetAttenuationForLevel(!!(camera->Level->flags3 & LEVEL3_ATTENUATE));

	// Render (potentially) multiple views for stereo 3d
	// Fixme. The view offsetting should be done with a static table and not require setup of the entire render state for the mode.
	auto vrmode = VRMode::GetVRMode(mainview && toscreen);
	for (int eye_ix = 0; eye_ix < vrmode->mEyeCount; ++eye_ix)
	{
		const auto &eye = vrmode->mEyes[eye_ix];
		SetViewportRects(bounds);

		if (mainview) // Bind the scene frame buffer and turn on draw buffers used by ssao
		{
			//mRenderState->SetRenderTarget(GetBuffers()->SceneColor.View.get(), GetBuffers()->SceneDepthStencil.View.get(), GetBuffers()->GetWidth(), GetBuffers()->GetHeight(), Poly_FORMAT_R16G16B16A16_SFLOAT, GetBuffers()->GetSceneSamples());
			bool useSSAO = (gl_ssao != 0);
			GetRenderState()->SetPassType(useSSAO ? GBUFFER_PASS : NORMAL_PASS);
			GetRenderState()->EnableDrawBuffers(GetRenderState()->GetPassDrawBufferCount());
		}

		auto di = HWDrawInfo::StartDrawInfo(mainvp.ViewLevel, nullptr, mainvp, nullptr);
		auto &vp = di->Viewpoint;

		di->Set3DViewport(*GetRenderState());
		di->SetViewArea();
		auto cm = di->SetFullbrightFlags(mainview ? vp.camera->player : nullptr);
		di->Viewpoint.FieldOfView = fov;	// Set the real FOV for the current scene (it's not necessarily the same as the global setting in r_viewpoint)

		// Stereo mode specific perspective projection
		di->VPUniforms.mProjectionMatrix = eye.GetProjection(fov, ratio, fovratio);
		// Stereo mode specific viewpoint adjustment
		vp.Pos += eye.GetViewShift(vp.HWAngles.Yaw.Degrees);
		di->SetupView(*GetRenderState(), vp.Pos.X, vp.Pos.Y, vp.Pos.Z, false, false);

		// std::function until this can be done better in a cross-API fashion.
		di->ProcessScene(toscreen, [&](HWDrawInfo *di, int mode) {
			DrawScene(di, mode);
		});

		if (mainview)
		{
			PostProcess.Clock();
			if (toscreen) di->EndDrawScene(mainvp.sector, *GetRenderState()); // do not call this for camera textures.

			if (GetRenderState()->GetPassType() == GBUFFER_PASS) // Turn off ssao draw buffers
			{
				GetRenderState()->SetPassType(NORMAL_PASS);
				GetRenderState()->EnableDrawBuffers(1);
			}

			//mPostprocess->BlitSceneToPostprocess(); // Copy the resulting scene to the current post process texture

			PostProcessScene(cm, [&]() { di->DrawEndScene2D(mainvp.sector, *GetRenderState()); });

			PostProcess.Unclock();
		}
		di->EndDrawInfo();

#if 0
		if (vrmode->mEyeCount > 1)
			mBuffers->BlitToEyeTexture(eye_ix);
#endif
	}

	return mainvp.sector;
}

void PolyFrameBuffer::RenderTextureView(FCanvasTexture *tex, AActor *Viewpoint, double FOV)
{
	// This doesn't need to clear the fake flat cache. It can be shared between camera textures and the main view of a scene.
	FMaterial *mat = FMaterial::ValidateTexture(tex, false);
	auto BaseLayer = static_cast<PolyHardwareTexture*>(mat->GetLayer(0, 0));

	int width = mat->TextureWidth();
	int height = mat->TextureHeight();
	DCanvas *image = BaseLayer->GetImage(tex, 0, 0);
	PolyDepthStencil *depthStencil = BaseLayer->GetDepthStencil(tex);
	mRenderState->SetRenderTarget(image, depthStencil, false);

	IntRect bounds;
	bounds.left = bounds.top = 0;
	bounds.width = MIN(mat->GetWidth(), image->GetWidth());
	bounds.height = MIN(mat->GetHeight(), image->GetHeight());

	FRenderViewpoint texvp;
	RenderViewpoint(texvp, Viewpoint, &bounds, FOV, (float)width / height, (float)width / height, false, false);

	FlushDrawCommands();
	DrawerThreads::WaitForWorkers();
	mRenderState->SetRenderTarget(GetCanvas(), GetDepthStencil(), true);

	tex->SetUpdated(true);
}

void PolyFrameBuffer::DrawScene(HWDrawInfo *di, int drawmode)
{
	// To do: this is virtually identical to FGLRenderer::DrawScene and should be merged.

	static int recursion = 0;
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

	GetRenderState()->SetDepthMask(true);
	if (!gl_no_skyclear) mPortalState->RenderFirstSkyPortal(recursion, di, *GetRenderState());

	di->RenderScene(*GetRenderState());

	if (applySSAO && GetRenderState()->GetPassType() == GBUFFER_PASS)
	{
		//mPostprocess->AmbientOccludeScene(di->VPUniforms.mProjectionMatrix.get()[5]);
		//mViewpoints->Bind(*GetRenderState(), di->vpIndex);
	}

	// Handle all portals after rendering the opaque objects but before
	// doing all translucent stuff
	recursion++;
	mPortalState->EndFrame(di, *GetRenderState());
	recursion--;
	di->RenderTranslucent(*GetRenderState());
}

static uint8_t ToIntColorComponent(float v)
{
	return clamp((int)(v * 255.0f + 0.5f), 0, 255);
}

void PolyFrameBuffer::PostProcessScene(int fixedcm, const std::function<void()> &afterBloomDrawEndScene2D)
{
	afterBloomDrawEndScene2D();

	if (fixedcm >= CM_FIRSTSPECIALCOLORMAP && fixedcm < CM_MAXCOLORMAP)
	{
		FSpecialColormap* scm = &SpecialColormaps[fixedcm - CM_FIRSTSPECIALCOLORMAP];

		mRenderState->SetViewport(mScreenViewport.left, mScreenViewport.top, mScreenViewport.width, mScreenViewport.height);
		screen->mViewpoints->Set2D(*mRenderState, screen->GetWidth(), screen->GetHeight());

		ScreenQuadVertex vertices[4] =
		{
			{ 0.0f, 0.0f, 0.0f, 0.0f },
			{ (float)mScreenViewport.width, 0.0f, 1.0f, 0.0f },
			{ 0.0f, (float)mScreenViewport.height, 0.0f, 1.0f },
			{ (float)mScreenViewport.width, (float)mScreenViewport.height, 1.0f, 1.0f }
		};
		mScreenQuad.VertexBuffer->SetData(4 * sizeof(ScreenQuadVertex), vertices, false);

		mRenderState->SetVertexBuffer(mScreenQuad.VertexBuffer, 0, 0);
		mRenderState->SetIndexBuffer(mScreenQuad.IndexBuffer);

		mRenderState->SetObjectColor(PalEntry(255, int(scm->ColorizeStart[0] * 127.5f), int(scm->ColorizeStart[1] * 127.5f), int(scm->ColorizeStart[2] * 127.5f)));
		mRenderState->SetAddColor(PalEntry(255, int(scm->ColorizeEnd[0] * 127.5f), int(scm->ColorizeEnd[1] * 127.5f), int(scm->ColorizeEnd[2] * 127.5f)));

		mRenderState->EnableDepthTest(false);
		mRenderState->EnableMultisampling(false);
		mRenderState->SetCulling(Cull_None);

		mRenderState->SetScissor(-1, -1, -1, -1);
		mRenderState->SetColor(1, 1, 1, 1);
		mRenderState->AlphaFunc(Alpha_GEqual, 0.f);
		mRenderState->EnableTexture(false);
		mRenderState->SetColormapShader(true);
		mRenderState->DrawIndexed(DT_Triangles, 0, 6);
		mRenderState->SetColormapShader(false);
		mRenderState->SetObjectColor(0xffffffff);
		mRenderState->SetAddColor(0);
		mRenderState->SetVertexBuffer(screen->mVertexData);
		mRenderState->EnableTexture(true);
		mRenderState->ResetColor();
	}
}

uint32_t PolyFrameBuffer::GetCaps()
{
	if (!V_IsHardwareRenderer())
		return Super::GetCaps();

	// describe our basic feature set
	ActorRenderFeatureFlags FlagSet = RFF_FLATSPRITES | RFF_MODELS | RFF_SLOPE3DFLOORS |
		RFF_TILTPITCH | RFF_ROLLSPRITES | RFF_POLYGONAL | RFF_MATSHADER | RFF_POSTSHADER | RFF_BRIGHTMAP;
	if (r_drawvoxels)
		FlagSet |= RFF_VOXELS;

	if (gl_tonemap != 5) // not running palette tonemap shader
		FlagSet |= RFF_TRUECOLOR;

	return (uint32_t)FlagSet;
}

void PolyFrameBuffer::SetVSync(bool vsync)
{
	cur_vsync = vsync;
}

void PolyFrameBuffer::CleanForRestart()
{
	// force recreation of the SW scene drawer to ensure it gets a new set of resources.
	swdrawer.reset();
}

void PolyFrameBuffer::PrecacheMaterial(FMaterial *mat, int translation)
{
	auto tex = mat->tex;
	if (tex->isSWCanvas()) return;

	// Textures that are already scaled in the texture lump will not get replaced by hires textures.
	int flags = mat->isExpanded() ? CTF_Expand : (gl_texture_usehires && !tex->isScaled()) ? CTF_CheckHires : 0;
	auto base = static_cast<PolyHardwareTexture*>(mat->GetLayer(0, translation));

	base->Precache(mat, translation, flags);
}

IHardwareTexture *PolyFrameBuffer::CreateHardwareTexture()
{
	return new PolyHardwareTexture();
}

FModelRenderer *PolyFrameBuffer::CreateModelRenderer(int mli) 
{
	return new FHWModelRenderer(nullptr, *GetRenderState(), mli);
}

IVertexBuffer *PolyFrameBuffer::CreateVertexBuffer()
{
	return new PolyVertexBuffer();
}

IIndexBuffer *PolyFrameBuffer::CreateIndexBuffer()
{
	return new PolyIndexBuffer();
}

IDataBuffer *PolyFrameBuffer::CreateDataBuffer(int bindingpoint, bool ssbo, bool needsresize)
{
	IDataBuffer *buffer = new PolyDataBuffer(bindingpoint, ssbo, needsresize);
	if (bindingpoint == LIGHTBUF_BINDINGPOINT)
		mLightBuffer = buffer;
	return buffer;
}

void PolyFrameBuffer::SetTextureFilterMode()
{
	TextureFilterChanged();
}

void PolyFrameBuffer::TextureFilterChanged()
{
}

void PolyFrameBuffer::StartPrecaching()
{
}

void PolyFrameBuffer::BlurScene(float amount)
{
}

void PolyFrameBuffer::UpdatePalette()
{
}

FTexture *PolyFrameBuffer::WipeStartScreen()
{
	SetViewportRects(nullptr);

	auto tex = new FWrapperTexture(mScreenViewport.width, mScreenViewport.height, 1);
	auto systex = static_cast<PolyHardwareTexture*>(tex->GetSystemTexture());

	systex->CreateWipeTexture(mScreenViewport.width, mScreenViewport.height, "WipeStartScreen");

	return tex;
}

FTexture *PolyFrameBuffer::WipeEndScreen()
{
	Draw2D();
	Clear2D();

	auto tex = new FWrapperTexture(mScreenViewport.width, mScreenViewport.height, 1);
	auto systex = static_cast<PolyHardwareTexture*>(tex->GetSystemTexture());

	systex->CreateWipeTexture(mScreenViewport.width, mScreenViewport.height, "WipeEndScreen");

	return tex;
}

TArray<uint8_t> PolyFrameBuffer::GetScreenshotBuffer(int &pitch, ESSType &color_type, float &gamma)
{
	int w = SCREENWIDTH;
	int h = SCREENHEIGHT;

	TArray<uint8_t> ScreenshotBuffer(w * h * 3, true);
	const uint8_t* pixels = GetCanvas()->GetPixels();
	int dindex = 0;

	// Convert to RGB
	for (int y = 0; y < h; y++)
	{
		int sindex = y * w * 4;

		for (int x = 0; x < w; x++)
		{
			ScreenshotBuffer[dindex    ] = pixels[sindex + 2];
			ScreenshotBuffer[dindex + 1] = pixels[sindex + 1];
			ScreenshotBuffer[dindex + 2] = pixels[sindex    ];
			dindex += 3;
			sindex += 4;
		}
	}

	pitch = w * 3;
	color_type = SS_RGB;
	gamma = 1.0f;
	return ScreenshotBuffer;
}

void PolyFrameBuffer::BeginFrame()
{
	SetViewportRects(nullptr);
	CheckCanvas();

	swrenderer::R_InitFuzzTable(GetCanvas()->GetPitch());
	static int next_random = 0;
	swrenderer::fuzzpos = (swrenderer::fuzzpos + swrenderer::fuzz_random_x_offset[next_random] * FUZZTABLE / 100) % FUZZTABLE;
	next_random++;
	if (next_random == FUZZ_RANDOM_X_SIZE)
		next_random = 0;
}

void PolyFrameBuffer::Draw2D()
{
	::Draw2D(&m2DDrawer, *mRenderState);
}

unsigned int PolyFrameBuffer::GetLightBufferBlockSize() const
{
	return mLights->GetBlockSize();
}

void PolyFrameBuffer::UpdateShadowMap()
{
}
