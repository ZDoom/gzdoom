/*
**  DrawWall code generation
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
#include "fixedfunction/drawwallcodegen.h"
#include "ssa/ssa_function.h"
#include "ssa/ssa_scope.h"
#include "ssa/ssa_for_block.h"
#include "ssa/ssa_if_block.h"
#include "ssa/ssa_stack.h"
#include "ssa/ssa_function.h"
#include "ssa/ssa_struct_type.h"
#include "ssa/ssa_value.h"

void DrawWallCodegen::Generate(DrawWallVariant variant, bool fourColumns, SSAValue args, SSAValue thread_data)
{
	dest = args[0][0].load(true);
	source[0] = args[0][1].load(true);
	source[1] = args[0][2].load(true);
	source[2] = args[0][3].load(true);
	source[3] = args[0][4].load(true);
	source2[0] = args[0][5].load(true);
	source2[1] = args[0][6].load(true);
	source2[2] = args[0][7].load(true);
	source2[3] = args[0][8].load(true);
	pitch = args[0][9].load(true);
	count = args[0][10].load(true);
	dest_y = args[0][11].load(true);
	texturefrac[0] = args[0][12].load(true);
	texturefrac[1] = args[0][13].load(true);
	texturefrac[2] = args[0][14].load(true);
	texturefrac[3] = args[0][15].load(true);
	texturefracx[0] = args[0][16].load(true);
	texturefracx[1] = args[0][17].load(true);
	texturefracx[2] = args[0][18].load(true);
	texturefracx[3] = args[0][19].load(true);
	iscale[0] = args[0][20].load(true);
	iscale[1] = args[0][21].load(true);
	iscale[2] = args[0][22].load(true);
	iscale[3] = args[0][23].load(true);
	textureheight[0] = args[0][24].load(true);
	textureheight[1] = args[0][25].load(true);
	textureheight[2] = args[0][26].load(true);
	textureheight[3] = args[0][27].load(true);
	light[0] = args[0][28].load(true);
	light[1] = args[0][29].load(true);
	light[2] = args[0][30].load(true);
	light[3] = args[0][31].load(true);
	srcalpha = args[0][32].load(true);
	destalpha = args[0][33].load(true);
	SSAShort light_alpha = args[0][34].load(true);
	SSAShort light_red = args[0][35].load(true);
	SSAShort light_green = args[0][36].load(true);
	SSAShort light_blue = args[0][37].load(true);
	SSAShort fade_alpha = args[0][38].load(true);
	SSAShort fade_red = args[0][39].load(true);
	SSAShort fade_green = args[0][40].load(true);
	SSAShort fade_blue = args[0][41].load(true);
	SSAShort desaturate = args[0][42].load(true);
	SSAInt flags = args[0][43].load(true);
	start_z = args[0][44].load(true);
	step_z = args[0][45].load(true);
	dynlights = args[0][46].load(true);
	num_dynlights = args[0][47].load(true);
	shade_constants.light = SSAVec4i(light_blue.zext_int(), light_green.zext_int(), light_red.zext_int(), light_alpha.zext_int());
	shade_constants.fade = SSAVec4i(fade_blue.zext_int(), fade_green.zext_int(), fade_red.zext_int(), fade_alpha.zext_int());
	shade_constants.desaturate = desaturate.zext_int();

	thread.core = thread_data[0][0].load(true);
	thread.num_cores = thread_data[0][1].load(true);
	thread.pass_start_y = thread_data[0][2].load(true);
	thread.pass_end_y = thread_data[0][3].load(true);

	is_simple_shade = (flags & DrawWallArgs::simple_shade) == SSAInt(DrawWallArgs::simple_shade);
	is_nearest_filter = (flags & DrawWallArgs::nearest_filter) == SSAInt(DrawWallArgs::nearest_filter);

	count = count_for_thread(dest_y, count, thread);
	dest = dest_for_thread(dest_y, pitch, dest, thread);

	pitch = pitch * thread.num_cores;

	int numColumns = fourColumns ? 4 : 1;
	for (int i = 0; i < numColumns; i++)
	{
		stack_frac[i].store(texturefrac[i] + iscale[i] * skipped_by_thread(dest_y, thread));
		fracstep[i] = iscale[i] * thread.num_cores;
		one[i] = ((0x80000000 + textureheight[i] - 1) / textureheight[i]) * 2 + 1;
	}

	start_z = start_z + step_z * SSAFloat(skipped_by_thread(dest_y, thread));
	step_z = step_z * SSAFloat(thread.num_cores);

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
	int numColumns = fourColumns ? 4 : 1;
	for (int i = 0; i < numColumns; i++)
		stack_frac[i].store(stack_frac[i].load() - (one[i] / 2));
	Loop(variant, fourColumns, isSimpleShade, false);
	branch.end_block();
}

void DrawWallCodegen::Loop(DrawWallVariant variant, bool fourColumns, bool isSimpleShade, bool isNearestFilter)
{
	int numColumns = fourColumns ? 4 : 1;

	stack_index.store(SSAInt(0));
	stack_z.store(start_z);
	{
		SSAForBlock loop;
		SSAInt index = stack_index.load();
		z = stack_z.load();
		loop.loop_block(index < count);

		SSAInt frac[4];
		for (int i = 0; i < numColumns; i++)
			frac[i] = stack_frac[i].load();

		SSAInt offset = index * pitch * 4;

		if (fourColumns)
		{
			SSAVec16ub bg = dest[offset].load_unaligned_vec16ub(false);
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
				colors[i] = Blend(Shade(Sample(frac[i], i, isNearestFilter), i, isSimpleShade), bgcolors[i], variant);

			SSAVec16ub color(SSAVec8s(colors[0], colors[1]), SSAVec8s(colors[2], colors[3]));
			dest[offset].store_unaligned_vec16ub(color);
		}
		else
		{
			SSAVec4i bgcolor = dest[offset].load_vec4ub(false);
			SSAVec4i color = Blend(Shade(Sample(frac[0], 0, isNearestFilter), 0, isSimpleShade), bgcolor, variant);
			dest[offset].store_vec4ub(color);
		}

		stack_z.store(z + step_z);
		stack_index.store(index.add(SSAInt(1), true, true));
		for (int i = 0; i < numColumns; i++)
			stack_frac[i].store(frac[i] + fracstep[i]);
		loop.end_block();
	}
}

SSAVec4i DrawWallCodegen::Sample(SSAInt frac, int index, bool isNearestFilter)
{
	if (isNearestFilter)
	{
		SSAInt sample_index = ((frac >> FRACBITS) * textureheight[index]) >> FRACBITS;
		return source[index][sample_index * 4].load_vec4ub(false);
	}
	else
	{
		return SampleLinear(source[index], source2[index], texturefracx[index], frac, one[index], textureheight[index]);
	}
}

SSAVec4i DrawWallCodegen::SampleLinear(SSAUBytePtr col0, SSAUBytePtr col1, SSAInt texturefracx, SSAInt texturefracy, SSAInt one, SSAInt height)
{
	SSAInt frac_y0 = (texturefracy >> FRACBITS) * height;
	SSAInt frac_y1 = ((texturefracy + one) >> FRACBITS) * height;
	SSAInt y0 = frac_y0 >> FRACBITS;
	SSAInt y1 = frac_y1 >> FRACBITS;

	SSAVec4i p00 = col0[y0 * 4].load_vec4ub(true);
	SSAVec4i p01 = col0[y1 * 4].load_vec4ub(true);
	SSAVec4i p10 = col1[y0 * 4].load_vec4ub(true);
	SSAVec4i p11 = col1[y1 * 4].load_vec4ub(true);

	SSAInt inv_b = texturefracx;
	SSAInt a = (frac_y1 >> (FRACBITS - 4)) & 15;
	SSAInt inv_a = 16 - a;
	SSAInt b = 16 - inv_b;

	return (p00 * (a * b) + p01 * (inv_a * b) + p10 * (a * inv_b) + p11 * (inv_a * inv_b) + 127) >> 8;
}

SSAVec4i DrawWallCodegen::Shade(SSAVec4i fg, int index, bool isSimpleShade)
{
	SSAVec4i c;
	if (isSimpleShade)
		c = shade_bgra_simple(fg, light[index]);
	else
		c = shade_bgra_advanced(fg, light[index], shade_constants);

	stack_lit_color.store(c);
	stack_light_index.store(SSAInt(0));

	SSAForBlock block;
	SSAInt light_index = stack_light_index.load();
	SSAVec4i lit_color = stack_lit_color.load();
	block.loop_block(light_index < num_dynlights);
	{
		SSAVec4i light_color = SSAUBytePtr(SSAValue(dynlights[light_index][0]).v).load_vec4ub(true);
		SSAFloat light_x = dynlights[light_index][1].load(true);
		//SSAFloat light_y = dynlights[light_index][2].load(true);
		SSAFloat light_z = dynlights[light_index][3].load(true);
		SSAFloat light_rcp_radius = dynlights[light_index][4].load(true);

		// L = light-pos
		// dist = sqrt(dot(L, L))
		// attenuation = 1 - MIN(dist * (1/radius), 1)
		SSAFloat Lxy2 = light_x; // L.x*L.x + L.y*L.y
		SSAFloat Lz = light_z - z;
		SSAFloat dist = SSAFloat::sqrt(Lxy2 + Lz * Lz);
		SSAInt attenuation = SSAInt(SSAFloat(256.0f) - SSAFloat::MIN(dist * light_rcp_radius, SSAFloat(256.0f)), true);
		SSAVec4i contribution = (light_color * fg * attenuation) >> 16;

		stack_lit_color.store(lit_color + contribution);
		stack_light_index.store(light_index + 1);
	}
	block.end_block();

	return stack_lit_color.load();
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
		return blend_add(fg, bg, srcalpha, calc_blend_bgalpha(fg, destalpha));
	case DrawWallVariant::SubClamp:
		return blend_sub(fg, bg, srcalpha, calc_blend_bgalpha(fg, destalpha));
	case DrawWallVariant::RevSubClamp:
		return blend_revsub(fg, bg, srcalpha, calc_blend_bgalpha(fg, destalpha));
	}
}
