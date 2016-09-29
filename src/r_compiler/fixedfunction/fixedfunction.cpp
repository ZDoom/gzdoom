
#include "i_system.h"
#include "r_compiler/fixedfunction/fixedfunction.h"
#include "r_compiler/ssa/ssa_function.h"
#include "r_compiler/ssa/ssa_scope.h"
#include "r_compiler/ssa/ssa_for_block.h"
#include "r_compiler/ssa/ssa_if_block.h"
#include "r_compiler/ssa/ssa_stack.h"
#include "r_compiler/ssa/ssa_function.h"
#include "r_compiler/ssa/ssa_struct_type.h"
#include "r_compiler/ssa/ssa_value.h"
#include "r_compiler/ssa/ssa_barycentric_weight.h"

void DrawSpanCodegen::Generate(SSAValue args)
{
	destorg = args[0][0].load();
	source = args[0][1].load();
	destpitch = args[0][2].load();
	stack_xfrac.store(args[0][3].load());
	stack_yfrac.store(args[0][4].load());
	xstep = args[0][5].load();
	ystep = args[0][6].load();
	x1 = args[0][7].load();
	x2 = args[0][8].load();
	y = args[0][9].load();
	xbits = args[0][10].load();
	ybits = args[0][11].load();
	light = args[0][12].load();
	srcalpha = args[0][13].load();
	destalpha = args[0][14].load();
	SSAShort light_alpha = args[0][15].load();
	SSAShort light_red = args[0][16].load();
	SSAShort light_green = args[0][17].load();
	SSAShort light_blue = args[0][18].load();
	SSAShort fade_alpha = args[0][19].load();
	SSAShort fade_red = args[0][20].load();
	SSAShort fade_green = args[0][21].load();
	SSAShort fade_blue = args[0][22].load();
	SSAShort desaturate = args[0][23].load();
	SSAInt flags = args[0][24].load();
	shade_constants.light = SSAVec4i(light_blue.zext_int(), light_green.zext_int(), light_red.zext_int(), light_alpha.zext_int());
	shade_constants.fade = SSAVec4i(fade_blue.zext_int(), fade_green.zext_int(), fade_red.zext_int(), fade_alpha.zext_int());
	shade_constants.desaturate = desaturate.zext_int();

	count = x2 - x1 + 1;
	data = destorg[(x1 + y * destpitch) * 4];

	yshift = 32 - ybits;
	xshift = yshift - xbits;
	xmask = ((SSAInt(1) << xbits) - 1) << ybits;

	// 64x64 is the most common case by far, so special case it.
	is_64x64 = xbits == 6 && ybits == 6;
	is_simple_shade = (flags & RenderArgs::simple_shade) == RenderArgs::simple_shade;
	is_nearest_filter = (flags & RenderArgs::nearest_filter) == RenderArgs::nearest_filter;

	SSAIfBlock branch;
	branch.if_block(is_simple_shade);
	LoopShade(true);
	branch.else_block();
	LoopShade(false);
	branch.end_block();
}

void DrawSpanCodegen::LoopShade(bool isSimpleShade)
{
	SSAIfBlock branch;
	branch.if_block(is_nearest_filter);
	LoopFilter(isSimpleShade, true);
	branch.else_block();
	LoopFilter(isSimpleShade, false);
	branch.end_block();
}

void DrawSpanCodegen::LoopFilter(bool isSimpleShade, bool isNearestFilter)
{
	SSAIfBlock branch;
	branch.if_block(is_64x64);
	{
		SSAInt sseLength = Loop4x(isSimpleShade, isNearestFilter, true);
		Loop(sseLength * 4, isSimpleShade, isNearestFilter, true);
	}
	branch.else_block();
	{
		SSAInt sseLength = Loop4x(isSimpleShade, isNearestFilter, false);
		Loop(sseLength * 4, isSimpleShade, isNearestFilter, false);
	}
	branch.end_block();
}

SSAInt DrawSpanCodegen::Loop4x(bool isSimpleShade, bool isNearestFilter, bool is64x64)
{
	SSAInt sseLength = count / 4;
	stack_index.store(0);
	{
		SSAForBlock loop;
		SSAInt index = stack_index.load();
		loop.loop_block(index < sseLength);

		SSAVec4i colors[4];
		for (int i = 0; i < 4; i++)
		{
			SSAInt xfrac = stack_xfrac.load();
			SSAInt yfrac = stack_yfrac.load();

			SSAVec4i fg = Sample(xfrac, yfrac, isNearestFilter, is64x64);
			if (isSimpleShade)
				colors[i] = shade_bgra_simple(fg, light);
			else
				colors[i] = shade_bgra_advanced(fg, light, shade_constants);

			stack_xfrac.store(xfrac + xstep);
			stack_yfrac.store(yfrac + ystep);
		}

		SSAVec16ub ssecolors(SSAVec8s(colors[0], colors[1]), SSAVec8s(colors[2], colors[3]));
		data[index * 16].store_unaligned_vec16ub(ssecolors);

		stack_index.store(index + 1);
		loop.end_block();
	}
	return sseLength;
}

void DrawSpanCodegen::Loop(SSAInt start, bool isSimpleShade, bool isNearestFilter, bool is64x64)
{
	stack_index.store(start);
	{
		SSAForBlock loop;
		SSAInt index = stack_index.load();
		loop.loop_block(index < count);

		SSAInt xfrac = stack_xfrac.load();
		SSAInt yfrac = stack_yfrac.load();

		SSAVec4i fg = Sample(xfrac, yfrac, isNearestFilter, is64x64);
		SSAVec4i color;
		if (isSimpleShade)
			color = shade_bgra_simple(fg, light);
		else
			color = shade_bgra_advanced(fg, light, shade_constants);

		data[index * 4].store_vec4ub(color);

		stack_index.store(index + 1);
		stack_xfrac.store(xfrac + xstep);
		stack_yfrac.store(yfrac + ystep);
		loop.end_block();
	}
}

SSAVec4i DrawSpanCodegen::Sample(SSAInt xfrac, SSAInt yfrac, bool isNearestFilter, bool is64x64)
{
	if (isNearestFilter)
	{
		SSAInt spot;
		if (is64x64)
			spot = ((xfrac >> (32 - 6 - 6))&(63 * 64)) + (yfrac >> (32 - 6));
		else
			spot = ((xfrac >> xshift) & xmask) + (yfrac >> yshift);
		return source[spot * 4].load_vec4ub();
	}
	else
	{
		if (is64x64)
		{
			return sample_linear(source, xfrac, yfrac, 26, 26);
		}
		else
		{
			return sample_linear(source, xfrac, yfrac, 32 - xbits, 32 - ybits);
		}
	}
}

/////////////////////////////////////////////////////////////////////////////

SSAInt DrawerCodegen::calc_light_multiplier(SSAInt light)
{
	return 256 - (light >> (FRACBITS - 8));
}

SSAVec4i DrawerCodegen::shade_pal_index_simple(SSAInt index, SSAInt light, SSAUBytePtr basecolors)
{
	SSAVec4i color = basecolors[index * 4].load_vec4ub(); // = GPalette.BaseColors[index];
	return shade_bgra_simple(color, light);
}

SSAVec4i DrawerCodegen::shade_pal_index_advanced(SSAInt index, SSAInt light, const SSAShadeConstants &constants, SSAUBytePtr basecolors)
{
	SSAVec4i color = basecolors[index * 4].load_vec4ub(); // = GPalette.BaseColors[index];
	return shade_bgra_advanced(color, light, constants);
}

SSAVec4i DrawerCodegen::shade_bgra_simple(SSAVec4i color, SSAInt light)
{
	color = color * light / 256;
	return color.insert(3, 255);
}

SSAVec4i DrawerCodegen::shade_bgra_advanced(SSAVec4i color, SSAInt light, const SSAShadeConstants &constants)
{
	SSAInt blue = color[0];
	SSAInt green = color[1];
	SSAInt red = color[2];
	SSAInt alpha = color[3];

	SSAInt intensity = ((red * 77 + green * 143 + blue * 37) >> 8) * constants.desaturate;

	SSAVec4i inv_light = 256 - light;
	SSAVec4i inv_desaturate = 256 - constants.desaturate;

	color = (color * inv_desaturate + intensity) / 256;
	color = (constants.fade * inv_light + color * light) / 256;
	color = (color * constants.light) / 256;

	return color.insert(3, alpha);
}

SSAVec4i DrawerCodegen::blend_copy(SSAVec4i fg)
{
	return fg;
}

SSAVec4i DrawerCodegen::blend_add(SSAVec4i fg, SSAVec4i bg, SSAInt srcalpha, SSAInt destalpha)
{
	SSAVec4i color = (fg * srcalpha + bg * destalpha) / 256;
	return color.insert(3, 255);
}

SSAVec4i DrawerCodegen::blend_sub(SSAVec4i fg, SSAVec4i bg, SSAInt srcalpha, SSAInt destalpha)
{
	SSAVec4i color = (bg * destalpha - fg * srcalpha) / 256;
	return color.insert(3, 255);
}

SSAVec4i DrawerCodegen::blend_revsub(SSAVec4i fg, SSAVec4i bg, SSAInt srcalpha, SSAInt destalpha)
{
	SSAVec4i color = (fg * srcalpha - bg * destalpha) / 256;
	return color.insert(3, 255);
}

SSAVec4i DrawerCodegen::blend_alpha_blend(SSAVec4i fg, SSAVec4i bg)
{
	SSAInt alpha = fg[3];
	alpha = alpha + (alpha >> 7); // // 255 -> 256
	SSAInt inv_alpha = 256 - alpha;
	SSAVec4i color = (fg * alpha + bg * inv_alpha) / 256;
	return color.insert(3, 255);
}

SSAVec4i DrawerCodegen::sample_linear(SSAUBytePtr col0, SSAUBytePtr col1, SSAInt texturefracx, SSAInt texturefracy, SSAInt one, SSAInt height)
{
	SSAInt frac_y0 = (texturefracy >> FRACBITS) * height;
	SSAInt frac_y1 = ((texturefracy + one) >> FRACBITS) * height;
	SSAInt y0 = frac_y0 >> FRACBITS;
	SSAInt y1 = frac_y1 >> FRACBITS;

	SSAVec4i p00 = col0[y0].load_vec4ub();
	SSAVec4i p01 = col0[y1].load_vec4ub();
	SSAVec4i p10 = col1[y0].load_vec4ub();
	SSAVec4i p11 = col1[y1].load_vec4ub();

	SSAInt inv_b = texturefracx;
	SSAInt inv_a = (frac_y1 >> (FRACBITS - 4)) & 15;
	SSAInt a = 16 - inv_a;
	SSAInt b = 16 - inv_b;

	return (p00 * (a * b) + p01 * (inv_a * b) + p10 * (a * inv_b) + p11 * (inv_a * inv_b) + 127) >> 8;
}

SSAVec4i DrawerCodegen::sample_linear(SSAUBytePtr texture, SSAInt xfrac, SSAInt yfrac, SSAInt xbits, SSAInt ybits)
{
	SSAInt xshift = (32 - xbits);
	SSAInt yshift = (32 - ybits);
	SSAInt xmask = (SSAInt(1) << xshift) - 1;
	SSAInt ymask = (SSAInt(1) << yshift) - 1;
	SSAInt x = xfrac >> xbits;
	SSAInt y = yfrac >> ybits;

	SSAVec4i p00 = texture[(y & ymask) + ((x & xmask) << yshift)].load_vec4ub();
	SSAVec4i p01 = texture[((y + 1) & ymask) + ((x & xmask) << yshift)].load_vec4ub();
	SSAVec4i p10 = texture[(y & ymask) + (((x + 1) & xmask) << yshift)].load_vec4ub();
	SSAVec4i p11 = texture[((y + 1) & ymask) + (((x + 1) & xmask) << yshift)].load_vec4ub();

	SSAInt inv_b = (xfrac >> (xbits - 4)) & 15;
	SSAInt inv_a = (yfrac >> (ybits - 4)) & 15;
	SSAInt a = 16 - inv_a;
	SSAInt b = 16 - inv_b;

	return (p00 * (a * b) + p01 * (inv_a * b) + p10 * (a * inv_b) + p11 * (inv_a * inv_b) + 127) >> 8;
}
