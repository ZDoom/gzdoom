//-----------------------------------------------------------------------------
//
// Copyright 1993-1996 id Software
// Copyright 1999-2016 Randy Heit
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//		Mission begin melt/wipe screen special effect.
//
//-----------------------------------------------------------------------------

#include "v_video.h"
#include "m_random.h"
#include "f_wipe.h"
#include "templates.h"
#include "textures/bitmap.h"
#include "hwrenderer/textures/hw_material.h"

class FBurnTexture : public FTexture
{
	uint32_t *WorkBuffer;
public:
	FBurnTexture(int w, int h)
	{
		Width = w;
		Height = h;
		WorkBuffer = new uint32_t[w * h];
	}
	
	~FBurnTexture()
	{
		delete [] WorkBuffer;
	}
	
	int CopyTrueColorPixels(FBitmap *bmp, int x, int y, int rotate, FCopyInfo *inf) override
	{
		bmp->CopyPixelDataRGB(x, y, (uint8_t*)WorkBuffer, Width, Height, 4, Width*4, rotate, CF_RGBA, inf);
		return 0;
	}
	
	uint32_t *GetBuffer()
	{
		return WorkBuffer;
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
		v = MIN(from[offs] + 4 + (v & 15) + (v >> 3) + (M_Random() & 31), 255u);
		from[offs] = from[width*2 + ((offs + width*3/2) & (width - 1))] = v;
	}

	density = MIN(density + 10, width * 7);

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

class Wiper_Crossfade : public Wiper
{
public:
	bool Run(int ticks) override;
	
private:
	int Clock = 0;
};

class Wiper_Melt : public Wiper
{
public:
	Wiper_Melt();
	bool Run(int ticks) override;
	
private:
	static const int WIDTH = 320, HEIGHT = 200;
	int y[WIDTH];
};

class Wiper_Burn : public Wiper
{
public:
	~Wiper_Burn();
	bool Run(int ticks) override;
	void SetTextures(FTexture *startscreen, FTexture *endscreen) override;

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
	screen->DrawTexture(startScreen, 0, 0, DTA_FlipY, screen->RenderTextureIsFlipped(), DTA_Masked, false, TAG_DONE);
	screen->DrawTexture(endScreen, 0, 0, DTA_FlipY, screen->RenderTextureIsFlipped(), DTA_Masked, false, DTA_Alpha, clamp(Clock / 32.f, 0.f, 1.f), TAG_DONE);
	return Clock >= 32;
}

//==========================================================================
//
// OpenGLFrameBuffer :: Wiper_Melt Constructor
//
//==========================================================================

Wiper_Melt::Wiper_Melt()
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
// Melts the old screen into the new one over 32 ticks.
//
//==========================================================================

bool Wiper_Melt::Run(int ticks)
{
	bool done;
	screen->DrawTexture(endScreen, 0, 0, DTA_FlipY, screen->RenderTextureIsFlipped(), DTA_Masked, false,  TAG_DONE);
	
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
				int dy = (y[i] < 16) ? y[i] + 1 : 8;
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
				
				int w = startScreen->GetWidth();
				int h = startScreen->GetHeight();
				dpt.x = i * w / WIDTH;
				dpt.y = MAX(0, y[i] * h / HEIGHT);
				rect.left = dpt.x;
				rect.top = 0;
				rect.right = (i + 1) * w / WIDTH;
				rect.bottom = h - dpt.y;
				if (rect.bottom > rect.top)
				{
					screen->DrawTexture(startScreen, 0, dpt.y, DTA_FlipY, screen->RenderTextureIsFlipped(), DTA_ClipLeft, rect.left, DTA_ClipRight, rect.right, DTA_Masked, false, TAG_DONE);
				}
			}
		}
	}
	return done;
}

//==========================================================================
//
// OpenGLFrameBuffer :: Wiper_Burn Constructor
//
//==========================================================================

void Wiper_Burn::SetTextures(FTexture *startscreen, FTexture *endscreen)
{
	startScreen = startscreen;
	endScreen = endscreen;
	BurnTexture = new FBurnTexture(WIDTH, HEIGHT);
	auto mat = FMaterial::ValidateTexture(startscreen, false);
	mat->AddTextureLayer(BurnTexture);
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
	
	auto mat = FMaterial::ValidateTexture(BurnTexture, false);
	mat->Clean(true);
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

	screen->DrawTexture(startScreen, 0, 0, DTA_FlipY, screen->RenderTextureIsFlipped(), DTA_Masked, false, TAG_DONE);
	screen->DrawTexture(endScreen, 0, 0, DTA_FlipY, screen->RenderTextureIsFlipped(), DTA_Burn, true, DTA_Masked, false, TAG_DONE);
	
	// The fire may not always stabilize, so the wipe is forced to end
	// after an arbitrary maximum time.
	return done || (BurnTime > 40);
}
