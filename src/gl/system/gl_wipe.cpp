// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2008-2016 Christoph Oelckers
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
** Screen wipe stuff
**
*/

#include "gl_load/gl_system.h"
#include "f_wipe.h"
#include "m_random.h"
#include "w_wad.h"
#include "v_palette.h"
#include "templates.h"

#include "gl_load/gl_interface.h"
#include "gl/renderer/gl_renderer.h"
#include "gl/renderer/gl_renderstate.h"
#include "gl/renderer/gl_renderbuffers.h"
#include "gl/system/gl_framebuffer.h"
#include "gl/textures/gl_samplers.h"
#include "gl/data/gl_vertexbuffer.h"


//===========================================================================
// 
//	Screen wipes
//
//===========================================================================

// TYPES -------------------------------------------------------------------

class OpenGLFrameBuffer::Wiper_Crossfade : public OpenGLFrameBuffer::Wiper
{
public:
	Wiper_Crossfade();
	bool Run(int ticks, OpenGLFrameBuffer *fb);

private:
	int Clock;
};

class OpenGLFrameBuffer::Wiper_Melt : public OpenGLFrameBuffer::Wiper
{
public:
	Wiper_Melt();
	int MakeVBO(int ticks, OpenGLFrameBuffer *fb, bool &done);
	bool Run(int ticks, OpenGLFrameBuffer *fb);

private:
	static const int WIDTH = 320, HEIGHT = 200;
	int y[WIDTH];
};

class OpenGLFrameBuffer::Wiper_Burn : public OpenGLFrameBuffer::Wiper
{
public:
	Wiper_Burn();
	~Wiper_Burn();
	bool Run(int ticks, OpenGLFrameBuffer *fb);

private:
	static const int WIDTH = 64, HEIGHT = 64;
	uint8_t BurnArray[WIDTH * (HEIGHT + 5)];
	FHardwareTexture *BurnTexture;
	int Density;
	int BurnTime;
};

//==========================================================================
//
// OpenGLFrameBuffer :: WipeStartScreen
//
// Called before the current screen has started rendering. This needs to
// save what was drawn the previous frame so that it can be animated into
// what gets drawn this frame.
//
//==========================================================================

bool OpenGLFrameBuffer::WipeStartScreen(int type)
{
	switch (type)
	{
	case wipe_Burn:
		ScreenWipe = new Wiper_Burn;
		break;

	case wipe_Fade:
		ScreenWipe = new Wiper_Crossfade;
		break;

	case wipe_Melt:
		ScreenWipe = new Wiper_Melt;
		break;

	default:
		return false;
	}

	const auto &viewport = screen->mScreenViewport;
	wipestartscreen = new FHardwareTexture(true);
	wipestartscreen->CreateTexture(NULL, viewport.width, viewport.height, 0, false, 0, "WipeStartScreen");
	GLRenderer->mSamplerManager->Bind(0, CLAMP_NOFILTER, -1);
	GLRenderer->mSamplerManager->Bind(1, CLAMP_NONE, -1);
	glFinish();
	wipestartscreen->Bind(0, false, false);

	const auto copyPixels = [&viewport]()
	{
		glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, viewport.left, viewport.top, viewport.width, viewport.height);
	};

	GLRenderer->mBuffers->BindCurrentFB();
	copyPixels();

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	return true;
}

//==========================================================================
//
// OpenGLFrameBuffer :: WipeEndScreen
//
// The screen we want to animate to has just been drawn.
//
//==========================================================================

void OpenGLFrameBuffer::WipeEndScreen()
{
	GLRenderer->Flush();
	const auto &viewport = screen->mScreenViewport;
	wipeendscreen = new FHardwareTexture(true);
	wipeendscreen->CreateTexture(NULL, viewport.width, viewport.height, 0, false, 0, "WipeEndScreen");
	GLRenderer->mSamplerManager->Bind(0, CLAMP_NOFILTER, -1);
	glFinish();
	wipeendscreen->Bind(0, false, false);

	GLRenderer->mBuffers->BindCurrentFB();

	glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, viewport.left, viewport.top, viewport.width, viewport.height);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

//==========================================================================
//
// OpenGLFrameBuffer :: WipeDo
//
// Perform the actual wipe animation. The number of tics since the last
// time this function was called is passed in. Returns true when the wipe
// is over. The first time this function has been called, the screen is
// still locked from before and EndScene() still has not been called.
// Successive times need to call BeginScene().
//
//==========================================================================

bool OpenGLFrameBuffer::WipeDo(int ticks)
{
	bool done = true;
	// Sanity checks.
	if (wipestartscreen != nullptr && wipeendscreen != nullptr)
	{
		gl_RenderState.EnableTexture(true);
		gl_RenderState.EnableFog(false);
		glDisable(GL_DEPTH_TEST);
		glDepthMask(false);

		GLRenderer->mBuffers->BindCurrentFB();
		const auto &bounds = screen->mScreenViewport;
		glViewport(bounds.left, bounds.top, bounds.width, bounds.height);

		done = ScreenWipe->Run(ticks, this);
		glDepthMask(true);
	}
	gl_RenderState.SetVertexBuffer(GLRenderer->mVBO);
	return done;
}

//==========================================================================
//
// OpenGLFrameBuffer :: WipeCleanup
//
// Release any resources that were specifically created for the wipe.
//
//==========================================================================

void OpenGLFrameBuffer::WipeCleanup()
{
	if (ScreenWipe != NULL)
	{
		delete ScreenWipe;
		ScreenWipe = NULL;
	}
	if (wipestartscreen != NULL)
	{
		delete wipestartscreen;
		wipestartscreen = NULL;
	}
	if (wipeendscreen != NULL)
	{
		delete wipeendscreen;
		wipeendscreen = NULL;
	}
	gl_RenderState.ClearLastMaterial();
}

//==========================================================================
//
// OpenGLFrameBuffer :: Wiper Constructor
//
//==========================================================================
OpenGLFrameBuffer::Wiper::Wiper()
{
	mVertexBuf = new FSimpleVertexBuffer;
}

OpenGLFrameBuffer::Wiper::~Wiper()
{
	delete mVertexBuf;
}

void OpenGLFrameBuffer::Wiper::MakeVBO(OpenGLFrameBuffer *fb)
{
	FSimpleVertex make[4];
	FSimpleVertex *ptr = make;

	float ur = fb->GetWidth() / FHardwareTexture::GetTexDimension(fb->GetWidth());
	float vb = fb->GetHeight() / FHardwareTexture::GetTexDimension(fb->GetHeight());

	ptr->Set(0, 0, 0, 0, vb);
	ptr++;
	ptr->Set(0, fb->GetHeight(), 0, 0, 0);
	ptr++;
	ptr->Set(fb->GetWidth(), 0, 0, ur, vb);
	ptr++;
	ptr->Set(fb->GetWidth(), fb->GetHeight(), 0, ur, 0);
	mVertexBuf->set(make, 4);
}

// WIPE: CROSSFADE ---------------------------------------------------------

//==========================================================================
//
// OpenGLFrameBuffer :: Wiper_Crossfade Constructor
//
//==========================================================================

OpenGLFrameBuffer::Wiper_Crossfade::Wiper_Crossfade()
: Clock(0)
{
}

//==========================================================================
//
// OpenGLFrameBuffer :: Wiper_Crossfade :: Run
//
// Fades the old screen into the new one over 32 ticks.
//
//==========================================================================

bool OpenGLFrameBuffer::Wiper_Crossfade::Run(int ticks, OpenGLFrameBuffer *fb)
{
	Clock += ticks;

	MakeVBO(fb);

	gl_RenderState.SetTextureMode(TM_OPAQUE);
	gl_RenderState.AlphaFunc(GL_GEQUAL, 0.f);
	gl_RenderState.ResetColor();
	gl_RenderState.Apply();
	fb->wipestartscreen->Bind(0, 0, false);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	float a = clamp(Clock / 32.f, 0.f, 1.f);
	gl_RenderState.SetColorAlpha(0xffffff, a);
	gl_RenderState.Apply();
	fb->wipeendscreen->Bind(0, 0, false);
	mVertexBuf->EnableColorArray(false);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	gl_RenderState.AlphaFunc(GL_GEQUAL, 0.5f);
	gl_RenderState.SetTextureMode(TM_MODULATE);

	return Clock >= 32;
}

//==========================================================================
//
// OpenGLFrameBuffer :: Wiper_Melt Constructor
//
//==========================================================================

OpenGLFrameBuffer::Wiper_Melt::Wiper_Melt()
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
// OpenGLFrameBuffer :: Wiper_Melt :: Run
//
// Fades the old screen into the new one over 32 ticks.
//
//==========================================================================

int OpenGLFrameBuffer::Wiper_Melt::MakeVBO(int ticks, OpenGLFrameBuffer *fb, bool &done)
{
	FSimpleVertex *make = new FSimpleVertex[321*4];
	FSimpleVertex *ptr = make;
	int dy;

	float ur = fb->GetWidth() / FHardwareTexture::GetTexDimension(fb->GetWidth());
	float vb = fb->GetHeight() / FHardwareTexture::GetTexDimension(fb->GetHeight());

	ptr->Set(0, 0, 0, 0, vb);
	ptr++;
	ptr->Set(0, fb->GetHeight(), 0, 0, 0);
	ptr++;
	ptr->Set(fb->GetWidth(), 0, 0, ur, vb);
	ptr++;
	ptr->Set(fb->GetWidth(), fb->GetHeight(), 0, ur, 0);
	ptr++;

	// Copy the old screen in vertical strips on top of the new one.
	while (ticks--)
	{
		done = true;
		for (int i = 0; i < WIDTH; i++)
		{
			if (y[i] < 0)
			{
				y[i]++;
				done = false;
			}
			else if (y[i] < HEIGHT)
			{
				dy = (y[i] < 16) ? y[i] + 1 : 8;
				y[i] = MIN(y[i] + dy, HEIGHT);
				done = false;
			}
			if (ticks == 0)
			{
				struct {
					int32_t x;
					int32_t y;
				} dpt;
				struct {
					int32_t left;
					int32_t top;
					int32_t right;
					int32_t bottom;
				} rect;

				// Only draw for the final tick.
				// No need for optimization. Wipes won't ever be drawn with anything else.

				dpt.x = i * fb->GetWidth() / WIDTH;
				dpt.y = MAX(0, y[i] * fb->GetHeight() / HEIGHT);
				rect.left = dpt.x;
				rect.top = 0;
				rect.right = (i + 1) * fb->GetWidth() / WIDTH;
				rect.bottom = fb->GetHeight() - dpt.y;
				if (rect.bottom > rect.top)
				{
					float tw = (float)FHardwareTexture::GetTexDimension(fb->GetWidth());
					float th = (float)FHardwareTexture::GetTexDimension(fb->GetHeight());
					rect.bottom = fb->GetHeight() - rect.bottom;
					rect.top = fb->GetHeight() - rect.top;

					ptr->Set(rect.left, rect.bottom, 0, rect.left / tw, rect.top / th);
					ptr++;
					ptr->Set(rect.left, rect.top, 0, rect.left / tw, rect.bottom / th);
					ptr++;
					ptr->Set(rect.right, rect.bottom, 0, rect.right / tw, rect.top / th);
					ptr++;
					ptr->Set(rect.right, rect.top, 0, rect.right / tw, rect.bottom / th);
					ptr++;
				}
			}
		}
	}
	int numverts = int(ptr - make);
	mVertexBuf->set(make, numverts);
	delete[] make;
	return numverts;
}

bool OpenGLFrameBuffer::Wiper_Melt::Run(int ticks, OpenGLFrameBuffer *fb)
{
	bool done = false;
	int maxvert = MakeVBO(ticks, fb, done);

	// Draw the new screen on the bottom.
	gl_RenderState.SetTextureMode(TM_OPAQUE);
	gl_RenderState.ResetColor();
	gl_RenderState.Apply();
	fb->wipeendscreen->Bind(0, 0, false);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	fb->wipestartscreen->Bind(0, 0, false);
	gl_RenderState.SetTextureMode(TM_MODULATE);
	for (int i = 4; i < maxvert; i += 4)
	{
		glDrawArrays(GL_TRIANGLE_STRIP, i, 4);
	}
	return done;
}

//==========================================================================
//
// OpenGLFrameBuffer :: Wiper_Burn Constructor
//
//==========================================================================

OpenGLFrameBuffer::Wiper_Burn::Wiper_Burn()
{
	Density = 4;
	BurnTime = 0;
	memset(BurnArray, 0, sizeof(BurnArray));
	BurnTexture = NULL;
}

//==========================================================================
//
// OpenGLFrameBuffer :: Wiper_Burn Destructor
//
//==========================================================================

OpenGLFrameBuffer::Wiper_Burn::~Wiper_Burn()
{
	if (BurnTexture != NULL)
	{
		delete BurnTexture;
		BurnTexture = NULL;
	}
}

//==========================================================================
//
// OpenGLFrameBuffer :: Wiper_Burn :: Run
//
//==========================================================================

bool OpenGLFrameBuffer::Wiper_Burn::Run(int ticks, OpenGLFrameBuffer *fb)
{
	bool done;

	MakeVBO(fb);

	BurnTime += ticks;
	ticks *= 2;

	// Make the fire burn
	done = false;
	while (!done && ticks--)
	{
		Density = wipe_CalcBurn(BurnArray, WIDTH, HEIGHT, Density);
		done = (Density < 0);
	}

	if (BurnTexture != NULL) delete BurnTexture;
	BurnTexture = new FHardwareTexture(true);

	// Update the burn texture with the new burn data
	uint8_t rgb_buffer[WIDTH*HEIGHT*4];

	const uint8_t *src = BurnArray;
	uint32_t *dest = (uint32_t *)rgb_buffer;
	for (int y = HEIGHT; y != 0; --y)
	{
		for (int x = WIDTH; x != 0; --x)
		{
			uint8_t s = clamp<int>((*src++)*2, 0, 255);
			*dest++ = MAKEARGB(s,255,255,255);
		}
	}

	float ur = fb->GetWidth() / FHardwareTexture::GetTexDimension(fb->GetWidth());
	float vb = fb->GetHeight() / FHardwareTexture::GetTexDimension(fb->GetHeight());


	// Put the initial screen back to the buffer.
	gl_RenderState.SetTextureMode(TM_OPAQUE);
	gl_RenderState.AlphaFunc(GL_GEQUAL, 0.f);
	gl_RenderState.ResetColor();
	gl_RenderState.Apply();
	fb->wipestartscreen->Bind(0, 0, false);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	gl_RenderState.SetTextureMode(TM_MODULATE);
	gl_RenderState.SetEffect(EFF_BURN);
	gl_RenderState.ResetColor();
	gl_RenderState.Apply();

	// Burn the new screen on top of it.
	fb->wipeendscreen->Bind(0, 0, false);

	BurnTexture->CreateTexture(rgb_buffer, WIDTH, HEIGHT, 1, true, 0, "BurnTexture");

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	gl_RenderState.SetEffect(EFF_NONE);

	// The fire may not always stabilize, so the wipe is forced to end
	// after an arbitrary maximum time.
	return done || (BurnTime > 40);
}
