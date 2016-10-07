
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

void DrawColumnCodegen::Generate(DrawColumnVariant variant, SSAValue args, SSAValue thread_data)
{
	dest = args[0][0].load();
	source = args[0][1].load();
	colormap = args[0][2].load();
	translation = args[0][3].load();
	basecolors = args[0][4].load();
	pitch = args[0][5].load();
	count = args[0][6].load();
	dest_y = args[0][7].load();
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

	is_simple_shade = (flags & DrawColumnArgs::simple_shade) == SSAInt(DrawColumnArgs::simple_shade);

	count = count_for_thread(dest_y, count, thread);
	dest = dest_for_thread(dest_y, pitch, dest, thread);
	pitch = pitch * thread.num_cores;
	stack_frac.store(texturefrac + iscale * skipped_by_thread(dest_y, thread));
	iscale = iscale * thread.num_cores;

	SSAIfBlock branch;
	branch.if_block(is_simple_shade);
	Loop(variant, true);
	branch.else_block();
	Loop(variant, false);
	branch.end_block();
}

void DrawColumnCodegen::Loop(DrawColumnVariant variant, bool isSimpleShade)
{
	stack_index.store(SSAInt(0));
	{
		SSAForBlock loop;
		SSAInt index = stack_index.load();
		loop.loop_block(index < count);

		SSAInt frac = stack_frac.load();

		SSAInt offset = index * pitch * 4;
		SSAVec4i bgcolor = dest[offset].load_vec4ub();

		SSAInt alpha, inv_alpha;
		SSAVec4i outcolor;
		switch (variant)
		{
		default:
		case DrawColumnVariant::Draw:
			outcolor = blend_copy(Shade(ColormapSample(frac), isSimpleShade));
			break;
		case DrawColumnVariant::DrawAdd:
		case DrawColumnVariant::DrawAddClamp:
			outcolor = blend_add(Shade(ColormapSample(frac), isSimpleShade), bgcolor, srcalpha, destalpha);
			break;
		case DrawColumnVariant::DrawShaded:
			alpha = SSAInt::MAX(SSAInt::MIN(ColormapSample(frac), SSAInt(64)), SSAInt(0)) * 4;
			inv_alpha = 256 - alpha;
			outcolor = blend_add(color, bgcolor, alpha, inv_alpha);
			break;
		case DrawColumnVariant::DrawSubClamp:
			outcolor = blend_sub(Shade(ColormapSample(frac), isSimpleShade), bgcolor, srcalpha, destalpha);
			break;
		case DrawColumnVariant::DrawRevSubClamp:
			outcolor = blend_revsub(Shade(ColormapSample(frac), isSimpleShade), bgcolor, srcalpha, destalpha);
			break;
		case DrawColumnVariant::DrawTranslated:
			outcolor = blend_copy(Shade(TranslateSample(frac), isSimpleShade));
			break;
		case DrawColumnVariant::DrawTlatedAdd:
		case DrawColumnVariant::DrawAddClampTranslated:
			outcolor = blend_add(Shade(TranslateSample(frac), isSimpleShade), bgcolor, srcalpha, destalpha);
			break;
		case DrawColumnVariant::DrawSubClampTranslated:
			outcolor = blend_sub(Shade(TranslateSample(frac), isSimpleShade), bgcolor, srcalpha, destalpha);
			break;
		case DrawColumnVariant::DrawRevSubClampTranslated:
			outcolor = blend_revsub(Shade(TranslateSample(frac), isSimpleShade), bgcolor, srcalpha, destalpha);
			break;
		case DrawColumnVariant::Fill:
			outcolor = blend_copy(color);
			break;
		case DrawColumnVariant::FillAdd:
			alpha = srccolor[3];
			alpha = alpha + (alpha >> 7);
			inv_alpha = 256 - alpha;
			outcolor = blend_add(srccolor, bgcolor, alpha, inv_alpha);
			break;
		case DrawColumnVariant::FillAddClamp:
			outcolor = blend_add(srccolor, bgcolor, srcalpha, destalpha);
			break;
		case DrawColumnVariant::FillSubClamp:
			outcolor = blend_sub(srccolor, bgcolor, srcalpha, destalpha);
			break;
		case DrawColumnVariant::FillRevSubClamp:
			outcolor = blend_revsub(srccolor, bgcolor, srcalpha, destalpha);
			break;
		}

		dest[offset].store_vec4ub(outcolor);

		stack_index.store(index + 1);
		stack_frac.store(frac + iscale);
		loop.end_block();
	}
}

SSAInt DrawColumnCodegen::ColormapSample(SSAInt frac)
{
	SSAInt sample_index = frac >> FRACBITS;
	return colormap[source[sample_index].load().zext_int()].load().zext_int();
}

SSAInt DrawColumnCodegen::TranslateSample(SSAInt frac)
{
	SSAInt sample_index = frac >> FRACBITS;
	return translation[source[sample_index].load().zext_int()].load().zext_int();
}

SSAVec4i DrawColumnCodegen::Shade(SSAInt palIndex, bool isSimpleShade)
{
	if (isSimpleShade)
		return shade_pal_index_simple(palIndex, light, basecolors);
	else
		return shade_pal_index_advanced(palIndex, light, shade_constants, basecolors);
}
