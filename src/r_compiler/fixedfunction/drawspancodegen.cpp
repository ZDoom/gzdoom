
#include "i_system.h"
#include "r_compiler/llvm_include.h"
#include "r_compiler/fixedfunction/drawspancodegen.h"
#include "r_compiler/ssa/ssa_function.h"
#include "r_compiler/ssa/ssa_scope.h"
#include "r_compiler/ssa/ssa_for_block.h"
#include "r_compiler/ssa/ssa_if_block.h"
#include "r_compiler/ssa/ssa_stack.h"
#include "r_compiler/ssa/ssa_function.h"
#include "r_compiler/ssa/ssa_struct_type.h"
#include "r_compiler/ssa/ssa_value.h"

void DrawSpanCodegen::Generate(DrawSpanVariant variant, SSAValue args)
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
	{
		SSAForBlock loop;
		SSAInt index = stack_index.load();
		loop.loop_block(index < sseLength);

		SSAVec16ub bg = data[index * 16].load_unaligned_vec16ub();
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

			colors[i] = Blend(Shade(Sample(xfrac, yfrac, isNearestFilter, is64x64), isSimpleShade), bgcolors[i], variant);

			stack_xfrac.store(xfrac + xstep);
			stack_yfrac.store(yfrac + ystep);
		}

		SSAVec16ub color(SSAVec8s(colors[0], colors[1]), SSAVec8s(colors[2], colors[3]));
		data[index * 16].store_unaligned_vec16ub(color);

		stack_index.store(index + 1);
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
		loop.loop_block(index < count);

		SSAInt xfrac = stack_xfrac.load();
		SSAInt yfrac = stack_yfrac.load();

		SSAVec4i bgcolor = data[index * 4].load_vec4ub();
		SSAVec4i color = Blend(Shade(Sample(xfrac, yfrac, isNearestFilter, is64x64), isSimpleShade), bgcolor, variant);
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
			return sample_linear(source, xfrac, yfrac, SSAInt(26), SSAInt(26));
		}
		else
		{
			return sample_linear(source, xfrac, yfrac, 32 - xbits, 32 - ybits);
		}
	}
}

SSAVec4i DrawSpanCodegen::Shade(SSAVec4i fg, bool isSimpleShade)
{
	if (isSimpleShade)
		return shade_bgra_simple(fg, light);
	else
		return shade_bgra_advanced(fg, light, shade_constants);
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
