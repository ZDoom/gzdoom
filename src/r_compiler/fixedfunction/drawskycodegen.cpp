
#include "i_system.h"
#include "r_compiler/llvm_include.h"
#include "r_compiler/fixedfunction/drawskycodegen.h"
#include "r_compiler/ssa/ssa_function.h"
#include "r_compiler/ssa/ssa_scope.h"
#include "r_compiler/ssa/ssa_for_block.h"
#include "r_compiler/ssa/ssa_if_block.h"
#include "r_compiler/ssa/ssa_stack.h"
#include "r_compiler/ssa/ssa_function.h"
#include "r_compiler/ssa/ssa_struct_type.h"
#include "r_compiler/ssa/ssa_value.h"

void DrawSkyCodegen::Generate(DrawSkyVariant variant, bool fourColumns, SSAValue args, SSAValue thread_data)
{
	dest = args[0][0].load(true);
	source0[0] = args[0][1].load(true);
	source0[1] = args[0][2].load(true);
	source0[2] = args[0][3].load(true);
	source0[3] = args[0][4].load(true);
	source1[0] = args[0][5].load(true);
	source1[1] = args[0][6].load(true);
	source1[2] = args[0][7].load(true);
	source1[3] = args[0][8].load(true);
	pitch = args[0][9].load(true);
	count = args[0][10].load(true);
	dest_y = args[0][11].load(true);
	texturefrac[0] = args[0][12].load(true);
	texturefrac[1] = args[0][13].load(true);
	texturefrac[2] = args[0][14].load(true);
	texturefrac[3] = args[0][15].load(true);
	iscale[0] = args[0][16].load(true);
	iscale[1] = args[0][17].load(true);
	iscale[2] = args[0][18].load(true);
	iscale[3] = args[0][19].load(true);
	textureheight0 = args[0][20].load(true);
	textureheight1 = args[0][21].load(true);
	top_color = SSAVec4i::unpack(args[0][22].load(true));
	bottom_color = SSAVec4i::unpack(args[0][23].load(true));

	thread.core = thread_data[0][0].load(true);
	thread.num_cores = thread_data[0][1].load(true);
	thread.pass_start_y = thread_data[0][2].load(true);
	thread.pass_end_y = thread_data[0][3].load(true);

	count = count_for_thread(dest_y, count, thread);
	dest = dest_for_thread(dest_y, pitch, dest, thread);

	pitch = pitch * thread.num_cores;

	int numColumns = fourColumns ? 4 : 1;
	for (int i = 0; i < numColumns; i++)
	{
		stack_frac[i].store(texturefrac[i] + iscale[i] * skipped_by_thread(dest_y, thread));
		fracstep[i] = iscale[i] * thread.num_cores;
	}

	Loop(variant, fourColumns);
}

void DrawSkyCodegen::Loop(DrawSkyVariant variant, bool fourColumns)
{
	int numColumns = fourColumns ? 4 : 1;

	stack_index.store(SSAInt(0));
	{
		SSAForBlock loop;
		SSAInt index = stack_index.load();
		loop.loop_block(index < count);

		SSAInt frac[4];
		for (int i = 0; i < numColumns; i++)
			frac[i] = stack_frac[i].load();

		SSAInt offset = index * pitch * 4;

		if (fourColumns)
		{
			SSAVec4i colors[4];
			for (int i = 0; i < 4; i++)
				colors[i] = FadeOut(frac[i], Sample(frac[i], i, variant));

			SSAVec16ub color(SSAVec8s(colors[0], colors[1]), SSAVec8s(colors[2], colors[3]));
			dest[offset].store_unaligned_vec16ub(color);
		}
		else
		{
			SSAVec4i color = FadeOut(frac[0], Sample(frac[0], 0, variant));
			dest[offset].store_vec4ub(color);
		}

		stack_index.store(index.add(SSAInt(1), true, true));
		for (int i = 0; i < numColumns; i++)
			stack_frac[i].store(frac[i] + fracstep[i]);
		loop.end_block();
	}
}

SSAVec4i DrawSkyCodegen::Sample(SSAInt frac, int index, DrawSkyVariant variant)
{
	SSAInt sample_index = (((frac << 8) >> FRACBITS) * textureheight0) >> FRACBITS;
	if (variant == DrawSkyVariant::Single)
	{
		return source0[index][sample_index * 4].load_vec4ub(false);
	}
	else
	{
		SSAInt sample_index2 = SSAInt::MIN(sample_index, textureheight1);
		SSAVec4i color0 = source0[index][sample_index * 4].load_vec4ub(false);
		SSAVec4i color1 = source1[index][sample_index2 * 4].load_vec4ub(false);
		return blend_alpha_blend(color0, color1);
	}
}

SSAVec4i DrawSkyCodegen::FadeOut(SSAInt frac, SSAVec4i color)
{
	int start_fade = 2; // How fast it should fade out

	SSAInt alpha_top = SSAInt::MAX(SSAInt::MIN(frac.ashr(16 - start_fade), SSAInt(256)), SSAInt(0));
	SSAInt alpha_bottom = SSAInt::MAX(SSAInt::MIN(((2 << 24) - frac).ashr(16 - start_fade), SSAInt(256)), SSAInt(0));
	SSAInt inv_alpha_top = 256 - alpha_top;
	SSAInt inv_alpha_bottom = 256 - alpha_bottom;

	color = (color * alpha_top + top_color * inv_alpha_top) / 256;
	color = (color * alpha_bottom + bottom_color * inv_alpha_bottom) / 256;
	return color.insert(3, 255);
}
