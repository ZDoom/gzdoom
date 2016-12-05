// Emacs style mode select	 -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// DESCRIPTION:
//		System specific interface stuff.
//
//-----------------------------------------------------------------------------


#ifndef __R_DRAW_RGBA__
#define __R_DRAW_RGBA__

#include "r_draw.h"
#include "v_palette.h"
#include "r_thread.h"

#ifndef NO_SSE
#include <immintrin.h>
#endif

struct FSpecialColormap;

EXTERN_CVAR(Bool, r_mipmap)
EXTERN_CVAR(Float, r_lod_bias)

namespace swrenderer       
{

/////////////////////////////////////////////////////////////////////////////
// Drawer commands:

class ApplySpecialColormapRGBACommand : public DrawerCommand
{
	BYTE *buffer;
	int pitch;
	int width;
	int height;
	int start_red;
	int start_green;
	int start_blue;
	int end_red;
	int end_green;
	int end_blue;

public:
	ApplySpecialColormapRGBACommand(FSpecialColormap *colormap, DFrameBuffer *screen);
	void Execute(DrawerThread *thread) override;
	FString DebugInfo() override { return "ApplySpecialColormapRGBACommand"; }
};

template<typename CommandType, typename BlendMode>
class DrawerBlendCommand : public CommandType
{
public:
	void Execute(DrawerThread *thread) override
	{
		typename CommandType::LoopIterator loop(this, thread);
		if (!loop) return;
		BlendMode blend(*this, loop);
		do
		{
			blend.Blend(*this, loop);
		} while (loop.next());
	}
};

/////////////////////////////////////////////////////////////////////////////
// Pixel shading inline functions:

// Give the compiler a strong hint we want these functions inlined:
#ifndef FORCEINLINE
#if defined(_MSC_VER)
#define FORCEINLINE __forceinline
#elif defined(__GNUC__)
#define FORCEINLINE __attribute__((always_inline)) inline
#else
#define FORCEINLINE inline
#endif
#endif

// Promise compiler we have no aliasing of this pointer
#ifndef RESTRICT
#if defined(_MSC_VER)
#define RESTRICT __restrict
#elif defined(__GNUC__)
#define RESTRICT __restrict__
#else
#define RESTRICT
#endif
#endif

class LightBgra
{
public:
	// calculates the light constant passed to the shade_pal_index function
	FORCEINLINE static uint32_t calc_light_multiplier(dsfixed_t light)
	{
		return 256 - (light >> (FRACBITS - 8));
	}

	// Calculates a ARGB8 color for the given palette index and light multiplier
	FORCEINLINE static uint32_t shade_pal_index_simple(uint32_t index, uint32_t light)
	{
		const PalEntry &color = GPalette.BaseColors[index];
		uint32_t red = color.r;
		uint32_t green = color.g;
		uint32_t blue = color.b;

		red = red * light / 256;
		green = green * light / 256;
		blue = blue * light / 256;

		return 0xff000000 | (red << 16) | (green << 8) | blue;
	}

	// Calculates a ARGB8 color for the given palette index, light multiplier and dynamic colormap
	FORCEINLINE static uint32_t shade_pal_index(uint32_t index, uint32_t light, const ShadeConstants &constants)
	{
		const PalEntry &color = GPalette.BaseColors[index];
		uint32_t alpha = color.d & 0xff000000;
		uint32_t red = color.r;
		uint32_t green = color.g;
		uint32_t blue = color.b;
		if (constants.simple_shade)
		{
			red = red * light / 256;
			green = green * light / 256;
			blue = blue * light / 256;
		}
		else
		{
			uint32_t inv_light = 256 - light;
			uint32_t inv_desaturate = 256 - constants.desaturate;

			uint32_t intensity = ((red * 77 + green * 143 + blue * 37) >> 8) * constants.desaturate;

			red = (red * inv_desaturate + intensity) / 256;
			green = (green * inv_desaturate + intensity) / 256;
			blue = (blue * inv_desaturate + intensity) / 256;

			red = (constants.fade_red * inv_light + red * light) / 256;
			green = (constants.fade_green * inv_light + green * light) / 256;
			blue = (constants.fade_blue * inv_light + blue * light) / 256;

			red = (red * constants.light_red) / 256;
			green = (green * constants.light_green) / 256;
			blue = (blue * constants.light_blue) / 256;
		}
		return alpha | (red << 16) | (green << 8) | blue;
	}

	FORCEINLINE static uint32_t shade_bgra_simple(uint32_t color, uint32_t light)
	{
		uint32_t red = RPART(color) * light / 256;
		uint32_t green = GPART(color) * light / 256;
		uint32_t blue = BPART(color) * light / 256;
		return 0xff000000 | (red << 16) | (green << 8) | blue;
	}

	FORCEINLINE static uint32_t shade_bgra(uint32_t color, uint32_t light, const ShadeConstants &constants)
	{
		uint32_t alpha = color & 0xff000000;
		uint32_t red = (color >> 16) & 0xff;
		uint32_t green = (color >> 8) & 0xff;
		uint32_t blue = color & 0xff;
		if (constants.simple_shade)
		{
			red = red * light / 256;
			green = green * light / 256;
			blue = blue * light / 256;
		}
		else
		{
			uint32_t inv_light = 256 - light;
			uint32_t inv_desaturate = 256 - constants.desaturate;

			uint32_t intensity = ((red * 77 + green * 143 + blue * 37) >> 8) * constants.desaturate;

			red = (red * inv_desaturate + intensity) / 256;
			green = (green * inv_desaturate + intensity) / 256;
			blue = (blue * inv_desaturate + intensity) / 256;

			red = (constants.fade_red * inv_light + red * light) / 256;
			green = (constants.fade_green * inv_light + green * light) / 256;
			blue = (constants.fade_blue * inv_light + blue * light) / 256;

			red = (red * constants.light_red) / 256;
			green = (green * constants.light_green) / 256;
			blue = (blue * constants.light_blue) / 256;
		}
		return alpha | (red << 16) | (green << 8) | blue;
	}
};

}

#endif
