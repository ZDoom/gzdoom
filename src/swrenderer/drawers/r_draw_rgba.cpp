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

#include "templates.h"
#include "doomdef.h"
#include "i_system.h"
#include "w_wad.h"
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
#include "x86.h"
#include <vector>

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
	void SWTruecolorDrawers::DrawWallColumn(const WallDrawerArgs &args)
	{
		Queue->Push<DrawWall32Command>(args);
	}
	
	void SWTruecolorDrawers::DrawWallMaskedColumn(const WallDrawerArgs &args)
	{
		Queue->Push<DrawWallMasked32Command>(args);
	}
	
	void SWTruecolorDrawers::DrawWallAddColumn(const WallDrawerArgs &args)
	{
		Queue->Push<DrawWallAddClamp32Command>(args);
	}
	
	void SWTruecolorDrawers::DrawWallAddClampColumn(const WallDrawerArgs &args)
	{
		Queue->Push<DrawWallAddClamp32Command>(args);
	}
	
	void SWTruecolorDrawers::DrawWallSubClampColumn(const WallDrawerArgs &args)
	{
		Queue->Push<DrawWallSubClamp32Command>(args);
	}
	
	void SWTruecolorDrawers::DrawWallRevSubClampColumn(const WallDrawerArgs &args)
	{
		Queue->Push<DrawWallRevSubClamp32Command>(args);
	}
	
	void SWTruecolorDrawers::DrawColumn(const SpriteDrawerArgs &args)
	{
		Queue->Push<DrawSprite32Command>(args);
	}

	void SWTruecolorDrawers::FillColumn(const SpriteDrawerArgs &args)
	{
		Queue->Push<FillSprite32Command>(args);
	}

	void SWTruecolorDrawers::FillAddColumn(const SpriteDrawerArgs &args)
	{
		Queue->Push<FillSpriteAddClamp32Command>(args);
	}

	void SWTruecolorDrawers::FillAddClampColumn(const SpriteDrawerArgs &args)
	{
		Queue->Push<FillSpriteAddClamp32Command>(args);
	}

	void SWTruecolorDrawers::FillSubClampColumn(const SpriteDrawerArgs &args)
	{
		Queue->Push<FillSpriteSubClamp32Command>(args);
	}

	void SWTruecolorDrawers::FillRevSubClampColumn(const SpriteDrawerArgs &args)
	{
		Queue->Push<FillSpriteRevSubClamp32Command>(args);
	}

	void SWTruecolorDrawers::DrawFuzzColumn(const SpriteDrawerArgs &args)
	{
		if (r_fuzzscale)
			Queue->Push<DrawScaledFuzzColumnRGBACommand>(args);
		else
			Queue->Push<DrawFuzzColumnRGBACommand>(args);
		R_UpdateFuzzPos(args);
	}

	void SWTruecolorDrawers::DrawAddColumn(const SpriteDrawerArgs &args)
	{
		Queue->Push<DrawSpriteAddClamp32Command>(args);
	}

	void SWTruecolorDrawers::DrawTranslatedColumn(const SpriteDrawerArgs &args)
	{
		Queue->Push<DrawSpriteTranslated32Command>(args);
	}

	void SWTruecolorDrawers::DrawTranslatedAddColumn(const SpriteDrawerArgs &args)
	{
		Queue->Push<DrawSpriteTranslatedAddClamp32Command>(args);
	}

	void SWTruecolorDrawers::DrawShadedColumn(const SpriteDrawerArgs &args)
	{
		Queue->Push<DrawSpriteShaded32Command>(args);
	}

	void SWTruecolorDrawers::DrawAddClampShadedColumn(const SpriteDrawerArgs &args)
	{
		Queue->Push<DrawSpriteAddClampShaded32Command>(args);
	}

	void SWTruecolorDrawers::DrawAddClampColumn(const SpriteDrawerArgs &args)
	{
		Queue->Push<DrawSpriteAddClamp32Command>(args);
	}

	void SWTruecolorDrawers::DrawAddClampTranslatedColumn(const SpriteDrawerArgs &args)
	{
		Queue->Push<DrawSpriteTranslatedAddClamp32Command>(args);
	}

	void SWTruecolorDrawers::DrawSubClampColumn(const SpriteDrawerArgs &args)
	{
		Queue->Push<DrawSpriteSubClamp32Command>(args);
	}

	void SWTruecolorDrawers::DrawSubClampTranslatedColumn(const SpriteDrawerArgs &args)
	{
		Queue->Push<DrawSpriteTranslatedSubClamp32Command>(args);
	}

	void SWTruecolorDrawers::DrawRevSubClampColumn(const SpriteDrawerArgs &args)
	{
		Queue->Push<DrawSpriteRevSubClamp32Command>(args);
	}

	void SWTruecolorDrawers::DrawRevSubClampTranslatedColumn(const SpriteDrawerArgs &args)
	{
		Queue->Push<DrawSpriteTranslatedRevSubClamp32Command>(args);
	}

	void SWTruecolorDrawers::DrawVoxelBlocks(const SpriteDrawerArgs &args, const VoxelBlock *blocks, int blockcount)
	{
		Queue->Push<DrawVoxelBlocksRGBACommand>(args, blocks, blockcount);
	}

	void SWTruecolorDrawers::DrawSpan(const SpanDrawerArgs &args)
	{
		Queue->Push<DrawSpan32Command>(args);
	}
	
	void SWTruecolorDrawers::DrawSpanMasked(const SpanDrawerArgs &args)
	{
		Queue->Push<DrawSpanMasked32Command>(args);
	}
	
	void SWTruecolorDrawers::DrawSpanTranslucent(const SpanDrawerArgs &args)
	{
		Queue->Push<DrawSpanTranslucent32Command>(args);
	}
	
	void SWTruecolorDrawers::DrawSpanMaskedTranslucent(const SpanDrawerArgs &args)
	{
		Queue->Push<DrawSpanAddClamp32Command>(args);
	}
	
	void SWTruecolorDrawers::DrawSpanAddClamp(const SpanDrawerArgs &args)
	{
		Queue->Push<DrawSpanTranslucent32Command>(args);
	}
	
	void SWTruecolorDrawers::DrawSpanMaskedAddClamp(const SpanDrawerArgs &args)
	{
		Queue->Push<DrawSpanAddClamp32Command>(args);
	}
	
	void SWTruecolorDrawers::DrawSingleSkyColumn(const SkyDrawerArgs &args)
	{
		Queue->Push<DrawSkySingle32Command>(args);
	}
	
	void SWTruecolorDrawers::DrawDoubleSkyColumn(const SkyDrawerArgs &args)
	{
		Queue->Push<DrawSkyDouble32Command>(args);
	}

	/////////////////////////////////////////////////////////////////////////////

	DrawScaledFuzzColumnRGBACommand::DrawScaledFuzzColumnRGBACommand(const SpriteDrawerArgs &drawerargs)
	{
		_x = drawerargs.FuzzX();
		_yl = drawerargs.FuzzY1();
		_yh = drawerargs.FuzzY2();
		_destorg = drawerargs.Viewport()->GetDest(0, 0);
		_pitch = drawerargs.Viewport()->RenderTarget->GetPitch();
		_fuzzpos = fuzzpos;
		_fuzzviewheight = fuzzviewheight;
	}

	void DrawScaledFuzzColumnRGBACommand::Execute(DrawerThread *thread)
	{
		int x = _x;
		int yl = MAX(_yl, 1);
		int yh = MIN(_yh, _fuzzviewheight);

		int count = thread->count_for_thread(yl, yh - yl + 1);
		if (count <= 0) return;

		int pitch = _pitch;
		uint32_t *dest = _pitch * yl + x + (uint32_t*)_destorg;

		int scaled_x = x * 200 / _fuzzviewheight;
		int fuzz_x = fuzz_random_x_offset[scaled_x % FUZZ_RANDOM_X_SIZE] + _fuzzpos;

		fixed_t fuzzstep = (200 << FRACBITS) / _fuzzviewheight;
		fixed_t fuzzcount = FUZZTABLE << FRACBITS;
		fixed_t fuzz = (fuzz_x << FRACBITS) + yl * fuzzstep;

		dest = thread->dest_for_thread(yl, pitch, dest);
		pitch *= thread->num_cores;

		fuzz += fuzzstep * thread->skipped_by_thread(yl);
		fuzz %= fuzzcount;
		fuzzstep *= thread->num_cores;

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

	FString DrawScaledFuzzColumnRGBACommand::DebugInfo()
	{
		return "DrawScaledFuzzColumn";
	}

	/////////////////////////////////////////////////////////////////////////////

	DrawFuzzColumnRGBACommand::DrawFuzzColumnRGBACommand(const SpriteDrawerArgs &drawerargs)
	{
		_x = drawerargs.FuzzX();
		_yl = drawerargs.FuzzY1();
		_yh = drawerargs.FuzzY2();
		_destorg = drawerargs.Viewport()->GetDest(0, 0);
		_pitch = drawerargs.Viewport()->RenderTarget->GetPitch();
		_fuzzpos = fuzzpos;
		_fuzzviewheight = fuzzviewheight;
	}

	void DrawFuzzColumnRGBACommand::Execute(DrawerThread *thread)
	{
		int yl = MAX(_yl, 1);
		int yh = MIN(_yh, _fuzzviewheight);

		int count = thread->count_for_thread(yl, yh - yl + 1);

		// Zero length.
		if (count <= 0)
			return;

		uint32_t *dest = thread->dest_for_thread(yl, _pitch, _pitch * yl + _x + (uint32_t*)_destorg);
		int pitch = _pitch * thread->num_cores;

		int fuzzstep = thread->num_cores;
		int fuzz = (_fuzzpos + thread->skipped_by_thread(yl)) % FUZZTABLE;

#ifndef ORIGINAL_FUZZ

		while (count > 0)
		{
			int available = (FUZZTABLE - fuzz);
			int next_wrap = available / fuzzstep;
			if (available % fuzzstep != 0)
				next_wrap++;

			int cnt = MIN(count, next_wrap);
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

		yl += thread->skipped_by_thread(yl);

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

			int cnt = MIN(count, next_wrap);
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

	FString DrawFuzzColumnRGBACommand::DebugInfo()
	{
		return "DrawFuzzColumn";
	}

	/////////////////////////////////////////////////////////////////////////////

	FillSpanRGBACommand::FillSpanRGBACommand(const SpanDrawerArgs &drawerargs)
	{
		_x1 = drawerargs.DestX1();
		_x2 = drawerargs.DestX2();
		_y = drawerargs.DestY();
		_dest = drawerargs.Viewport()->GetDest(_x1, _y);
		_light = drawerargs.Light();
		_color = drawerargs.SolidColor();
	}

	void FillSpanRGBACommand::Execute(DrawerThread *thread)
	{
		if (thread->line_skipped_by_thread(_y))
			return;

		uint32_t *dest = (uint32_t*)_dest;
		int count = (_x2 - _x1 + 1);
		uint32_t light = LightBgra::calc_light_multiplier(_light);
		uint32_t color = LightBgra::shade_pal_index_simple(_color, light);
		for (int i = 0; i < count; i++)
			dest[i] = color;
	}

	FString FillSpanRGBACommand::DebugInfo()
	{
		return "FillSpan";
	}

	/////////////////////////////////////////////////////////////////////////////

	DrawFogBoundaryLineRGBACommand::DrawFogBoundaryLineRGBACommand(const SpanDrawerArgs &drawerargs)
	{
		_y = drawerargs.DestY();
		_x = drawerargs.DestX1();
		_x2 = drawerargs.DestX2();
		_line = drawerargs.Viewport()->GetDest(0, _y);
		_light = drawerargs.Light();
		_shade_constants = drawerargs.ColormapConstants();
	}

	void DrawFogBoundaryLineRGBACommand::Execute(DrawerThread *thread)
	{
		if (thread->line_skipped_by_thread(_y))
			return;

		int y = _y;
		int x = _x;
		int x2 = _x2;

		uint32_t *dest = (uint32_t*)_line;

		uint32_t light = LightBgra::calc_light_multiplier(_light);
		ShadeConstants constants = _shade_constants;

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

	FString DrawFogBoundaryLineRGBACommand::DebugInfo()
	{
		return "DrawFogBoundaryLine";
	}

	/////////////////////////////////////////////////////////////////////////////

	DrawTiltedSpanRGBACommand::DrawTiltedSpanRGBACommand(const SpanDrawerArgs &drawerargs, const FVector3 &plane_sz, const FVector3 &plane_su, const FVector3 &plane_sv, bool plane_shade, int planeshade, float planelightfloat, fixed_t pviewx, fixed_t pviewy)
	{
		_x1 = drawerargs.DestX1();
		_x2 = drawerargs.DestX2();
		_y = drawerargs.DestY();
		_dest = drawerargs.Viewport()->GetDest(_x1, _y);
		_light = drawerargs.Light();
		_shade_constants = drawerargs.ColormapConstants();
		_plane_sz = plane_sz;
		_plane_su = plane_su;
		_plane_sv = plane_sv;
		_plane_shade = plane_shade;
		_planeshade = planeshade;
		_planelightfloat = planelightfloat;
		_pviewx = pviewx;
		_pviewy = pviewy;
		_source = (const uint32_t*)drawerargs.TexturePixels();
		_xbits = drawerargs.TextureWidthBits();
		_ybits = drawerargs.TextureHeightBits();
		viewport = drawerargs.Viewport();
	}

	void DrawTiltedSpanRGBACommand::Execute(DrawerThread *thread)
	{
		if (thread->line_skipped_by_thread(_y))
			return;

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

	FString DrawTiltedSpanRGBACommand::DebugInfo()
	{
		return "DrawTiltedSpan";
	}

	/////////////////////////////////////////////////////////////////////////////

	DrawColoredSpanRGBACommand::DrawColoredSpanRGBACommand(const SpanDrawerArgs &drawerargs)
	{
		_y = drawerargs.DestY();
		_x1 = drawerargs.DestX1();
		_x2 = drawerargs.DestX2();
		_dest = drawerargs.Viewport()->GetDest(_x1, _y);
		_light = drawerargs.Light();
		_color = drawerargs.SolidColor();
	}

	void DrawColoredSpanRGBACommand::Execute(DrawerThread *thread)
	{
		if (thread->line_skipped_by_thread(_y))
			return;

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

	FString DrawColoredSpanRGBACommand::DebugInfo()
	{
		return "DrawColoredSpan";
	}

	/////////////////////////////////////////////////////////////////////////////

#if 0
	ApplySpecialColormapRGBACommand::ApplySpecialColormapRGBACommand(FSpecialColormap *colormap, DFrameBuffer *screen)
	{
		buffer = screen->GetBuffer();
		pitch = screen->GetPitch();
		width = screen->GetWidth();
		height = screen->GetHeight();

		start_red = (int)(colormap->ColorizeStart[0] * 255);
		start_green = (int)(colormap->ColorizeStart[1] * 255);
		start_blue = (int)(colormap->ColorizeStart[2] * 255);
		end_red = (int)(colormap->ColorizeEnd[0] * 255);
		end_green = (int)(colormap->ColorizeEnd[1] * 255);
		end_blue = (int)(colormap->ColorizeEnd[2] * 255);
	}

#ifdef NO_SSE
	void ApplySpecialColormapRGBACommand::Execute(DrawerThread *thread)
	{
		int y = thread->skipped_by_thread(0);
		int count = thread->count_for_thread(0, height);
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
			y += thread->num_cores;
			count--;
		}
	}
#else
	void ApplySpecialColormapRGBACommand::Execute(DrawerThread *thread)
	{
		int y = thread->skipped_by_thread(0);
		int count = thread->count_for_thread(0, height);
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

			y += thread->num_cores;
			count--;
		}
	}
#endif
#endif

	/////////////////////////////////////////////////////////////////////////////

	DrawParticleColumnRGBACommand::DrawParticleColumnRGBACommand(uint32_t *dest, int dest_y, int pitch, int count, uint32_t fg, uint32_t alpha, uint32_t fracposx)
	{
		_dest = dest;
		_pitch = pitch;
		_count = count;
		_fg = fg;
		_alpha = alpha;
		_fracposx = fracposx;
		_dest_y = dest_y;
	}

	void DrawParticleColumnRGBACommand::Execute(DrawerThread *thread)
	{
		int count = thread->count_for_thread(_dest_y, _count);
		if (count <= 0)
			return;

		uint32_t *dest = thread->dest_for_thread(_dest_y, _pitch, _dest);
		int pitch = _pitch * thread->num_cores;

		int particle_texture_index = MIN<int>(gl_particles_style, NUM_PARTICLE_TEXTURES - 1);
		const uint32_t *source = &particle_texture[particle_texture_index][(_fracposx >> FRACBITS) * PARTICLE_TEXTURE_SIZE];
		uint32_t particle_alpha = _alpha;

		uint32_t fracstep = PARTICLE_TEXTURE_SIZE * FRACUNIT / _count;
		uint32_t fracpos = fracstep * thread->skipped_by_thread(_dest_y) + fracstep / 2;
		fracstep *= thread->num_cores;

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

	FString DrawParticleColumnRGBACommand::DebugInfo()
	{
		return "DrawParticle";
	}

	/////////////////////////////////////////////////////////////////////////////

	DrawVoxelBlocksRGBACommand::DrawVoxelBlocksRGBACommand(const SpriteDrawerArgs &args, const VoxelBlock *blocks, int blockcount) : args(args), blocks(blocks), blockcount(blockcount)
	{
	}

	void DrawVoxelBlocksRGBACommand::Execute(DrawerThread *thread)
	{
		int pitch = args.Viewport()->RenderTarget->GetPitch();
		uint8_t *destorig = args.Viewport()->RenderTarget->GetPixels();

		DrawSprite32Command drawer(args);
		drawer.args.dc_texturefracx = 0;
		drawer.args.dc_source2 = 0;
		for (int i = 0; i < blockcount; i++)
		{
			const VoxelBlock &block = blocks[i];

			double v = block.vPos / (double)block.voxelsCount / FRACUNIT;
			double vstep = block.vStep / (double)block.voxelsCount / FRACUNIT;
			drawer.args.dc_texturefrac = (int)(v * (1 << 30));
			drawer.args.dc_iscale = (int)(vstep * (1 << 30));
			drawer.args.dc_source = block.voxels;
			drawer.args.dc_textureheight = block.voxelsCount;
			drawer.args.dc_count = block.height;
			drawer.args.dc_dest_y = block.y;
			drawer.args.dc_dest = destorig + (block.x + block.y * pitch) * 4;

			for (int j = 0; j < block.width; j++)
			{
				drawer.Execute(thread);
				drawer.args.dc_dest += 4;
			}
		}
	}

	FString DrawVoxelBlocksRGBACommand::DebugInfo()
	{
		return "DrawVoxelBlocks";
	}
}
