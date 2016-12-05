/*
**  Drawer commands for the RT family of drawers
**  Copyright (c) 2016 Magnus Norddahl
**
**  This software is provided 'as-is', without any express or implied
**  warranty.  In no event will the authors be held liable for any damages
**  arising from the use of this software.
**
**  Permission is granted to anyone to use this software for any purpose,
**  including commercial applications, and to alter it and redistribute it
**  freely, subject to the following restrictions:
**
**  1. The origin of this software must not be misrepresented; you must not
**     claim that you wrote the original software. If you use this software
**     in a product, an acknowledgment in the product documentation would be
**     appreciated but is not required.
**  2. Altered source versions must be plainly marked as such, and must not be
**     misrepresented as being the original software.
**  3. This notice may not be removed or altered from any source distribution.
**
*/

#include "templates.h"
#include "doomtype.h"
#include "doomdef.h"
#include "r_defs.h"
#include "r_draw.h"
#include "r_main.h"
#include "r_things.h"
#include "v_video.h"
#include "r_draw_rgba.h"
#include "r_drawers.h"

namespace swrenderer       
{
	WorkerThreadData DrawColumnRt1LLVMCommand::ThreadData(DrawerThread *thread)
	{
		WorkerThreadData d;
		d.core = thread->core;
		d.num_cores = thread->num_cores;
		d.pass_start_y = thread->pass_start_y;
		d.pass_end_y = thread->pass_end_y;
		d.temp = thread->dc_temp_rgba;
		return d;
	}

	DrawColumnRt1LLVMCommand::DrawColumnRt1LLVMCommand(int hx, int sx, int yl, int yh)
	{
		using namespace drawerargs;

		args.dest = (uint32_t*)dc_destorg + ylookup[yl] + sx;
		args.source = nullptr;
		args.source2 = nullptr;
		args.colormap = dc_colormap;
		args.translation = dc_translation;
		args.basecolors = (const uint32_t *)GPalette.BaseColors;
		args.pitch = dc_pitch;
		args.count = yh - yl + 1;
		args.dest_y = yl;
		args.iscale = dc_iscale;
		args.texturefrac = hx;
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

	void DrawColumnRt1LLVMCommand::Execute(DrawerThread *thread)
	{
		WorkerThreadData d = ThreadData(thread);
		Drawers::Instance()->DrawColumnRt1(&args, &d);
	}

	FString DrawColumnRt1LLVMCommand::DebugInfo()
	{
		return "DrawColumnRt\n" + args.ToString();
	}

	/////////////////////////////////////////////////////////////////////////////

	RtInitColsRGBACommand::RtInitColsRGBACommand(BYTE *buff)
	{
		this->buff = buff;
	}

	void RtInitColsRGBACommand::Execute(DrawerThread *thread)
	{
		thread->dc_temp_rgba = buff == NULL ? thread->dc_temp_rgbabuff_rgba : (uint32_t*)buff;
	}

	FString RtInitColsRGBACommand::DebugInfo()
	{
		return "RtInitCols";
	}

	/////////////////////////////////////////////////////////////////////////////

	template<typename InputPixelType>
	DrawColumnHorizRGBACommand<InputPixelType>::DrawColumnHorizRGBACommand()
	{
		using namespace drawerargs;

		_count = dc_count;
		_iscale = dc_iscale;
		_texturefrac = dc_texturefrac;
		_source = (const InputPixelType *)dc_source;
		_x = dc_x;
		_yl = dc_yl;
		_yh = dc_yh;
	}

	template<typename InputPixelType>
	void DrawColumnHorizRGBACommand<InputPixelType>::Execute(DrawerThread *thread)
	{
		int count = _count;
		uint32_t *dest;
		fixed_t fracstep;
		fixed_t frac;

		if (count <= 0)
			return;

		{
			int x = _x & 3;
			dest = &thread->dc_temp_rgba[x + 4 * _yl];
		}
		fracstep = _iscale;
		frac = _texturefrac;

		const InputPixelType *source = _source;

		if (count & 1) {
			*dest = source[frac >> FRACBITS]; dest += 4; frac += fracstep;
		}
		if (count & 2) {
			dest[0] = source[frac >> FRACBITS]; frac += fracstep;
			dest[4] = source[frac >> FRACBITS]; frac += fracstep;
			dest += 8;
		}
		if (count & 4) {
			dest[0] = source[frac >> FRACBITS]; frac += fracstep;
			dest[4] = source[frac >> FRACBITS]; frac += fracstep;
			dest[8] = source[frac >> FRACBITS]; frac += fracstep;
			dest[12] = source[frac >> FRACBITS]; frac += fracstep;
			dest += 16;
		}
		count >>= 3;
		if (!count) return;

		do
		{
			dest[0] = source[frac >> FRACBITS]; frac += fracstep;
			dest[4] = source[frac >> FRACBITS]; frac += fracstep;
			dest[8] = source[frac >> FRACBITS]; frac += fracstep;
			dest[12] = source[frac >> FRACBITS]; frac += fracstep;
			dest[16] = source[frac >> FRACBITS]; frac += fracstep;
			dest[20] = source[frac >> FRACBITS]; frac += fracstep;
			dest[24] = source[frac >> FRACBITS]; frac += fracstep;
			dest[28] = source[frac >> FRACBITS]; frac += fracstep;
			dest += 32;
		} while (--count);
	}

	template<typename InputPixelType>
	FString DrawColumnHorizRGBACommand<InputPixelType>::DebugInfo()
	{
		return "DrawColumnHoriz";
	}

	// Generate code for the versions we use:
	template class DrawColumnHorizRGBACommand<uint8_t>;
	template class DrawColumnHorizRGBACommand<uint32_t>;

	/////////////////////////////////////////////////////////////////////////////

	FillColumnHorizRGBACommand::FillColumnHorizRGBACommand()
	{
		using namespace drawerargs;

		_x = dc_x;
		_count = dc_count;
		_color = GPalette.BaseColors[dc_color].d | (uint32_t)0xff000000;
		_yl = dc_yl;
		_yh = dc_yh;
	}

	void FillColumnHorizRGBACommand::Execute(DrawerThread *thread)
	{
		int count = _count;
		uint32_t color = _color;
		uint32_t *dest;

		if (count <= 0)
			return;

		{
			int x = _x & 3;
			dest = &thread->dc_temp_rgba[x + 4 * _yl];
		}

		if (count & 1) {
			*dest = color;
			dest += 4;
		}
		if (!(count >>= 1))
			return;
		do {
			dest[0] = color; dest[4] = color;
			dest += 8;
		} while (--count);
	}

	FString FillColumnHorizRGBACommand::DebugInfo()
	{
		return "FillColumnHoriz";
	}
}
