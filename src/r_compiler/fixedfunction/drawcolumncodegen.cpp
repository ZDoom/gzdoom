
#include "i_system.h"
#include "r_compiler/llvm_include.h"
#include "r_compiler/fixedfunction/drawcolumncodegen.h"
#include "r_compiler/ssa/ssa_function.h"
#include "r_compiler/ssa/ssa_scope.h"
#include "r_compiler/ssa/ssa_for_block.h"
#include "r_compiler/ssa/ssa_if_block.h"
#include "r_compiler/ssa/ssa_stack.h"
#include "r_compiler/ssa/ssa_function.h"
#include "r_compiler/ssa/ssa_struct_type.h"
#include "r_compiler/ssa/ssa_value.h"

void DrawColumnCodegen::Generate(DrawColumnVariant variant, DrawColumnMethod method, SSAValue args, SSAValue thread_data)
{
	dest = args[0][0].load();
	source = args[0][1].load();
	colormap = args[0][2].load();
	translation = args[0][3].load();
	basecolors = args[0][4].load();
	pitch = args[0][5].load();
	count = args[0][6].load();
	dest_y = args[0][7].load();
	if (method == DrawColumnMethod::Normal)
		iscale = args[0][8].load();
	texturefrac = args[0][9].load();
	light = args[0][10].load();
	color = SSAVec4i::unpack(args[0][11].load());
	srccolor = SSAVec4i::unpack(args[0][12].load());
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

	thread.core = thread_data[0][0].load();
	thread.num_cores = thread_data[0][1].load();
	thread.pass_start_y = thread_data[0][2].load();
	thread.pass_end_y = thread_data[0][3].load();
	thread.temp = thread_data[0][4].load();

	is_simple_shade = (flags & DrawColumnArgs::simple_shade) == SSAInt(DrawColumnArgs::simple_shade);

	count = count_for_thread(dest_y, count, thread);
	dest = dest_for_thread(dest_y, pitch, dest, thread);
	pitch = pitch * thread.num_cores;
	if (method == DrawColumnMethod::Normal)
	{
		stack_frac.store(texturefrac + iscale * skipped_by_thread(dest_y, thread));
		iscale = iscale * thread.num_cores;
	}
	else
	{
		source = thread.temp[((dest_y + skipped_by_thread(dest_y, thread)) * 4 + texturefrac) * 4];
	}

	SSAIfBlock branch;
	branch.if_block(is_simple_shade);
	Loop(variant, method, true);
	branch.else_block();
	Loop(variant, method, false);
	branch.end_block();
}

void DrawColumnCodegen::Loop(DrawColumnVariant variant, DrawColumnMethod method, bool isSimpleShade)
{
	SSAInt sincr;
	if (method != DrawColumnMethod::Normal)
		sincr = thread.num_cores * 4;

	stack_index.store(SSAInt(0));
	{
		SSAForBlock loop;
		SSAInt index = stack_index.load();
		loop.loop_block(index < count);

		SSAInt sample_index, frac;
		if (method == DrawColumnMethod::Normal)
		{
			frac = stack_frac.load();
			sample_index = frac >> FRACBITS;
		}
		else
		{
			sample_index = index * sincr * 4;
		}

		SSAInt offset = index * pitch * 4;
		SSAVec4i bgcolor[4];

		int numColumns = (method == DrawColumnMethod::Rt4) ? 4 : 1;

		if (numColumns == 4)
		{
			SSAVec16ub bg = dest[offset].load_unaligned_vec16ub();
			SSAVec8s bg0 = SSAVec8s::extendlo(bg);
			SSAVec8s bg1 = SSAVec8s::extendhi(bg);
			bgcolor[0] = SSAVec4i::extendlo(bg0);
			bgcolor[1] = SSAVec4i::extendhi(bg0);
			bgcolor[2] = SSAVec4i::extendlo(bg1);
			bgcolor[3] = SSAVec4i::extendhi(bg1);
		}
		else
		{
			bgcolor[0] = dest[offset].load_vec4ub();
		}

		SSAVec4i outcolor[4];
		for (int i = 0; i < numColumns; i++)
			outcolor[i] = ProcessPixel(sample_index + i * 4, bgcolor[i], variant, isSimpleShade);

		if (numColumns == 4)
		{
			SSAVec16ub packedcolor(SSAVec8s(outcolor[0], outcolor[1]), SSAVec8s(outcolor[2], outcolor[3]));
			dest[offset].store_unaligned_vec16ub(packedcolor);
		}
		else
		{
			dest[offset].store_vec4ub(outcolor[0]);
		}

		stack_index.store(index + 1);
		if (method == DrawColumnMethod::Normal)
			stack_frac.store(frac + iscale);
		loop.end_block();
	}
}

SSAVec4i DrawColumnCodegen::ProcessPixel(SSAInt sample_index, SSAVec4i bgcolor, DrawColumnVariant variant, bool isSimpleShade)
{
	SSAInt alpha, inv_alpha;
	switch (variant)
	{
	default:
	case DrawColumnVariant::DrawCopy:
		return blend_copy(basecolors[ColormapSample(sample_index) * 4].load_vec4ub());
	case DrawColumnVariant::Draw:
		return blend_copy(Shade(ColormapSample(sample_index), isSimpleShade));
	case DrawColumnVariant::DrawAdd:
	case DrawColumnVariant::DrawAddClamp:
		return blend_add(Shade(ColormapSample(sample_index), isSimpleShade), bgcolor, srcalpha, destalpha);
	case DrawColumnVariant::DrawShaded:
		alpha = SSAInt::MAX(SSAInt::MIN(ColormapSample(sample_index), SSAInt(64)), SSAInt(0)) * 4;
		inv_alpha = 256 - alpha;
		return blend_add(color, bgcolor, alpha, inv_alpha);
	case DrawColumnVariant::DrawSubClamp:
		return blend_sub(Shade(ColormapSample(sample_index), isSimpleShade), bgcolor, srcalpha, destalpha);
	case DrawColumnVariant::DrawRevSubClamp:
		return blend_revsub(Shade(ColormapSample(sample_index), isSimpleShade), bgcolor, srcalpha, destalpha);
	case DrawColumnVariant::DrawTranslated:
		return blend_copy(Shade(TranslateSample(sample_index), isSimpleShade));
	case DrawColumnVariant::DrawTlatedAdd:
	case DrawColumnVariant::DrawAddClampTranslated:
		return blend_add(Shade(TranslateSample(sample_index), isSimpleShade), bgcolor, srcalpha, destalpha);
	case DrawColumnVariant::DrawSubClampTranslated:
		return blend_sub(Shade(TranslateSample(sample_index), isSimpleShade), bgcolor, srcalpha, destalpha);
	case DrawColumnVariant::DrawRevSubClampTranslated:
		return blend_revsub(Shade(TranslateSample(sample_index), isSimpleShade), bgcolor, srcalpha, destalpha);
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

SSAInt DrawColumnCodegen::ColormapSample(SSAInt sample_index)
{
	return colormap[source[sample_index].load().zext_int()].load().zext_int();
}

SSAInt DrawColumnCodegen::TranslateSample(SSAInt sample_index)
{
	return translation[source[sample_index].load().zext_int()].load().zext_int();
}

SSAVec4i DrawColumnCodegen::Shade(SSAInt palIndex, bool isSimpleShade)
{
	if (isSimpleShade)
		return shade_pal_index_simple(palIndex, light, basecolors);
	else
		return shade_pal_index_advanced(palIndex, light, shade_constants, basecolors);
}
