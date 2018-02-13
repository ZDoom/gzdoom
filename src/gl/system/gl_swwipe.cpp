/*
** gl_swwipe.cpp
** Implements the different screen wipes using OpenGL calls.
**
**---------------------------------------------------------------------------
** Copyright 1998-2008 Randy Heit
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

// HEADER FILES ------------------------------------------------------------

#include "gl/system/gl_system.h"
#include "m_swap.h"
#include "v_video.h"
#include "doomstat.h"
#include "m_png.h"
#include "m_crc32.h"
#include "vectors.h"
#include "v_palette.h"
#include "templates.h"

#include "c_dispatch.h"
#include "templates.h"
#include "i_system.h"
#include "i_video.h"
#include "v_pfx.h"
#include "stats.h"
#include "doomerrors.h"
#include "r_data/r_translate.h"
#include "f_wipe.h"
#include "sbar.h"
#include "w_wad.h"
#include "r_data/colormaps.h"

#include "gl/system/gl_interface.h"
#include "gl/system/gl_swframebuffer.h"
#include "gl/data/gl_data.h"
#include "gl/utility/gl_clock.h"
#include "gl/utility/gl_templates.h"
#include "gl/gl_functions.h"
#include "gl_debug.h"
#include "m_random.h"

class OpenGLSWFrameBuffer::Wiper_Crossfade : public OpenGLSWFrameBuffer::Wiper
{
public:
	Wiper_Crossfade();
	bool Run(int ticks, OpenGLSWFrameBuffer *fb);

private:
	int Clock;
};

class OpenGLSWFrameBuffer::Wiper_Melt : public OpenGLSWFrameBuffer::Wiper
{
public:
	Wiper_Melt();
	bool Run(int ticks, OpenGLSWFrameBuffer *fb);

private:
	// Match the strip sizes that oldschool Doom used.
	static const int WIDTH = 160, HEIGHT = 200;
	int y[WIDTH];
};

class OpenGLSWFrameBuffer::Wiper_Burn : public OpenGLSWFrameBuffer::Wiper
{
public:
	Wiper_Burn(OpenGLSWFrameBuffer *fb);
	~Wiper_Burn();
	bool Run(int ticks, OpenGLSWFrameBuffer *fb);

private:
	static const int WIDTH = 64, HEIGHT = 64;
	uint8_t BurnArray[WIDTH * (HEIGHT + 5)];
	std::unique_ptr<HWTexture> BurnTexture;
	int Density;
	int BurnTime;
};

//==========================================================================
//
// OpenGLSWFrameBuffer :: WipeStartScreen
//
// Called before the current screen has started rendering. This needs to
// save what was drawn the previous frame so that it can be animated into
// what gets drawn this frame.
//
// In fullscreen mode, we use GetFrontBufferData() to grab the data that
// is visible on screen right now.
//
// In windowed mode, we can't do that because we'll get the whole desktop.
// Instead, we can conveniently use the TempRenderTexture, which is normally
// used for gamma-correcting copying the image to the back buffer.
//
//==========================================================================

bool OpenGLSWFrameBuffer::WipeStartScreen(int type)
{
	if (!Accel2D)
	{
		return Super::WipeStartScreen(type);
	}

	switch (type)
	{
	case wipe_Melt:
		ScreenWipe = new Wiper_Melt;
		break;

	case wipe_Burn:
		ScreenWipe = new Wiper_Burn(this);
		break;

	case wipe_Fade:
		ScreenWipe = new Wiper_Crossfade;
		break;

	default:
		return false;
	}

	InitialWipeScreen = CopyCurrentScreen();

	// Make even fullscreen model render to the TempRenderTexture, so
	// we can have a copy of the new screen readily available.
	GatheringWipeScreen = true;
	return true;
}

//==========================================================================
//
// OpenGLSWFrameBuffer :: WipeEndScreen
//
// The screen we want to animate to has just been drawn. This function is
// called in place of Update(), so it has not been Presented yet.
//
//==========================================================================

void OpenGLSWFrameBuffer::WipeEndScreen()
{
	if (!Accel2D)
	{
		Super::WipeEndScreen();
		return;
	}

	// Don't do anything if there is no starting point.
	if (InitialWipeScreen == NULL)
	{
		return;
	}

	// If the whole screen was drawn without 2D accel, get it in to
	// video memory now.
	if (!In2D)
	{
		Begin2D(true);
	}

	EndBatch();			// Make sure all batched primitives have been drawn.

	FinalWipeScreen = CopyCurrentScreen();

	// At this point, InitialWipeScreen holds the screen we are wiping from.
	// FinalWipeScreen holds the screen we are wiping to, which may be the
	// same texture as TempRenderTexture.
}

//==========================================================================
//
// OpenGLSWFrameBuffer :: WipeDo
//
// Perform the actual wipe animation. The number of tics since the last
// time this function was called is passed in. Returns true when the wipe
// is over. The first time this function has been called, the screen is
// still locked from before and EndScene() still has not been called.
// Successive times need to call BeginScene().
//
//==========================================================================

bool OpenGLSWFrameBuffer::WipeDo(int ticks)
{
	if (!Accel2D)
	{
		return Super::WipeDo(ticks);
	}

	// Sanity checks.
	if (InitialWipeScreen == NULL || FinalWipeScreen == NULL)
	{
		return true;
	}
	if (GatheringWipeScreen)
	{ // This is the first time we've been called for this wipe.
		GatheringWipeScreen = false;
	}
	else
	{ // This is the second or later time we've been called for this wipe.
		InScene = true;
	}

	In2D = 3;

	EnableAlphaTest(false);
	bool done = ScreenWipe->Run(ticks, this);
	return done;
}

//==========================================================================
//
// OpenGLSWFrameBuffer :: WipeCleanup
//
// Release any resources that were specifically created for the wipe.
//
//==========================================================================

void OpenGLSWFrameBuffer::WipeCleanup()
{
	if (ScreenWipe != NULL)
	{
		delete ScreenWipe;
		ScreenWipe = NULL;
	}
	InitialWipeScreen.reset();
	FinalWipeScreen.reset();
	GatheringWipeScreen = false;
	if (!Accel2D)
	{
		Super::WipeCleanup();
		return;
	}
}

//==========================================================================
//
// OpenGLSWFrameBuffer :: Wiper Constructor
//
//==========================================================================

OpenGLSWFrameBuffer::Wiper::~Wiper()
{
}

//==========================================================================
//
// OpenGLSWFrameBuffer :: Wiper :: DrawScreen
//
// Draw either the initial or target screen completely to the screen.
//
//==========================================================================

void OpenGLSWFrameBuffer::Wiper::DrawScreen(OpenGLSWFrameBuffer *fb, HWTexture *tex,
	int blendop, uint32_t color0, uint32_t color1)
{
	FBVERTEX verts[4];

	fb->CalcFullscreenCoords(verts, false, color0, color1);
	fb->SetTexture(0, tex);
	fb->SetAlphaBlend(blendop, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	fb->SetPixelShader(fb->Shaders[SHADER_NormalColor].get());
	fb->DrawTriangleFans(2, verts);
}

// WIPE: CROSSFADE ---------------------------------------------------------

//==========================================================================
//
// OpenGLSWFrameBuffer :: Wiper_Crossfade Constructor
//
//==========================================================================

OpenGLSWFrameBuffer::Wiper_Crossfade::Wiper_Crossfade()
: Clock(0)
{
}

//==========================================================================
//
// OpenGLSWFrameBuffer :: Wiper_Crossfade :: Run
//
// Fades the old screen into the new one over 32 ticks.
//
//==========================================================================

bool OpenGLSWFrameBuffer::Wiper_Crossfade::Run(int ticks, OpenGLSWFrameBuffer *fb)
{
	Clock += ticks;

	// Put the initial screen back to the buffer.
	DrawScreen(fb, fb->InitialWipeScreen.get());

	// Draw the new screen on top of it.
	DrawScreen(fb, fb->FinalWipeScreen.get(), GL_FUNC_ADD, ColorValue(0,0,0,Clock / 32.f), ColorRGBA(255,255,255,0));

	return Clock >= 32;
}

// WIPE: MELT --------------------------------------------------------------

//==========================================================================
//
// OpenGLSWFrameBuffer :: Wiper_Melt Constructor
//
//==========================================================================

OpenGLSWFrameBuffer::Wiper_Melt::Wiper_Melt()
{
	int i, r;

	// setup initial column positions
	// (y<0 => not ready to scroll yet)
	y[0] = -(M_Random() & 15);
	for (i = 1; i < WIDTH; ++i)
	{
		r = (M_Random()%3) - 1;
		y[i] = clamp(y[i-1] + r, -15, 0);
	}
}

//==========================================================================
//
// OpenGLSWFrameBuffer :: Wiper_Melt :: Run
//
// Fades the old screen into the new one over 32 ticks.
//
//==========================================================================

bool OpenGLSWFrameBuffer::Wiper_Melt::Run(int ticks, OpenGLSWFrameBuffer *fb)
{
	// Draw the new screen on the bottom.
	DrawScreen(fb, fb->FinalWipeScreen.get());

	int i, dy;
	int fbwidth = fb->Width;
	int fbheight = fb->Height;
	bool done = true;

	// Copy the old screen in vertical strips on top of the new one.
	while (ticks--)
	{
		done = true;
		for (i = 0; i < WIDTH; i++)
		{
			if (y[i] < 0)
			{
				y[i]++;
				done = false;
			} 
			else if (y[i] < HEIGHT)
			{
				dy = (y[i] < 16) ? y[i]+1 : 8;
				y[i] = MIN(y[i] + dy, HEIGHT);
				done = false;
			}
			if (ticks == 0)
			{ // Only draw for the final tick.
				LTRBRect rect;
				struct Point { int x, y; } dpt;

				dpt.x = i * fbwidth / WIDTH;
				dpt.y = MAX(0, y[i] * fbheight / HEIGHT);
				rect.left = dpt.x;
				rect.top = 0;
				rect.right = (i + 1) * fbwidth / WIDTH;
				rect.bottom = fbheight - dpt.y;
				if (rect.bottom > rect.top)
				{
					fb->CheckQuadBatch();

					BufferedTris *quad = &fb->QuadExtra[fb->QuadBatchPos];
					FBVERTEX *vert = &fb->VertexData[fb->VertexPos];
					uint16_t *index = &fb->IndexData[fb->IndexPos];

					quad->ClearSetup();
					quad->Flags = BQF_DisableAlphaTest;
					quad->ShaderNum = BQS_Plain;
					quad->Palette = NULL;
					quad->Texture = fb->InitialWipeScreen.get();
					quad->NumVerts = 4;
					quad->NumTris = 2;

					// Fill the vertex buffer.
					float u0 = rect.left / float(fb->Width);
					float v0 = 0;
					float u1 = rect.right / float(fb->Width);
					float v1 = (rect.bottom - rect.top) / float(fb->Height);

					float x0 = float(rect.left);
					float x1 = float(rect.right);
					float y0 = float(dpt.y);
					float y1 = float(fbheight);

					vert[0].x = x0;
					vert[0].y = y0;
					vert[0].z = 0;
					vert[0].rhw = 1;
					vert[0].color0 = 0;
					vert[0].color1 = 0xFFFFFFF;
					vert[0].tu = u0;
					vert[0].tv = v0;

					vert[1].x = x1;
					vert[1].y = y0;
					vert[1].z = 0;
					vert[1].rhw = 1;
					vert[1].color0 = 0;
					vert[1].color1 = 0xFFFFFFF;
					vert[1].tu = u1;
					vert[1].tv = v0;

					vert[2].x = x1;
					vert[2].y = y1;
					vert[2].z = 0;
					vert[2].rhw = 1;
					vert[2].color0 = 0;
					vert[2].color1 = 0xFFFFFFF;
					vert[2].tu = u1;
					vert[2].tv = v1;

					vert[3].x = x0;
					vert[3].y = y1;
					vert[3].z = 0;
					vert[3].rhw = 1;
					vert[3].color0 = 0;
					vert[3].color1 = 0xFFFFFFF;
					vert[3].tu = u0;
					vert[3].tv = v1;

					// Fill the vertex index buffer.
					index[0] = fb->VertexPos;
					index[1] = fb->VertexPos + 1;
					index[2] = fb->VertexPos + 2;
					index[3] = fb->VertexPos;
					index[4] = fb->VertexPos + 2;
					index[5] = fb->VertexPos + 3;

					// Batch the quad.
					fb->QuadBatchPos++;
					fb->VertexPos += 4;
					fb->IndexPos += 6;
				}
			}
		}
	}
	fb->EndQuadBatch();
	return done;
}

// WIPE: BURN --------------------------------------------------------------

//==========================================================================
//
// OpenGLSWFrameBuffer :: Wiper_Burn Constructor
//
//==========================================================================

OpenGLSWFrameBuffer::Wiper_Burn::Wiper_Burn(OpenGLSWFrameBuffer *fb)
{
	Density = 4;
	BurnTime = 0;
	memset(BurnArray, 0, sizeof(BurnArray));
	if (fb->Shaders[SHADER_BurnWipe] == nullptr)
	{
		BurnTexture = nullptr;
	}

	BurnTexture = fb->CreateTexture("BurnWipe", WIDTH, HEIGHT, 1, GL_R8);
}

//==========================================================================
//
// OpenGLSWFrameBuffer :: Wiper_Burn Destructor
//
//==========================================================================

OpenGLSWFrameBuffer::Wiper_Burn::~Wiper_Burn()
{
	BurnTexture.reset();
}

//==========================================================================
//
// OpenGLSWFrameBuffer :: Wiper_Burn :: Run
//
//==========================================================================

bool OpenGLSWFrameBuffer::Wiper_Burn::Run(int ticks, OpenGLSWFrameBuffer *fb)
{
	bool done;

	BurnTime += ticks;
	ticks *= 2;

	// Make the fire burn
	done = false;
	while (!done && ticks--)
	{
		Density = wipe_CalcBurn(BurnArray, WIDTH, HEIGHT, Density);
		done = (Density < 0);
	}

	// Update the burn texture with the new burn data

	if (BurnTexture->Buffers[0] == 0)
	{
		glGenBuffers(2, (GLuint*)BurnTexture->Buffers);
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, BurnTexture->Buffers[0]);
		glBufferData(GL_PIXEL_UNPACK_BUFFER, WIDTH * HEIGHT, nullptr, GL_STREAM_DRAW);
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, BurnTexture->Buffers[1]);
		glBufferData(GL_PIXEL_UNPACK_BUFFER, WIDTH * HEIGHT, nullptr, GL_STREAM_DRAW);
	}
	else
	{
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, BurnTexture->Buffers[BurnTexture->CurrentBuffer]);
		BurnTexture->CurrentBuffer = (BurnTexture->CurrentBuffer + 1) & 1;
	}

	uint8_t *dest = (uint8_t*)glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, WIDTH * HEIGHT, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
	if (dest)
	{
		memcpy(dest, BurnArray, WIDTH * HEIGHT);
		glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);

		GLint oldBinding = 0;
		glGetIntegerv(GL_TEXTURE_BINDING_2D, &oldBinding);
		glBindTexture(GL_TEXTURE_2D, BurnTexture->Texture);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, WIDTH, HEIGHT, GL_RED, GL_UNSIGNED_BYTE, 0);
		glBindTexture(GL_TEXTURE_2D, oldBinding);

		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
	}

	// Put the initial screen back to the buffer.
	DrawScreen(fb, fb->InitialWipeScreen.get());

	// Burn the new screen on top of it.
	float right = float(fb->Width);
	float bot = float(fb->Height);

	BURNVERTEX verts[4] =
	{
		{ 0.f,   0.f, 0.f, 1.f, 0.f, 0.f, 0, 0 },
		{ right, 0.f, 0.f, 1.f, 1.f, 0.f, 1, 0 },
		{ right, bot, 0.f, 1.f, 1.f, 1.f, 1, 1 },
		{ 0.f,   bot, 0.f, 1.f, 0.f, 1.f, 0, 1 }
	};

	fb->SetTexture(0, fb->FinalWipeScreen.get());
	fb->SetTexture(1, BurnTexture.get());
	fb->SetAlphaBlend(GL_FUNC_ADD, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	fb->SetPixelShader(fb->Shaders[SHADER_BurnWipe].get());
	glActiveTexture(GL_TEXTURE1);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	fb->DrawTriangleFans(2, verts);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glActiveTexture(GL_TEXTURE0);

	// The fire may not always stabilize, so the wipe is forced to end
	// after an arbitrary maximum time.
	return done || (BurnTime > 40);
}
