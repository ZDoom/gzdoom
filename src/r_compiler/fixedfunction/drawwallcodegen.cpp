
#include "i_system.h"
#include "r_compiler/fixedfunction/drawwallcodegen.h"
#include "r_compiler/ssa/ssa_function.h"
#include "r_compiler/ssa/ssa_scope.h"
#include "r_compiler/ssa/ssa_for_block.h"
#include "r_compiler/ssa/ssa_if_block.h"
#include "r_compiler/ssa/ssa_stack.h"
#include "r_compiler/ssa/ssa_function.h"
#include "r_compiler/ssa/ssa_struct_type.h"
#include "r_compiler/ssa/ssa_value.h"

void DrawWallCodegen::Generate(DrawWallVariant variant, bool fourColumns, SSAValue args)
{
	dest = args[0][0].load();
	source[0] = args[0][1].load();
	source[1] = args[0][2].load();
	source[2] = args[0][3].load();
	source[3] = args[0][4].load();
	source2[0] = args[0][5].load();
	source2[1] = args[0][6].load();
	source2[2] = args[0][7].load();
	source2[3] = args[0][8].load();
	pitch = args[0][9].load();
	count = args[0][10].load();
	dest_y = args[0][11].load();
	texturefrac[0] = args[0][12].load();
	texturefrac[1] = args[0][13].load();
	texturefrac[2] = args[0][14].load();
	texturefrac[3] = args[0][15].load();
	texturefracx[0] = args[0][16].load();
	texturefracx[1] = args[0][17].load();
	texturefracx[2] = args[0][18].load();
	texturefracx[3] = args[0][19].load();
	iscale[0] = args[0][20].load();
	iscale[1] = args[0][21].load();
	iscale[2] = args[0][22].load();
	iscale[3] = args[0][23].load();
	textureheight[0] = args[0][24].load();
	textureheight[1] = args[0][25].load();
	textureheight[2] = args[0][26].load();
	textureheight[3] = args[0][27].load();
	light[0] = args[0][28].load();
	light[1] = args[0][29].load();
	light[2] = args[0][30].load();
	light[3] = args[0][31].load();
	srcalpha = args[0][32].load();
	destalpha = args[0][33].load();
	SSAShort light_alpha = args[0][34].load();
	SSAShort light_red = args[0][35].load();
	SSAShort light_green = args[0][36].load();
	SSAShort light_blue = args[0][37].load();
	SSAShort fade_alpha = args[0][38].load();
	SSAShort fade_red = args[0][39].load();
	SSAShort fade_green = args[0][40].load();
	SSAShort fade_blue = args[0][41].load();
	SSAShort desaturate = args[0][42].load();
	SSAInt flags = args[0][43].load();
	shade_constants.light = SSAVec4i(light_blue.zext_int(), light_green.zext_int(), light_red.zext_int(), light_alpha.zext_int());
	shade_constants.fade = SSAVec4i(fade_blue.zext_int(), fade_green.zext_int(), fade_red.zext_int(), fade_alpha.zext_int());
	shade_constants.desaturate = desaturate.zext_int();

	is_simple_shade = (flags & DrawWallArgs::simple_shade) == DrawWallArgs::simple_shade;
	is_nearest_filter = (flags & DrawWallArgs::nearest_filter) == DrawWallArgs::nearest_filter;

	/*
	count = thread->count_for_thread(command->_dest_y, command->_count);
	fracstep = command->_iscale * thread->num_cores;
	frac = command->_texturefrac + command->_iscale * thread->skipped_by_thread(command->_dest_y);
	texturefracx = command->_texturefracx;
	dest = thread->dest_for_thread(command->_dest_y, command->_pitch, (uint32_t*)command->_dest);
	pitch = command->_pitch * thread->num_cores;
	height = command->_textureheight;
	one = ((0x80000000 + height - 1) / height) * 2 + 1;
	*/
	int numColumns = fourColumns ? 4 : 1;
	for (int i = 0; i < numColumns; i++)
	{
		stack_frac[i].store(texturefrac[i] + iscale[i]);// * skipped_by_thread(dest_y);
		fracstep[i] = iscale[i];// * num_cores;
		one[i] = ((0x80000000 + textureheight[i] - 1) / textureheight[i]) * 2 + 1;
	}

	SSAIfBlock branch;
	branch.if_block(is_simple_shade);
	LoopShade(variant, fourColumns, true);
	branch.else_block();
	LoopShade(variant, fourColumns, false);
	branch.end_block();
}

void DrawWallCodegen::LoopShade(DrawWallVariant variant, bool fourColumns, bool isSimpleShade)
{
	SSAIfBlock branch;
	branch.if_block(is_nearest_filter);
	Loop(variant, fourColumns, isSimpleShade, true);
	branch.else_block();
	Loop(variant, fourColumns, isSimpleShade, false);
	branch.end_block();
}

void DrawWallCodegen::Loop(DrawWallVariant variant, bool fourColumns, bool isSimpleShade, bool isNearestFilter)
{
	int numColumns = fourColumns ? 4 : 1;

	stack_index.store(0);
	{
		SSAForBlock loop;
		SSAInt index = stack_index.load();
		loop.loop_block(index < count);

		SSAInt frac[4];
		for (int i = 0; i < numColumns; i++)
			frac[i] = stack_frac[i].load();

		SSAInt offset = (dest_y + index) * pitch * 4;

		if (fourColumns)
		{

		}
		else
		{
			SSAVec4i bgcolor = dest[offset].load_vec4ub();
			SSAVec4i color = Blend(Shade(Sample(frac[0], isNearestFilter), 0, isSimpleShade), bgcolor, variant);
			dest[offset].store_vec4ub(color);
		}

		stack_index.store(index + 1);
		for (int i = 0; i < numColumns; i++)
			stack_frac[i].store(frac[i] + fracstep[i]);
		loop.end_block();
	}
}

SSAVec4i DrawWallCodegen::Sample(SSAInt frac, bool isNearestFilter)
{
	// int sample_index() { return ((frac >> FRACBITS) * height) >> FRACBITS; }
	return SSAVec4i(0);
}

SSAVec4i DrawWallCodegen::Shade(SSAVec4i fg, int index, bool isSimpleShade)
{
	if (isSimpleShade)
		return shade_bgra_simple(fg, light[index]);
	else
		return shade_bgra_advanced(fg, light[index], shade_constants);
}

SSAVec4i DrawWallCodegen::Blend(SSAVec4i fg, SSAVec4i bg, DrawWallVariant variant)
{
	switch (variant)
	{
	default:
	case DrawWallVariant::Opaque:
		return blend_copy(fg);
	case DrawWallVariant::Masked:
		return blend_alpha_blend(fg, bg);
	case DrawWallVariant::Add:
	case DrawWallVariant::AddClamp:
		return blend_add(fg, bg, srcalpha, destalpha);
	case DrawWallVariant::SubClamp:
		return blend_sub(fg, bg, srcalpha, destalpha);
	case DrawWallVariant::RevSubClamp:
		return blend_revsub(fg, bg, srcalpha, destalpha);
	}
}
