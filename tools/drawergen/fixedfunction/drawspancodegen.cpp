/*
**  DrawSpan code generation
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

#include "precomp.h"
#include "timestamp.h"
#include "fixedfunction/drawspancodegen.h"
#include "ssa/ssa_function.h"
#include "ssa/ssa_scope.h"
#include "ssa/ssa_for_block.h"
#include "ssa/ssa_if_block.h"
#include "ssa/ssa_stack.h"
#include "ssa/ssa_function.h"
#include "ssa/ssa_struct_type.h"
#include "ssa/ssa_value.h"

void DrawSpanCodegen::Generate(DrawSpanVariant variant, SSAValue args)
{
	destorg = args[0][0].load(true);
	source = args[0][1].load(true);
	destpitch = args[0][2].load(true);
	stack_xfrac.store(args[0][3].load(true));
	stack_yfrac.store(args[0][4].load(true));
	xstep = args[0][5].load(true);
	ystep = args[0][6].load(true);
	x1 = args[0][7].load(true);
	x2 = args[0][8].load(true);
	y = args[0][9].load(true);
	xbits = args[0][10].load(true);
	ybits = args[0][11].load(true);
	light = args[0][12].load(true);
	srcalpha = args[0][13].load(true);
	destalpha = args[0][14].load(true);
	SSAShort light_alpha = args[0][15].load(true);
	SSAShort light_red = args[0][16].load(true);
	SSAShort light_green = args[0][17].load(true);
	SSAShort light_blue = args[0][18].load(true);
	SSAShort fade_alpha = args[0][19].load(true);
	SSAShort fade_red = args[0][20].load(true);
	SSAShort fade_green = args[0][21].load(true);
	SSAShort fade_blue = args[0][22].load(true);
	SSAShort desaturate = args[0][23].load(true);
	SSAInt flags = args[0][24].load(true);
	start_viewpos_x = args[0][25].load(true);
	step_viewpos_x = args[0][26].load(true);
	dynlights = args[0][27].load(true);
	num_dynlights = args[0][28].load(true);
	shade_constants.light = SSAVec4i(light_blue.zext_int(), light_green.zext_int(), light_red.zext_int(), light_alpha.zext_int());
	shade_constants.fade = SSAVec4i(fade_blue.zext_int(), fade_green.zext_int(), fade_red.zext_int(), fade_alpha.zext_int());
	shade_constants.desaturate = desaturate.zext_int();

	count = x2 - x1 + 1;
	data = destorg[(x1 + y * destpitch) * 4];

	yshift = 32 - ybits;
	xshift = yshift - xbits;
	xmask = ((SSAInt(1) << xbits) - 1) << ybits;

	// 64x64 is the most common case by far, so special case it.
	is_64x64 = xbits == SSAInt(6) && ybits == SSAInt(6);
	is_simple_shade = (flags & DrawSpanArgs::simple_shade) == SSAInt(DrawSpanArgs::simple_shade);
	is_nearest_filter = (flags & DrawSpanArgs::nearest_filter) == SSAInt(DrawSpanArgs::nearest_filter);

	SSAIfBlock branch;
	branch.if_block(is_simple_shade);
	LoopShade(variant, true);
	branch.else_block();
	LoopShade(variant, false);
	branch.end_block();
}

void DrawSpanCodegen::LoopShade(DrawSpanVariant variant, bool isSimpleShade)
{
	SSAIfBlock branch;
	branch.if_block(is_nearest_filter);
	LoopFilter(variant, isSimpleShade, true);
	branch.else_block();
	stack_xfrac.store(stack_xfrac.load() - (SSAInt(1) << (31 - xbits)));
	stack_yfrac.store(stack_yfrac.load() - (SSAInt(1) << (31 - ybits)));
	LoopFilter(variant, isSimpleShade, false);
	branch.end_block();
}

void DrawSpanCodegen::LoopFilter(DrawSpanVariant variant, bool isSimpleShade, bool isNearestFilter)
{
	SSAIfBlock branch;
	branch.if_block(is_64x64);
	{
		SSAInt sseLength = Loop4x(variant, isSimpleShade, isNearestFilter, true);
		Loop(sseLength * 4, variant, isSimpleShade, isNearestFilter, true);
	}
	branch.else_block();
	{
		SSAInt sseLength = Loop4x(variant, isSimpleShade, isNearestFilter, false);
		Loop(sseLength * 4, variant, isSimpleShade, isNearestFilter, false);
	}
	branch.end_block();
}

SSAInt DrawSpanCodegen::Loop4x(DrawSpanVariant variant, bool isSimpleShade, bool isNearestFilter, bool is64x64)
{
	SSAInt sseLength = count / 4;
	stack_index.store(SSAInt(0));
	stack_viewpos_x.store(start_viewpos_x);
	{
		SSAForBlock loop;
		SSAInt index = stack_index.load();
		loop.loop_block(index < sseLength);

		SSAVec16ub bg = data[index * 16].load_unaligned_vec16ub(false);
		SSAVec8s bg0 = SSAVec8s::extendlo(bg);
		SSAVec8s bg1 = SSAVec8s::extendhi(bg);
		SSAVec4i bgcolors[4] =
		{
			SSAVec4i::extendlo(bg0),
			SSAVec4i::extendhi(bg0),
			SSAVec4i::extendlo(bg1),
			SSAVec4i::extendhi(bg1)
		};

		SSAVec4i colors[4];
		for (int i = 0; i < 4; i++)
		{
			SSAInt xfrac = stack_xfrac.load();
			SSAInt yfrac = stack_yfrac.load();
			viewpos_x = stack_viewpos_x.load();

			colors[i] = Blend(Shade(Sample(xfrac, yfrac, isNearestFilter, is64x64), isSimpleShade), bgcolors[i], variant);

			stack_viewpos_x.store(viewpos_x + step_viewpos_x);
			stack_xfrac.store(xfrac + xstep);
			stack_yfrac.store(yfrac + ystep);
		}

		SSAVec16ub color(SSAVec8s(colors[0], colors[1]), SSAVec8s(colors[2], colors[3]));
		data[index * 16].store_unaligned_vec16ub(color);

		stack_index.store(index.add(SSAInt(1), true, true));
		loop.end_block();
	}
	return sseLength;
}

void DrawSpanCodegen::Loop(SSAInt start, DrawSpanVariant variant, bool isSimpleShade, bool isNearestFilter, bool is64x64)
{
	stack_index.store(start);
	{
		SSAForBlock loop;
		SSAInt index = stack_index.load();
		viewpos_x = stack_viewpos_x.load();
		loop.loop_block(index < count);

		SSAInt xfrac = stack_xfrac.load();
		SSAInt yfrac = stack_yfrac.load();

		SSAVec4i bgcolor = data[index * 4].load_vec4ub(false);
		SSAVec4i color = Blend(Shade(Sample(xfrac, yfrac, isNearestFilter, is64x64), isSimpleShade), bgcolor, variant);
		data[index * 4].store_vec4ub(color);

		stack_viewpos_x.store(viewpos_x + step_viewpos_x);
		stack_index.store(index.add(SSAInt(1), true, true));
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
		return source[spot * 4].load_vec4ub(true);
	}
	else
	{
		if (is64x64)
		{
			return SampleLinear(source, xfrac, yfrac, SSAInt(26), SSAInt(26));
		}
		else
		{
			return SampleLinear(source, xfrac, yfrac, 32 - xbits, 32 - ybits);
		}
	}
}

SSAVec4i DrawSpanCodegen::SampleLinear(SSAUBytePtr texture, SSAInt xfrac, SSAInt yfrac, SSAInt xbits, SSAInt ybits)
{
	SSAInt xshift = (32 - xbits);
	SSAInt yshift = (32 - ybits);
	SSAInt xmask = (SSAInt(1) << xshift) - 1;
	SSAInt ymask = (SSAInt(1) << yshift) - 1;
	SSAInt x = xfrac >> xbits;
	SSAInt y = yfrac >> ybits;

	SSAVec4i p00 = texture[((y & ymask) + ((x & xmask) << yshift)) * 4].load_vec4ub(true);
	SSAVec4i p01 = texture[(((y + 1) & ymask) + ((x & xmask) << yshift)) * 4].load_vec4ub(true);
	SSAVec4i p10 = texture[((y & ymask) + (((x + 1) & xmask) << yshift)) * 4].load_vec4ub(true);
	SSAVec4i p11 = texture[(((y + 1) & ymask) + (((x + 1) & xmask) << yshift)) * 4].load_vec4ub(true);

	SSAInt inv_b = (xfrac >> (xbits - 4)) & 15;
	SSAInt inv_a = (yfrac >> (ybits - 4)) & 15;
	SSAInt a = 16 - inv_a;
	SSAInt b = 16 - inv_b;

	return (p00 * (a * b) + p01 * (inv_a * b) + p10 * (a * inv_b) + p11 * (inv_a * inv_b) + 127) >> 8;
}

SSAVec4i DrawSpanCodegen::Shade(SSAVec4i fg, bool isSimpleShade)
{
	SSAVec4i c;
	if (isSimpleShade)
		c = shade_bgra_simple(fg, light);
	else
		c = shade_bgra_advanced(fg, light, shade_constants);

	stack_lit_color.store(c);
	stack_light_index.store(SSAInt(0));

	SSAForBlock block;
	SSAInt light_index = stack_light_index.load();
	SSAVec4i lit_color = stack_lit_color.load();
	block.loop_block(light_index < num_dynlights);
	{
		SSAVec4i light_color = SSAUBytePtr(SSAValue(dynlights[light_index][0]).v).load_vec4ub(true);
		SSAFloat light_x = dynlights[light_index][1].load(true);
		SSAFloat light_y = dynlights[light_index][2].load(true);
		SSAFloat light_z = dynlights[light_index][3].load(true);
		SSAFloat light_rcp_radius = dynlights[light_index][4].load(true);

		// L = light-pos
		// dist = sqrt(dot(L, L))
		// attenuation = 1 - MIN(dist * (1/radius), 1)
		SSAFloat Lyz2 = light_y; // L.y*L.y + L.z*L.z
		SSAFloat Lx = light_x - viewpos_x;
		SSAFloat dist2 = Lyz2 + Lx * Lx;
		SSAFloat rcp_dist = SSAFloat::rsqrt(dist2);
		SSAFloat dist = dist2 * rcp_dist;
		SSAFloat distance_attenuation = SSAFloat(256.0f) - SSAFloat::MIN(dist * light_rcp_radius, SSAFloat(256.0f));

		// The simple light type
		SSAFloat simple_attenuation = distance_attenuation;

		// The point light type
		// diffuse = dot(N,L) * attenuation
		SSAFloat point_attenuation = light_z * rcp_dist * distance_attenuation;

		SSAInt attenuation = SSAInt((light_z == SSAFloat(0.0f)).select(simple_attenuation, point_attenuation), true);
		SSAVec4i contribution = (light_color * fg * attenuation) >> 16;

		stack_lit_color.store(lit_color + contribution);
		stack_light_index.store(light_index + 1);
	}
	block.end_block();

	return stack_lit_color.load();
}

SSAVec4i DrawSpanCodegen::Blend(SSAVec4i fg, SSAVec4i bg, DrawSpanVariant variant)
{
	switch (variant)
	{
	default:
	case DrawSpanVariant::Opaque:
		return blend_copy(fg);
	case DrawSpanVariant::Masked:
		return blend_alpha_blend(fg, bg);
	case DrawSpanVariant::Translucent:
	case DrawSpanVariant::AddClamp:
		return blend_add(fg, bg, srcalpha, destalpha);
	case DrawSpanVariant::MaskedTranslucent:
	case DrawSpanVariant::MaskedAddClamp:
		return blend_add(fg, bg, srcalpha, calc_blend_bgalpha(fg, destalpha));
	}
}
