/*
** r_draw_rgba.cpp
**
**---------------------------------------------------------------------------
** Copyright 1998-2016 Randy Heit
** Copyright 2016 Magnus Norddahl
** Copyright 2016 Rachael Alexanderson
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
#include <stddef.h>


#include "doomdef.h"

#include "filesystem.h"
#include "v_video.h"
#include "doomstat.h"
#include "st_stuff.h"
#include "g_game.h"
#include "g_level.h"
#include "r_data/r_translate.h"
#include "v_palette.h"
#include "r_data/colormaps.h"
#include "r_draw_rgba.h"
#include "swrenderer/viewport/r_viewport.h"
#include "swrenderer/scene/r_light.h"
#ifdef NO_SSE
#include "r_draw_wall32.h"
#include "r_draw_sprite32.h"
#include "r_draw_span32.h"
#include "r_draw_sky32.h"
#else
#include "r_draw_wall32_sse2.h"
#include "r_draw_sprite32_sse2.h"
#include "r_draw_span32_sse2.h"
#include "r_draw_sky32_sse2.h"
#endif

#include "gi.h"
#include "stats.h"
#include <vector>

;
// Use linear filtering when scaling up
CVAR(Bool, r_magfilter, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG);

// Use linear filtering when scaling down
CVAR(Bool, r_minfilter, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG);

// Use mipmapped textures
CVAR(Bool, r_mipmap, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG);

// Level of detail texture bias
CVAR(Float, r_lod_bias, -1.5, 0); // To do: add CVAR_ARCHIVE | CVAR_GLOBALCONFIG when a good default has been decided

namespace swrenderer
{
	void SWTruecolorDrawers::DrawWall(const WallDrawerArgs &args)
	{
		DrawWallColumns<DrawWall32Command>(args);
	}
	
	void SWTruecolorDrawers::DrawWallMasked(const WallDrawerArgs &args)
	{
		DrawWallColumns<DrawWallMasked32Command>(args);
	}
	
	void SWTruecolorDrawers::DrawWallAdd(const WallDrawerArgs &args)
	{
		DrawWallColumns<DrawWallAddClamp32Command>(args);
	}
	
	void SWTruecolorDrawers::DrawWallAddClamp(const WallDrawerArgs &args)
	{
		DrawWallColumns<DrawWallAddClamp32Command>(args);
	}
	
	void SWTruecolorDrawers::DrawWallSubClamp(const WallDrawerArgs &args)
	{
		DrawWallColumns<DrawWallSubClamp32Command>(args);
	}
	
	void SWTruecolorDrawers::DrawWallRevSubClamp(const WallDrawerArgs &args)
	{
		DrawWallColumns<DrawWallRevSubClamp32Command>(args);
	}
	
	void SWTruecolorDrawers::DrawColumn(const SpriteDrawerArgs &args)
	{
		DrawSprite32Command::DrawColumn(args);
	}

	void SWTruecolorDrawers::FillColumn(const SpriteDrawerArgs &args)
	{
		FillSprite32Command::DrawColumn(args);
	}

	void SWTruecolorDrawers::FillAddColumn(const SpriteDrawerArgs &args)
	{
		FillSpriteAddClamp32Command::DrawColumn(args);
	}

	void SWTruecolorDrawers::FillAddClampColumn(const SpriteDrawerArgs &args)
	{
		FillSpriteAddClamp32Command::DrawColumn(args);
	}

	void SWTruecolorDrawers::FillSubClampColumn(const SpriteDrawerArgs &args)
	{
		FillSpriteSubClamp32Command::DrawColumn(args);
	}

	void SWTruecolorDrawers::FillRevSubClampColumn(const SpriteDrawerArgs &args)
	{
		FillSpriteRevSubClamp32Command::DrawColumn(args);
	}

	void SWTruecolorDrawers::DrawFuzzColumn(const SpriteDrawerArgs &args)
	{
		if (r_fuzzscale)
			DrawScaledFuzzColumn(args);
		else
			DrawUnscaledFuzzColumn(args);
		R_UpdateFuzzPos(args);
	}

	void SWTruecolorDrawers::DrawAddColumn(const SpriteDrawerArgs &args)
	{
		DrawSpriteAddClamp32Command::DrawColumn(args);
	}

	void SWTruecolorDrawers::DrawTranslatedColumn(const SpriteDrawerArgs &args)
	{
		DrawSpriteTranslated32Command::DrawColumn(args);
	}

	void SWTruecolorDrawers::DrawTranslatedAddColumn(const SpriteDrawerArgs &args)
	{
		DrawSpriteTranslatedAddClamp32Command::DrawColumn(args);
	}

	void SWTruecolorDrawers::DrawShadedColumn(const SpriteDrawerArgs &args)
	{
		DrawSpriteShaded32Command::DrawColumn(args);
	}

	void SWTruecolorDrawers::DrawAddClampShadedColumn(const SpriteDrawerArgs &args)
	{
		DrawSpriteAddClampShaded32Command::DrawColumn(args);
	}

	void SWTruecolorDrawers::DrawAddClampColumn(const SpriteDrawerArgs &args)
	{
		DrawSpriteAddClamp32Command::DrawColumn(args);
	}

	void SWTruecolorDrawers::DrawAddClampTranslatedColumn(const SpriteDrawerArgs &args)
	{
		DrawSpriteTranslatedAddClamp32Command::DrawColumn(args);
	}

	void SWTruecolorDrawers::DrawSubClampColumn(const SpriteDrawerArgs &args)
	{
		DrawSpriteSubClamp32Command::DrawColumn(args);
	}

	void SWTruecolorDrawers::DrawSubClampTranslatedColumn(const SpriteDrawerArgs &args)
	{
		DrawSpriteTranslatedSubClamp32Command::DrawColumn(args);
	}

	void SWTruecolorDrawers::DrawRevSubClampColumn(const SpriteDrawerArgs &args)
	{
		DrawSpriteRevSubClamp32Command::DrawColumn(args);
	}

	void SWTruecolorDrawers::DrawRevSubClampTranslatedColumn(const SpriteDrawerArgs &args)
	{
		DrawSpriteTranslatedRevSubClamp32Command::DrawColumn(args);
	}

	void SWTruecolorDrawers::DrawSpan(const SpanDrawerArgs &args)
	{
		DrawSpan32Command::DrawColumn(args);
	}
	
	void SWTruecolorDrawers::DrawSpanMasked(const SpanDrawerArgs &args)
	{
		DrawSpanMasked32Command::DrawColumn(args);
	}
	
	void SWTruecolorDrawers::DrawSpanTranslucent(const SpanDrawerArgs &args)
	{
		DrawSpanTranslucent32Command::DrawColumn(args);
	}
	
	void SWTruecolorDrawers::DrawSpanMaskedTranslucent(const SpanDrawerArgs &args)
	{
		DrawSpanAddClamp32Command::DrawColumn(args);
	}
	
	void SWTruecolorDrawers::DrawSpanAddClamp(const SpanDrawerArgs &args)
	{
		DrawSpanTranslucent32Command::DrawColumn(args);
	}
	
	void SWTruecolorDrawers::DrawSpanMaskedAddClamp(const SpanDrawerArgs &args)
	{
		DrawSpanAddClamp32Command::DrawColumn(args);
	}
	
	void SWTruecolorDrawers::DrawSingleSkyColumn(const SkyDrawerArgs &args)
	{
		DrawSkySingle32Command::DrawColumn(args);
	}
	
	void SWTruecolorDrawers::DrawDoubleSkyColumn(const SkyDrawerArgs &args)
	{
		DrawSkyDouble32Command::DrawColumn(args);
	}

	/////////////////////////////////////////////////////////////////////////////

	void SWTruecolorDrawers::DrawScaledFuzzColumn(const SpriteDrawerArgs& drawerargs)
	{
		int _x = drawerargs.FuzzX();
		int _yl = drawerargs.FuzzY1();
		int _yh = drawerargs.FuzzY2();
		uint8_t* RESTRICT _destorg = drawerargs.Viewport()->GetDest(0, 0);
		int _pitch = drawerargs.Viewport()->RenderTarget->GetPitch();
		int _fuzzpos = fuzzpos;
		int _fuzzviewheight = fuzzviewheight;

		int x = _x;
		int yl = max(_yl, 1);
		int yh = min(_yh, _fuzzviewheight);

		int count = yh - yl + 1;
		if (count <= 0) return;

		int pitch = _pitch;
		uint32_t *dest = _pitch * yl + x + (uint32_t*)_destorg;

		int scaled_x = x * 200 / _fuzzviewheight;
		int fuzz_x = fuzz_random_x_offset[scaled_x % FUZZ_RANDOM_X_SIZE] + _fuzzpos;

		fixed_t fuzzstep = (200 << FRACBITS) / _fuzzviewheight;
		fixed_t fuzzcount = FUZZTABLE << FRACBITS;
		fixed_t fuzz = ((fuzz_x << FRACBITS) + yl * fuzzstep) % fuzzcount;

		while (count > 0)
		{
			int alpha = 32 - fuzzoffset[fuzz >> FRACBITS];

			uint32_t bg = *dest;
			uint32_t red = (RPART(bg) * alpha) >> 5;
			uint32_t green = (GPART(bg) * alpha) >> 5;
			uint32_t blue = (BPART(bg) * alpha) >> 5;

			*dest = 0xff000000 | (red << 16) | (green << 8) | blue;
			dest += pitch;

			fuzz += fuzzstep;
			if (fuzz >= fuzzcount) fuzz -= fuzzcount;

			count--;
		}
	}

	/////////////////////////////////////////////////////////////////////////////

	void SWTruecolorDrawers::DrawUnscaledFuzzColumn(const SpriteDrawerArgs& drawerargs)
	{
		int _x = drawerargs.FuzzX();
		int _yl = drawerargs.FuzzY1();
		int _yh = drawerargs.FuzzY2();
		uint8_t* RESTRICT _destorg = drawerargs.Viewport()->GetDest(0, 0);
		int _pitch = drawerargs.Viewport()->RenderTarget->GetPitch();
		int _fuzzpos = fuzzpos;
		int _fuzzviewheight = fuzzviewheight;

		int yl = max(_yl, 1);
		int yh = min(_yh, _fuzzviewheight);

		int count = yh - yl + 1;

		// Zero length.
		if (count <= 0)
			return;

		uint32_t *dest = _pitch * yl + _x + (uint32_t*)_destorg;
		int pitch = _pitch;

		int fuzzstep = 1;
		int fuzz = _fuzzpos % FUZZTABLE;

#ifndef ORIGINAL_FUZZ

		while (count > 0)
		{
			int available = (FUZZTABLE - fuzz);
			int next_wrap = available / fuzzstep;
			if (available % fuzzstep != 0)
				next_wrap++;

			int cnt = min(count, next_wrap);
			count -= cnt;
			do
			{
				int alpha = 32 - fuzzoffset[fuzz];

				uint32_t bg = *dest;
				uint32_t red = (RPART(bg) * alpha) >> 5;
				uint32_t green = (GPART(bg) * alpha) >> 5;
				uint32_t blue = (BPART(bg) * alpha) >> 5;

				*dest = 0xff000000 | (red << 16) | (green << 8) | blue;
				dest += pitch;
				fuzz += fuzzstep;
			} while (--cnt);

			fuzz %= FUZZTABLE;
		}

#else

		// Handle the case where we would go out of bounds at the top:
		if (yl < fuzzstep)
		{
			uint32_t *srcdest = dest + fuzzoffset[fuzz] * fuzzstep + pitch;
			//assert(static_cast<int>((srcdest - (uint32_t*)dc_destorg) / (_pitch)) < viewheight);

			uint32_t bg = *srcdest;

			uint32_t red = RPART(bg) * 3 / 4;
			uint32_t green = GPART(bg) * 3 / 4;
			uint32_t blue = BPART(bg) * 3 / 4;

			*dest = 0xff000000 | (red << 16) | (green << 8) | blue;
			dest += pitch;
			fuzz += fuzzstep;
			fuzz %= FUZZTABLE;

			count--;
			if (count == 0)
				return;
		}

		bool lowerbounds = (yl + (count + fuzzstep - 1) * fuzzstep > _fuzzviewheight);
		if (lowerbounds)
			count--;

		// Fuzz where fuzzoffset stays within bounds
		while (count > 0)
		{
			int available = (FUZZTABLE - fuzz);
			int next_wrap = available / fuzzstep;
			if (available % fuzzstep != 0)
				next_wrap++;

			int cnt = min(count, next_wrap);
			count -= cnt;
			do
			{
				uint32_t *srcdest = dest + fuzzoffset[fuzz] * fuzzstep;
				//assert(static_cast<int>((srcdest - (uint32_t*)dc_destorg) / (_pitch)) < viewheight);

				uint32_t bg = *srcdest;

				uint32_t red = RPART(bg) * 3 / 4;
				uint32_t green = GPART(bg) * 3 / 4;
				uint32_t blue = BPART(bg) * 3 / 4;

				*dest = 0xff000000 | (red << 16) | (green << 8) | blue;
				dest += pitch;
				fuzz += fuzzstep;
			} while (--cnt);

			fuzz %= FUZZTABLE;
		}

		// Handle the case where we would go out of bounds at the bottom
		if (lowerbounds)
		{
			uint32_t *srcdest = dest + fuzzoffset[fuzz] * fuzzstep - pitch;
			//assert(static_cast<int>((srcdest - (uint32_t*)dc_destorg) / (_pitch)) < viewheight);

			uint32_t bg = *srcdest;

			uint32_t red = RPART(bg) * 3 / 4;
			uint32_t green = GPART(bg) * 3 / 4;
			uint32_t blue = BPART(bg) * 3 / 4;

			*dest = 0xff000000 | (red << 16) | (green << 8) | blue;
		}
#endif
	}

	/////////////////////////////////////////////////////////////////////////////

	void SWTruecolorDrawers::FillSpan(const SpanDrawerArgs& drawerargs)
	{
		int _x1 = drawerargs.DestX1();
		int _x2 = drawerargs.DestX2();
		int _y = drawerargs.DestY();
		uint8_t* RESTRICT _dest = drawerargs.Viewport()->GetDest(_x1, _y);
		fixed_t _light = drawerargs.Light();
		int _color = drawerargs.SolidColor();

		uint32_t *dest = (uint32_t*)_dest;
		int count = (_x2 - _x1 + 1);
		uint32_t light = LightBgra::calc_light_multiplier(_light);
		uint32_t color = LightBgra::shade_pal_index_simple(_color, light);
		for (int i = 0; i < count; i++)
			dest[i] = color;
	}

	/////////////////////////////////////////////////////////////////////////////

	void SWTruecolorDrawers::DrawFogBoundaryLine(const SpanDrawerArgs& drawerargs)
	{
		int _y = drawerargs.DestY();
		int _x = drawerargs.DestX1();
		int _x2 = drawerargs.DestX2();
		uint8_t* RESTRICT _line = drawerargs.Viewport()->GetDest(0, _y);
		fixed_t _light = drawerargs.Light();
		ShadeConstants constants = drawerargs.ColormapConstants();

		int y = _y;
		int x = _x;
		int x2 = _x2;

		uint32_t *dest = (uint32_t*)_line;

		uint32_t light = LightBgra::calc_light_multiplier(_light);

		do
		{
			uint32_t red = (dest[x] >> 16) & 0xff;
			uint32_t green = (dest[x] >> 8) & 0xff;
			uint32_t blue = dest[x] & 0xff;

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

			dest[x] = 0xff000000 | (red << 16) | (green << 8) | blue;
		} while (++x <= x2);
	}

	/////////////////////////////////////////////////////////////////////////////

	void SWTruecolorDrawers::DrawTiltedSpan(const SpanDrawerArgs& drawerargs, const FVector3& _plane_sz, const FVector3& _plane_su, const FVector3& _plane_sv, bool _plane_shade, int _planeshade, float _planelightfloat, fixed_t _pviewx, fixed_t _pviewy, FDynamicColormap* _basecolormap)
	{
		int _x1 = drawerargs.DestX1();
		int _x2 = drawerargs.DestX2();
		int _y = drawerargs.DestY();
		uint8_t* _dest = drawerargs.Viewport()->GetDest(_x1, _y);
		fixed_t _light = drawerargs.Light();
		ShadeConstants _shade_constants = drawerargs.ColormapConstants();
		const uint32_t* _source = (const uint32_t*)drawerargs.TexturePixels();
		int _xbits = drawerargs.TextureWidthBits();
		int _ybits = drawerargs.TextureHeightBits();
		RenderViewport* viewport = drawerargs.Viewport();

		//#define SPANSIZE 32
		//#define INVSPAN 0.03125f
		//#define SPANSIZE 8
		//#define INVSPAN 0.125f
		#define SPANSIZE 16
		#define INVSPAN	0.0625f

		int source_width = 1 << _xbits;
		int source_height = 1 << _ybits;

		uint32_t *dest = (uint32_t*)_dest;
		int count = _x2 - _x1 + 1;

		// Depth (Z) change across the span
		double iz = _plane_sz[2] + _plane_sz[1] * (viewport->viewwindow.centery - _y) + _plane_sz[0] * (_x1 - viewport->viewwindow.centerx);

		// Light change across the span
		fixed_t lightstart = _light;
		fixed_t lightend = lightstart;
		if (_plane_shade)
		{
			double vis_start = iz * _planelightfloat;
			double vis_end = (iz + _plane_sz[0] * count) * _planelightfloat;

			lightstart = LIGHTSCALE(vis_start, _planeshade);
			lightend = LIGHTSCALE(vis_end, _planeshade);
		}
		fixed_t light = lightstart;
		fixed_t steplight = (lightend - lightstart) / count;

		// Texture coordinates
		double uz = _plane_su[2] + _plane_su[1] * (viewport->viewwindow.centery - _y) + _plane_su[0] * (_x1 - viewport->viewwindow.centerx);
		double vz = _plane_sv[2] + _plane_sv[1] * (viewport->viewwindow.centery - _y) + _plane_sv[0] * (_x1 - viewport->viewwindow.centerx);
		double startz = 1.f / iz;
		double startu = uz*startz;
		double startv = vz*startz;
		double izstep = _plane_sz[0] * SPANSIZE;
		double uzstep = _plane_su[0] * SPANSIZE;
		double vzstep = _plane_sv[0] * SPANSIZE;

		// Linear interpolate in sizes of SPANSIZE to increase speed
		while (count >= SPANSIZE)
		{
			iz += izstep;
			uz += uzstep;
			vz += vzstep;

			double endz = 1.f / iz;
			double endu = uz*endz;
			double endv = vz*endz;
			uint32_t stepu = (uint32_t)(int64_t((endu - startu) * INVSPAN));
			uint32_t stepv = (uint32_t)(int64_t((endv - startv) * INVSPAN));
			uint32_t u = (uint32_t)(int64_t(startu) + _pviewx);
			uint32_t v = (uint32_t)(int64_t(startv) + _pviewy);

			for (int i = 0; i < SPANSIZE; i++)
			{
				uint32_t sx = ((u >> 16) * source_width) >> 16;
				uint32_t sy = ((v >> 16) * source_height) >> 16;
				uint32_t fg = _source[sy + sx * source_height];

				if (_shade_constants.simple_shade)
					*(dest++) = LightBgra::shade_bgra_simple(fg, LightBgra::calc_light_multiplier(light));
				else
					*(dest++) = LightBgra::shade_bgra(fg, LightBgra::calc_light_multiplier(light), _shade_constants);

				u += stepu;
				v += stepv;
				light += steplight;
			}
			startu = endu;
			startv = endv;
			count -= SPANSIZE;
		}

		// The last few pixels at the end
		while (count > 0)
		{
			double endz = 1.f / iz;
			startu = uz*endz;
			startv = vz*endz;
			uint32_t u = (uint32_t)(int64_t(startu) + _pviewx);
			uint32_t v = (uint32_t)(int64_t(startv) + _pviewy);

			uint32_t sx = ((u >> 16) * source_width) >> 16;
			uint32_t sy = ((v >> 16) * source_height) >> 16;
			uint32_t fg = _source[sy + sx * source_height];

			if (_shade_constants.simple_shade)
				*(dest++) = LightBgra::shade_bgra_simple(fg, LightBgra::calc_light_multiplier(light));
			else
				*(dest++) = LightBgra::shade_bgra(fg, LightBgra::calc_light_multiplier(light), _shade_constants);

			iz += _plane_sz[0];
			uz += _plane_su[0];
			vz += _plane_sv[0];
			light += steplight;
			count--;
		}
	}

	/////////////////////////////////////////////////////////////////////////////

	void SWTruecolorDrawers::DrawColoredSpan(const SpanDrawerArgs& drawerargs)
	{
		int _y = drawerargs.DestY();
		int _x1 = drawerargs.DestX1();
		int _x2 = drawerargs.DestX2();
		uint8_t* RESTRICT _dest = drawerargs.Viewport()->GetDest(_x1, _y);
		fixed_t _light = drawerargs.Light();
		int _color = drawerargs.SolidColor();

		int y = _y;
		int x1 = _x1;
		int x2 = _x2;

		uint32_t *dest = (uint32_t*)_dest;
		int count = (x2 - x1 + 1);
		uint32_t light = LightBgra::calc_light_multiplier(_light);
		uint32_t color = LightBgra::shade_pal_index_simple(_color, light);
		for (int i = 0; i < count; i++)
			dest[i] = color;
	}

	/////////////////////////////////////////////////////////////////////////////

#if 0
#ifdef NO_SSE
	void SWTruecolorDrawers::ApplySpecialColormap(FSpecialColormap* colormap, DFrameBuffer* screen)
	{
		uint8_t* buffer = screen->GetBuffer();
		int pitch = screen->GetPitch();
		int width = screen->GetWidth();
		int height = screen->GetHeight();

		start_red = (int)(colormap->ColorizeStart[0] * 255);
		start_green = (int)(colormap->ColorizeStart[1] * 255);
		start_blue = (int)(colormap->ColorizeStart[2] * 255);
		end_red = (int)(colormap->ColorizeEnd[0] * 255);
		end_green = (int)(colormap->ColorizeEnd[1] * 255);
		end_blue = (int)(colormap->ColorizeEnd[2] * 255);

		int y = 0;
		int count = height;
		while (count > 0)
		{
			uint8_t *pixels = buffer + y * pitch * 4;
			for (int x = 0; x < width; x++)
			{
				int fg_red = pixels[2];
				int fg_green = pixels[1];
				int fg_blue = pixels[0];

				int gray = (fg_red * 77 + fg_green * 143 + fg_blue * 37) >> 8;
				gray += (gray >> 7); // gray*=256/255
				int inv_gray = 256 - gray;

				int red = clamp((start_red * inv_gray + end_red * gray) >> 8, 0, 255);
				int green = clamp((start_green * inv_gray + end_green * gray) >> 8, 0, 255);
				int blue = clamp((start_blue * inv_gray + end_blue * gray) >> 8, 0, 255);

				pixels[0] = (uint8_t)blue;
				pixels[1] = (uint8_t)green;
				pixels[2] = (uint8_t)red;
				pixels[3] = 0xff;

				pixels += 4;
			}
			y++;
			count--;
		}
	}
#else
	void SWTruecolorDrawers::ApplySpecialColormap(FSpecialColormap* colormap, DFrameBuffer* screen)
	{
		uint8_t* buffer = screen->GetBuffer();
		int pitch = screen->GetPitch();
		int width = screen->GetWidth();
		int height = screen->GetHeight();

		start_red = (int)(colormap->ColorizeStart[0] * 255);
		start_green = (int)(colormap->ColorizeStart[1] * 255);
		start_blue = (int)(colormap->ColorizeStart[2] * 255);
		end_red = (int)(colormap->ColorizeEnd[0] * 255);
		end_green = (int)(colormap->ColorizeEnd[1] * 255);
		end_blue = (int)(colormap->ColorizeEnd[2] * 255);

		int y = 0;
		int count = height;
		__m128i gray_weight = _mm_set_epi16(256, 77, 143, 37, 256, 77, 143, 37);
		__m128i start_end = _mm_set_epi16(255, start_red, start_green, start_blue, 255, end_red, end_green, end_blue);
		while (count > 0)
		{
			uint8_t *pixels = buffer + y * pitch * 4;
			int sse_length = width / 4;
			for (int x = 0; x < sse_length; x++)
			{
				// Unpack to integers:
				__m128i p = _mm_loadu_si128((const __m128i*)pixels);

				__m128i p16_0 = _mm_unpacklo_epi8(p, _mm_setzero_si128());
				__m128i p16_1 = _mm_unpackhi_epi8(p, _mm_setzero_si128());

				// Add gray weighting to colors
				__m128i mullo0 = _mm_mullo_epi16(p16_0, gray_weight);
				__m128i mullo1 = _mm_mullo_epi16(p16_1, gray_weight);
				__m128i p32_0 = _mm_unpacklo_epi16(mullo0, _mm_setzero_si128());
				__m128i p32_1 = _mm_unpackhi_epi16(mullo0, _mm_setzero_si128());
				__m128i p32_2 = _mm_unpacklo_epi16(mullo1, _mm_setzero_si128());
				__m128i p32_3 = _mm_unpackhi_epi16(mullo1, _mm_setzero_si128());

				// Transpose to get color components in individual vectors:
				__m128 tmpx = _mm_castsi128_ps(p32_0);
				__m128 tmpy = _mm_castsi128_ps(p32_1);
				__m128 tmpz = _mm_castsi128_ps(p32_2);
				__m128 tmpw = _mm_castsi128_ps(p32_3);
				_MM_TRANSPOSE4_PS(tmpx, tmpy, tmpz, tmpw);
				__m128i blue = _mm_castps_si128(tmpx);
				__m128i green = _mm_castps_si128(tmpy);
				__m128i red = _mm_castps_si128(tmpz);
				__m128i alpha = _mm_castps_si128(tmpw);

				// Calculate gray and 256-gray values:
				__m128i gray = _mm_srli_epi32(_mm_add_epi32(_mm_add_epi32(red, green), blue), 8);
				__m128i inv_gray = _mm_sub_epi32(_mm_set1_epi32(256), gray);

				// p32 = start * inv_gray + end * gray:
				__m128i gray0 = _mm_shuffle_epi32(gray, _MM_SHUFFLE(0, 0, 0, 0));
				__m128i gray1 = _mm_shuffle_epi32(gray, _MM_SHUFFLE(1, 1, 1, 1));
				__m128i gray2 = _mm_shuffle_epi32(gray, _MM_SHUFFLE(2, 2, 2, 2));
				__m128i gray3 = _mm_shuffle_epi32(gray, _MM_SHUFFLE(3, 3, 3, 3));
				__m128i inv_gray0 = _mm_shuffle_epi32(inv_gray, _MM_SHUFFLE(0, 0, 0, 0));
				__m128i inv_gray1 = _mm_shuffle_epi32(inv_gray, _MM_SHUFFLE(1, 1, 1, 1));
				__m128i inv_gray2 = _mm_shuffle_epi32(inv_gray, _MM_SHUFFLE(2, 2, 2, 2));
				__m128i inv_gray3 = _mm_shuffle_epi32(inv_gray, _MM_SHUFFLE(3, 3, 3, 3));
				__m128i gray16_0 = _mm_packs_epi32(gray0, inv_gray0);
				__m128i gray16_1 = _mm_packs_epi32(gray1, inv_gray1);
				__m128i gray16_2 = _mm_packs_epi32(gray2, inv_gray2);
				__m128i gray16_3 = _mm_packs_epi32(gray3, inv_gray3);
				__m128i gray16_0_mullo = _mm_mullo_epi16(gray16_0, start_end);
				__m128i gray16_1_mullo = _mm_mullo_epi16(gray16_1, start_end);
				__m128i gray16_2_mullo = _mm_mullo_epi16(gray16_2, start_end);
				__m128i gray16_3_mullo = _mm_mullo_epi16(gray16_3, start_end);
				__m128i gray16_0_mulhi = _mm_mulhi_epi16(gray16_0, start_end);
				__m128i gray16_1_mulhi = _mm_mulhi_epi16(gray16_1, start_end);
				__m128i gray16_2_mulhi = _mm_mulhi_epi16(gray16_2, start_end);
				__m128i gray16_3_mulhi = _mm_mulhi_epi16(gray16_3, start_end);
				p32_0 = _mm_srli_epi32(_mm_add_epi32(_mm_unpacklo_epi16(gray16_0_mullo, gray16_0_mulhi), _mm_unpackhi_epi16(gray16_0_mullo, gray16_0_mulhi)), 8);
				p32_1 = _mm_srli_epi32(_mm_add_epi32(_mm_unpacklo_epi16(gray16_1_mullo, gray16_1_mulhi), _mm_unpackhi_epi16(gray16_1_mullo, gray16_1_mulhi)), 8);
				p32_2 = _mm_srli_epi32(_mm_add_epi32(_mm_unpacklo_epi16(gray16_2_mullo, gray16_2_mulhi), _mm_unpackhi_epi16(gray16_2_mullo, gray16_2_mulhi)), 8);
				p32_3 = _mm_srli_epi32(_mm_add_epi32(_mm_unpacklo_epi16(gray16_3_mullo, gray16_3_mulhi), _mm_unpackhi_epi16(gray16_3_mullo, gray16_3_mulhi)), 8);

				p16_0 = _mm_packs_epi32(p32_0, p32_1);
				p16_1 = _mm_packs_epi32(p32_2, p32_3);
				p = _mm_packus_epi16(p16_0, p16_1);

				_mm_storeu_si128((__m128i*)pixels, p);
				pixels += 16;
			}

			for (int x = sse_length * 4; x < width; x++)
			{
				int fg_red = pixels[2];
				int fg_green = pixels[1];
				int fg_blue = pixels[0];

				int gray = (fg_red * 77 + fg_green * 143 + fg_blue * 37) >> 8;
				gray += (gray >> 7); // gray*=256/255
				int inv_gray = 256 - gray;

				int red = clamp((start_red * inv_gray + end_red * gray) >> 8, 0, 255);
				int green = clamp((start_green * inv_gray + end_green * gray) >> 8, 0, 255);
				int blue = clamp((start_blue * inv_gray + end_blue * gray) >> 8, 0, 255);

				pixels[0] = (uint8_t)blue;
				pixels[1] = (uint8_t)green;
				pixels[2] = (uint8_t)red;
				pixels[3] = 0xff;

				pixels += 4;
			}

			y++;
			count--;
		}
	}
#endif
#endif

	/////////////////////////////////////////////////////////////////////////////

	void SWTruecolorDrawers::DrawParticleColumn(int x, int _dest_y, int _count, uint32_t _fg, uint32_t _alpha, uint32_t _fracposx)
	{
		uint32_t* dest = (uint32_t*)thread->Viewport->GetDest(x, _dest_y);
		int pitch = thread->Viewport->RenderTarget->GetPitch();

		int count = _count;
		if (count <= 0)
			return;

		int particle_texture_index = min<int>(gl_particles_style, NUM_PARTICLE_TEXTURES - 1);
		const uint32_t *source = &particle_texture[particle_texture_index][(_fracposx >> FRACBITS) * PARTICLE_TEXTURE_SIZE];
		uint32_t particle_alpha = _alpha;

		uint32_t fracstep = PARTICLE_TEXTURE_SIZE * FRACUNIT / _count;
		uint32_t fracpos = fracstep / 2;

		uint32_t fg_red = (_fg >> 16) & 0xff;
		uint32_t fg_green = (_fg >> 8) & 0xff;
		uint32_t fg_blue = _fg & 0xff;

		for (int y = 0; y < count; y++)
		{
			uint32_t alpha = (source[fracpos >> FRACBITS] * particle_alpha) >> 7;
			uint32_t inv_alpha = 256 - alpha;

			uint32_t bg_red = (*dest >> 16) & 0xff;
			uint32_t bg_green = (*dest >> 8) & 0xff;
			uint32_t bg_blue = (*dest) & 0xff;

			uint32_t red = (fg_red * alpha + bg_red * inv_alpha) / 256;
			uint32_t green = (fg_green * alpha + bg_green * inv_alpha) / 256;
			uint32_t blue = (fg_blue * alpha + bg_blue * inv_alpha) / 256;

			*dest = 0xff000000 | (red << 16) | (green << 8) | blue;
			dest += pitch;
			fracpos += fracstep;
		}
	}

	/////////////////////////////////////////////////////////////////////////////

	void SWTruecolorDrawers::DrawVoxelBlocks(const SpriteDrawerArgs& args, const VoxelBlock* blocks, int blockcount)
	{
		int pitch = args.Viewport()->RenderTarget->GetPitch();
		uint8_t *destorig = args.Viewport()->RenderTarget->GetPixels();

		SpriteDrawerArgs drawerargs = args;
		drawerargs.dc_texturefracx = 0;
		drawerargs.dc_source2 = 0;
		for (int i = 0; i < blockcount; i++)
		{
			const VoxelBlock &block = blocks[i];

			double v = block.vPos / (double)block.voxelsCount / FRACUNIT;
			double vstep = block.vStep / (double)block.voxelsCount / FRACUNIT;
			drawerargs.dc_texturefrac = (int)(v * (1 << 30));
			drawerargs.dc_iscale = (int)(vstep * (1 << 30));
			drawerargs.dc_source = block.voxels;
			drawerargs.dc_textureheight = block.voxelsCount;
			drawerargs.dc_count = block.height;
			drawerargs.dc_dest_y = block.y;
			drawerargs.dc_dest = destorig + (block.x + block.y * pitch) * 4;

			for (int j = 0; j < block.width; j++)
			{
				DrawSprite32Command::DrawColumn(drawerargs);
				drawerargs.dc_dest += 4;
			}
		}
	}

	/////////////////////////////////////////////////////////////////////////////

	template<typename DrawerT>
	void SWTruecolorDrawers::DrawWallColumns(const WallDrawerArgs& wallargs)
	{
		wallcolargs.wallargs = &wallargs;

		bool haslights = r_dynlights && wallargs.lightlist;
		if (haslights)
		{
			float dx = wallargs.WallC.tright.X - wallargs.WallC.tleft.X;
			float dy = wallargs.WallC.tright.Y - wallargs.WallC.tleft.Y;
			float length = sqrt(dx * dx + dy * dy);
			wallcolargs.dc_normal.X = dy / length;
			wallcolargs.dc_normal.Y = -dx / length;
			wallcolargs.dc_normal.Z = 0.0f;
		}

		wallcolargs.SetTextureFracBits(wallargs.fracbits);

		float curlight = wallargs.lightpos;
		float lightstep = wallargs.lightstep;
		int shade = wallargs.Shade();

		if (wallargs.fixedlight)
		{
			curlight = wallargs.FixedLight();
			lightstep = 0;
		}

		float upos = wallargs.texcoords.upos, ustepX = wallargs.texcoords.ustepX, ustepY = wallargs.texcoords.ustepY;
		float vpos = wallargs.texcoords.vpos, vstepX = wallargs.texcoords.vstepX, vstepY = wallargs.texcoords.vstepY;
		float wpos = wallargs.texcoords.wpos, wstepX = wallargs.texcoords.wstepX, wstepY = wallargs.texcoords.wstepY;
		float startX = wallargs.texcoords.startX;

		int x1 = wallargs.x1;
		int x2 = wallargs.x2;

		upos += ustepX * (x1 + 0.5f - startX);
		vpos += vstepX * (x1 + 0.5f - startX);
		wpos += wstepX * (x1 + 0.5f - startX);

		float centerY = wallargs.CenterY;
		centerY -= 0.5f;

		auto uwal = wallargs.uwal;
		auto dwal = wallargs.dwal;
		for (int x = x1; x < x2; x++)
		{
			int y1 = uwal[x];
			int y2 = dwal[x];
			if (y2 > y1)
			{
				wallcolargs.SetLight(curlight, shade);
				if (haslights)
					SetLights(wallcolargs, x, y1, wallargs);
				else
					wallcolargs.dc_num_lights = 0;

				float dy = (y1 - centerY);
				float u = upos + ustepY * dy;
				float v = vpos + vstepY * dy;
				float w = wpos + wstepY * dy;
				float scaleU = ustepX;
				float scaleV = vstepY;
				w = 1.0f / w;
				u *= w;
				v *= w;
				scaleU *= w;
				scaleV *= w;

				uint32_t texelX = (uint32_t)(int64_t)((u - std::floor(u)) * 0x1'0000'0000LL);
				uint32_t texelY = (uint32_t)(int64_t)((v - std::floor(v)) * 0x1'0000'0000LL);
				uint32_t texelStepX = (uint32_t)(int64_t)(scaleU * 0x1'0000'0000LL);
				uint32_t texelStepY = (uint32_t)(int64_t)(scaleV * 0x1'0000'0000LL);

				DrawWallColumn32<DrawerT>(wallcolargs, x, y1, y2, texelX, texelY, texelStepX, texelStepY);
			}

			upos += ustepX;
			vpos += vstepX;
			wpos += wstepX;
			curlight += lightstep;
		}

		if (r_modelscene)
		{
			for (int x = x1; x < x2; x++)
			{
				int y1 = uwal[x];
				int y2 = dwal[x];
				if (y2 > y1)
				{
					int count = y2 - y1;

					float w1 = 1.0f / wallargs.WallC.sz1;
					float w2 = 1.0f / wallargs.WallC.sz2;
					float t = (x - wallargs.WallC.sx1 + 0.5f) / (wallargs.WallC.sx2 - wallargs.WallC.sx1);
					float wcol = w1 * (1.0f - t) + w2 * t;
					float zcol = 1.0f / wcol;
					float zbufferdepth = 1.0f / (zcol / wallargs.FocalTangent);

					wallcolargs.SetDest(x, y1);
					wallcolargs.SetCount(count);
					DrawDepthColumn(wallcolargs, zbufferdepth);
				}
			}
		}
	}

	template<typename DrawerT>
	void SWTruecolorDrawers::DrawWallColumn32(WallColumnDrawerArgs& drawerargs, int x, int y1, int y2, uint32_t texelX, uint32_t texelY, uint32_t texelStepX, uint32_t texelStepY)
	{
		auto& wallargs = *drawerargs.wallargs;
		int texwidth = wallargs.texwidth;
		int texheight = wallargs.texheight;

		double xmagnitude = fabs(static_cast<int32_t>(texelStepX) * (1.0 / 0x1'0000'0000LL));
		double ymagnitude = fabs(static_cast<int32_t>(texelStepY) * (1.0 / 0x1'0000'0000LL));
		double magnitude = max(ymagnitude, xmagnitude);
		double min_lod = -1000.0;
		double lod = max(log2(magnitude) + r_lod_bias, min_lod);
		bool magnifying = lod < 0.0f;

		int mipmap_offset = 0;
		int mip_width = texwidth;
		int mip_height = texheight;
		if (wallargs.mipmapped && mip_width > 1 && mip_height > 1)
		{
			int level = (int)lod;
			while (level > 0 && mip_width > 1 && mip_height > 1)
			{
				mipmap_offset += mip_width * mip_height;
				level--;
				mip_width = max(mip_width >> 1, 1);
				mip_height = max(mip_height >> 1, 1);
			}
		}

		const uint32_t* pixels = static_cast<const uint32_t*>(wallargs.texpixels) + mipmap_offset;
		fixed_t xxoffset = (texelX >> 16) * mip_width;

		const uint8_t* source;
		const uint8_t* source2;
		uint32_t texturefracx;
		bool filter_nearest = (magnifying && !r_magfilter) || (!magnifying && !r_minfilter);
		if (filter_nearest)
		{
			int tx = (xxoffset >> FRACBITS) % mip_width;
			source = (uint8_t*)(pixels + tx * mip_height);
			source2 = nullptr;
			texturefracx = 0;
		}
		else
		{
			xxoffset -= FRACUNIT / 2;
			int tx0 = (xxoffset >> FRACBITS) % mip_width;
			if (tx0 < 0)
				tx0 += mip_width;
			int tx1 = (tx0 + 1) % mip_width;
			source = (uint8_t*)(pixels + tx0 * mip_height);
			source2 = (uint8_t*)(pixels + tx1 * mip_height);
			texturefracx = (xxoffset >> (FRACBITS - 4)) & 15;
		}

		int count = y2 - y1;
		drawerargs.SetDest(x, y1);
		drawerargs.SetCount(count);
		drawerargs.SetTexture(source, source2, mip_height);
		drawerargs.SetTextureUPos(texturefracx);
		drawerargs.SetTextureVPos(texelY);
		drawerargs.SetTextureVStep(texelStepY);
		DrawerT::DrawColumn(drawerargs);
	}
}
