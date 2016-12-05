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
#include "r_local.h"
#include "v_video.h"
#include "doomstat.h"
#include "st_stuff.h"
#include "g_game.h"
#include "g_level.h"
#include "r_data/r_translate.h"
#include "v_palette.h"
#include "r_data/colormaps.h"
#include "r_plane.h"
#include "r_draw_rgba.h"
#include "r_drawers.h"
#include "gl/data/gl_matrix.h"

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

extern "C" short spanend[MAXHEIGHT];
extern float rw_light;
extern float rw_lightstep;
extern int wallshade;

/////////////////////////////////////////////////////////////////////////////

class DrawSpanLLVMCommand : public DrawerCommand
{
public:
	DrawSpanLLVMCommand()
	{
		using namespace drawerargs;

		args.xfrac = ds_xfrac;
		args.yfrac = ds_yfrac;
		args.xstep = ds_xstep;
		args.ystep = ds_ystep;
		args.x1 = ds_x1;
		args.x2 = ds_x2;
		args.y = ds_y;
		args.xbits = ds_xbits;
		args.ybits = ds_ybits;
		args.destorg = (uint32_t*)dc_destorg;
		args.destpitch = dc_pitch;
		args.source = (const uint32_t*)ds_source;
		args.light = LightBgra::calc_light_multiplier(ds_light);
		args.light_red = ds_shade_constants.light_red;
		args.light_green = ds_shade_constants.light_green;
		args.light_blue = ds_shade_constants.light_blue;
		args.light_alpha = ds_shade_constants.light_alpha;
		args.fade_red = ds_shade_constants.fade_red;
		args.fade_green = ds_shade_constants.fade_green;
		args.fade_blue = ds_shade_constants.fade_blue;
		args.fade_alpha = ds_shade_constants.fade_alpha;
		args.desaturate = ds_shade_constants.desaturate;
		args.srcalpha = dc_srcalpha >> (FRACBITS - 8);
		args.destalpha = dc_destalpha >> (FRACBITS - 8);
		args.flags = 0;
		if (ds_shade_constants.simple_shade)
			args.flags |= DrawSpanArgs::simple_shade;
		if (!sampler_setup(args.source, args.xbits, args.ybits, ds_source_mipmapped))
			args.flags |= DrawSpanArgs::nearest_filter;
	}

	void Execute(DrawerThread *thread) override
	{
		if (thread->skipped_by_thread(args.y))
			return;
		Drawers::Instance()->DrawSpan(&args);
	}

	FString DebugInfo() override
	{
		return "DrawSpan\n" + args.ToString();
	}

protected:
	DrawSpanArgs args;

private:
	inline static bool sampler_setup(const uint32_t * &source, int &xbits, int &ybits, bool mipmapped)
	{
		using namespace drawerargs;

		bool magnifying = ds_lod < 0.0f;
		if (r_mipmap && mipmapped)
		{
			int level = (int)ds_lod;
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
};

class DrawSpanMaskedLLVMCommand : public DrawSpanLLVMCommand
{
public:
	void Execute(DrawerThread *thread) override
	{
		if (thread->skipped_by_thread(args.y))
			return;
		Drawers::Instance()->DrawSpanMasked(&args);
	}
};

class DrawSpanTranslucentLLVMCommand : public DrawSpanLLVMCommand
{
public:
	void Execute(DrawerThread *thread) override
	{
		if (thread->skipped_by_thread(args.y))
			return;
		Drawers::Instance()->DrawSpanTranslucent(&args);
	}
};

class DrawSpanMaskedTranslucentLLVMCommand : public DrawSpanLLVMCommand
{
public:
	void Execute(DrawerThread *thread) override
	{
		if (thread->skipped_by_thread(args.y))
			return;
		Drawers::Instance()->DrawSpanMaskedTranslucent(&args);
	}
};

class DrawSpanAddClampLLVMCommand : public DrawSpanLLVMCommand
{
public:
	void Execute(DrawerThread *thread) override
	{
		if (thread->skipped_by_thread(args.y))
			return;
		Drawers::Instance()->DrawSpanAddClamp(&args);
	}
};

class DrawSpanMaskedAddClampLLVMCommand : public DrawSpanLLVMCommand
{
public:
	void Execute(DrawerThread *thread) override
	{
		if (thread->skipped_by_thread(args.y))
			return;
		Drawers::Instance()->DrawSpanMaskedAddClamp(&args);
	}
};

/////////////////////////////////////////////////////////////////////////////

class DrawWall4LLVMCommand : public DrawerCommand
{
protected:
	DrawWallArgs args;

	WorkerThreadData ThreadData(DrawerThread *thread)
	{
		WorkerThreadData d;
		d.core = thread->core;
		d.num_cores = thread->num_cores;
		d.pass_start_y = thread->pass_start_y;
		d.pass_end_y = thread->pass_end_y;
		return d;
	}

public:
	DrawWall4LLVMCommand()
	{
		using namespace drawerargs;

		args.dest = (uint32_t*)dc_dest;
		args.dest_y = _dest_y;
		args.count = dc_count;
		args.pitch = dc_pitch;
		args.light_red = dc_shade_constants.light_red;
		args.light_green = dc_shade_constants.light_green;
		args.light_blue = dc_shade_constants.light_blue;
		args.light_alpha = dc_shade_constants.light_alpha;
		args.fade_red = dc_shade_constants.fade_red;
		args.fade_green = dc_shade_constants.fade_green;
		args.fade_blue = dc_shade_constants.fade_blue;
		args.fade_alpha = dc_shade_constants.fade_alpha;
		args.desaturate = dc_shade_constants.desaturate;
		for (int i = 0; i < 4; i++)
		{
			args.texturefrac[i] = vplce[i];
			args.iscale[i] = vince[i];
			args.texturefracx[i] = buftexturefracx[i];
			args.textureheight[i] = bufheight[i];
			args.source[i] = (const uint32_t *)bufplce[i];
			args.source2[i] = (const uint32_t *)bufplce2[i];
			args.light[i] = LightBgra::calc_light_multiplier(palookuplight[i]);
		}
		args.srcalpha = dc_srcalpha >> (FRACBITS - 8);
		args.destalpha = dc_destalpha >> (FRACBITS - 8);
		args.flags = 0;
		if (dc_shade_constants.simple_shade)
			args.flags |= DrawWallArgs::simple_shade;
		if (args.source2[0] == nullptr)
			args.flags |= DrawWallArgs::nearest_filter;

		DetectRangeError(args.dest, args.dest_y, args.count);
	}

	void Execute(DrawerThread *thread) override
	{
		WorkerThreadData d = ThreadData(thread);
		Drawers::Instance()->vlinec4(&args, &d);
	}

	FString DebugInfo() override
	{
		return "DrawWall4\n" + args.ToString();
	}
};

class DrawWall1LLVMCommand : public DrawerCommand
{
protected:
	DrawWallArgs args;

	WorkerThreadData ThreadData(DrawerThread *thread)
	{
		WorkerThreadData d;
		d.core = thread->core;
		d.num_cores = thread->num_cores;
		d.pass_start_y = thread->pass_start_y;
		d.pass_end_y = thread->pass_end_y;
		return d;
	}

public:
	DrawWall1LLVMCommand()
	{
		using namespace drawerargs;

		args.dest = (uint32_t*)dc_dest;
		args.dest_y = _dest_y;
		args.pitch = dc_pitch;
		args.count = dc_count;
		args.texturefrac[0] = dc_texturefrac;
		args.texturefracx[0] = dc_texturefracx;
		args.iscale[0] = dc_iscale;
		args.textureheight[0] = dc_textureheight;
		args.source[0] = (const uint32 *)dc_source;
		args.source2[0] = (const uint32 *)dc_source2;
		args.light[0] = LightBgra::calc_light_multiplier(dc_light);
		args.light_red = dc_shade_constants.light_red;
		args.light_green = dc_shade_constants.light_green;
		args.light_blue = dc_shade_constants.light_blue;
		args.light_alpha = dc_shade_constants.light_alpha;
		args.fade_red = dc_shade_constants.fade_red;
		args.fade_green = dc_shade_constants.fade_green;
		args.fade_blue = dc_shade_constants.fade_blue;
		args.fade_alpha = dc_shade_constants.fade_alpha;
		args.desaturate = dc_shade_constants.desaturate;
		args.srcalpha = dc_srcalpha >> (FRACBITS - 8);
		args.destalpha = dc_destalpha >> (FRACBITS - 8);
		args.flags = 0;
		if (dc_shade_constants.simple_shade)
			args.flags |= DrawWallArgs::simple_shade;
		if (args.source2[0] == nullptr)
			args.flags |= DrawWallArgs::nearest_filter;

		DetectRangeError(args.dest, args.dest_y, args.count);
	}

	void Execute(DrawerThread *thread) override
	{
		WorkerThreadData d = ThreadData(thread);
		Drawers::Instance()->vlinec1(&args, &d);
	}

	FString DebugInfo() override
	{
		return "DrawWall1\n" + args.ToString();
	}
};

class DrawColumnLLVMCommand : public DrawerCommand
{
protected:
	DrawColumnArgs args;

	WorkerThreadData ThreadData(DrawerThread *thread)
	{
		WorkerThreadData d;
		d.core = thread->core;
		d.num_cores = thread->num_cores;
		d.pass_start_y = thread->pass_start_y;
		d.pass_end_y = thread->pass_end_y;
		return d;
	}

	FString DebugInfo() override
	{
		return "DrawColumn\n" + args.ToString();
	}

public:
	DrawColumnLLVMCommand()
	{
		using namespace drawerargs;

		args.dest = (uint32_t*)dc_dest;
		args.source = dc_source;
		args.source2 = dc_source2;
		args.colormap = dc_colormap;
		args.translation = dc_translation;
		args.basecolors = (const uint32_t *)GPalette.BaseColors;
		args.pitch = dc_pitch;
		args.count = dc_count;
		args.dest_y = _dest_y;
		args.iscale = dc_iscale;
		args.texturefracx = dc_texturefracx;
		args.textureheight = dc_textureheight;
		args.texturefrac = dc_texturefrac;
		args.light = LightBgra::calc_light_multiplier(dc_light);
		args.color = LightBgra::shade_pal_index_simple(dc_color, args.light);
		args.srccolor = dc_srccolor_bgra;
		args.srcalpha = dc_srcalpha >> (FRACBITS - 8);
		args.destalpha = dc_destalpha >> (FRACBITS - 8);
		args.light_red = dc_shade_constants.light_red;
		args.light_green = dc_shade_constants.light_green;
		args.light_blue = dc_shade_constants.light_blue;
		args.light_alpha = dc_shade_constants.light_alpha;
		args.fade_red = dc_shade_constants.fade_red;
		args.fade_green = dc_shade_constants.fade_green;
		args.fade_blue = dc_shade_constants.fade_blue;
		args.fade_alpha = dc_shade_constants.fade_alpha;
		args.desaturate = dc_shade_constants.desaturate;
		args.flags = 0;
		if (dc_shade_constants.simple_shade)
			args.flags |= DrawColumnArgs::simple_shade;
		if (args.source2 == nullptr)
			args.flags |= DrawColumnArgs::nearest_filter;

		DetectRangeError(args.dest, args.dest_y, args.count);
	}

	void Execute(DrawerThread *thread) override
	{
		WorkerThreadData d = ThreadData(thread);
		Drawers::Instance()->DrawColumn(&args, &d);
	}
};

class DrawSkyLLVMCommand : public DrawerCommand
{
protected:
	DrawSkyArgs args;

	WorkerThreadData ThreadData(DrawerThread *thread)
	{
		WorkerThreadData d;
		d.core = thread->core;
		d.num_cores = thread->num_cores;
		d.pass_start_y = thread->pass_start_y;
		d.pass_end_y = thread->pass_end_y;
		return d;
	}

public:
	DrawSkyLLVMCommand(uint32_t solid_top, uint32_t solid_bottom)
	{
		using namespace drawerargs;

		args.dest = (uint32_t*)dc_dest;
		args.dest_y = _dest_y;
		args.count = dc_count;
		args.pitch = dc_pitch;
		for (int i = 0; i < 4; i++)
		{
			args.texturefrac[i] = vplce[i];
			args.iscale[i] = vince[i];
			args.source0[i] = (const uint32_t *)bufplce[i];
			args.source1[i] = (const uint32_t *)bufplce2[i];
		}
		args.textureheight0 = bufheight[0];
		args.textureheight1 = bufheight[1];
		args.top_color = solid_top;
		args.bottom_color = solid_bottom;

		DetectRangeError(args.dest, args.dest_y, args.count);
	}

	FString DebugInfo() override
	{
		return "DrawSky\n" + args.ToString();
	}
};

#define DECLARE_DRAW_COMMAND(name, func, base) \
class name##LLVMCommand : public base \
{ \
public: \
	using base::base; \
	void Execute(DrawerThread *thread) override \
	{ \
		WorkerThreadData d = ThreadData(thread); \
		Drawers::Instance()->func(&args, &d); \
	} \
};

//DECLARE_DRAW_COMMAND(name, func, DrawSpanLLVMCommand);

DECLARE_DRAW_COMMAND(DrawWallMasked4, mvlinec4, DrawWall4LLVMCommand);
DECLARE_DRAW_COMMAND(DrawWallAdd4, tmvline4_add, DrawWall4LLVMCommand);
DECLARE_DRAW_COMMAND(DrawWallAddClamp4, tmvline4_addclamp, DrawWall4LLVMCommand);
DECLARE_DRAW_COMMAND(DrawWallSubClamp4, tmvline4_subclamp, DrawWall4LLVMCommand);
DECLARE_DRAW_COMMAND(DrawWallRevSubClamp4, tmvline4_revsubclamp, DrawWall4LLVMCommand);
DECLARE_DRAW_COMMAND(DrawWallMasked1, mvlinec1, DrawWall1LLVMCommand);
DECLARE_DRAW_COMMAND(DrawWallAdd1, tmvline1_add, DrawWall1LLVMCommand);
DECLARE_DRAW_COMMAND(DrawWallAddClamp1, tmvline1_addclamp, DrawWall1LLVMCommand);
DECLARE_DRAW_COMMAND(DrawWallSubClamp1, tmvline1_subclamp, DrawWall1LLVMCommand);
DECLARE_DRAW_COMMAND(DrawWallRevSubClamp1, tmvline1_revsubclamp, DrawWall1LLVMCommand);
DECLARE_DRAW_COMMAND(DrawColumnAdd, DrawColumnAdd, DrawColumnLLVMCommand);
DECLARE_DRAW_COMMAND(DrawColumnTranslated, DrawColumnTranslated, DrawColumnLLVMCommand);
DECLARE_DRAW_COMMAND(DrawColumnTlatedAdd, DrawColumnTlatedAdd, DrawColumnLLVMCommand);
DECLARE_DRAW_COMMAND(DrawColumnShaded, DrawColumnShaded, DrawColumnLLVMCommand);
DECLARE_DRAW_COMMAND(DrawColumnAddClamp, DrawColumnAddClamp, DrawColumnLLVMCommand);
DECLARE_DRAW_COMMAND(DrawColumnAddClampTranslated, DrawColumnAddClampTranslated, DrawColumnLLVMCommand);
DECLARE_DRAW_COMMAND(DrawColumnSubClamp, DrawColumnSubClamp, DrawColumnLLVMCommand);
DECLARE_DRAW_COMMAND(DrawColumnSubClampTranslated, DrawColumnSubClampTranslated, DrawColumnLLVMCommand);
DECLARE_DRAW_COMMAND(DrawColumnRevSubClamp, DrawColumnRevSubClamp, DrawColumnLLVMCommand);
DECLARE_DRAW_COMMAND(DrawColumnRevSubClampTranslated, DrawColumnRevSubClampTranslated, DrawColumnLLVMCommand);
DECLARE_DRAW_COMMAND(FillColumn, FillColumn, DrawColumnLLVMCommand);
DECLARE_DRAW_COMMAND(FillColumnAdd, FillColumnAdd, DrawColumnLLVMCommand);
DECLARE_DRAW_COMMAND(FillColumnAddClamp, FillColumnAddClamp, DrawColumnLLVMCommand);
DECLARE_DRAW_COMMAND(FillColumnSubClamp, FillColumnSubClamp, DrawColumnLLVMCommand);
DECLARE_DRAW_COMMAND(FillColumnRevSubClamp, FillColumnRevSubClamp, DrawColumnLLVMCommand);
DECLARE_DRAW_COMMAND(DrawSingleSky1, DrawSky1, DrawSkyLLVMCommand);
DECLARE_DRAW_COMMAND(DrawSingleSky4, DrawSky4, DrawSkyLLVMCommand);
DECLARE_DRAW_COMMAND(DrawDoubleSky1, DrawDoubleSky1, DrawSkyLLVMCommand);
DECLARE_DRAW_COMMAND(DrawDoubleSky4, DrawDoubleSky4, DrawSkyLLVMCommand);

/////////////////////////////////////////////////////////////////////////////

class DrawFuzzColumnRGBACommand : public DrawerCommand
{
	int _x;
	int _yl;
	int _yh;
	BYTE * RESTRICT _destorg;
	int _pitch;
	int _fuzzpos;
	int _fuzzviewheight;

public:
	DrawFuzzColumnRGBACommand()
	{
		using namespace drawerargs;

		_x = dc_x;
		_yl = dc_yl;
		_yh = dc_yh;
		_destorg = dc_destorg;
		_pitch = dc_pitch;
		_fuzzpos = fuzzpos;
		_fuzzviewheight = fuzzviewheight;
	}

	void Execute(DrawerThread *thread) override
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

	FString DebugInfo() override
	{
		return "DrawFuzzColumn";
	}
};

class FillSpanRGBACommand : public DrawerCommand
{
	int _x1;
	int _x2;
	int _y;
	BYTE * RESTRICT _destorg;
	fixed_t _light;
	int _color;

public:
	FillSpanRGBACommand()
	{
		using namespace drawerargs;

		_x1 = ds_x1;
		_x2 = ds_x2;
		_y = ds_y;
		_destorg = dc_destorg;
		_light = ds_light;
		_color = ds_color;
	}

	void Execute(DrawerThread *thread) override
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

	FString DebugInfo() override
	{
		return "FillSpan";
	}
};

/////////////////////////////////////////////////////////////////////////////

class DrawSlabRGBACommand : public DrawerCommand
{
	int _dx;
	fixed_t _v;
	int _dy;
	fixed_t _vi;
	const BYTE *_voxelptr;
	uint32_t *_p;
	ShadeConstants _shade_constants;
	const BYTE *_colormap;
	fixed_t _light;
	int _pitch;
	int _start_y;

public:
	DrawSlabRGBACommand(int dx, fixed_t v, int dy, fixed_t vi, const BYTE *vptr, BYTE *p, ShadeConstants shade_constants, const BYTE *colormap, fixed_t light)
	{
		using namespace drawerargs;

		_dx = dx;
		_v = v;
		_dy = dy;
		_vi = vi;
		_voxelptr = vptr;
		_p = (uint32_t *)p;
		_shade_constants = shade_constants;
		_colormap = colormap;
		_light = light;
		_pitch = dc_pitch;
		_start_y = static_cast<int>((p - dc_destorg) / (dc_pitch * 4));
		assert(dx > 0);
	}

	void Execute(DrawerThread *thread) override
	{
		int dx = _dx;
		fixed_t v = _v;
		int dy = _dy;
		fixed_t vi = _vi;
		const BYTE *vptr = _voxelptr;
		uint32_t *p = _p;
		ShadeConstants shade_constants = _shade_constants;
		const BYTE *colormap = _colormap;
		uint32_t light = LightBgra::calc_light_multiplier(_light);
		int pitch = _pitch;
		int x;

		dy = thread->count_for_thread(_start_y, dy);
		p = thread->dest_for_thread(_start_y, pitch, p);
		v += vi * thread->skipped_by_thread(_start_y);
		vi *= thread->num_cores;
		pitch *= thread->num_cores;

		if (dx == 1)
		{
			while (dy > 0)
			{
				*p = LightBgra::shade_pal_index(colormap[vptr[v >> FRACBITS]], light, shade_constants);
				p += pitch;
				v += vi;
				dy--;
			}
		}
		else if (dx == 2)
		{
			while (dy > 0)
			{
				uint32_t color = LightBgra::shade_pal_index(colormap[vptr[v >> FRACBITS]], light, shade_constants);
				p[0] = color;
				p[1] = color;
				p += pitch;
				v += vi;
				dy--;
			}
		}
		else if (dx == 3)
		{
			while (dy > 0)
			{
				uint32_t color = LightBgra::shade_pal_index(colormap[vptr[v >> FRACBITS]], light, shade_constants);
				p[0] = color;
				p[1] = color;
				p[2] = color;
				p += pitch;
				v += vi;
				dy--;
			}
		}
		else if (dx == 4)
		{
			while (dy > 0)
			{
				uint32_t color = LightBgra::shade_pal_index(colormap[vptr[v >> FRACBITS]], light, shade_constants);
				p[0] = color;
				p[1] = color;
				p[2] = color;
				p[3] = color;
				p += pitch;
				v += vi;
				dy--;
			}
		}
		else while (dy > 0)
		{
			uint32_t color = LightBgra::shade_pal_index(colormap[vptr[v >> FRACBITS]], light, shade_constants);
			// The optimizer will probably turn this into a memset call.
			// Since dx is not likely to be large, I'm not sure that's a good thing,
			// hence the alternatives above.
			for (x = 0; x < dx; x++)
			{
				p[x] = color;
			}
			p += pitch;
			v += vi;
			dy--;
		}
	}

	FString DebugInfo() override
	{
		return "DrawSlab";
	}
};

/////////////////////////////////////////////////////////////////////////////

class DrawFogBoundaryLineRGBACommand : public DrawerCommand
{
	int _y;
	int _x;
	int _x2;
	BYTE * RESTRICT _destorg;
	fixed_t _light;
	ShadeConstants _shade_constants;

public:
	DrawFogBoundaryLineRGBACommand(int y, int x, int x2)
	{
		using namespace drawerargs;

		_y = y;
		_x = x;
		_x2 = x2;

		_destorg = dc_destorg;
		_light = dc_light;
		_shade_constants = dc_shade_constants;
	}

	void Execute(DrawerThread *thread) override
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

	FString DebugInfo() override
	{
		return "DrawFogBoundaryLine";
	}
};

class DrawTiltedSpanRGBACommand : public DrawerCommand
{
	int _x1;
	int _x2;
	int _y;
	BYTE * RESTRICT _destorg;
	fixed_t _light;
	ShadeConstants _shade_constants;
	FVector3 _plane_sz;
	FVector3 _plane_su;
	FVector3 _plane_sv;
	bool _plane_shade;
	int _planeshade;
	float _planelightfloat;
	fixed_t _pviewx;
	fixed_t _pviewy;
	int _xbits;
	int _ybits;
	const uint32_t * RESTRICT _source;

public:
	DrawTiltedSpanRGBACommand(int y, int x1, int x2, const FVector3 &plane_sz, const FVector3 &plane_su, const FVector3 &plane_sv, bool plane_shade, int planeshade, float planelightfloat, fixed_t pviewx, fixed_t pviewy)
	{
		using namespace drawerargs;

		_x1 = x1;
		_x2 = x2;
		_y = y;
		_destorg = dc_destorg;
		_light = ds_light;
		_shade_constants = ds_shade_constants;
		_plane_sz = plane_sz;
		_plane_su = plane_su;
		_plane_sv = plane_sv;
		_plane_shade = plane_shade;
		_planeshade = planeshade;
		_planelightfloat = planelightfloat;
		_pviewx = pviewx;
		_pviewy = pviewy;
		_source = (const uint32_t*)ds_source;
		_xbits = ds_xbits;
		_ybits = ds_ybits;
	}

	void Execute(DrawerThread *thread) override
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

	FString DebugInfo() override
	{
		return "DrawTiltedSpan";
	}
};

class DrawColoredSpanRGBACommand : public DrawerCommand
{
	int _y;
	int _x1;
	int _x2;
	BYTE * RESTRICT _destorg;
	fixed_t _light;
	int _color;

public:
	DrawColoredSpanRGBACommand(int y, int x1, int x2)
	{
		using namespace drawerargs;

		_y = y;
		_x1 = x1;
		_x2 = x2;

		_destorg = dc_destorg;
		_light = ds_light;
		_color = ds_color;
	}

	void Execute(DrawerThread *thread) override
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

	FString DebugInfo() override
	{
		return "DrawColoredSpan";
	}
};

class FillTransColumnRGBACommand : public DrawerCommand
{
	int _x;
	int _y1;
	int _y2;
	int _color;
	int _a;
	BYTE * RESTRICT _destorg;
	int _pitch;
	fixed_t _light;

public:
	FillTransColumnRGBACommand(int x, int y1, int y2, int color, int a)
	{
		using namespace drawerargs;

		_x = x;
		_y1 = y1;
		_y2 = y2;
		_color = color;
		_a = a;

		_destorg = dc_destorg;
		_pitch = dc_pitch;
	}

	void Execute(DrawerThread *thread) override
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

	FString DebugInfo() override
	{
		return "FillTransColumn";
	}
};

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
		BYTE *pixels = buffer + y * pitch * 4;
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

			pixels[0] = (BYTE)blue;
			pixels[1] = (BYTE)green;
			pixels[2] = (BYTE)red;
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
		BYTE *pixels = buffer + y * pitch * 4;
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

			pixels[0] = (BYTE)blue;
			pixels[1] = (BYTE)green;
			pixels[2] = (BYTE)red;
			pixels[3] = 0xff;

			pixels += 4;
		}

		y += thread->num_cores;
		count--;
	}
}
#endif

/////////////////////////////////////////////////////////////////////////////

void R_DrawSingleSkyCol1_rgba(uint32_t solid_top, uint32_t solid_bottom)
{
	DrawerCommandQueue::QueueCommand<DrawSingleSky1LLVMCommand>(solid_top, solid_bottom);
}

void R_DrawSingleSkyCol4_rgba(uint32_t solid_top, uint32_t solid_bottom)
{
	DrawerCommandQueue::QueueCommand<DrawSingleSky4LLVMCommand>(solid_top, solid_bottom);
}

void R_DrawDoubleSkyCol1_rgba(uint32_t solid_top, uint32_t solid_bottom)
{
	DrawerCommandQueue::QueueCommand<DrawDoubleSky1LLVMCommand>(solid_top, solid_bottom);
}

void R_DrawDoubleSkyCol4_rgba(uint32_t solid_top, uint32_t solid_bottom)
{
	DrawerCommandQueue::QueueCommand<DrawDoubleSky4LLVMCommand>(solid_top, solid_bottom);
}

void R_DrawColumn_rgba()
{
	DrawerCommandQueue::QueueCommand<DrawColumnLLVMCommand>();
}

void R_FillColumn_rgba()
{
	DrawerCommandQueue::QueueCommand<FillColumnLLVMCommand>();
}

void R_FillAddColumn_rgba()
{
	DrawerCommandQueue::QueueCommand<FillColumnAddLLVMCommand>();
}

void R_FillAddClampColumn_rgba()
{
	DrawerCommandQueue::QueueCommand<FillColumnAddClampLLVMCommand>();
}

void R_FillSubClampColumn_rgba()
{
	DrawerCommandQueue::QueueCommand<FillColumnSubClampLLVMCommand>();
}

void R_FillRevSubClampColumn_rgba()
{
	DrawerCommandQueue::QueueCommand<FillColumnRevSubClampLLVMCommand>();
}

void R_DrawFuzzColumn_rgba()
{
	using namespace drawerargs;

	DrawerCommandQueue::QueueCommand<DrawFuzzColumnRGBACommand>();

	dc_yl = MAX(dc_yl, 1);
	dc_yh = MIN(dc_yh, fuzzviewheight);
	if (dc_yl <= dc_yh)
		fuzzpos = (fuzzpos + dc_yh - dc_yl + 1) % FUZZTABLE;
}

void R_DrawAddColumn_rgba()
{
	DrawerCommandQueue::QueueCommand<DrawColumnAddLLVMCommand>();
}

void R_DrawTranslatedColumn_rgba()
{
	DrawerCommandQueue::QueueCommand<DrawColumnTranslatedLLVMCommand>();
}

void R_DrawTlatedAddColumn_rgba()
{
	DrawerCommandQueue::QueueCommand<DrawColumnTlatedAddLLVMCommand>();
}

void R_DrawShadedColumn_rgba()
{
	DrawerCommandQueue::QueueCommand<DrawColumnShadedLLVMCommand>();
}

void R_DrawAddClampColumn_rgba()
{
	DrawerCommandQueue::QueueCommand<DrawColumnAddClampLLVMCommand>();
}

void R_DrawAddClampTranslatedColumn_rgba()
{
	DrawerCommandQueue::QueueCommand<DrawColumnAddClampTranslatedLLVMCommand>();
}

void R_DrawSubClampColumn_rgba()
{
	DrawerCommandQueue::QueueCommand<DrawColumnSubClampLLVMCommand>();
}

void R_DrawSubClampTranslatedColumn_rgba()
{
	DrawerCommandQueue::QueueCommand<DrawColumnSubClampTranslatedLLVMCommand>();
}

void R_DrawRevSubClampColumn_rgba()
{
	DrawerCommandQueue::QueueCommand<DrawColumnRevSubClampLLVMCommand>();
}

void R_DrawRevSubClampTranslatedColumn_rgba()
{
	DrawerCommandQueue::QueueCommand<DrawColumnRevSubClampTranslatedLLVMCommand>();
}

void R_DrawSpan_rgba()
{
	DrawerCommandQueue::QueueCommand<DrawSpanLLVMCommand>();
}

void R_DrawSpanMasked_rgba()
{
	DrawerCommandQueue::QueueCommand<DrawSpanMaskedLLVMCommand>();
}

void R_DrawSpanTranslucent_rgba()
{
	DrawerCommandQueue::QueueCommand<DrawSpanTranslucentLLVMCommand>();
}

void R_DrawSpanMaskedTranslucent_rgba()
{
	DrawerCommandQueue::QueueCommand<DrawSpanMaskedTranslucentLLVMCommand>();
}

void R_DrawSpanAddClamp_rgba()
{
	DrawerCommandQueue::QueueCommand<DrawSpanAddClampLLVMCommand>();
}

void R_DrawSpanMaskedAddClamp_rgba()
{
	DrawerCommandQueue::QueueCommand<DrawSpanMaskedAddClampLLVMCommand>();
}

void R_FillSpan_rgba()
{
	DrawerCommandQueue::QueueCommand<FillSpanRGBACommand>();
}

void R_DrawTiltedSpan_rgba(int y, int x1, int x2, const FVector3 &plane_sz, const FVector3 &plane_su, const FVector3 &plane_sv, bool plane_shade, int planeshade, float planelightfloat, fixed_t pviewx, fixed_t pviewy)
{
	DrawerCommandQueue::QueueCommand<DrawTiltedSpanRGBACommand>(y, x1, x2, plane_sz, plane_su, plane_sv, plane_shade, planeshade, planelightfloat, pviewx, pviewy);
}

void R_DrawColoredSpan_rgba(int y, int x1, int x2)
{
	DrawerCommandQueue::QueueCommand<DrawColoredSpanRGBACommand>(y, x1, x2);
}

static ShadeConstants slab_rgba_shade_constants;
static const BYTE *slab_rgba_colormap;
static fixed_t slab_rgba_light;

void R_SetupDrawSlab_rgba(FSWColormap *base_colormap, float light, int shade)
{
	slab_rgba_shade_constants.light_red = base_colormap->Color.r * 256 / 255;
	slab_rgba_shade_constants.light_green = base_colormap->Color.g * 256 / 255;
	slab_rgba_shade_constants.light_blue = base_colormap->Color.b * 256 / 255;
	slab_rgba_shade_constants.light_alpha = base_colormap->Color.a * 256 / 255;
	slab_rgba_shade_constants.fade_red = base_colormap->Fade.r;
	slab_rgba_shade_constants.fade_green = base_colormap->Fade.g;
	slab_rgba_shade_constants.fade_blue = base_colormap->Fade.b;
	slab_rgba_shade_constants.fade_alpha = base_colormap->Fade.a;
	slab_rgba_shade_constants.desaturate = MIN(abs(base_colormap->Desaturate), 255) * 255 / 256;
	slab_rgba_shade_constants.simple_shade = (base_colormap->Color.d == 0x00ffffff && base_colormap->Fade.d == 0x00000000 && base_colormap->Desaturate == 0);
	slab_rgba_colormap = base_colormap->Maps;
	slab_rgba_light = LIGHTSCALE(light, shade);
}

void R_DrawSlab_rgba(int dx, fixed_t v, int dy, fixed_t vi, const BYTE *vptr, BYTE *p)
{
	DrawerCommandQueue::QueueCommand<DrawSlabRGBACommand>(dx, v, dy, vi, vptr, p, slab_rgba_shade_constants, slab_rgba_colormap, slab_rgba_light);
}

DWORD vlinec1_rgba()
{
	using namespace drawerargs;

	DrawerCommandQueue::QueueCommand<DrawWall1LLVMCommand>();
	return dc_texturefrac + dc_count * dc_iscale;
}

void vlinec4_rgba()
{
	using namespace drawerargs;

	DrawerCommandQueue::QueueCommand<DrawWall4LLVMCommand>();
	for (int i = 0; i < 4; i++)
		vplce[i] += vince[i] * dc_count;
}

DWORD mvlinec1_rgba()
{
	using namespace drawerargs;

	DrawerCommandQueue::QueueCommand<DrawWallMasked1LLVMCommand>();
	return dc_texturefrac + dc_count * dc_iscale;
}

void mvlinec4_rgba()
{
	using namespace drawerargs;

	DrawerCommandQueue::QueueCommand<DrawWallMasked4LLVMCommand>();
	for (int i = 0; i < 4; i++)
		vplce[i] += vince[i] * dc_count;
}

fixed_t tmvline1_add_rgba()
{
	using namespace drawerargs;

	DrawerCommandQueue::QueueCommand<DrawWallAdd1LLVMCommand>();
	return dc_texturefrac + dc_count * dc_iscale;
}

void tmvline4_add_rgba()
{
	using namespace drawerargs;

	DrawerCommandQueue::QueueCommand<DrawWallAdd4LLVMCommand>();
	for (int i = 0; i < 4; i++)
		vplce[i] += vince[i] * dc_count;
}

fixed_t tmvline1_addclamp_rgba()
{
	using namespace drawerargs;

	DrawerCommandQueue::QueueCommand<DrawWallAddClamp1LLVMCommand>();
	return dc_texturefrac + dc_count * dc_iscale;
}

void tmvline4_addclamp_rgba()
{
	using namespace drawerargs;

	DrawerCommandQueue::QueueCommand<DrawWallAddClamp4LLVMCommand>();
	for (int i = 0; i < 4; i++)
		vplce[i] += vince[i] * dc_count;
}

fixed_t tmvline1_subclamp_rgba()
{
	using namespace drawerargs;

	DrawerCommandQueue::QueueCommand<DrawWallSubClamp1LLVMCommand>();
	return dc_texturefrac + dc_count * dc_iscale;
}

void tmvline4_subclamp_rgba()
{
	using namespace drawerargs;

	DrawerCommandQueue::QueueCommand<DrawWallSubClamp4LLVMCommand>();
	for (int i = 0; i < 4; i++)
		vplce[i] += vince[i] * dc_count;
}

fixed_t tmvline1_revsubclamp_rgba()
{
	using namespace drawerargs;

	DrawerCommandQueue::QueueCommand<DrawWallRevSubClamp1LLVMCommand>();
	return dc_texturefrac + dc_count * dc_iscale;
}

void tmvline4_revsubclamp_rgba()
{
	using namespace drawerargs;

	DrawerCommandQueue::QueueCommand<DrawWallRevSubClamp4LLVMCommand>();
	for (int i = 0; i < 4; i++)
		vplce[i] += vince[i] * dc_count;
}

void R_DrawFogBoundarySection_rgba(int y, int y2, int x1)
{
	for (; y < y2; ++y)
	{
		int x2 = spanend[y];
		DrawerCommandQueue::QueueCommand<DrawFogBoundaryLineRGBACommand>(y, x1, x2);
	}
}

void R_DrawFogBoundary_rgba(int x1, int x2, short *uclip, short *dclip)
{
	// To do: we do not need to create new spans when using rgba output - instead we should calculate light on a per pixel basis

	// This is essentially the same as R_MapVisPlane but with an extra step
	// to create new horizontal spans whenever the light changes enough that
	// we need to use a new colormap.

	double lightstep = rw_lightstep;
	double light = rw_light + rw_lightstep*(x2 - x1 - 1);
	int x = x2 - 1;
	int t2 = uclip[x];
	int b2 = dclip[x];
	int rcolormap = GETPALOOKUP(light, wallshade);
	int lcolormap;
	BYTE *basecolormapdata = basecolormap->Maps;

	if (b2 > t2)
	{
		clearbufshort(spanend + t2, b2 - t2, x);
	}

	R_SetColorMapLight(basecolormap, (float)light, wallshade);

	BYTE *fake_dc_colormap = basecolormap->Maps + (GETPALOOKUP(light, wallshade) << COLORMAPSHIFT);

	for (--x; x >= x1; --x)
	{
		int t1 = uclip[x];
		int b1 = dclip[x];
		const int xr = x + 1;
		int stop;

		light -= rw_lightstep;
		lcolormap = GETPALOOKUP(light, wallshade);
		if (lcolormap != rcolormap)
		{
			if (t2 < b2 && rcolormap != 0)
			{ // Colormap 0 is always the identity map, so rendering it is
			  // just a waste of time.
				R_DrawFogBoundarySection_rgba(t2, b2, xr);
			}
			if (t1 < t2) t2 = t1;
			if (b1 > b2) b2 = b1;
			if (t2 < b2)
			{
				clearbufshort(spanend + t2, b2 - t2, x);
			}
			rcolormap = lcolormap;
			R_SetColorMapLight(basecolormap, (float)light, wallshade);
			fake_dc_colormap = basecolormap->Maps + (GETPALOOKUP(light, wallshade) << COLORMAPSHIFT);
		}
		else
		{
			if (fake_dc_colormap != basecolormapdata)
			{
				stop = MIN(t1, b2);
				while (t2 < stop)
				{
					int y = t2++;
					DrawerCommandQueue::QueueCommand<DrawFogBoundaryLineRGBACommand>(y, xr, spanend[y]);
				}
				stop = MAX(b1, t2);
				while (b2 > stop)
				{
					int y = --b2;
					DrawerCommandQueue::QueueCommand<DrawFogBoundaryLineRGBACommand>(y, xr, spanend[y]);
				}
			}
			else
			{
				t2 = MAX(t2, MIN(t1, b2));
				b2 = MIN(b2, MAX(b1, t2));
			}

			stop = MIN(t2, b1);
			while (t1 < stop)
			{
				spanend[t1++] = x;
			}
			stop = MAX(b2, t2);
			while (b1 > stop)
			{
				spanend[--b1] = x;
			}
		}

		t2 = uclip[x];
		b2 = dclip[x];
	}
	if (t2 < b2 && rcolormap != 0)
	{
		R_DrawFogBoundarySection_rgba(t2, b2, x1);
	}
}

}
