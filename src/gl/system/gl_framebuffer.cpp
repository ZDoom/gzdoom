/*
** gl_framebuffer.cpp
** Implementation of the non-hardware specific parts of the
** OpenGL frame buffer
**
**---------------------------------------------------------------------------
** Copyright 2000-2007 Christoph Oelckers
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
** 4. When not used as part of GZDoom or a GZDoom derivative, this code will be
**    covered by the terms of the GNU Lesser General Public License as published
**    by the Free Software Foundation; either version 2.1 of the License, or (at
**    your option) any later version.
** 5. Full disclosure of the entire project's source code, except for third
**    party libraries is mandatory. (NOTE: This clause is non-negotiable!)
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

#include "gl/system/gl_system.h"
#include "files.h"
#include "m_swap.h"
#include "v_video.h"
#include "doomstat.h"
#include "m_png.h"
#include "m_crc32.h"
#include "vectors.h"
#include "v_palette.h"
#include "templates.h"
#include "farchive.h"

#include "gl/system/gl_interface.h"
#include "gl/system/gl_framebuffer.h"
#include "gl/renderer/gl_renderer.h"
#include "gl/renderer/gl_renderstate.h"
#include "gl/renderer/gl_lightdata.h"
#include "gl/data/gl_data.h"
#include "gl/textures/gl_hwtexture.h"
#include "gl/textures/gl_texture.h"
#include "gl/textures/gl_translate.h"
#include "gl/textures/gl_skyboxtexture.h"
#include "gl/utility/gl_clock.h"
#include "gl/utility/gl_templates.h"
#include "gl/gl_functions.h"
#include "gl/renderer/gl_2ddrawer.h"

IMPLEMENT_CLASS(OpenGLFrameBuffer)
EXTERN_CVAR (Float, vid_brightness)
EXTERN_CVAR (Float, vid_contrast)
EXTERN_CVAR (Bool, vid_vsync)

CVAR(Bool, gl_aalines, false, CVAR_ARCHIVE)

FGLRenderer *GLRenderer;

void gl_LoadExtensions();
void gl_PrintStartupLog();
void gl_SetupMenu();

CUSTOM_CVAR(Int, vid_hwgamma, 2, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)
{
	if (self < 0 || self > 2) self = 2;
	if (GLRenderer != NULL && GLRenderer->framebuffer != NULL) GLRenderer->framebuffer->DoSetGamma();
}

//==========================================================================
//
//
//
//==========================================================================

OpenGLFrameBuffer::OpenGLFrameBuffer(void *hMonitor, int width, int height, int bits, int refreshHz, bool fullscreen) : 
	Super(hMonitor, width, height, bits, refreshHz, fullscreen) 
{
	// SetVSync needs to be at the very top to workaround a bug in Nvidia's OpenGL driver.
	// If wglSwapIntervalEXT is called after glBindFramebuffer in a frame the setting is not changed!
	SetVSync(vid_vsync);

	GLRenderer = new FGLRenderer(this);
	memcpy (SourcePalette, GPalette.BaseColors, sizeof(PalEntry)*256);
	UpdatePalette ();
	ScreenshotBuffer = NULL;
	LastCamera = NULL;

	InitializeState();
	gl_SetupMenu();
	gl_GenerateGlobalBrightmapFromColormap();
	DoSetGamma();
	needsetgamma = true;
	swapped = false;
	Accel2D = true;
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
		ogl_LoadFunctions();
	}

	gl_LoadExtensions();
	Super::InitializeState();

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
	glEnable(GL_TEXTURE_2D);
	glDisable(GL_LINE_SMOOTH);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	GLRenderer->Initialize(GetWidth(), GetHeight());
	GLRenderer->SetOutputViewport(nullptr);
	Begin2D(false);
}

//==========================================================================
//
// Updates the screen
//
//==========================================================================

// Testing only for now. 
CVAR(Bool, gl_draw_sync, true, 0) //false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)

void OpenGLFrameBuffer::Update()
{
	if (!CanUpdate()) 
	{
		GLRenderer->Flush();
		return;
	}

	Begin2D(false);

	DrawRateStuff();
	GLRenderer->Flush();

	GLRenderer->SetOutputViewport(nullptr);

	if (gl_draw_sync || !swapped)
	{
		Swap();
	}
	swapped = false;
	Unlock();
	CheckBench();
}


//==========================================================================
//
// Swap the buffers
//
//==========================================================================

void OpenGLFrameBuffer::Swap()
{
	Finish.Reset();
	Finish.Clock();
	glFinish();
	if (needsetgamma) 
	{
		//DoSetGamma();
		needsetgamma = false;
	}
	SwapBuffers();
	Finish.Unclock();
	swapped = true;
	FHardwareTexture::UnbindAll();
}

//===========================================================================
//
// DoSetGamma
//
// (Unfortunately Windows has some safety precautions that block gamma ramps
//  that are considered too extreme. As a result this doesn't work flawlessly)
//
//===========================================================================

void OpenGLFrameBuffer::DoSetGamma()
{
	bool useHWGamma = m_supportsGamma && ((vid_hwgamma == 0) || (vid_hwgamma == 2 && IsFullscreen()));
	if (useHWGamma)
	{
		WORD gammaTable[768];

		// This formula is taken from Doomsday
		float gamma = clamp<float>(Gamma, 0.1f, 4.f);
		float contrast = clamp<float>(vid_contrast, 0.1f, 3.f);
		float bright = clamp<float>(vid_brightness, -0.8f, 0.8f);

		double invgamma = 1 / gamma;
		double norm = pow(255., invgamma - 1);

		for (int i = 0; i < 256; i++)
		{
			double val = i * contrast - (contrast - 1) * 127;
			if(gamma != 1) val = pow(val, invgamma) / norm;
			val += bright * 128;

			gammaTable[i] = gammaTable[i + 256] = gammaTable[i + 512] = (WORD)clamp<double>(val*256, 0, 0xffff);
		}
		SetGammaTable(gammaTable);

		HWGammaActive = true;
	}
	else if (HWGammaActive)
	{
		ResetGammaTable();
		HWGammaActive = false;
	}
}

bool OpenGLFrameBuffer::SetGamma(float gamma)
{
	DoSetGamma();
	return true;
}

bool OpenGLFrameBuffer::SetBrightness(float bright)
{
	DoSetGamma();
	return true;
}

bool OpenGLFrameBuffer::SetContrast(float contrast)
{
	DoSetGamma();
	return true;
}

//===========================================================================
//
//
//===========================================================================

void OpenGLFrameBuffer::UpdatePalette()
{
	int rr=0,gg=0,bb=0;
	for(int x=0;x<256;x++)
	{
		rr+=GPalette.BaseColors[x].r;
		gg+=GPalette.BaseColors[x].g;
		bb+=GPalette.BaseColors[x].b;
	}
	rr>>=8;
	gg>>=8;
	bb>>=8;

	palette_brightness = (rr*77 + gg*143 + bb*35)/255;
}

void OpenGLFrameBuffer::GetFlashedPalette (PalEntry pal[256])
{
	memcpy(pal, SourcePalette, 256*sizeof(PalEntry));
}

PalEntry *OpenGLFrameBuffer::GetPalette ()
{
	return SourcePalette;
}

bool OpenGLFrameBuffer::SetFlash(PalEntry rgb, int amount)
{
	Flash = PalEntry(amount, rgb.r, rgb.g, rgb.b);
	return true;
}

void OpenGLFrameBuffer::GetFlash(PalEntry &rgb, int &amount)
{
	rgb = Flash;
	rgb.a = 0;
	amount = Flash.a;
}

int OpenGLFrameBuffer::GetPageCount()
{
	return 1;
}


//==========================================================================
//
// DFrameBuffer :: CreatePalette
//
// Creates a native palette from a remap table, if supported.
//
//==========================================================================

FNativePalette *OpenGLFrameBuffer::CreatePalette(FRemapTable *remap)
{
	return GLTranslationPalette::CreatePalette(remap);
}

//==========================================================================
//
//
//
//==========================================================================
bool OpenGLFrameBuffer::Begin2D(bool)
{
	gl_RenderState.mViewMatrix.loadIdentity();
	gl_RenderState.mProjectionMatrix.ortho(0, GetWidth(), GetHeight(), 0, -1.0f, 1.0f);
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

	if (GLRenderer != NULL)
			GLRenderer->Begin2D();
	return true;
}

//==========================================================================
//
// Draws a texture
//
//==========================================================================

void OpenGLFrameBuffer::DrawTextureParms(FTexture *img, DrawParms &parms)
{
	if (GLRenderer != nullptr && GLRenderer->m2DDrawer != nullptr)
		GLRenderer->m2DDrawer->AddTexture(img, parms);
}

//==========================================================================
//
//
//
//==========================================================================
void OpenGLFrameBuffer::DrawLine(int x1, int y1, int x2, int y2, int palcolor, uint32 color)
{
	if (GLRenderer != nullptr && GLRenderer->m2DDrawer != nullptr) 
		GLRenderer->m2DDrawer->AddLine(x1, y1, x2, y2, palcolor, color);
}

//==========================================================================
//
//
//
//==========================================================================
void OpenGLFrameBuffer::DrawPixel(int x1, int y1, int palcolor, uint32 color)
{
	if (GLRenderer != nullptr && GLRenderer->m2DDrawer != nullptr)
		GLRenderer->m2DDrawer->AddPixel(x1, y1, palcolor, color);
}

//==========================================================================
//
//
//
//==========================================================================
void OpenGLFrameBuffer::Dim(PalEntry)
{
	// Unlike in the software renderer the color is being ignored here because
	// view blending only affects the actual view with the GL renderer.
	Super::Dim(0);
}

void OpenGLFrameBuffer::Dim(PalEntry color, float damount, int x1, int y1, int w, int h)
{
	if (GLRenderer != nullptr && GLRenderer->m2DDrawer != nullptr)
		GLRenderer->m2DDrawer->AddDim(color, damount, x1, y1, w, h);
}

//==========================================================================
//
//
//
//==========================================================================
void OpenGLFrameBuffer::FlatFill (int left, int top, int right, int bottom, FTexture *src, bool local_origin)
{

	if (GLRenderer != nullptr && GLRenderer->m2DDrawer != nullptr)
		GLRenderer->m2DDrawer->AddFlatFill(left, top, right, bottom, src, local_origin);
}

//==========================================================================
//
//
//
//==========================================================================
void OpenGLFrameBuffer::Clear(int left, int top, int right, int bottom, int palcolor, uint32 color)
{
	if (GLRenderer != nullptr && GLRenderer->m2DDrawer != nullptr)
		GLRenderer->m2DDrawer->AddClear(left, top, right, bottom, palcolor, color);
}

//==========================================================================
//
// D3DFB :: FillSimplePoly
//
// Here, "simple" means that a simple triangle fan can draw it.
//
//==========================================================================

void OpenGLFrameBuffer::FillSimplePoly(FTexture *texture, FVector2 *points, int npoints,
	double originx, double originy, double scalex, double scaley,
	DAngle rotation, FDynamicColormap *colormap, int lightlevel)
{
	if (GLRenderer != nullptr && GLRenderer->m2DDrawer != nullptr && npoints >= 3)
	{
		GLRenderer->m2DDrawer->AddPoly(texture, points, npoints, originx, originy, scalex, scaley, rotation, colormap, lightlevel);
	}
}


//===========================================================================
// 
//	Takes a screenshot
//
//===========================================================================

void OpenGLFrameBuffer::GetScreenshotBuffer(const BYTE *&buffer, int &pitch, ESSType &color_type)
{
	const auto &viewport = GLRenderer->mOutputLetterbox;

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

	ReleaseScreenshotBuffer();
	ScreenshotBuffer = new BYTE[w * h * 3];

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
			int dindex = (x + y * w) * 3;
			ScreenshotBuffer[dindex] = pixels[sindex];
			ScreenshotBuffer[dindex + 1] = pixels[sindex + 1];
			ScreenshotBuffer[dindex + 2] = pixels[sindex + 2];
		}
	}

	pitch = -w*3;
	color_type = SS_RGB;
	buffer = ScreenshotBuffer + w * 3 * (h - 1);
}

//===========================================================================
// 
// Releases the screenshot buffer.
//
//===========================================================================

void OpenGLFrameBuffer::ReleaseScreenshotBuffer()
{
	if (ScreenshotBuffer != NULL) delete [] ScreenshotBuffer;
	ScreenshotBuffer = NULL;
}


void OpenGLFrameBuffer::GameRestart()
{
	memcpy (SourcePalette, GPalette.BaseColors, sizeof(PalEntry)*256);
	UpdatePalette ();
	ScreenshotBuffer = NULL;
	LastCamera = NULL;
	gl_GenerateGlobalBrightmapFromColormap();
}

