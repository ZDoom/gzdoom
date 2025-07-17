/*
** wipe.cpp
** Screen wipe implementation
**
**---------------------------------------------------------------------------
** Copyright 1998-2016 Randy Heit
** Copyright 2005-2022 Christoph Oelckers
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

#include "v_video.h"
#include "m_random.h"
#include "wipe.h"

#include "bitmap.h"
#include "hw_material.h"
#include "v_draw.h"
#include "s_soundinternal.h"
#include "i_time.h"

EXTERN_CVAR(Bool, cl_capfps)

class FBurnTexture : public FTexture
{
	TArray<uint32_t> WorkBuffer;
public:
	FBurnTexture(int w, int h)
		: WorkBuffer(w*h, true)
	{
		Width = w;
		Height = h;
	}
	
	FBitmap GetBgraBitmap(const PalEntry*, int *trans) override
	{
		FBitmap bmp;
		bmp.Create(Width, Height);
		bmp.CopyPixelDataRGB(0, 0, (uint8_t*)WorkBuffer.Data(), Width, Height, 4, Width*4, 0, CF_RGBA, nullptr);
		if (trans) *trans = 0;
		return bmp;
	}
	
	uint32_t *GetBuffer()
	{
		return WorkBuffer.Data();
	}
};

int wipe_CalcBurn (uint8_t *burnarray, int width, int height, int density)
{
	// This is a modified version of the fire that was once used
	// on the player setup menu.
	static int voop;

	int a, b;
	uint8_t *from;

	// generator
	from = &burnarray[width * height];
	b = voop;
	voop += density / 3;
	for (a = 0; a < density/8; a++)
	{
		unsigned int offs = (a+b) & (width - 1);
		unsigned int v = M_Random();
		v = min(from[offs] + 4 + (v & 15) + (v >> 3) + (M_Random() & 31), 255u);
		from[offs] = from[width*2 + ((offs + width*3/2) & (width - 1))] = v;
	}

	density = min(density + 10, width * 7);

	from = burnarray;
	for (b = 0; b <= height; b += 2)
	{
		uint8_t *pixel = from;

		// special case: first pixel on line
		uint8_t *p = pixel + (width << 1);
		unsigned int top = *p + *(p + width - 1) + *(p + 1);
		unsigned int bottom = *(pixel + (width << 2));
		unsigned int c1 = (top + bottom) >> 2;
		if (c1 > 1) c1--;
		*pixel = c1;
		*(pixel + width) = (c1 + bottom) >> 1;
		pixel++;

		// main line loop
		for (a = 1; a < width-1; a++)
		{
			// sum top pixels
			p = pixel + (width << 1);
			top = *p + *(p - 1) + *(p + 1);

			// bottom pixel
			bottom = *(pixel + (width << 2));

			// combine pixels
			c1 = (top + bottom) >> 2;
			if (c1 > 1) c1--;

			// store pixels
			*pixel = c1;
			*(pixel + width) = (c1 + bottom) >> 1;		// interpolate

			// next pixel
			pixel++;
		}

		// special case: last pixel on line
		p = pixel + (width << 1);
		top = *p + *(p - 1) + *(p - width + 1);
		bottom = *(pixel + (width << 2));
		c1 = (top + bottom) >> 2;
		if (c1 > 1) c1--;
		*pixel = c1;
		*(pixel + width) = (c1 + bottom) >> 1;

		// next line
		from += width << 1;
	}

	// Check for done-ness. (Every pixel with level 126 or higher counts as done.)
	for (a = width * height, from = burnarray; a != 0; --a, ++from)
	{
		if (*from < 126)
		{
			return density;
		}
	}
	return -1;
}


// TYPES -------------------------------------------------------------------

class Wiper
{
protected:
	FGameTexture* startScreen = nullptr, * endScreen = nullptr;
public:
	virtual ~Wiper();
	virtual bool Run(int ticks) = 0;
	virtual bool RunInterpolated(double ticks) { return true; };
	virtual bool Interpolatable() { return false; }
	virtual void SetTextures(FGameTexture* startscreen, FGameTexture* endscreen)
	{
		startScreen = startscreen;
		endScreen = endscreen;
	}

	static Wiper* Create(int type);
};


class Wiper_Crossfade : public Wiper
{
public:
	bool Run(int ticks) override;
	bool RunInterpolated(double ticks) override;
	bool Interpolatable() override { return true; }
	
private:
	float Clock = 0;
};

class Wiper_Melt : public Wiper
{
public:
	Wiper_Melt();
	bool Run(int ticks) override;
	bool RunInterpolated(double ticks) override;
	bool Interpolatable() override { return true; }
	
private:
	enum { WIDTH = 320, HEIGHT = 200 };
	double y[WIDTH];
};

class Wiper_Burn : public Wiper
{
public:
	~Wiper_Burn();
	bool Run(int ticks) override;
	void SetTextures(FGameTexture *startscreen, FGameTexture *endscreen) override;

private:
	static const int WIDTH = 64, HEIGHT = 64;
	uint8_t BurnArray[WIDTH * (HEIGHT + 5)] = {0};
	FBurnTexture *BurnTexture = nullptr;
	int Density = 4;
	int BurnTime = 8;
};

//===========================================================================
//
//    Screen wipes
//
//===========================================================================

Wiper *Wiper::Create(int type)
{
	switch(type)
	{
		case wipe_Burn:
			return new Wiper_Burn;
			
		case wipe_Fade:
			return new Wiper_Crossfade;
			
		case wipe_Melt:
			return new Wiper_Melt;
			
		default:
			return nullptr;
	}
}




//==========================================================================
//
// OpenGLFrameBuffer :: WipeCleanup
//
// Release any resources that were specifically created for the wipe.
//
//==========================================================================

Wiper::~Wiper()
{
	if (startScreen != nullptr) delete startScreen;
	if (endScreen != nullptr) delete endScreen;
}

//==========================================================================
//
// WIPE: CROSSFADE ---------------------------------------------------------
//
//==========================================================================

//==========================================================================
//
// OpenGLFrameBuffer :: Wiper_Crossfade :: Run
//
// Fades the old screen into the new one over 32 ticks.
//
//==========================================================================

bool Wiper_Crossfade::Run(int ticks)
{
	Clock += ticks;
	DrawTexture(twod, startScreen, 0, 0, DTA_FlipY, screen->RenderTextureIsFlipped(), DTA_Masked, false, TAG_DONE);
	DrawTexture(twod, endScreen, 0, 0, DTA_FlipY, screen->RenderTextureIsFlipped(), DTA_Masked, false, DTA_Alpha, clamp(Clock / 32.f, 0.f, 1.f), TAG_DONE);
	return Clock >= 32.;
}

//==========================================================================
//
// OpenGLFrameBuffer :: Wiper_Crossfade :: Run
//
// Fades the old screen into the new one over 32 ticks.
//
//==========================================================================

bool Wiper_Crossfade::RunInterpolated(double ticks)
{
	Clock += ticks;
	DrawTexture(twod, startScreen, 0, 0, DTA_FlipY, screen->RenderTextureIsFlipped(), DTA_Masked, false, TAG_DONE);
	DrawTexture(twod, endScreen, 0, 0, DTA_FlipY, screen->RenderTextureIsFlipped(), DTA_Masked, false, DTA_Alpha, clamp(Clock / 32.f, 0.f, 1.f), TAG_DONE);
	return Clock >= 32.;
}

//==========================================================================
//
// OpenGLFrameBuffer :: Wiper_Melt Constructor
//
//==========================================================================

Wiper_Melt::Wiper_Melt()
{
	y[0] = -(M_Random() & 15);
	for (int i = 1; i < WIDTH; ++i)
	{
		y[i] = clamp(y[i-1] + (double)(M_Random() % 3) - 1., -15., 0.);
	}
}

//==========================================================================
//
// OpenGLFrameBuffer :: Wiper_Melt :: Run
//
// Melts the old screen into the new one over 32 ticks.
//
//==========================================================================

bool Wiper_Melt::Run(int ticks)
{
	bool done = false;
	DrawTexture(twod, endScreen, 0, 0, DTA_FlipY, screen->RenderTextureIsFlipped(), DTA_Masked, false,  TAG_DONE);
	
	// Copy the old screen in vertical strips on top of the new one.
	while (ticks--)
	{
		done = true;
		for (int i = 0; i < WIDTH; i++)
		{
			if (y[i] < HEIGHT)
			{
				if (y[i] < 0.)
					y[i] = y[i] + 1.;
				else if (y[i] < 16.)
					y[i] += y[i] + 1.;
				else
					y[i] = min<double>(y[i] + 8., HEIGHT);
				done = false;
			}
			if (ticks == 0)
			{
				struct {
					int32_t x;
					double y;
				} dpt;
				struct {
					int32_t left;
					double top;
					int32_t right;
					double bottom;
				} rect;
				
				// Only draw for the final tick.
				
				int w = startScreen->GetTexelWidth();
				int h = startScreen->GetTexelHeight();
				dpt.x = i * w / WIDTH;
				dpt.y = max(0., y[i] * (double)h / (double)HEIGHT);
				rect.left = dpt.x;
				rect.top = 0;
				rect.right = (i + 1) * w / WIDTH;
				rect.bottom = h - dpt.y;
				if (rect.bottom > rect.top)
				{
					DrawTexture(twod, startScreen, 0, dpt.y, DTA_FlipY, screen->RenderTextureIsFlipped(), DTA_ClipLeft, rect.left, DTA_ClipRight, rect.right, DTA_Masked, false, TAG_DONE);
				}
			}
		}
	}
	return done;
}

//==========================================================================
//
// Wiper_Melt :: RunInterpolated
//
// Melts the old screen into the new one over 32 ticks (interpolated).
//
//==========================================================================

bool Wiper_Melt::RunInterpolated(double ticks)
{
	bool done = false;
	DrawTexture(twod, endScreen, 0, 0, DTA_FlipY, screen->RenderTextureIsFlipped(), DTA_Masked, false, TAG_DONE);

	// Copy the old screen in vertical strips on top of the new one.
	while (ticks > 0.)
	{
		done = true;
		for (int i = 0; i < WIDTH; i++)
		{
			if (y[i] < (double)HEIGHT)
			{
				if (ticks > 0. && ticks < 1.)
				{
					if (y[i] < 0)
						y[i] += ticks;
					else if (y[i] < 16)
						y[i] += (y[i] + 1) * ticks;
					else
						y[i] = min<double>(y[i] + (8 * ticks), (double)HEIGHT);
				}
				else if (y[i] < 0.)
					y[i] = y[i] + 1.;
				else if (y[i] < 16.)
					y[i] += y[i] + 1.;
				else
					y[i] = min<double>(y[i] + 8., HEIGHT);
				done = false;
			}
		}
		ticks -= 1.;
	}
	for (int i = 0; i < WIDTH; i++)
	{
		struct {
			int32_t x;
			double y;
		} dpt;
		struct {
			int32_t left;
			double top;
			int32_t right;
			double bottom;
		} rect;

		// Only draw for the final tick.
		int w = startScreen->GetTexelWidth();
		double h = startScreen->GetTexelHeight();
		dpt.x = i * w / WIDTH;
		dpt.y = max(0., y[i] * (double)h / (double)HEIGHT);
		rect.left = dpt.x;
		rect.top = 0;
		rect.right = (i + 1) * w / WIDTH;
		rect.bottom = h - dpt.y;
		if (rect.bottom > rect.top)
		{
			DrawTexture(twod, startScreen, 0, dpt.y, DTA_FlipY, screen->RenderTextureIsFlipped(), DTA_ClipLeft, rect.left, DTA_ClipRight, rect.right, DTA_Masked, false, TAG_DONE);
		}
	}
	return done;
}

//==========================================================================
//
// OpenGLFrameBuffer :: Wiper_Burn Constructor
//
//==========================================================================

void Wiper_Burn::SetTextures(FGameTexture *startscreen, FGameTexture *endscreen)
{
	startScreen = startscreen;
	endScreen = endscreen;
	BurnTexture = new FBurnTexture(WIDTH, HEIGHT);
	auto mat = FMaterial::ValidateTexture(endScreen, false);
	mat->ClearLayers();
	mat->AddTextureLayer(BurnTexture, false);
}

//==========================================================================
//
// OpenGLFrameBuffer :: Wiper_Burn Destructor
//
//==========================================================================

Wiper_Burn::~Wiper_Burn()
{
	if (BurnTexture != nullptr)	delete BurnTexture;
}

//==========================================================================
//
// OpenGLFrameBuffer :: Wiper_Burn :: Run
//
//==========================================================================

bool Wiper_Burn::Run(int ticks)
{
	bool done = false;

	
	BurnTime += ticks;
	ticks *= 2;
	
	// Make the fire burn
	while (!done && ticks--)
	{
		Density = wipe_CalcBurn(BurnArray, WIDTH, HEIGHT, Density);
		done = (Density < 0);
	}

	BurnTexture->CleanHardwareTextures();
	endScreen->CleanHardwareData(false);	// this only cleans the descriptor sets for the Vulkan backend. We do not want to delete the wipe screen's hardware texture here.

	const uint8_t *src = BurnArray;
	uint32_t *dest = (uint32_t *)BurnTexture->GetBuffer();
	for (int y = HEIGHT; y != 0; --y)
	{
		for (int x = WIDTH; x != 0; --x)
		{
			uint8_t s = clamp<int>((*src++)*2, 0, 255);
			*dest++ = MAKEARGB(s,255,255,255);
		}
	}

	DrawTexture(twod, startScreen, 0, 0, DTA_FlipY, screen->RenderTextureIsFlipped(), DTA_Masked, false, TAG_DONE);
	DrawTexture(twod, endScreen, 0, 0, DTA_FlipY, screen->RenderTextureIsFlipped(), DTA_Burn, true, DTA_Masked, false, TAG_DONE);
	
	// The fire may not always stabilize, so the wipe is forced to end
	// after an arbitrary maximum time.
	return done || (BurnTime > 40);
}


void PerformWipe(FTexture* startimg, FTexture* endimg, int wipe_type, bool stopsound, std::function<void()> overlaydrawer)
{
	// wipe update
	uint64_t wipestart, nowtime, diff;
	double diff_frac;
	bool done;

	GSnd->SetSfxPaused(true, 1);
	I_FreezeTime(true);
	twod->End();
	assert(startimg != nullptr && endimg != nullptr);
	auto starttex = MakeGameTexture(startimg, nullptr, ETextureType::SWCanvas);
	auto endtex = MakeGameTexture(endimg, nullptr, ETextureType::SWCanvas);
	auto wiper = Wiper::Create(wipe_type);
	wiper->SetTextures(starttex, endtex);

	wipestart = I_msTime();

	do
	{
		if (wiper->Interpolatable() && !cl_capfps)
		{
			nowtime = I_msTime();
			diff_frac = (nowtime - wipestart) * 40. / 1000.;	// Using 35 here feels too slow.
			wipestart = nowtime;
			twod->Begin(screen->GetWidth(), screen->GetHeight());
			done = wiper->RunInterpolated(diff_frac);
			if (overlaydrawer) overlaydrawer();
			twod->End();
			screen->Update();
			twod->OnFrameDone();
		}
		else
		{
			do
			{
				I_WaitVBL(2);
				nowtime = I_msTime();
				diff = (nowtime - wipestart) * 40 / 1000;	// Using 35 here feels too slow.
			} while (diff < 1);
			wipestart = nowtime;
			twod->Begin(screen->GetWidth(), screen->GetHeight());
			done = wiper->Run(1);
			if (overlaydrawer) overlaydrawer();
			twod->End();
			screen->Update();
			twod->OnFrameDone();
		}
	} while (!done);
	delete wiper;
	I_FreezeTime(false);
	GSnd->SetSfxPaused(false, 1);

}
