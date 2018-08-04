// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2010-2016 Christoph Oelckers
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
** gl_framebuffer.cpp
** Implementation of the non-hardware specific parts of the
** OpenGL frame buffer
**
*/

#include "gl_load/gl_system.h"
#include "v_video.h"
#include "m_png.h"
#include "templates.h"

#include "gl_load/gl_interface.h"
#include "gl/system/gl_framebuffer.h"
#include "gl/renderer/gl_renderer.h"
#include "gl/renderer/gl_renderbuffers.h"
#include "gl/textures/gl_samplers.h"
#include "hwrenderer/utility/hw_clock.h"
#include "hwrenderer/utility/hw_vrmodes.h"
#include "gl/data/gl_vertexbuffer.h"
#include "gl/data/gl_uniformbuffer.h"
#include "gl/models/gl_models.h"
#include "gl/shaders/gl_shaderprogram.h"
#include "gl_debug.h"
#include "r_videoscale.h"

EXTERN_CVAR (Bool, vid_vsync)

FGLRenderer *GLRenderer;

void gl_LoadExtensions();
void gl_PrintStartupLog();

CUSTOM_CVAR(Int, vid_hwgamma, 2, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)
{
	if (self < 0 || self > 2) self = 2;
	if (screen != nullptr) screen->SetGamma();
}

//==========================================================================
//
//
//
//==========================================================================

OpenGLFrameBuffer::OpenGLFrameBuffer(void *hMonitor, bool fullscreen) : 
	Super(hMonitor, fullscreen) 
{
	// SetVSync needs to be at the very top to workaround a bug in Nvidia's OpenGL driver.
	// If wglSwapIntervalEXT is called after glBindFramebuffer in a frame the setting is not changed!
	Super::SetVSync(vid_vsync);

	// Make sure all global variables tracking OpenGL context state are reset..
	FHardwareTexture::InitGlobalState();
	gl_RenderState.Reset();

	GLRenderer = new FGLRenderer(this);
	InitPalette();

	InitializeState();
	mDebug = std::make_shared<FGLDebug>();
	mDebug->Update();
	SetGamma();

	// Move some state to the framebuffer object for easier access.
	hwcaps = gl.flags;
	glslversion = gl.glslversion;
}

OpenGLFrameBuffer::~OpenGLFrameBuffer()
{
	delete GLRenderer;
	GLRenderer = NULL;
}

//==========================================================================
//
// Initializes the GL renderer
//
//==========================================================================

void OpenGLFrameBuffer::InitializeState()
{
	static bool first=true;

	if (first)
	{
		if (ogl_LoadFunctions() == ogl_LOAD_FAILED)
		{
			I_FatalError("Failed to load OpenGL functions.");
		}
	}

	gl_LoadExtensions();

	if (first)
	{
		first=false;
		gl_PrintStartupLog();
	}

	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClearDepth(1.0f);
	glDepthFunc(GL_LESS);

	glEnable(GL_DITHER);
	glDisable(GL_CULL_FACE);
	glDisable(GL_POLYGON_OFFSET_FILL);
	glEnable(GL_POLYGON_OFFSET_LINE);
	glEnable(GL_BLEND);
	glEnable(GL_DEPTH_CLAMP);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_LINE_SMOOTH);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	GLRenderer->Initialize(GetWidth(), GetHeight());
	SetViewportRects(nullptr);
}

//==========================================================================
//
// Updates the screen
//
//==========================================================================

void OpenGLFrameBuffer::Update()
{
	twoD.Reset();
	Flush3D.Reset();

	DrawRateStuff();
	Flush3D.Clock();
	GLRenderer->Flush();
	Flush3D.Unclock();

	Swap();
	CheckBench();

	int initialWidth = GetClientWidth();
	int initialHeight = GetClientHeight();
	int clientWidth = ViewportScaledWidth(initialWidth, initialHeight);
	int clientHeight = ViewportScaledHeight(initialWidth, initialHeight);
	if (clientWidth < 320) clientWidth = 320;
	if (clientHeight < 200) clientHeight = 200;
	if (clientWidth > 0 && clientHeight > 0 && (GetWidth() != clientWidth || GetHeight() != clientHeight))
	{
		SetVirtualSize(clientWidth, clientHeight);
		V_OutputResized(clientWidth, clientHeight);
		GLRenderer->mVBO->OutputResized(clientWidth, clientHeight);
	}
}

//===========================================================================
//
// 
//
//===========================================================================

void OpenGLFrameBuffer::RenderTextureView(FCanvasTexture *tex, AActor *Viewpoint, double FOV)
{
	if (!V_IsHardwareRenderer())
	{
		Super::RenderTextureView(tex, Viewpoint, FOV);
	}
	else if (GLRenderer != nullptr)
	{
		GLRenderer->RenderTextureView(tex, Viewpoint, FOV);
		camtexcount++;
	}
}

//===========================================================================
//
// Render the view to a savegame picture
//
//===========================================================================

void OpenGLFrameBuffer::WriteSavePic(player_t *player, FileWriter *file, int width, int height)
{
	if (!V_IsHardwareRenderer())
		Super::WriteSavePic(player, file, width, height);
	else if (GLRenderer != nullptr)
		GLRenderer->WriteSavePic(player, file, width, height);
}

//===========================================================================
//
// 
//
//===========================================================================

sector_t *OpenGLFrameBuffer::RenderView(player_t *player)
{
	if (GLRenderer != nullptr)
		return GLRenderer->RenderView(player);
	return nullptr;
}



//===========================================================================
//
// 
//
//===========================================================================

EXTERN_CVAR(Bool, r_drawvoxels)
EXTERN_CVAR(Int, gl_tonemap)

uint32_t OpenGLFrameBuffer::GetCaps()
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

//==========================================================================
//
// Swap the buffers
//
//==========================================================================

CVAR(Bool, gl_finishbeforeswap, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG);

void OpenGLFrameBuffer::Swap()
{
	bool swapbefore = gl_finishbeforeswap && camtexcount == 0;
	Finish.Reset();
	Finish.Clock();
	if (swapbefore) glFinish();
	SwapBuffers();
	if (!swapbefore) glFinish();
	Finish.Unclock();
	camtexcount = 0;
	FHardwareTexture::UnbindAll();
	mDebug->Update();
}

//==========================================================================
//
// Enable/disable vertical sync
//
//==========================================================================

void OpenGLFrameBuffer::SetVSync(bool vsync)
{
	// Switch to the default frame buffer because some drivers associate the vsync state with the bound FB object.
	GLint oldDrawFramebufferBinding = 0, oldReadFramebufferBinding = 0;
	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &oldDrawFramebufferBinding);
	glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &oldReadFramebufferBinding);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

	Super::SetVSync(vsync);

	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, oldDrawFramebufferBinding);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, oldReadFramebufferBinding);
}

//===========================================================================
//
// DoSetGamma
//
// (Unfortunately Windows has some safety precautions that block gamma ramps
//  that are considered too extreme. As a result this doesn't work flawlessly)
//
//===========================================================================

void OpenGLFrameBuffer::SetGamma()
{
	bool useHWGamma = m_supportsGamma && ((vid_hwgamma == 0) || (vid_hwgamma == 2 && IsFullscreen()));
	if (useHWGamma)
	{
		uint16_t gammaTable[768];

		// This formula is taken from Doomsday
		BuildGammaTable(gammaTable);
		SetGammaTable(gammaTable);

		HWGammaActive = true;
	}
	else if (HWGammaActive)
	{
		ResetGammaTable();
		HWGammaActive = false;
	}
}

//===========================================================================
//
//
//===========================================================================

void OpenGLFrameBuffer::CleanForRestart()
{
	if (GLRenderer)
		GLRenderer->ResetSWScene();
}

void OpenGLFrameBuffer::SetTextureFilterMode()
{
	if (GLRenderer != nullptr && GLRenderer->mSamplerManager != nullptr) GLRenderer->mSamplerManager->SetTextureFilterMode();
}

IHardwareTexture *OpenGLFrameBuffer::CreateHardwareTexture(FTexture *tex) 
{ 
	return new FHardwareTexture(tex->bNoCompress);
}

void OpenGLFrameBuffer::PrecacheMaterial(FMaterial *mat, int translation)
{
	gl_RenderState.SetMaterial(mat, CLAMP_NONE, translation, false, false);
}

FModelRenderer *OpenGLFrameBuffer::CreateModelRenderer(int mli) 
{
	return new FGLModelRenderer(nullptr, mli);
}

IUniformBuffer *OpenGLFrameBuffer::CreateUniformBuffer(size_t size, bool staticuse)
{
    return new GLUniformBuffer(size, staticuse);
}

IShaderProgram *OpenGLFrameBuffer::CreateShaderProgram() 
{ 
	return new FShaderProgram; 
}


void OpenGLFrameBuffer::TextureFilterChanged()
{
	if (GLRenderer != NULL && GLRenderer->mSamplerManager != NULL) GLRenderer->mSamplerManager->SetTextureFilterMode();
}

void OpenGLFrameBuffer::BlurScene(float amount)
{
	GLRenderer->BlurScene(amount);
}

void OpenGLFrameBuffer::SetViewportRects(IntRect *bounds)
{
	Super::SetViewportRects(bounds);
	if (!bounds)
	{
		auto vrmode = VRMode::GetVRMode(true);
		vrmode->AdjustViewport(this);
	}
}


void OpenGLFrameBuffer::InitForLevel()
{
	if (GLRenderer != NULL)
	{
		GLRenderer->SetupLevel();
	}
}

void OpenGLFrameBuffer::UpdatePalette()
{
	if (GLRenderer)
		GLRenderer->ClearTonemapPalette();
}


//===========================================================================
//
// 
//
//===========================================================================

void OpenGLFrameBuffer::SetClearColor(int color)
{
	PalEntry pe = GPalette.BaseColors[color];
	GLRenderer->mSceneClearColor[0] = pe.r / 255.f;
	GLRenderer->mSceneClearColor[1] = pe.g / 255.f;
	GLRenderer->mSceneClearColor[2] = pe.b / 255.f;
}


void OpenGLFrameBuffer::BeginFrame()
{
	SetViewportRects(nullptr);
	if (GLRenderer != nullptr)
		GLRenderer->BeginFrame();
}

//===========================================================================
// 
//	Takes a screenshot
//
//===========================================================================

void OpenGLFrameBuffer::GetScreenshotBuffer(const uint8_t *&buffer, int &pitch, ESSType &color_type, float &gamma)
{
	const auto &viewport = mOutputLetterbox;

	// Grab what is in the back buffer.
	// We cannot rely on SCREENWIDTH/HEIGHT here because the output may have been scaled.
	TArray<uint8_t> pixels;
	pixels.Resize(viewport.width * viewport.height * 3);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(viewport.left, viewport.top, viewport.width, viewport.height, GL_RGB, GL_UNSIGNED_BYTE, &pixels[0]);
	glPixelStorei(GL_PACK_ALIGNMENT, 4);

	// Copy to screenshot buffer:
	int w = SCREENWIDTH;
	int h = SCREENHEIGHT;

	auto ScreenshotBuffer = new uint8_t[w * h * 3];

	float rcpWidth = 1.0f / w;
	float rcpHeight = 1.0f / h;
	for (int y = 0; y < h; y++)
	{
		for (int x = 0; x < w; x++)
		{
			float u = (x + 0.5f) * rcpWidth;
			float v = (y + 0.5f) * rcpHeight;
			int sx = u * viewport.width;
			int sy = v * viewport.height;
			int sindex = (sx + sy * viewport.width) * 3;
			int dindex = (x + (h - y - 1) * w) * 3;
			ScreenshotBuffer[dindex] = pixels[sindex];
			ScreenshotBuffer[dindex + 1] = pixels[sindex + 1];
			ScreenshotBuffer[dindex + 2] = pixels[sindex + 2];
		}
	}

	pitch = w * 3;
	color_type = SS_RGB;
	buffer = ScreenshotBuffer;

	// Screenshot should not use gamma correction if it was already applied to rendered image
	gamma = 1 == vid_hwgamma || (2 == vid_hwgamma && !fullscreen) ? 1.0f : Gamma;
}

//===========================================================================
// 
// 2D drawing
//
//===========================================================================

void OpenGLFrameBuffer::Draw2D()
{
	if (GLRenderer != nullptr) GLRenderer->Draw2D(&m2DDrawer);
}

void OpenGLFrameBuffer::PostProcessScene(int fixedcm, const std::function<void()> &afterBloomDrawEndScene2D)
{
	GLRenderer->PostProcessScene(fixedcm, afterBloomDrawEndScene2D);
}
