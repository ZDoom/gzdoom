/*
**  Drawer code generation
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

#include "i_system.h"
#include "r_compiler/llvm_include.h"
#include "r_compiler/fixedfunction/drawercodegen.h"
#include "r_compiler/ssa/ssa_function.h"
#include "r_compiler/ssa/ssa_scope.h"
#include "r_compiler/ssa/ssa_for_block.h"
#include "r_compiler/ssa/ssa_if_block.h"
#include "r_compiler/ssa/ssa_stack.h"
#include "r_compiler/ssa/ssa_function.h"
#include "r_compiler/ssa/ssa_struct_type.h"
#include "r_compiler/ssa/ssa_value.h"

SSABool DrawerCodegen::line_skipped_by_thread(SSAInt line, SSAWorkerThread thread)
{
	return line < thread.pass_start_y || line >= thread.pass_end_y || !(line % thread.num_cores == thread.core);
}

SSAInt DrawerCodegen::skipped_by_thread(SSAInt first_line, SSAWorkerThread thread)
{
	SSAInt pass_skip = SSAInt::MAX(thread.pass_start_y - first_line, SSAInt(0));
	SSAInt core_skip = (thread.num_cores - (first_line + pass_skip - thread.core) % thread.num_cores) % thread.num_cores;
	return pass_skip + core_skip;
}

SSAInt DrawerCodegen::count_for_thread(SSAInt first_line, SSAInt count, SSAWorkerThread thread)
{
	SSAInt lines_until_pass_end = SSAInt::MAX(thread.pass_end_y - first_line, SSAInt(0));
	count = SSAInt::MIN(count, lines_until_pass_end);
	SSAInt c = (count - skipped_by_thread(first_line, thread) + thread.num_cores - 1) / thread.num_cores;
	return SSAInt::MAX(c, SSAInt(0));
}

SSAUBytePtr DrawerCodegen::dest_for_thread(SSAInt first_line, SSAInt pitch, SSAUBytePtr dest, SSAWorkerThread thread)
{
	return dest[skipped_by_thread(first_line, thread) * pitch * 4];
}

SSAInt DrawerCodegen::calc_light_multiplier(SSAInt light)
{
	return 256 - (light >> (FRACBITS - 8));
}

SSAVec4i DrawerCodegen::shade_pal_index_simple(SSAInt index, SSAInt light, SSAUBytePtr basecolors)
{
	SSAVec4i color = basecolors[index * 4].load_vec4ub(true); // = GPalette.BaseColors[index];
	return shade_bgra_simple(color, light);
}

SSAVec4i DrawerCodegen::shade_pal_index_advanced(SSAInt index, SSAInt light, const SSAShadeConstants &constants, SSAUBytePtr basecolors)
{
	SSAVec4i color = basecolors[index * 4].load_vec4ub(true); // = GPalette.BaseColors[index];
	return shade_bgra_advanced(color, light, constants);
}

SSAVec4i DrawerCodegen::shade_bgra_simple(SSAVec4i color, SSAInt light)
{
	SSAInt alpha = color[3];
	color = color * light / 256;
	return color.insert(3, alpha);
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

SSAInt DrawerCodegen::calc_blend_bgalpha(SSAVec4i fg, SSAInt destalpha)
{
	SSAInt alpha = fg[3];
	alpha = alpha + (alpha >> 7);
	SSAInt inv_alpha = 256 - alpha;
	return (destalpha * alpha + 256 * inv_alpha + 128) >> 8;
}
