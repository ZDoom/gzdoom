/*
**  DrawColumn code generation
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
#include "fixedfunction/drawcolumncodegen.h"
#include "ssa/ssa_function.h"
#include "ssa/ssa_scope.h"
#include "ssa/ssa_for_block.h"
#include "ssa/ssa_if_block.h"
#include "ssa/ssa_stack.h"
#include "ssa/ssa_function.h"
#include "ssa/ssa_struct_type.h"
#include "ssa/ssa_value.h"

void DrawColumnCodegen::Generate(DrawColumnVariant variant, SSAValue args, SSAValue thread_data)
{
	dest = args[0][0].load(true);
	source = args[0][1].load(true);
	source2 = args[0][2].load(true);
	colormap = args[0][3].load(true);
	translation = args[0][4].load(true);
	basecolors = args[0][5].load(true);
	pitch = args[0][6].load(true);
	count = args[0][7].load(true);
	dest_y = args[0][8].load(true);
	iscale = args[0][9].load(true);
	texturefracx = args[0][10].load(true);
	textureheight = args[0][11].load(true);
	texturefrac = args[0][12].load(true);
	light = args[0][13].load(true);
	color = SSAVec4i::unpack(args[0][14].load(true));
	srccolor = SSAVec4i::unpack(args[0][15].load(true));
	srcalpha = args[0][16].load(true);
	destalpha = args[0][17].load(true);
	SSAShort light_alpha = args[0][18].load(true);
	SSAShort light_red = args[0][19].load(true);
	SSAShort light_green = args[0][20].load(true);
	SSAShort light_blue = args[0][21].load(true);
	SSAShort fade_alpha = args[0][22].load(true);
	SSAShort fade_red = args[0][23].load(true);
	SSAShort fade_green = args[0][24].load(true);
	SSAShort fade_blue = args[0][25].load(true);
	SSAShort desaturate = args[0][26].load(true);
	SSAInt flags = args[0][27].load(true);
	shade_constants.light = SSAVec4i(light_blue.zext_int(), light_green.zext_int(), light_red.zext_int(), light_alpha.zext_int());
	shade_constants.fade = SSAVec4i(fade_blue.zext_int(), fade_green.zext_int(), fade_red.zext_int(), fade_alpha.zext_int());
	shade_constants.desaturate = desaturate.zext_int();

	thread.core = thread_data[0][0].load(true);
	thread.num_cores = thread_data[0][1].load(true);
	thread.pass_start_y = thread_data[0][2].load(true);
	thread.pass_end_y = thread_data[0][3].load(true);
	thread.temp = thread_data[0][4].load(true);

	is_simple_shade = (flags & DrawColumnArgs::simple_shade) == SSAInt(DrawColumnArgs::simple_shade);
	is_nearest_filter = (flags & DrawColumnArgs::nearest_filter) == SSAInt(DrawColumnArgs::nearest_filter);

	count = count_for_thread(dest_y, count, thread);
	dest = dest_for_thread(dest_y, pitch, dest, thread);
	pitch = pitch * thread.num_cores;

	stack_frac.store(texturefrac + iscale * skipped_by_thread(dest_y, thread));
	iscale = iscale * thread.num_cores;
	one = (1 << 30) / textureheight;

	SSAIfBlock branch;
	branch.if_block(is_simple_shade);
	LoopShade(variant, true);
	branch.else_block();
	LoopShade(variant, false);
	branch.end_block();
}

void DrawColumnCodegen::LoopShade(DrawColumnVariant variant, bool isSimpleShade)
{
	SSAIfBlock branch;
	branch.if_block(is_nearest_filter);
	Loop(variant, isSimpleShade, true);
	branch.else_block();
	stack_frac.store(stack_frac.load() - (one >> 1));
	Loop(variant, isSimpleShade, false);
	branch.end_block();
}

void DrawColumnCodegen::Loop(DrawColumnVariant variant, bool isSimpleShade, bool isNearestFilter)
{
	stack_index.store(SSAInt(0));
	{
		SSAForBlock loop;
		SSAInt index = stack_index.load();
		loop.loop_block(index < count);

		SSAInt sample_index, frac;
		frac = stack_frac.load();
		if (IsPaletteInput(variant))
			sample_index = frac >> FRACBITS;
		else
			sample_index = frac;

		SSAInt offset = index * pitch * 4;
		SSAVec4i bgcolor = dest[offset].load_vec4ub(false);

		SSAVec4i outcolor = ProcessPixel(sample_index, bgcolor, variant, isSimpleShade, isNearestFilter);

		dest[offset].store_vec4ub(outcolor);

		stack_index.store(index.add(SSAInt(1), true, true));
		stack_frac.store(frac + iscale);
		loop.end_block();
	}
}

bool DrawColumnCodegen::IsPaletteInput(DrawColumnVariant variant)
{
	switch (variant)
	{
	default:
	case DrawColumnVariant::DrawCopy:
	case DrawColumnVariant::Draw:
	case DrawColumnVariant::DrawAdd:
	case DrawColumnVariant::DrawAddClamp:
	case DrawColumnVariant::DrawSubClamp:
	case DrawColumnVariant::DrawRevSubClamp:
	case DrawColumnVariant::Fill:
	case DrawColumnVariant::FillAdd:
	case DrawColumnVariant::FillAddClamp:
	case DrawColumnVariant::FillSubClamp:
	case DrawColumnVariant::FillRevSubClamp:
		return false;
	case DrawColumnVariant::DrawShaded:
	case DrawColumnVariant::DrawTranslated:
	case DrawColumnVariant::DrawTlatedAdd:
	case DrawColumnVariant::DrawAddClampTranslated:
	case DrawColumnVariant::DrawSubClampTranslated:
	case DrawColumnVariant::DrawRevSubClampTranslated:
		return true;
	}
}

SSAVec4i DrawColumnCodegen::ProcessPixel(SSAInt sample_index, SSAVec4i bgcolor, DrawColumnVariant variant, bool isSimpleShade, bool isNearestFilter)
{
	SSAInt alpha, inv_alpha;
	SSAVec4i fg;
	switch (variant)
	{
	default:
	case DrawColumnVariant::DrawCopy:
		return blend_copy(Sample(sample_index, isNearestFilter));
	case DrawColumnVariant::Draw:
		return blend_copy(Shade(Sample(sample_index, isNearestFilter), isSimpleShade));
	case DrawColumnVariant::DrawAdd:
	case DrawColumnVariant::DrawAddClamp:
		fg = Shade(Sample(sample_index, isNearestFilter), isSimpleShade);
		return blend_add(fg, bgcolor, srcalpha, calc_blend_bgalpha(fg, destalpha));
	case DrawColumnVariant::DrawShaded:
		alpha = SSAInt::MAX(SSAInt::MIN(ColormapSample(sample_index), SSAInt(64)), SSAInt(0)) * 4;
		inv_alpha = 256 - alpha;
		return blend_add(color, bgcolor, alpha, inv_alpha);
	case DrawColumnVariant::DrawSubClamp:
		fg = Shade(Sample(sample_index, isNearestFilter), isSimpleShade);
		return blend_sub(fg, bgcolor, srcalpha, calc_blend_bgalpha(fg, destalpha));
	case DrawColumnVariant::DrawRevSubClamp:
		fg = Shade(Sample(sample_index, isNearestFilter), isSimpleShade);
		return blend_revsub(fg, bgcolor, srcalpha, calc_blend_bgalpha(fg, destalpha));
	case DrawColumnVariant::DrawTranslated:
		return blend_copy(Shade(TranslateSample(sample_index), isSimpleShade));
	case DrawColumnVariant::DrawTlatedAdd:
	case DrawColumnVariant::DrawAddClampTranslated:
		fg = Shade(TranslateSample(sample_index), isSimpleShade);
		return blend_add(fg, bgcolor, srcalpha, calc_blend_bgalpha(fg, destalpha));
	case DrawColumnVariant::DrawSubClampTranslated:
		fg = Shade(TranslateSample(sample_index), isSimpleShade);
		return blend_sub(fg, bgcolor, srcalpha, calc_blend_bgalpha(fg, destalpha));
	case DrawColumnVariant::DrawRevSubClampTranslated:
		fg = Shade(TranslateSample(sample_index), isSimpleShade);
		return blend_revsub(fg, bgcolor, srcalpha, calc_blend_bgalpha(fg, destalpha));
	case DrawColumnVariant::Fill:
		return blend_copy(color);
	case DrawColumnVariant::FillAdd:
		alpha = srccolor[3];
		alpha = alpha + (alpha >> 7);
		inv_alpha = 256 - alpha;
		return blend_add(srccolor, bgcolor, alpha, inv_alpha);
	case DrawColumnVariant::FillAddClamp:
		return blend_add(srccolor, bgcolor, srcalpha, destalpha);
	case DrawColumnVariant::FillSubClamp:
		return blend_sub(srccolor, bgcolor, srcalpha, destalpha);
	case DrawColumnVariant::FillRevSubClamp:
		return blend_revsub(srccolor, bgcolor, srcalpha, destalpha);
	}
}

SSAVec4i DrawColumnCodegen::ProcessPixelPal(SSAInt sample_index, SSAVec4i bgcolor, DrawColumnVariant variant, bool isSimpleShade)
{
	SSAInt alpha, inv_alpha;
	switch (variant)
	{
	default:
	case DrawColumnVariant::DrawCopy:
		return blend_copy(basecolors[ColormapSample(sample_index) * 4].load_vec4ub(true));
	case DrawColumnVariant::Draw:
		return blend_copy(ShadePal(ColormapSample(sample_index), isSimpleShade));
	case DrawColumnVariant::DrawAdd:
	case DrawColumnVariant::DrawAddClamp:
		return blend_add(ShadePal(ColormapSample(sample_index), isSimpleShade), bgcolor, srcalpha, destalpha);
	case DrawColumnVariant::DrawShaded:
		alpha = SSAInt::MAX(SSAInt::MIN(ColormapSample(sample_index), SSAInt(64)), SSAInt(0)) * 4;
		inv_alpha = 256 - alpha;
		return blend_add(color, bgcolor, alpha, inv_alpha);
	case DrawColumnVariant::DrawSubClamp:
		return blend_sub(ShadePal(ColormapSample(sample_index), isSimpleShade), bgcolor, srcalpha, destalpha);
	case DrawColumnVariant::DrawRevSubClamp:
		return blend_revsub(ShadePal(ColormapSample(sample_index), isSimpleShade), bgcolor, srcalpha, destalpha);
	case DrawColumnVariant::DrawTranslated:
		return blend_copy(ShadePal(TranslateSamplePal(sample_index), isSimpleShade));
	case DrawColumnVariant::DrawTlatedAdd:
	case DrawColumnVariant::DrawAddClampTranslated:
		return blend_add(ShadePal(TranslateSamplePal(sample_index), isSimpleShade), bgcolor, srcalpha, destalpha);
	case DrawColumnVariant::DrawSubClampTranslated:
		return blend_sub(ShadePal(TranslateSamplePal(sample_index), isSimpleShade), bgcolor, srcalpha, destalpha);
	case DrawColumnVariant::DrawRevSubClampTranslated:
		return blend_revsub(ShadePal(TranslateSamplePal(sample_index), isSimpleShade), bgcolor, srcalpha, destalpha);
	case DrawColumnVariant::Fill:
		return blend_copy(color);
	case DrawColumnVariant::FillAdd:
		alpha = srccolor[3];
		alpha = alpha + (alpha >> 7);
		inv_alpha = 256 - alpha;
		return blend_add(srccolor, bgcolor, alpha, inv_alpha);
	case DrawColumnVariant::FillAddClamp:
		return blend_add(srccolor, bgcolor, srcalpha, destalpha);
	case DrawColumnVariant::FillSubClamp:
		return blend_sub(srccolor, bgcolor, srcalpha, destalpha);
	case DrawColumnVariant::FillRevSubClamp:
		return blend_revsub(srccolor, bgcolor, srcalpha, destalpha);
	}
}

SSAVec4i DrawColumnCodegen::Sample(SSAInt frac, bool isNearestFilter)
{
	if (isNearestFilter)
	{
		SSAInt sample_index = (((frac << 2) >> FRACBITS) * textureheight) >> FRACBITS;
		return source[sample_index * 4].load_vec4ub(false);
	}
	else
	{
		return SampleLinear(source, source2, texturefracx, frac, one, textureheight);
	}
}

SSAVec4i DrawColumnCodegen::SampleLinear(SSAUBytePtr col0, SSAUBytePtr col1, SSAInt texturefracx, SSAInt texturefracy, SSAInt one, SSAInt height)
{
	// Clamp to edge
	SSAInt frac_y0 = (SSAInt::MAX(SSAInt::MIN(texturefracy, SSAInt((1 << 30) - 1)), SSAInt(0)) >> (FRACBITS - 2)) * height;
	SSAInt frac_y1 = (SSAInt::MAX(SSAInt::MIN(texturefracy + one, SSAInt((1 << 30) - 1)), SSAInt(0)) >> (FRACBITS - 2)) * height;
	SSAInt y0 = frac_y0 >> FRACBITS;
	SSAInt y1 = frac_y1 >> FRACBITS;

	SSAVec4i p00 = col0[y0 * 4].load_vec4ub(true);
	SSAVec4i p01 = col0[y1 * 4].load_vec4ub(true);
	SSAVec4i p10 = col1[y0 * 4].load_vec4ub(true);
	SSAVec4i p11 = col1[y1 * 4].load_vec4ub(true);

	SSAInt inv_b = texturefracx;
	SSAInt inv_a = (frac_y1 >> (FRACBITS - 4)) & 15;
	SSAInt a = 16 - inv_a;
	SSAInt b = 16 - inv_b;

	return (p00 * (a * b) + p01 * (inv_a * b) + p10 * (a * inv_b) + p11 * (inv_a * inv_b) + 127) >> 8;
}

SSAInt DrawColumnCodegen::ColormapSample(SSAInt sample_index)
{
	return colormap[source[sample_index].load(true).zext_int()].load(true).zext_int();
}

SSAVec4i DrawColumnCodegen::TranslateSample(SSAInt sample_index)
{
	return translation[source[sample_index].load(true).zext_int() * 4].load_vec4ub(true);
}

SSAInt DrawColumnCodegen::TranslateSamplePal(SSAInt sample_index)
{
	return translation[source[sample_index].load(true).zext_int()].load(true).zext_int();
}

SSAVec4i DrawColumnCodegen::Shade(SSAVec4i fg, bool isSimpleShade)
{
	if (isSimpleShade)
		return shade_bgra_simple(fg, light);
	else
		return shade_bgra_advanced(fg, light, shade_constants);
}

SSAVec4i DrawColumnCodegen::ShadePal(SSAInt palIndex, bool isSimpleShade)
{
	if (isSimpleShade)
		return shade_pal_index_simple(palIndex, light, basecolors);
	else
		return shade_pal_index_advanced(palIndex, light, shade_constants, basecolors);
}
