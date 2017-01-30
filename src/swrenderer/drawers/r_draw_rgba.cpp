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
// $Log:$
//
// DESCRIPTION:
//		True color span/column drawing functions.
//
//-----------------------------------------------------------------------------

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
#include "r_drawers.h"
#include "gl/data/gl_matrix.h"
#include "swrenderer/scene/r_viewport.h"
#include "swrenderer/scene/r_light.h"

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
	DrawSpanLLVMCommand::DrawSpanLLVMCommand(const SpanDrawerArgs &drawerargs)
	{
		args.xfrac = drawerargs.ds_xfrac;
		args.yfrac = drawerargs.ds_yfrac;
		args.xstep = drawerargs.ds_xstep;
		args.ystep = drawerargs.ds_ystep;
		args.x1 = drawerargs.ds_x1;
		args.x2 = drawerargs.ds_x2;
		args.y = drawerargs.ds_y;
		args.xbits = drawerargs.ds_xbits;
		args.ybits = drawerargs.ds_ybits;
		args.destorg = (uint32_t*)dc_destorg;
		args.destpitch = dc_pitch;
		args.source = (const uint32_t*)drawerargs.ds_source;
		args.light = LightBgra::calc_light_multiplier(drawerargs.dc_light);
		args.light_red = drawerargs.dc_shade_constants.light_red;
		args.light_green = drawerargs.dc_shade_constants.light_green;
		args.light_blue = drawerargs.dc_shade_constants.light_blue;
		args.light_alpha = drawerargs.dc_shade_constants.light_alpha;
		args.fade_red = drawerargs.dc_shade_constants.fade_red;
		args.fade_green = drawerargs.dc_shade_constants.fade_green;
		args.fade_blue = drawerargs.dc_shade_constants.fade_blue;
		args.fade_alpha = drawerargs.dc_shade_constants.fade_alpha;
		args.desaturate = drawerargs.dc_shade_constants.desaturate;
		args.srcalpha = drawerargs.dc_srcalpha >> (FRACBITS - 8);
		args.destalpha = drawerargs.dc_destalpha >> (FRACBITS - 8);
		args.flags = 0;
		if (drawerargs.dc_shade_constants.simple_shade)
			args.flags |= DrawSpanArgs::simple_shade;
		if (!sampler_setup(drawerargs.ds_lod, args.source, args.xbits, args.ybits, drawerargs.ds_source_mipmapped))
			args.flags |= DrawSpanArgs::nearest_filter;

		args.viewpos_x = drawerargs.dc_viewpos.X;
		args.step_viewpos_x = drawerargs.dc_viewpos_step.X;
		args.dynlights = drawerargs.dc_lights;
		args.num_dynlights = drawerargs.dc_num_lights;
	}

	void DrawSpanLLVMCommand::Execute(DrawerThread *thread)
	{
		if (thread->skipped_by_thread(args.y))
			return;
		Drawers::Instance()->DrawSpan(&args);
	}

	FString DrawSpanLLVMCommand::DebugInfo()
	{
		return "DrawSpan\n" + args.ToString();
	}

	bool DrawSpanLLVMCommand::sampler_setup(double lod, const uint32_t * &source, int &xbits, int &ybits, bool mipmapped)
	{
		bool magnifying = lod < 0.0;
		if (r_mipmap && mipmapped)
		{
			int level = (int)lod;
			while (level > 0)
			{
				if (xbits <= 2 || ybits <= 2)
					break;

				source += (1 << (xbits)) * (1 << (ybits));
				xbits -= 1;
				ybits -= 1;
				level--;
			}
		}

		return (magnifying && r_magfilter) || (!magnifying && r_minfilter);
	}

	/////////////////////////////////////////////////////////////////////////////

	void DrawSpanMaskedLLVMCommand::Execute(DrawerThread *thread)
	{
		if (thread->skipped_by_thread(args.y))
			return;
		Drawers::Instance()->DrawSpanMasked(&args);
	}

	void DrawSpanTranslucentLLVMCommand::Execute(DrawerThread *thread)
	{
		if (thread->skipped_by_thread(args.y))
			return;
		Drawers::Instance()->DrawSpanTranslucent(&args);
	}

	void DrawSpanMaskedTranslucentLLVMCommand::Execute(DrawerThread *thread)
	{
		if (thread->skipped_by_thread(args.y))
			return;
		Drawers::Instance()->DrawSpanMaskedTranslucent(&args);
	}

	void DrawSpanAddClampLLVMCommand::Execute(DrawerThread *thread)
	{
		if (thread->skipped_by_thread(args.y))
			return;
		Drawers::Instance()->DrawSpanAddClamp(&args);
	}

	void DrawSpanMaskedAddClampLLVMCommand::Execute(DrawerThread *thread)
	{
		if (thread->skipped_by_thread(args.y))
			return;
		Drawers::Instance()->DrawSpanMaskedAddClamp(&args);
	}

	/////////////////////////////////////////////////////////////////////////////

	WorkerThreadData DrawWall1LLVMCommand::ThreadData(DrawerThread *thread)
	{
		WorkerThreadData d;
		d.core = thread->core;
		d.num_cores = thread->num_cores;
		d.pass_start_y = thread->pass_start_y;
		d.pass_end_y = thread->pass_end_y;
		return d;
	}

	DrawWall1LLVMCommand::DrawWall1LLVMCommand(const WallDrawerArgs &drawerargs)
	{
		args.dest = (uint32_t*)drawerargs.Dest();
		args.dest_y = drawerargs.DestY();
		args.pitch = dc_pitch;
		args.count = drawerargs.dc_count;
		args.texturefrac[0] = drawerargs.dc_texturefrac;
		args.texturefracx[0] = drawerargs.dc_texturefracx;
		args.iscale[0] = drawerargs.dc_iscale;
		args.textureheight[0] = drawerargs.dc_textureheight;
		args.source[0] = (const uint32 *)drawerargs.dc_source;
		args.source2[0] = (const uint32 *)drawerargs.dc_source2;
		args.light[0] = LightBgra::calc_light_multiplier(drawerargs.dc_light);
		args.light_red = drawerargs.dc_shade_constants.light_red;
		args.light_green = drawerargs.dc_shade_constants.light_green;
		args.light_blue = drawerargs.dc_shade_constants.light_blue;
		args.light_alpha = drawerargs.dc_shade_constants.light_alpha;
		args.fade_red = drawerargs.dc_shade_constants.fade_red;
		args.fade_green = drawerargs.dc_shade_constants.fade_green;
		args.fade_blue = drawerargs.dc_shade_constants.fade_blue;
		args.fade_alpha = drawerargs.dc_shade_constants.fade_alpha;
		args.desaturate = drawerargs.dc_shade_constants.desaturate;
		args.srcalpha = drawerargs.dc_srcalpha >> (FRACBITS - 8);
		args.destalpha = drawerargs.dc_destalpha >> (FRACBITS - 8);
		args.flags = 0;
		if (drawerargs.dc_shade_constants.simple_shade)
			args.flags |= DrawWallArgs::simple_shade;
		if (args.source2[0] == nullptr)
			args.flags |= DrawWallArgs::nearest_filter;

		args.z = drawerargs.dc_viewpos.Z;
		args.step_z = drawerargs.dc_viewpos_step.Z;
		args.dynlights = drawerargs.dc_lights;
		args.num_dynlights = drawerargs.dc_num_lights;
	}

	void DrawWall1LLVMCommand::Execute(DrawerThread *thread)
	{
		WorkerThreadData d = ThreadData(thread);
		Drawers::Instance()->vlinec1(&args, &d);
	}

	FString DrawWall1LLVMCommand::DebugInfo()
	{
		return "DrawWall1\n" + args.ToString();
	}

	/////////////////////////////////////////////////////////////////////////////

	WorkerThreadData DrawColumnLLVMCommand::ThreadData(DrawerThread *thread)
	{
		WorkerThreadData d;
		d.core = thread->core;
		d.num_cores = thread->num_cores;
		d.pass_start_y = thread->pass_start_y;
		d.pass_end_y = thread->pass_end_y;
		return d;
	}

	FString DrawColumnLLVMCommand::DebugInfo()
	{
		return "DrawColumn\n" + args.ToString();
	}

	DrawColumnLLVMCommand::DrawColumnLLVMCommand(const ColumnDrawerArgs &drawerargs)
	{
		args.dest = (uint32_t*)drawerargs.Dest();
		args.source = drawerargs.dc_source;
		args.source2 = drawerargs.dc_source2;
		args.colormap = drawerargs.dc_colormap;
		args.translation = drawerargs.dc_translation;
		args.basecolors = (const uint32_t *)GPalette.BaseColors;
		args.pitch = dc_pitch;
		args.count = drawerargs.dc_count;
		args.dest_y = drawerargs.DestY();
		args.iscale = drawerargs.dc_iscale;
		args.texturefracx = drawerargs.dc_texturefracx;
		args.textureheight = drawerargs.dc_textureheight;
		args.texturefrac = drawerargs.dc_texturefrac;
		args.light = LightBgra::calc_light_multiplier(drawerargs.dc_light);
		args.color = LightBgra::shade_pal_index_simple(drawerargs.dc_color, args.light);
		args.srccolor = drawerargs.dc_srccolor_bgra;
		args.srcalpha = drawerargs.dc_srcalpha >> (FRACBITS - 8);
		args.destalpha = drawerargs.dc_destalpha >> (FRACBITS - 8);
		args.light_red = drawerargs.dc_shade_constants.light_red;
		args.light_green = drawerargs.dc_shade_constants.light_green;
		args.light_blue = drawerargs.dc_shade_constants.light_blue;
		args.light_alpha = drawerargs.dc_shade_constants.light_alpha;
		args.fade_red = drawerargs.dc_shade_constants.fade_red;
		args.fade_green = drawerargs.dc_shade_constants.fade_green;
		args.fade_blue = drawerargs.dc_shade_constants.fade_blue;
		args.fade_alpha = drawerargs.dc_shade_constants.fade_alpha;
		args.desaturate = drawerargs.dc_shade_constants.desaturate;
		args.flags = 0;
		if (drawerargs.dc_shade_constants.simple_shade)
			args.flags |= DrawColumnArgs::simple_shade;
		if (args.source2 == nullptr)
			args.flags |= DrawColumnArgs::nearest_filter;
	}

	void DrawColumnLLVMCommand::Execute(DrawerThread *thread)
	{
		WorkerThreadData d = ThreadData(thread);
		Drawers::Instance()->DrawColumn(&args, &d);
	}

	/////////////////////////////////////////////////////////////////////////////

	WorkerThreadData DrawSkyLLVMCommand::ThreadData(DrawerThread *thread)
	{
		WorkerThreadData d;
		d.core = thread->core;
		d.num_cores = thread->num_cores;
		d.pass_start_y = thread->pass_start_y;
		d.pass_end_y = thread->pass_end_y;
		return d;
	}

	DrawSkyLLVMCommand::DrawSkyLLVMCommand(const SkyDrawerArgs &drawerargs, uint32_t solid_top, uint32_t solid_bottom, bool fadeSky)
	{
		args.dest = (uint32_t*)drawerargs.Dest();
		args.dest_y = drawerargs.DestY();
		args.count = drawerargs.dc_count;
		args.pitch = dc_pitch;
		args.texturefrac[0] = drawerargs.dc_wall_texturefrac;
		args.iscale[0] = drawerargs.dc_wall_iscale;
		args.source0[0] = (const uint32_t *)drawerargs.dc_wall_source;
		args.source1[0] = (const uint32_t *)drawerargs.dc_wall_source2;
		args.textureheight0 = drawerargs.dc_wall_sourceheight[0];
		args.textureheight1 = drawerargs.dc_wall_sourceheight[1];
		args.top_color = solid_top;
		args.bottom_color = solid_bottom;
		args.flags = fadeSky ? DrawSkyArgs::fade_sky : 0;
	}

	FString DrawSkyLLVMCommand::DebugInfo()
	{
		return "DrawSky\n" + args.ToString();
	}

	/////////////////////////////////////////////////////////////////////////////

	DrawFuzzColumnRGBACommand::DrawFuzzColumnRGBACommand(const ColumnDrawerArgs &drawerargs)
	{
		_x = drawerargs.dc_x;
		_yl = drawerargs.dc_yl;
		_yh = drawerargs.dc_yh;
		_destorg = dc_destorg;
		_pitch = dc_pitch;
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

		uint32_t *dest = thread->dest_for_thread(yl, _pitch, ylookup[yl] + _x + (uint32_t*)_destorg);

		int pitch = _pitch * thread->num_cores;
		int fuzzstep = thread->num_cores;
		int fuzz = (_fuzzpos + thread->skipped_by_thread(yl)) % FUZZTABLE;

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
	}

	FString DrawFuzzColumnRGBACommand::DebugInfo()
	{
		return "DrawFuzzColumn";
	}

	/////////////////////////////////////////////////////////////////////////////

	FillSpanRGBACommand::FillSpanRGBACommand(const SpanDrawerArgs &drawerargs)
	{
		_x1 = drawerargs.ds_x1;
		_x2 = drawerargs.ds_x2;
		_y = drawerargs.ds_y;
		_destorg = dc_destorg;
		_light = drawerargs.dc_light;
		_color = drawerargs.ds_color;
	}

	void FillSpanRGBACommand::Execute(DrawerThread *thread)
	{
		if (thread->line_skipped_by_thread(_y))
			return;

		uint32_t *dest = ylookup[_y] + _x1 + (uint32_t*)_destorg;
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

	DrawFogBoundaryLineRGBACommand::DrawFogBoundaryLineRGBACommand(const SpanDrawerArgs &drawerargs, int y, int x, int x2)
	{
		_y = y;
		_x = x;
		_x2 = x2;

		_destorg = dc_destorg;
		_light = drawerargs.dc_light;
		_shade_constants = drawerargs.dc_shade_constants;
	}

	void DrawFogBoundaryLineRGBACommand::Execute(DrawerThread *thread)
	{
		if (thread->line_skipped_by_thread(_y))
			return;

		int y = _y;
		int x = _x;
		int x2 = _x2;

		uint32_t *dest = ylookup[y] + (uint32_t*)_destorg;

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

	DrawTiltedSpanRGBACommand::DrawTiltedSpanRGBACommand(const SpanDrawerArgs &drawerargs, int y, int x1, int x2, const FVector3 &plane_sz, const FVector3 &plane_su, const FVector3 &plane_sv, bool plane_shade, int planeshade, float planelightfloat, fixed_t pviewx, fixed_t pviewy)
	{
		_x1 = x1;
		_x2 = x2;
		_y = y;
		_destorg = dc_destorg;
		_light = drawerargs.dc_light;
		_shade_constants = drawerargs.dc_shade_constants;
		_plane_sz = plane_sz;
		_plane_su = plane_su;
		_plane_sv = plane_sv;
		_plane_shade = plane_shade;
		_planeshade = planeshade;
		_planelightfloat = planelightfloat;
		_pviewx = pviewx;
		_pviewy = pviewy;
		_source = (const uint32_t*)drawerargs.ds_source;
		_xbits = drawerargs.ds_xbits;
		_ybits = drawerargs.ds_ybits;
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

		uint32_t *dest = ylookup[_y] + _x1 + (uint32_t*)_destorg;
		int count = _x2 - _x1 + 1;

		// Depth (Z) change across the span
		double iz = _plane_sz[2] + _plane_sz[1] * (centery - _y) + _plane_sz[0] * (_x1 - centerx);

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
		double uz = _plane_su[2] + _plane_su[1] * (centery - _y) + _plane_su[0] * (_x1 - centerx);
		double vz = _plane_sv[2] + _plane_sv[1] * (centery - _y) + _plane_sv[0] * (_x1 - centerx);
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
			uint32_t stepu = (uint32_t)(SQWORD((endu - startu) * INVSPAN));
			uint32_t stepv = (uint32_t)(SQWORD((endv - startv) * INVSPAN));
			uint32_t u = (uint32_t)(SQWORD(startu) + _pviewx);
			uint32_t v = (uint32_t)(SQWORD(startv) + _pviewy);

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
			uint32_t u = (uint32_t)(SQWORD(startu) + _pviewx);
			uint32_t v = (uint32_t)(SQWORD(startv) + _pviewy);

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

	DrawColoredSpanRGBACommand::DrawColoredSpanRGBACommand(const SpanDrawerArgs &drawerargs, int y, int x1, int x2)
	{
		_y = y;
		_x1 = x1;
		_x2 = x2;

		_destorg = dc_destorg;
		_light = drawerargs.dc_light;
		_color = drawerargs.ds_color;
	}

	void DrawColoredSpanRGBACommand::Execute(DrawerThread *thread)
	{
		if (thread->line_skipped_by_thread(_y))
			return;

		int y = _y;
		int x1 = _x1;
		int x2 = _x2;

		uint32_t *dest = ylookup[y] + x1 + (uint32_t*)_destorg;
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

	FillTransColumnRGBACommand::FillTransColumnRGBACommand(const DrawerArgs &drawerargs, int x, int y1, int y2, int color, int a)
	{
		_x = x;
		_y1 = y1;
		_y2 = y2;
		_color = color;
		_a = a;

		_destorg = dc_destorg;
		_pitch = dc_pitch;
	}

	void FillTransColumnRGBACommand::Execute(DrawerThread *thread)
	{
		int x = _x;
		int y1 = _y1;
		int y2 = _y2;
		int color = _color;
		int a = _a;

		int ycount = thread->count_for_thread(y1, y2 - y1 + 1);
		if (ycount <= 0)
			return;

		uint32_t fg = GPalette.BaseColors[color].d;
		uint32_t fg_red = (fg >> 16) & 0xff;
		uint32_t fg_green = (fg >> 8) & 0xff;
		uint32_t fg_blue = fg & 0xff;

		uint32_t alpha = a + 1;
		uint32_t inv_alpha = 256 - alpha;

		fg_red *= alpha;
		fg_green *= alpha;
		fg_blue *= alpha;

		int spacing = _pitch * thread->num_cores;
		uint32_t *dest = thread->dest_for_thread(y1, _pitch, ylookup[y1] + x + (uint32_t*)_destorg);

		for (int y = 0; y < ycount; y++)
		{
			uint32_t bg_red = (*dest >> 16) & 0xff;
			uint32_t bg_green = (*dest >> 8) & 0xff;
			uint32_t bg_blue = (*dest) & 0xff;

			uint32_t red = (fg_red + bg_red * inv_alpha) / 256;
			uint32_t green = (fg_green + bg_green * inv_alpha) / 256;
			uint32_t blue = (fg_blue + bg_blue * inv_alpha) / 256;

			*dest = 0xff000000 | (red << 16) | (green << 8) | blue;
			dest += spacing;
		}
	}

	FString FillTransColumnRGBACommand::DebugInfo()
	{
		return "FillTransColumn";
	}

	/////////////////////////////////////////////////////////////////////////////

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

		const uint32_t *source = &particle_texture[(_fracposx >> FRACBITS) * PARTICLE_TEXTURE_SIZE];
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

}
